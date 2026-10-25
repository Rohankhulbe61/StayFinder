# StayFinder — Curated Stays & Accommodation Marketplace

**StayFinder** is a full-stack accommodation booking web application and college engineering viva showcase featuring:
- **C++17 ISO Standard Backend** with object-oriented domain architecture
- **Embedded Multi-threaded HTTP Server** (`cpp-httplib`)
- **Embedded SQLite 3 Engine** with zero external database dependencies
- **Warm Ivory & Earthy Charcoal UI/UX** inspired by boutique hotel platforms
- **Authoritative Server Pricing** utilizing the **Strategy Pattern**
- **Atomic Concurrency Protection** with `std::mutex` and SQLite transactions
- **Interactive Developer-Facing OOP Explorer** at `/oop.html`
- **Automated Integration Test Suite** in `test_suite.py`

---

## Quick Start (Windows)

### 1. Build the Backend
From the root of the repository, navigate to the `backend` directory:

```powershell
cd StayFinder\backend
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```
*(If you are using Visual Studio 2022 Developer PowerShell, run `cmake -S . -B build -G "Visual Studio 17 2022"` followed by `cmake --build build --config Release`)*

### 2. Start the Server
```powershell
build\stayfinder.exe
```
*(For MSVC: `build\Release\stayfinder.exe`)*

You will see:
```
Starting StayFinder C++17 Backend Server...
=========================================================
  StayFinder Server Ready
  Local Address : http://localhost:8080
  OOP Explorer  : http://localhost:8080/oop.html
=========================================================
```

### 3. Open in Browser
- **Marketplace Web Portal:** [http://localhost:8080](http://localhost:8080)
- **OOP Architecture & Viva Guide:** [http://localhost:8080/oop.html](http://localhost:8080/oop.html)
- **API Health:** [http://localhost:8080/api/health](http://localhost:8080/api/health)

### 4. Run Automated Test Verification
In a separate terminal:
```powershell
python StayFinder\test_suite.py
```
Expected result: **18 PASSED, 0 FAILED**.

---

## Project Structure

```
StayFinder/
├── README.md                          Project overview and setup instructions
├── VIVA_GUIDE.md                      30 Viva questions & answers, diagrams, and explanations
├── test_suite.py                      18 automated integration tests for backend & logic
├── frontend/                          Web assets served directly by C++ backend
│   ├── index.html                     Premium marketplace interface (search, cards, booking, reservations)
│   ├── oop.html                       Interactive developer-facing OOP architecture dashboard
│   ├── style.css                      Warm ivory palette, responsive layout, micro-interactions
│   ├── script.js                      Client controller, live pricing preview, cancellation handler
│   └── images/                        High-resolution local photography assets & SVG fallbacks
└── backend/                           C++17 backend source code
    ├── CMakeLists.txt                 CMake build configuration
    ├── main.cpp                       HTTP server routing, JSON serialization, polymorphic dispatch
    ├── models.h                       User, Guest, Host, Property, Booking, PricingStrategy
    ├── booking_service.h              Abstract service interface, domain exceptions, overloading
    ├── sqlite_booking_service.h / .cpp Concrete business logic, validation, transaction locking
    ├── database.h / .cpp              RAII SQLite connection wrapper, queries, migrations
    ├── httplib.h                      Vendored single-header HTTP/HTTPS server library
    ├── json.hpp                       Vendored nlohmann/json single-header JSON library
    ├── sqlite3.c / sqlite3.h          Vendored SQLite 3 embedded database engine
    └── stayfinder.db                  SQLite database file (persists properties and bookings)
```

---

## C++ OOP Architecture Highlights

| OOP Principle | Implementation in StayFinder | Source Location |
|---|---|---|
| **Encapsulation** | Private fields (`status_`, `pricePerNight_`, `db_`, `mutex_`), validated state transitions via `Booking::cancel()`, const accessors. | `models.h`, `database.h` |
| **Abstraction** | Pure virtual abstract interfaces (`User`, `BookingService`, `PricingStrategy`). Hides storage and pricing algorithms from callers. | `models.h`, `booking_service.h` |
| **Inheritance** | `Guest` and `Host` inherit from `User`. `SqliteBookingService` inherits from `BookingService`. Custom exceptions inherit from `std::runtime_error`. | `models.h`, `booking_service.h` |
| **Runtime Polymorphism** | Virtual method dispatch via `User*` for personalized stakeholder booking messages. Dynamic dispatch on `PricingStrategy*`. | `main.cpp`, `models.h` |
| **Function Overloading** | Compile-time polymorphism in `searchProperties(city)` vs. `searchProperties(city, minGuests, maxPrice)`. | `booking_service.h`, `database.h` |
| **Operator Overloading** | `operator<<` for streaming `Property` and `Booking` objects into `std::ostream`. `operator==` for identity comparison. | `models.h` |
| **Design Patterns** | **Strategy Pattern** for dynamic price calculations (`StandardPricingStrategy`, `ExtendedStayPricingStrategy`). | `models.h`, `sqlite_booking_service.cpp` |
| **RAII Resource Safety** | Automatic SQLite cleanup in `~Database()`. Scope-bound mutex locks with `std::lock_guard<std::mutex>`. Virtual destructors preventing object slicing/leaks. | `database.h`, `sqlite_booking_service.cpp` |

---

## API Endpoints

- `GET /api/health` — Service health and standard verification.
- `GET /api/properties` — List stays (supports optional `city`, `guests`, `max_price` query parameters).
- `GET /api/properties/:id` — Single property detail.
- `GET /api/price-estimate` — Real-time price breakdown using `PricingStrategy` (`property_id`, `nights`, `guests`).
- `POST /api/bookings` — Create a reservation with atomic calendar overlap check.
- `GET /api/bookings` — List all reservations from SQLite.
- `GET /api/bookings/:id` — Single reservation detail.
- `POST /api/bookings/:id/cancel` — Cancel a confirmed booking, immediately unlocking dates.

---

## College Viva Voce Defense

For 30 likely oral viva questions and model answers, architectural diagrams, and discussion of vtables, virtual destructors, and concurrency, consult:
📄 **[VIVA_GUIDE.md](VIVA_GUIDE.md)** or open the web dashboard at **[http://localhost:8080/oop.html](http://localhost:8080/oop.html)**.
