// database.cpp - SQLite database implementation for StayFinder.
#include "database.h"

#include <iostream>
#include <set>

#include "sqlite3.h"

namespace {

// Helper: throws a DatabaseError with SQLite's error description if an operation fails.
void checkSqlite(int resultCode, sqlite3* db, const std::string& action) {
    if (resultCode != SQLITE_OK && resultCode != SQLITE_ROW && resultCode != SQLITE_DONE) {
        throw DatabaseError(action + ": " + (db ? sqlite3_errmsg(db) : "unknown error"));
    }
}

// Reads one property object from the statement columns beginning at firstColumn.
Property readPropertyFromRow(sqlite3_stmt* row, int firstColumn) {
    auto text = [&](int column) {
        const unsigned char* value = sqlite3_column_text(row, firstColumn + column);
        return value ? std::string(reinterpret_cast<const char*>(value)) : std::string();
    };
    auto number = [&](int column) { return sqlite3_column_int(row, firstColumn + column); };

    // Columns: id, name, city, description, price_per_night, max_guests, image, host_name, host_email
    Host host(text(7), text(8));
    return Property(number(0), text(1), text(2), text(3), number(4), number(5), text(6), host);
}

struct SampleProperty {
    const char* name;
    const char* city;
    const char* description;
    int pricePerNight;
    int maxGuests;
    const char* image;
    const char* hostName;
    const char* hostEmail;
};

// Premium accommodation initial inventory with local high-resolution photography assets
const SampleProperty kSampleProperties[] = {
    {"Sunset Heritage Haveli", "Jaipur", "Restored 19th-century haveli with royal courtyards, carved jharokhas, and rooftop views of the Pink City.", 3200, 4, "jaipur-haveli.jpg", "Ravi Kumar", "ravi@example.com"},
    {"Marine Drive Sea-View Flat", "Mumbai", "Art-deco balcony apartment overlooking the Queen's Necklace and the Arabian Sea horizon.", 4500, 2, "mumbai-sea-flat.jpg", "Anita Rao", "anita@example.com"},
    {"Lodhi Garden Studio", "Delhi", "Sunlit boutique studio bordered by Mughal gardens, featuring high ceilings and an artisanal kitchen.", 2200, 2, "delhi-studio.jpg", "Kiran Menon", "kiran@example.com"},
    {"Old Town Heritage Room", "Delhi", "Warmly appointed haveli suite in Old Delhi with traditional teak woodwork and complimentary breakfast.", 1500, 2, "delhi-heritage-room.jpg", "Ravi Kumar", "ravi@example.com"},
    {"Cubbon Park Garden Cottage", "Bengaluru", "Private stone bungalow tucked into lush green gardens, equipped with dedicated high-speed workspace.", 2800, 3, "bengaluru-cottage.jpg", "Anita Rao", "anita@example.com"},
    {"Calangute Beach Villa", "Goa", "Coastal sanctuary featuring a private plunge pool, outdoor rain shower, and palms 3 minutes from the sand.", 6000, 6, "goa-beach-villa.jpg", "Kiran Menon", "kiran@example.com"},
    {"Solang Valley Log Cabin", "Manali", "Handcrafted deodar wood chalet with an open stone fireplace and panoramic snow-capped Himalayan vistas.", 3500, 4, "manali-log-cabin.jpg", "Ravi Kumar", "ravi@example.com"},
    {"Fort Kochi Homestay", "Kochi", "Dutch-colonial heritage retreat surrounded by spice trees, hammock verandas, and peaceful courtyard arches.", 2600, 3, "kochi-homestay.jpg", "Anita Rao", "anita@example.com"},
};

}  // namespace

// ---------------------------------------------------------------------------
// Lifecycle & RAII
// ---------------------------------------------------------------------------

Database::Database(const std::string& filePath) {
    if (sqlite3_open(filePath.c_str(), &db_) != SQLITE_OK) {
        const std::string reason = db_ ? sqlite3_errmsg(db_) : "out of memory";
        sqlite3_close(db_);
        db_ = nullptr;
        throw DatabaseError("Could not open database file '" + filePath + "': " + reason);
    }
    createTables();
    migrateSchemaIfNecessary();
    addSampleDataIfEmpty();
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Schema Setup & Safe Migrations
// ---------------------------------------------------------------------------

void Database::createTables() {
    const char* sql = R"SQL(
        CREATE TABLE IF NOT EXISTS properties (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            name            TEXT    NOT NULL,
            city            TEXT    NOT NULL,
            description     TEXT    NOT NULL,
            price_per_night INTEGER NOT NULL,
            max_guests      INTEGER NOT NULL,
            image           TEXT    NOT NULL,
            host_name       TEXT    NOT NULL,
            host_email      TEXT    NOT NULL
        );
        CREATE TABLE IF NOT EXISTS bookings (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            property_id  INTEGER NOT NULL REFERENCES properties(id),
            guest_name   TEXT    NOT NULL,
            guest_email  TEXT    NOT NULL,
            check_in     TEXT    NOT NULL,
            check_out    TEXT    NOT NULL,
            nights       INTEGER NOT NULL,
            guests       INTEGER NOT NULL DEFAULT 1,
            total_price  INTEGER NOT NULL,
            status       TEXT    NOT NULL DEFAULT 'CONFIRMED',
            created_at   TEXT    NOT NULL DEFAULT (datetime('now'))
        );
    )SQL";

    char* errorText = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errorText) != SQLITE_OK) {
        const std::string message = errorText ? errorText : "unknown error";
        sqlite3_free(errorText);
        throw DatabaseError("Could not create tables: " + message);
    }
}

void Database::migrateSchemaIfNecessary() {
    // Non-destructively inspect table structure to support preexisting databases
    sqlite3_stmt* stmt = nullptr;
    checkSqlite(sqlite3_prepare_v2(db_, "PRAGMA table_info(bookings);", -1, &stmt, nullptr),
                db_, "Could not inspect bookings table schema");

    std::set<std::string> existingColumns;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* colName = sqlite3_column_text(stmt, 1);
        if (colName) {
            existingColumns.insert(reinterpret_cast<const char*>(colName));
        }
    }
    sqlite3_finalize(stmt);

    char* errMsg = nullptr;
    if (existingColumns.find("status") == existingColumns.end()) {
        sqlite3_exec(db_, "ALTER TABLE bookings ADD COLUMN status TEXT NOT NULL DEFAULT 'CONFIRMED';",
                     nullptr, nullptr, &errMsg);
        if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }
    }
    if (existingColumns.find("guests") == existingColumns.end()) {
        sqlite3_exec(db_, "ALTER TABLE bookings ADD COLUMN guests INTEGER NOT NULL DEFAULT 1;",
                     nullptr, nullptr, &errMsg);
        if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }
    }

    // Safely upgrade existing properties image references from old .svg to modern .jpg
    sqlite3_exec(db_, "UPDATE properties SET image = REPLACE(image, '.svg', '.jpg') WHERE image LIKE '%.svg';",
                 nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); }
}

void Database::addSampleDataIfEmpty() {
    sqlite3_stmt* countStatement = nullptr;
    checkSqlite(sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM properties;", -1, &countStatement, nullptr),
                db_, "Could not count properties");
    sqlite3_step(countStatement);
    const int count = sqlite3_column_int(countStatement, 0);
    sqlite3_finalize(countStatement);
    if (count > 0) return;

    sqlite3_stmt* insert = nullptr;
    checkSqlite(sqlite3_prepare_v2(db_,
        "INSERT INTO properties (name, city, description, price_per_night, max_guests, image, host_name, host_email) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);", -1, &insert, nullptr),
        db_, "Could not prepare sample insert");

    for (const SampleProperty& sample : kSampleProperties) {
        sqlite3_bind_text(insert, 1, sample.name, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert, 2, sample.city, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert, 3, sample.description, -1, SQLITE_STATIC);
        sqlite3_bind_int(insert, 4, sample.pricePerNight);
        sqlite3_bind_int(insert, 5, sample.maxGuests);
        sqlite3_bind_text(insert, 6, sample.image, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert, 7, sample.hostName, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert, 8, sample.hostEmail, -1, SQLITE_STATIC);
        checkSqlite(sqlite3_step(insert) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR,
                    db_, "Could not insert sample property");
        sqlite3_reset(insert);
    }
    sqlite3_finalize(insert);
    std::cout << "[database] High-resolution sample property inventory created.\n";
}

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

void Database::beginTransaction() {
    char* err = nullptr;
    if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw DatabaseError("Failed to begin transaction: " + msg);
    }
}

void Database::commit() {
    char* err = nullptr;
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw DatabaseError("Failed to commit transaction: " + msg);
    }
}

void Database::rollback() {
    char* err = nullptr;
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
}

// ---------------------------------------------------------------------------
// Property Lookups & Overloaded Searches
// ---------------------------------------------------------------------------

std::vector<Property> Database::findProperties(const std::string& cityFilter) {
    const char* sql =
        "SELECT id, name, city, description, price_per_night, max_guests, image, host_name, host_email "
        "FROM properties "
        "WHERE ?1 = '' OR LOWER(city) LIKE '%' || LOWER(?1) || '%' "
        "ORDER BY id;";

    sqlite3_stmt* statement = nullptr;
    checkSqlite(sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr), db_, "Could not search properties");
    sqlite3_bind_text(statement, 1, cityFilter.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<Property> results;
    int resultCode = sqlite3_step(statement);
    while (resultCode == SQLITE_ROW) {
        results.push_back(readPropertyFromRow(statement, 0));
        resultCode = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    checkSqlite(resultCode, db_, "Reading properties failed");
    return results;
}

std::vector<Property> Database::findProperties(const std::string& cityFilter, int minGuests, int maxPrice) {
    const char* sql =
        "SELECT id, name, city, description, price_per_night, max_guests, image, host_name, host_email "
        "FROM properties "
        "WHERE (?1 = '' OR LOWER(city) LIKE '%' || LOWER(?1) || '%') "
        "  AND (?2 <= 0 OR max_guests >= ?2) "
        "  AND (?3 <= 0 OR price_per_night <= ?3) "
        "ORDER BY id;";

    sqlite3_stmt* statement = nullptr;
    checkSqlite(sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr), db_, "Could not search properties with filters");
    sqlite3_bind_text(statement, 1, cityFilter.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, minGuests);
    sqlite3_bind_int(statement, 3, maxPrice);

    std::vector<Property> results;
    int resultCode = sqlite3_step(statement);
    while (resultCode == SQLITE_ROW) {
        results.push_back(readPropertyFromRow(statement, 0));
        resultCode = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    checkSqlite(resultCode, db_, "Reading properties with filters failed");
    return results;
}

std::optional<Property> Database::findPropertyById(int id) {
    sqlite3_stmt* statement = nullptr;
    checkSqlite(sqlite3_prepare_v2(db_,
        "SELECT id, name, city, description, price_per_night, max_guests, image, host_name, host_email "
        "FROM properties WHERE id = ?1;", -1, &statement, nullptr),
        db_, "Could not look up property");
    sqlite3_bind_int(statement, 1, id);

    std::optional<Property> result;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        result = readPropertyFromRow(statement, 0);
    }
    sqlite3_finalize(statement);
    return result;
}

// ---------------------------------------------------------------------------
// Booking Business Rules & Persistence
// ---------------------------------------------------------------------------

bool Database::hasOverlap(int propertyId, const std::string& checkIn, const std::string& checkOut) {
    // Only CONFIRMED bookings lock calendar availability. CANCELLED bookings do not block dates.
    // Adjacent stays (where checkIn == previous checkOut) are allowed because check_in < checkOut AND check_out > checkIn.
    const char* sql =
        "SELECT COUNT(*) FROM bookings "
        "WHERE property_id = ?1 AND status = 'CONFIRMED' AND check_in < ?3 AND check_out > ?2;";

    sqlite3_stmt* statement = nullptr;
    checkSqlite(sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr), db_, "Could not check overlaps");
    sqlite3_bind_int(statement, 1, propertyId);
    sqlite3_bind_text(statement, 2, checkIn.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, checkOut.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(statement);
    const int count = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return count > 0;
}

int Database::insertBooking(int propertyId, const std::string& guestName, const std::string& guestEmail,
                            const std::string& checkIn, const std::string& checkOut,
                            int nights, int guests, int totalPrice) {
    sqlite3_stmt* statement = nullptr;
    checkSqlite(sqlite3_prepare_v2(db_,
        "INSERT INTO bookings (property_id, guest_name, guest_email, check_in, check_out, nights, guests, total_price, status) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 'CONFIRMED');", -1, &statement, nullptr),
        db_, "Could not prepare booking insert");

    sqlite3_bind_int(statement, 1, propertyId);
    sqlite3_bind_text(statement, 2, guestName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, guestEmail.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, checkIn.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, checkOut.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 6, nights);
    sqlite3_bind_int(statement, 7, guests);
    sqlite3_bind_int(statement, 8, totalPrice);

    const int resultCode = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (resultCode != SQLITE_DONE) {
        throw DatabaseError(std::string("Could not save booking: ") + sqlite3_errmsg(db_));
    }
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

bool Database::cancelBooking(int id) {
    sqlite3_stmt* statement = nullptr;
    checkSqlite(sqlite3_prepare_v2(db_,
        "UPDATE bookings SET status = 'CANCELLED' WHERE id = ?1 AND status = 'CONFIRMED';",
        -1, &statement, nullptr), db_, "Could not prepare booking cancellation");

    sqlite3_bind_int(statement, 1, id);
    const int resultCode = sqlite3_step(statement);
    const int changes = sqlite3_changes(db_);
    sqlite3_finalize(statement);

    if (resultCode != SQLITE_DONE) {
        throw DatabaseError(std::string("Cancellation query failed: ") + sqlite3_errmsg(db_));
    }
    return changes > 0;
}

std::vector<Booking> Database::queryBookings(const std::string& whereClause, int parameter, bool useParameter) {
    const std::string sql =
        "SELECT b.id, b.guest_name, b.guest_email, b.check_in, b.check_out, b.nights, "
        "       COALESCE(b.guests, 1), b.total_price, COALESCE(b.status, 'CONFIRMED'), b.created_at, "
        "       p.id, p.name, p.city, p.description, p.price_per_night, p.max_guests, p.image, p.host_name, p.host_email "
        "FROM bookings b JOIN properties p ON p.id = b.property_id " +
        whereClause + " ORDER BY b.id DESC;";

    sqlite3_stmt* statement = nullptr;
    checkSqlite(sqlite3_prepare_v2(db_, sql.c_str(), -1, &statement, nullptr), db_, "Could not read bookings");
    if (useParameter) sqlite3_bind_int(statement, 1, parameter);

    auto text = [&](int column) {
        const unsigned char* value = sqlite3_column_text(statement, column);
        return value ? std::string(reinterpret_cast<const char*>(value)) : std::string();
    };

    std::vector<Booking> results;
    int resultCode = sqlite3_step(statement);
    while (resultCode == SQLITE_ROW) {
        // Columns 0-9: Booking data
        // Columns 10-18: Property data
        Property property = readPropertyFromRow(statement, 10);
        Guest guest(text(1), text(2));

        const int bookingId = sqlite3_column_int(statement, 0);
        const std::string checkIn = text(3);
        const std::string checkOut = text(4);
        const int nights = sqlite3_column_int(statement, 5);
        const int guests = sqlite3_column_int(statement, 6);
        const int totalPrice = sqlite3_column_int(statement, 7);
        const std::string status = text(8);
        const std::string createdAt = text(9);

        results.emplace_back(bookingId, property, guest, checkIn, checkOut,
                             nights, totalPrice, createdAt, status, guests);
        resultCode = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    checkSqlite(resultCode, db_, "Reading bookings failed");
    return results;
}

std::vector<Booking> Database::findAllBookings() {
    return queryBookings("", 0, false);
}

std::optional<Booking> Database::findBookingById(int id) {
    std::vector<Booking> found = queryBookings("WHERE b.id = ?1", id, true);
    if (found.empty()) return std::nullopt;
    return found.front();
}
