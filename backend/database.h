// database.h - encapsulation of all SQLite database operations for StayFinder.
//
// Core OOP & Design Concepts:
//   1. ENCAPSULATION:        Hides all raw SQLite handles (sqlite3*), SQL queries, and error codes.
//   2. RAII:                 Resource Acquisition Is Initialization: Connection opened in constructor,
//                            automatically closed in destructor. Copying is deleted.
//   3. OVERLOADING:          findProperties overloaded for simple vs multi-criteria search.
#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "models.h"

struct sqlite3;  // Forward declaration of SQLite connection type

// Exception thrown on any unrecoverable SQLite error
class DatabaseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Database {
public:
    // RAII: Opens SQLite database connection and runs safe non-destructive migrations
    explicit Database(const std::string& filePath);

    // Destructor guarantees connection is closed (RAII)
    ~Database();

    // Prevent copy semantics (database handle cannot be copied)
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // FUNCTION OVERLOADING: Property Search
    // Overload 1: City name filter (empty returns all)
    std::vector<Property> findProperties(const std::string& cityFilter);

    // Overload 2: Multi-criteria filter with min capacity and max budget
    std::vector<Property> findProperties(const std::string& cityFilter, int minGuests, int maxPrice);

    // Single property lookup
    std::optional<Property> findPropertyById(int id);

    // Overlap validation: Returns true only for CONFIRMED bookings overlapping [checkIn, checkOut)
    bool hasOverlap(int propertyId, const std::string& checkIn, const std::string& checkOut);

    // Booking persistence
    int insertBooking(int propertyId, const std::string& guestName, const std::string& guestEmail,
                      const std::string& checkIn, const std::string& checkOut,
                      int nights, int guests, int totalPrice);

    // Booking cancellation: marks status as 'CANCELLED', releasing dates for rebooking
    bool cancelBooking(int id);

    // Lookups
    std::optional<Booking> findBookingById(int id);
    std::vector<Booking> findAllBookings();

    // Transaction Management for Atomic Operations
    void beginTransaction();
    void commit();
    void rollback();

private:
    void createTables();
    void addSampleDataIfEmpty();
    void migrateSchemaIfNecessary();
    std::vector<Booking> queryBookings(const std::string& whereClause, int parameter, bool useParameter);

    sqlite3* db_ = nullptr;  // Encapsulated SQLite connection
};
