// booking_service.h - abstract service interface for StayFinder business logic.
//
// Core OOP Concepts:
//   1. ABSTRACTION:          BookingService specifies WHAT operations exist, decoupling callers
//                            from SQLite or other storage engines.
//   2. COMPILE-TIME POLY:    Function Overloading (searchProperties with different parameter sets).
//   3. EXCEPTION HIERARCHY:  Custom domain exceptions derived from std::runtime_error.
#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "models.h"

// ---------------------------------------------------------------------------
// Domain Exceptions (Exception Handling Pillar)
// ---------------------------------------------------------------------------

// Thrown when user inputs fail validation constraints (HTTP 400)
class ValidationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Thrown when an entity with the given ID cannot be found (HTTP 404)
class NotFoundError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Thrown when booking dates conflict with an existing reservation (HTTP 409)
class ConflictError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------
// Data Transfer Object
// ---------------------------------------------------------------------------

struct BookingRequest {
    int propertyId = 0;
    std::string guestName;
    std::string guestEmail;
    std::string checkIn;   // "YYYY-MM-DD"
    std::string checkOut;  // "YYYY-MM-DD"
    int guests = 1;        // Number of travelers
};

// ---------------------------------------------------------------------------
// BookingService Abstract Interface (Abstraction)
// ---------------------------------------------------------------------------

class BookingService {
public:
    virtual ~BookingService() = default;

    // FUNCTION OVERLOADING (Compile-Time Polymorphism)
    // Overload 1: Basic search by city filter (empty matches all)
    virtual std::vector<Property> searchProperties(const std::string& city) = 0;

    // Overload 2: Advanced search with capacity and price filters
    virtual std::vector<Property> searchProperties(const std::string& city,
                                                   int minGuests,
                                                   int maxPrice) = 0;

    // Entity lookup by ID
    virtual Property getProperty(int id) = 0;

    // Core booking lifecycle methods
    virtual Booking createBooking(const BookingRequest& request) = 0;
    virtual Booking getBooking(int id) = 0;
    virtual Booking cancelBooking(int id) = 0;
    virtual std::vector<Booking> listBookings() = 0;

    // Pricing calculation utility utilizing the PricingStrategy hierarchy
    virtual int estimatePrice(int propertyId, int nights, int guests,
                              int& outDiscount, std::string& outStrategy) = 0;
};
