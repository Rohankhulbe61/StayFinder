// models.h - the domain model classes for StayFinder.
//
// Demonstrates Core C++ OOP Pillars & Principles:
//   1. ENCAPSULATION:        Data members are private with validated public accessors/mutators.
//   2. ABSTRACTION:          User and PricingStrategy are abstract interfaces with pure virtual functions.
//   3. INHERITANCE:          Guest and Host inherit from User; concrete pricing strategies inherit from PricingStrategy.
//   4. RUNTIME POLYMORPHISM: Virtual dispatch via User* (for bookingMessage) and PricingStrategy* (for price calculation).
//   5. COMPOSITION:          Property has-a Host; Booking has-a Property and has-a Guest.
//   6. OPERATOR OVERLOADING: operator<< defined for Property and Booking to enable elegant stream formatting.
#pragma once

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

// ---------------------------------------------------------------------------
// 1. User Hierarchy (Abstraction + Inheritance + Runtime Polymorphism)
// ---------------------------------------------------------------------------

// Abstract base class representing any participant in the marketplace.
class User {
public:
    User(std::string name, std::string email)
        : name_(std::move(name)), email_(std::move(email)) {
        if (name_.empty()) {
            throw std::invalid_argument("User name cannot be empty.");
        }
    }

    // Virtual destructor is critical for polymorphic base classes
    virtual ~User() = default;

    // Const-correct getters (Encapsulation)
    const std::string& name() const noexcept { return name_; }
    const std::string& email() const noexcept { return email_; }

    // Pure virtual functions make User an ABSTRACT CLASS (Abstraction)
    virtual std::string roleName() const = 0;
    virtual std::string bookingMessage(const std::string& propertyName,
                                       const std::string& guestName) const = 0;

private:
    std::string name_;   // Private data hiding
    std::string email_;
};

// Guest: person booking an accommodation.
class Guest : public User {
public:
    Guest(std::string name, std::string email)
        : User(std::move(name), std::move(email)) {}

    // Runtime Polymorphism: Dynamic override of role-specific logic
    std::string roleName() const override { return "Guest"; }

    std::string bookingMessage(const std::string& propertyName,
                               const std::string& guestName) const override {
        return "Thank you " + guestName + "! Your stay at " + propertyName + " is confirmed.";
    }
};

// Host: owner of a listed property.
class Host : public User {
public:
    Host(std::string name, std::string email)
        : User(std::move(name), std::move(email)) {}

    // Runtime Polymorphism
    std::string roleName() const override { return "Host"; }

    std::string bookingMessage(const std::string& propertyName,
                               const std::string& guestName) const override {
        return "Host notification: New reservation at " + propertyName + " for " + guestName + ".";
    }
};

// ---------------------------------------------------------------------------
// 2. Pricing Strategy Hierarchy (Strategy Pattern: Abstraction & Polymorphism)
// ---------------------------------------------------------------------------

class PricingStrategy {
public:
    virtual ~PricingStrategy() = default;
    virtual std::string strategyName() const = 0;
    virtual int calculateTotal(int nightlyRate, int nights, int guests) const = 0;
    virtual int calculateDiscount(int nightlyRate, int nights, int guests) const = 0;
};

// Standard linear pricing: rate * nights
class StandardPricingStrategy : public PricingStrategy {
public:
    std::string strategyName() const override { return "Standard Daily Rate"; }

    int calculateTotal(int nightlyRate, int nights, int /*guests*/) const override {
        return nightlyRate * nights;
    }

    int calculateDiscount(int /*nightlyRate*/, int /*nights*/, int /*guests*/) const override {
        return 0;
    }
};

// Extended stay discount: 10% off for stays of 5 nights or longer
class ExtendedStayPricingStrategy : public PricingStrategy {
public:
    std::string strategyName() const override { return "Extended Stay Discount (10% off 5+ nights)"; }

    int calculateDiscount(int nightlyRate, int nights, int /*guests*/) const override {
        if (nights >= 5) {
            return (nightlyRate * nights * 10) / 100;
        }
        return 0;
    }

    int calculateTotal(int nightlyRate, int nights, int guests) const override {
        const int base = nightlyRate * nights;
        return base - calculateDiscount(nightlyRate, nights, guests);
    }
};

// ---------------------------------------------------------------------------
// 3. Property (Composition: Property contains a Host)
// ---------------------------------------------------------------------------

class Property {
public:
    Property(int id, std::string name, std::string city, std::string description,
             int pricePerNight, int maxGuests, std::string imageFile, Host host)
        : id_(id),
          name_(std::move(name)),
          city_(std::move(city)),
          description_(std::move(description)),
          pricePerNight_(pricePerNight),
          maxGuests_(maxGuests),
          imageFile_(std::move(imageFile)),
          host_(std::move(host)) {}

    // Encapsulation: read-only accessors
    int id() const noexcept { return id_; }
    const std::string& name() const noexcept { return name_; }
    const std::string& city() const noexcept { return city_; }
    const std::string& description() const noexcept { return description_; }
    int pricePerNight() const noexcept { return pricePerNight_; }
    int maxGuests() const noexcept { return maxGuests_; }
    const std::string& imageFile() const noexcept { return imageFile_; }
    const Host& host() const noexcept { return host_; }  // COMPOSITION

    // OPERATOR OVERLOADING: stream output operator
    friend std::ostream& operator<<(std::ostream& os, const Property& p) {
        os << "Property #" << p.id_ << " [" << p.name_ << " in " << p.city_
           << ", ₹" << p.pricePerNight_ << "/night, max " << p.maxGuests_ << " guests]";
        return os;
    }

private:
    int id_;
    std::string name_;
    std::string city_;
    std::string description_;
    int pricePerNight_;
    int maxGuests_;
    std::string imageFile_;
    Host host_;  // COMPOSITION
};

// ---------------------------------------------------------------------------
// 4. Booking (Composition: Booking has-a Property and has-a Guest)
// ---------------------------------------------------------------------------

class Booking {
public:
    Booking(int id, Property property, Guest guest, std::string checkIn,
            std::string checkOut, int nights, int totalPrice, std::string createdAt,
            std::string status = "CONFIRMED", int guests = 1)
        : id_(id),
          property_(std::move(property)),
          guest_(std::move(guest)),
          checkIn_(std::move(checkIn)),
          checkOut_(std::move(checkOut)),
          nights_(nights),
          totalPrice_(totalPrice),
          createdAt_(std::move(createdAt)),
          status_(std::move(status)),
          guests_(guests) {}

    int id() const noexcept { return id_; }
    const Property& property() const noexcept { return property_; }
    const Guest& guest() const noexcept { return guest_; }
    const std::string& checkIn() const noexcept { return checkIn_; }
    const std::string& checkOut() const noexcept { return checkOut_; }
    int nights() const noexcept { return nights_; }
    int totalPrice() const noexcept { return totalPrice_; }
    const std::string& createdAt() const noexcept { return createdAt_; }
    const std::string& status() const noexcept { return status_; }
    int guests() const noexcept { return guests_; }

    bool isConfirmed() const noexcept { return status_ == "CONFIRMED"; }
    bool isCancelled() const noexcept { return status_ == "CANCELLED"; }

    // Controlled state mutation with business invariant validation
    void cancel() {
        if (status_ == "CANCELLED") {
            throw std::logic_error("Reservation #" + std::to_string(id_) + " is already cancelled.");
        }
        status_ = "CANCELLED";
    }

    // OPERATOR OVERLOADING: stream output operator
    friend std::ostream& operator<<(std::ostream& os, const Booking& b) {
        os << "Booking #" << b.id_ << " [" << b.property_.name()
           << " | Guest: " << b.guest_.name()
           << " | " << b.checkIn_ << " to " << b.checkOut_
           << " (" << b.nights_ << " nights, " << b.guests_ << " guests)"
           << " | Total: ₹" << b.totalPrice_
           << " | Status: " << b.status_ << "]";
        return os;
    }

    // OPERATOR OVERLOADING: equality operator
    bool operator==(const Booking& other) const noexcept {
        return id_ == other.id_;
    }

private:
    int id_;
    Property property_;  // COMPOSITION
    Guest guest_;        // COMPOSITION
    std::string checkIn_;
    std::string checkOut_;
    int nights_;
    int totalPrice_;
    std::string createdAt_;
    std::string status_;
    int guests_;
};
