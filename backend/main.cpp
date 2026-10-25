// main.cpp - HTTP API server and routing layer for StayFinder.
//
// Demonstrates:
//   1. Separation of Concerns: Handles HTTP serialization/deserialization only.
//   2. Abstraction: Talks purely to the BookingService abstract interface.
//   3. Polymorphic Dispatch: Iterates over User* collection for role messages.
//   4. Operator Overloading: Logs bookings directly via operator<<.
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "httplib.h"
#include "json.hpp"

#include "booking_service.h"
#include "database.h"
#include "sqlite_booking_service.h"

using json = nlohmann::json;

namespace {

constexpr int kPort = 8080;

// ----- Serialization Helpers -----

json propertyToJson(const Property& property) {
    return {
        {"id", property.id()},
        {"name", property.name()},
        {"city", property.city()},
        {"description", property.description()},
        {"price_per_night", property.pricePerNight()},
        {"max_guests", property.maxGuests()},
        {"image", property.imageFile()},
        {"host_name", property.host().name()},
        {"host_email", property.host().email()},
    };
}

json bookingToJson(const Booking& booking) {
    return {
        {"id", booking.id()},
        {"property_id", booking.property().id()},
        {"property_name", booking.property().name()},
        {"city", booking.property().city()},
        {"image", booking.property().imageFile()},
        {"guest_name", booking.guest().name()},
        {"guest_email", booking.guest().email()},
        {"check_in", booking.checkIn()},
        {"check_out", booking.checkOut()},
        {"nights", booking.nights()},
        {"guests", booking.guests()},
        {"total_price", booking.totalPrice()},
        {"status", booking.status()},
        {"created_at", booking.createdAt()},
    };
}

// ----- HTTP Response Utilities -----

void sendError(httplib::Response& response, int status, const std::string& message) {
    response.status = status;
    response.set_content(json{{"error", message}}.dump(), "application/json");
}

void sendJson(httplib::Response& response, const json& body, int status = 200) {
    response.status = status;
    response.set_content(body.dump(), "application/json");
}

void runRoute(httplib::Response& response, const std::function<void()>& work) {
    try {
        work();
    } catch (const ValidationError& error) {
        sendError(response, 400, error.what());
    } catch (const NotFoundError& error) {
        sendError(response, 404, error.what());
    } catch (const ConflictError& error) {
        sendError(response, 409, error.what());
    } catch (const DatabaseError& error) {
        std::cerr << "[database error] " << error.what() << "\n";
        sendError(response, 500, "A database error occurred. Please try again.");
    } catch (const json::exception& error) {
        sendError(response, 400, std::string("Malformed JSON request: ") + error.what());
    } catch (const std::exception& error) {
        std::cerr << "[unexpected error] " << error.what() << "\n";
        sendError(response, 500, "Unexpected server error.");
    }
}

}  // namespace

int main() {
    std::cout << "Starting StayFinder C++17 Backend Server...\n";

    // Initialize SQLite Database and run safe migrations
    Database database(DATABASE_PATH);

    // Instantiate concrete service bound to abstract interface
    SqliteBookingService sqliteService(database);
    BookingService& bookingService = sqliteService;

    httplib::Server server;

    // Serve static frontend assets (HTML, CSS, JS, Images)
    if (!server.set_mount_point("/", FRONTEND_DIR)) {
        std::cerr << "Warning: frontend directory not found at " << FRONTEND_DIR << "\n";
    }

    // Health check
    server.Get("/api/health", [](const httplib::Request&, httplib::Response& response) {
        sendJson(response, {
            {"status", "ok"},
            {"app", "StayFinder"},
            {"version", "2.0"},
            {"standard", "C++17"},
            {"storage", "SQLite"}
        });
    });

    // GET /api/properties?city=Goa&guests=2&max_price=5000
    // Demonstrates compile-time polymorphism through method overloading
    server.Get("/api/properties", [&](const httplib::Request& request, httplib::Response& response) {
        runRoute(response, [&] {
            const std::string city = request.get_param_value("city");
            const std::string guestsStr = request.get_param_value("guests");
            const std::string maxPriceStr = request.get_param_value("max_price");

            int minGuests = 0;
            int maxPrice = 0;
            if (!guestsStr.empty()) {
                try { minGuests = std::stoi(guestsStr); } catch (...) {}
            }
            if (!maxPriceStr.empty()) {
                try { maxPrice = std::stoi(maxPriceStr); } catch (...) {}
            }

            std::vector<Property> properties;
            if (minGuests > 0 || maxPrice > 0) {
                // Calls Overload 2: multi-criteria search
                properties = bookingService.searchProperties(city, minGuests, maxPrice);
            } else {
                // Calls Overload 1: basic city filter
                properties = bookingService.searchProperties(city);
            }

            json list = json::array();
            for (const Property& property : properties) {
                list.push_back(propertyToJson(property));
            }
            sendJson(response, list);
        });
    });

    // GET /api/properties/:id
    server.Get(R"(/api/properties/(\d+))", [&](const httplib::Request& request, httplib::Response& response) {
        runRoute(response, [&] {
            const int id = std::stoi(request.matches[1].str());
            const Property property = bookingService.getProperty(id);
            sendJson(response, propertyToJson(property));
        });
    });

    // GET /api/price-estimate?property_id=1&nights=5&guests=2
    server.Get("/api/price-estimate", [&](const httplib::Request& request, httplib::Response& response) {
        runRoute(response, [&] {
            const std::string propIdStr = request.get_param_value("property_id");
            const std::string nightsStr = request.get_param_value("nights");
            const std::string guestsStr = request.get_param_value("guests");

            if (propIdStr.empty() || nightsStr.empty()) {
                throw ValidationError("property_id and nights parameters are required.");
            }

            const int propertyId = std::stoi(propIdStr);
            const int nights = std::stoi(nightsStr);
            const int guests = guestsStr.empty() ? 1 : std::stoi(guestsStr);

            int discount = 0;
            std::string strategyName;
            const int total = bookingService.estimatePrice(propertyId, nights, guests,
                                                           discount, strategyName);

            const Property prop = bookingService.getProperty(propertyId);
            sendJson(response, {
                {"property_id", propertyId},
                {"property_name", prop.name()},
                {"nightly_rate", prop.pricePerNight()},
                {"nights", nights},
                {"guests", guests},
                {"subtotal", prop.pricePerNight() * nights},
                {"discount", discount},
                {"total_price", total},
                {"strategy", strategyName}
            });
        });
    });

    // POST /api/bookings
    server.Post("/api/bookings", [&](const httplib::Request& request, httplib::Response& response) {
        runRoute(response, [&] {
            const json body = json::parse(request.body);

            BookingRequest input;
            input.propertyId = body.at("property_id").get<int>();
            input.guestName = body.at("name").get<std::string>();
            input.guestEmail = body.at("email").get<std::string>();
            input.checkIn = body.at("check_in").get<std::string>();
            input.checkOut = body.at("check_out").get<std::string>();
            if (body.contains("guests") && !body["guests"].is_null()) {
                input.guests = body.at("guests").get<int>();
            } else {
                input.guests = 1;
            }

            const Booking booking = bookingService.createBooking(input);

            // LOGGING with Operator Overloading: operator<<(std::ostream, const Booking&)
            std::cout << "[server] New reservation created: " << booking << "\n";

            // RUNTIME POLYMORPHISM: User hierarchy virtual dispatch
            const std::vector<const User*> stakeholders = {
                &booking.guest(),
                &booking.property().host()
            };

            json messages = json::array();
            for (const User* stakeholder : stakeholders) {
                messages.push_back({
                    {"to", stakeholder->roleName()},
                    {"text", stakeholder->bookingMessage(booking.property().name(),
                                                         booking.guest().name())}
                });
            }

            sendJson(response, {
                {"confirmation", "Reservation confirmed! Your booking reference is #" +
                                 std::to_string(booking.id()) + "."},
                {"booking", bookingToJson(booking)},
                {"messages", messages}
            }, 201);
        });
    });

    // GET /api/bookings - list all bookings
    server.Get("/api/bookings", [&](const httplib::Request&, httplib::Response& response) {
        runRoute(response, [&] {
            json list = json::array();
            for (const Booking& booking : bookingService.listBookings()) {
                list.push_back(bookingToJson(booking));
            }
            sendJson(response, list);
        });
    });

    // GET /api/bookings/:id - single booking lookup
    server.Get(R"(/api/bookings/(\d+))", [&](const httplib::Request& request, httplib::Response& response) {
        runRoute(response, [&] {
            const int id = std::stoi(request.matches[1].str());
            const Booking booking = bookingService.getBooking(id);
            sendJson(response, bookingToJson(booking));
        });
    });

    // POST /api/bookings/:id/cancel
    server.Post(R"(/api/bookings/(\d+)/cancel)", [&](const httplib::Request& request, httplib::Response& response) {
        runRoute(response, [&] {
            const int id = std::stoi(request.matches[1].str());
            const Booking updated = bookingService.cancelBooking(id);
            sendJson(response, {
                {"message", "Booking #" + std::to_string(id) + " cancelled successfully. Dates are now open."},
                {"booking", bookingToJson(updated)}
            });
        });
    });

    // POST /api/bookings/cancel - JSON body { "id": 123 }
    server.Post("/api/bookings/cancel", [&](const httplib::Request& request, httplib::Response& response) {
        runRoute(response, [&] {
            const json body = json::parse(request.body);
            const int id = body.at("id").get<int>();
            const Booking updated = bookingService.cancelBooking(id);
            sendJson(response, {
                {"message", "Booking #" + std::to_string(id) + " cancelled successfully. Dates are now open."},
                {"booking", bookingToJson(updated)}
            });
        });
    });

    std::cout << "=========================================================\n"
              << "  StayFinder Server Ready\n"
              << "  Local Address : http://localhost:" << kPort << "\n"
              << "  OOP Explorer  : http://localhost:" << kPort << "/oop.html\n"
              << "=========================================================\n";

    if (!server.listen("127.0.0.1", kPort)) {
        std::cerr << "Fatal: Failed to bind to port " << kPort << ". Is another process running?\n";
        return 1;
    }
    return 0;
}
