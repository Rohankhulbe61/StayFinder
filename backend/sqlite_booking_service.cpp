// sqlite_booking_service.cpp - StayFinder business logic and validation implementation.
#include "sqlite_booking_service.h"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <iostream>

namespace {

bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) return 29;
    return days[month - 1];
}

bool parseDate(const std::string& text, long long& dayNumber) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') return false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) return false;
    }
    const int year = std::stoi(text.substr(0, 4));
    const int month = std::stoi(text.substr(5, 2));
    const int day = std::stoi(text.substr(8, 2));
    if (year < 1 || month < 1 || month > 12) return false;
    if (day < 1 || day > daysInMonth(year, month)) return false;

    long long days = 0;
    for (int y = 1970; y < year; ++y) days += isLeapYear(y) ? 366 : 365;
    for (int m = 1; m < month; ++m) days += daysInMonth(year, m);
    days += day - 1;
    if (year < 1970) return false;

    dayNumber = days;
    return true;
}

long long todayDayNumber() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    long long days = 0;
    for (int y = 1970; y < local.tm_year + 1900; ++y) days += isLeapYear(y) ? 366 : 365;
    for (int m = 1; m < local.tm_mon + 1; ++m) days += daysInMonth(local.tm_year + 1900, m);
    days += local.tm_mday - 1;
    return days;
}

bool looksLikeEmail(const std::string& email) {
    const auto at = email.find('@');
    if (at == std::string::npos || at == 0) return false;
    const auto dot = email.find('.', at + 1);
    return dot != std::string::npos && dot < email.size() - 1;
}

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

}  // namespace

SqliteBookingService::SqliteBookingService(Database& database)
    : database_(database),
      standardStrategy_(std::make_unique<StandardPricingStrategy>()),
      extendedStayStrategy_(std::make_unique<ExtendedStayPricingStrategy>()) {}

const PricingStrategy& SqliteBookingService::selectStrategy(int nights) const {
    if (nights >= 5) {
        return *extendedStayStrategy_;
    }
    return *standardStrategy_;
}

std::vector<Property> SqliteBookingService::searchProperties(const std::string& city) {
    return database_.findProperties(trim(city));
}

std::vector<Property> SqliteBookingService::searchProperties(const std::string& city,
                                                             int minGuests,
                                                             int maxPrice) {
    return database_.findProperties(trim(city), minGuests, maxPrice);
}

Property SqliteBookingService::getProperty(int id) {
    const auto property = database_.findPropertyById(id);
    if (!property) {
        throw NotFoundError("Property #" + std::to_string(id) + " not found.");
    }
    return *property;
}

int SqliteBookingService::estimatePrice(int propertyId, int nights, int guests,
                                        int& outDiscount, std::string& outStrategy) {
    const Property property = getProperty(propertyId);
    if (nights <= 0) {
        throw ValidationError("Nights count must be at least 1.");
    }
    if (guests <= 0 || guests > property.maxGuests()) {
        throw ValidationError("Guests count must be between 1 and " +
                              std::to_string(property.maxGuests()) + ".");
    }

    // RUNTIME POLYMORPHISM: PricingStrategy virtual dispatch
    const PricingStrategy& strategy = selectStrategy(nights);
    outStrategy = strategy.strategyName();
    outDiscount = strategy.calculateDiscount(property.pricePerNight(), nights, guests);
    return strategy.calculateTotal(property.pricePerNight(), nights, guests);
}

Booking SqliteBookingService::createBooking(const BookingRequest& request) {
    // 1. Validate Guest Identity
    const std::string guestName = trim(request.guestName);
    const std::string guestEmail = trim(request.guestEmail);
    if (guestName.size() < 2) {
        throw ValidationError("Please provide a valid full name (minimum 2 characters).");
    }
    if (!looksLikeEmail(guestEmail)) {
        throw ValidationError("Please enter a valid email address (e.g., traveler@example.com).");
    }

    // 2. Validate Dates
    long long checkInDay = 0;
    long long checkOutDay = 0;
    if (!parseDate(request.checkIn, checkInDay)) {
        throw ValidationError("Check-in date is invalid. Please format as YYYY-MM-DD.");
    }
    if (!parseDate(request.checkOut, checkOutDay)) {
        throw ValidationError("Check-out date is invalid. Please format as YYYY-MM-DD.");
    }
    if (checkInDay < todayDayNumber()) {
        throw ValidationError("Check-in date cannot be in the past.");
    }
    if (checkOutDay <= checkInDay) {
        throw ValidationError("Check-out date must be strictly after the check-in date.");
    }
    const long long nights = checkOutDay - checkInDay;
    if (nights > 30) {
        throw ValidationError("Maximum booking duration is 30 nights.");
    }

    // 3. Property & Guest Capacity Validation
    const Property property = getProperty(request.propertyId);
    const int guests = (request.guests <= 0) ? 1 : request.guests;
    if (guests > property.maxGuests()) {
        throw ValidationError("Guest count (" + std::to_string(guests) +
                              ") exceeds property maximum capacity (" +
                              std::to_string(property.maxGuests()) + " guests).");
    }

    // 4. Calculate Price via Runtime Polymorphic PricingStrategy
    const PricingStrategy& strategy = selectStrategy(static_cast<int>(nights));
    const int totalPrice = strategy.calculateTotal(property.pricePerNight(),
                                                   static_cast<int>(nights), guests);

    // 5. Atomic Overlap Check and Insertion with Transaction & Mutex Lock
    std::lock_guard<std::mutex> lock(mutex_);
    database_.beginTransaction();
    try {
        if (database_.hasOverlap(request.propertyId, request.checkIn, request.checkOut)) {
            database_.rollback();
            throw ConflictError("The selected dates (" + request.checkIn + " to " +
                                request.checkOut + ") are already reserved for " +
                                property.name() + ".");
        }

        const int newId = database_.insertBooking(request.propertyId, guestName, guestEmail,
                                                  request.checkIn, request.checkOut,
                                                  static_cast<int>(nights), guests, totalPrice);
        database_.commit();

        const auto saved = database_.findBookingById(newId);
        if (!saved) {
            throw DatabaseError("Booking was inserted but could not be retrieved from database.");
        }
        return *saved;
    } catch (...) {
        database_.rollback();
        throw;
    }
}

Booking SqliteBookingService::getBooking(int id) {
    const auto booking = database_.findBookingById(id);
    if (!booking) {
        throw NotFoundError("Booking reference #" + std::to_string(id) + " does not exist.");
    }
    return *booking;
}

Booking SqliteBookingService::cancelBooking(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Fetch existing booking
    auto bookingOpt = database_.findBookingById(id);
    if (!bookingOpt) {
        throw NotFoundError("Booking reference #" + std::to_string(id) + " not found.");
    }

    Booking booking = *bookingOpt;
    if (booking.isCancelled()) {
        throw ValidationError("Booking #" + std::to_string(id) + " has already been cancelled.");
    }

    // Execute cancellation mutation
    booking.cancel();
    if (!database_.cancelBooking(id)) {
        throw DatabaseError("Failed to update booking status in database.");
    }

    std::cout << "[service] Cancelled " << booking << "\n";
    return booking;
}

std::vector<Booking> SqliteBookingService::listBookings() {
    return database_.findAllBookings();
}
