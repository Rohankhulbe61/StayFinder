// sqlite_booking_service.h - concrete implementation of BookingService using SQLite.
//
// Core OOP Principles:
//   1. INHERITANCE + POLYMORPHISM: Implements the BookingService interface.
//   2. COMPOSITION / AGGREGATION:  Holds a reference to Database and owns PricingStrategy objects.
//   3. CONCURRENCY CONTROL:       Uses std::mutex and std::lock_guard (RAII) to serialize writes.
#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "booking_service.h"
#include "database.h"
#include "models.h"

class SqliteBookingService : public BookingService {
public:
    explicit SqliteBookingService(Database& database);

    // Function Overloading implementations
    std::vector<Property> searchProperties(const std::string& city) override;
    std::vector<Property> searchProperties(const std::string& city,
                                           int minGuests,
                                           int maxPrice) override;

    Property getProperty(int id) override;

    // Booking Lifecycle
    Booking createBooking(const BookingRequest& request) override;
    Booking getBooking(int id) override;
    Booking cancelBooking(int id) override;
    std::vector<Booking> listBookings() override;

    // Runtime Polymorphic Price Calculation via PricingStrategy
    int estimatePrice(int propertyId, int nights, int guests,
                      int& outDiscount, std::string& outStrategy) override;

private:
    Database& database_;
    std::mutex mutex_;  // Thread synchronization for booking check-and-insert

    // Strategy Pattern: Pointers to polymorphic pricing strategies
    std::unique_ptr<PricingStrategy> standardStrategy_;
    std::unique_ptr<PricingStrategy> extendedStayStrategy_;

    const PricingStrategy& selectStrategy(int nights) const;
};
