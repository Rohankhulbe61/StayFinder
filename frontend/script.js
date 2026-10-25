// script.js - StayFinder Client Controller & Backend API Integration.
//
// Key Responsibilities:
//   1. Authoritative Backend Communication: Communicates with C++ REST API at /api/*.
//   2. Real-Time Dynamic Pricing: Queries PricingStrategy calculation endpoint.
//   3. Availability & Validation: Client pre-checks plus authoritative backend conflict rejection.
//   4. Booking Lifecycle: Creation, lookup, listing, and cancellation with automatic date unlocking.

(() => {
    "use strict";

    // -----------------------------------------------------------------------
    // State Store
    // -----------------------------------------------------------------------
    let allProperties = [];
    let selectedProperty = null;
    let currentFilterCity = "";
    let currentFilterGuests = 0;
    let currentSort = "recommended";
    let selectedGuestCount = 1;

    // Bookings state
    let allBookings = [];
    let currentBookingStatus = "all";
    let bookingSearchQuery = "";
    let isBookingsExpanded = false;

    // Secondary gallery images mapped to properties for rich modal experience
    const galleryMapping = {
        1: ["jaipur-haveli.jpg", "jaipur-haveli-2.jpg"],
        2: ["mumbai-sea-flat.jpg", "mumbai-sea-flat-2.jpg"],
        3: ["delhi-studio.jpg", "delhi-heritage-room.jpg"],
        4: ["delhi-heritage-room.jpg", "delhi-studio.jpg"],
        5: ["bengaluru-cottage.jpg", "delhi-studio.jpg"],
        6: ["goa-beach-villa.jpg", "goa-beach-villa-2.jpg"],
        7: ["manali-log-cabin.jpg", "manali-log-cabin-2.jpg"],
        8: ["kochi-homestay.jpg", "delhi-heritage-room.jpg"]
    };

    // -----------------------------------------------------------------------
    // DOM Elements
    // -----------------------------------------------------------------------
    const propertyGrid = document.getElementById("property-grid");
    const inventoryHeading = document.getElementById("inventory-heading");
    const inventorySubheading = document.getElementById("inventory-subheading");
    const cityPills = document.querySelectorAll(".city-pill");
    const filterSortSelect = document.getElementById("filter-sort");
    const filterGuestsSelect = document.getElementById("filter-guests");
    const btnResetFilters = document.getElementById("btn-reset-filters");

    // Hero Search
    const heroSearchForm = document.getElementById("hero-search-form");
    const heroDestInput = document.getElementById("search-destination");
    const heroCheckInInput = document.getElementById("search-check-in");
    const heroCheckOutInput = document.getElementById("search-check-out");
    const heroGuestsSelect = document.getElementById("search-guests");

    // Booking Form & Summary
    const bookingForm = document.getElementById("booking-form");
    const propertySelector = document.getElementById("property-selector");
    const checkInDateInput = document.getElementById("check-in-date");
    const checkOutDateInput = document.getElementById("check-out-date");
    const guestMinusBtn = document.getElementById("btn-guest-minus");
    const guestPlusBtn = document.getElementById("btn-guest-plus");
    const guestCountDisplay = document.getElementById("guest-count-display");
    const guestNameInput = document.getElementById("guest-name");
    const guestEmailInput = document.getElementById("guest-email");
    const btnSubmitBooking = document.getElementById("btn-submit-booking");
    const bookingAlert = document.getElementById("booking-alert");
    const capacityHint = document.getElementById("capacity-hint");

    // Summary Card Elements
    const summaryCard = document.getElementById("summary-card");
    const summaryPropThumb = document.getElementById("summary-prop-thumb");
    const summaryPropName = document.getElementById("summary-prop-name");
    const summaryPropCity = document.getElementById("summary-prop-city");
    const summaryPropRate = document.getElementById("summary-prop-rate");
    const breakdownRateCalc = document.getElementById("breakdown-rate-calc");
    const breakdownRateSubtotal = document.getElementById("breakdown-rate-subtotal");
    const rowDiscount = document.getElementById("row-discount");
    const breakdownDiscountLabel = document.getElementById("breakdown-discount-label");
    const breakdownDiscountAmount = document.getElementById("breakdown-discount-amount");
    const breakdownFinalTotal = document.getElementById("breakdown-final-total");

    // Confirmation Elements
    const bookingConfirmationPanel = document.getElementById("booking-confirmation-panel");
    const confirmHeadline = document.getElementById("confirm-headline");
    const confirmId = document.getElementById("confirm-id");
    const confirmProperty = document.getElementById("confirm-property");
    const confirmDates = document.getElementById("confirm-dates");
    const confirmGuest = document.getElementById("confirm-guest");
    const confirmPrice = document.getElementById("confirm-price");
    const confirmMessagesList = document.getElementById("confirm-messages-list");

    // Bookings DOM Elements
    const bookingsCardsGrid = document.getElementById("bookings-cards-grid");
    const bookingsTotalBadge = document.getElementById("bookings-total-badge");
    const bookingStatusTabs = document.querySelectorAll(".booking-status-tab");
    const countAll = document.getElementById("count-all");
    const countConfirmed = document.getElementById("count-confirmed");
    const countCancelled = document.getElementById("count-cancelled");
    const bookingsSearchInput = document.getElementById("bookings-search-input");
    const bookingsSearchClear = document.getElementById("bookings-search-clear");
    const bookingsActionAlert = document.getElementById("bookings-action-alert");
    const bookingsEmptyState = document.getElementById("bookings-empty-state");
    const bookingsEmptyTitle = document.getElementById("bookings-empty-title");
    const bookingsEmptyDesc = document.getElementById("bookings-empty-desc");
    const btnBookingsReset = document.getElementById("btn-bookings-reset");
    const bookingsExpandWrap = document.getElementById("bookings-expand-wrap");
    const bookingsVisibleHint = document.getElementById("bookings-visible-hint");
    const btnToggleBookingsExpand = document.getElementById("btn-toggle-bookings-expand");
    const btnToggleExpandText = document.getElementById("btn-toggle-expand-text");
    const btnToggleExpandIcon = document.getElementById("btn-toggle-expand-icon");

    // Modal Elements
    const propertyModal = document.getElementById("property-modal");
    const modalCloseBtn = document.getElementById("modal-close-btn");
    const modalImgPrimary = document.getElementById("modal-img-primary");
    const modalImgSecondary = document.getElementById("modal-img-secondary");
    const modalPropCity = document.getElementById("modal-prop-city");
    const modalPropName = document.getElementById("modal-prop-name");
    const modalPropPrice = document.getElementById("modal-prop-price");
    const modalPropDesc = document.getElementById("modal-prop-desc");
    const modalHostAvatar = document.getElementById("modal-host-avatar");
    const modalHostName = document.getElementById("modal-host-name");
    const modalHostEmail = document.getElementById("modal-host-email");
    const modalBookNowBtn = document.getElementById("modal-book-now-btn");

    // -----------------------------------------------------------------------
    // Utility Helpers
    // -----------------------------------------------------------------------
    function formatRupees(amount) {
        return "₹" + Number(amount || 0).toLocaleString("en-IN");
    }

    function todayText() {
        const d = new Date();
        const yyyy = d.getFullYear();
        const mm = String(d.getMonth() + 1).padStart(2, "0");
        const dd = String(d.getDate()).padStart(2, "0");
        return `${yyyy}-${mm}-${dd}`;
    }

    function addDays(dateText, days) {
        if (!dateText) return "";
        const parts = dateText.split("-");
        const d = new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]));
        d.setDate(d.getDate() + days);
        const yyyy = d.getFullYear();
        const mm = String(d.getMonth() + 1).padStart(2, "0");
        const dd = String(d.getDate()).padStart(2, "0");
        return `${yyyy}-${mm}-${dd}`;
    }

    function countNights(checkIn, checkOut) {
        if (!checkIn || !checkOut) return 0;
        const start = new Date(checkIn + "T00:00:00Z");
        const end = new Date(checkOut + "T00:00:00Z");
        const diff = Math.round((end - start) / (24 * 60 * 60 * 1000));
        return diff > 0 ? diff : 0;
    }

    function showAlert(message) {
        bookingAlert.textContent = message;
        bookingAlert.hidden = false;
        bookingAlert.style.display = "flex";
        bookingAlert.scrollIntoView({ behavior: "smooth", block: "nearest" });
    }

    function hideAlert() {
        bookingAlert.hidden = true;
        bookingAlert.style.display = "none";
        bookingAlert.textContent = "";
    }

    // -----------------------------------------------------------------------
    // Backend API Client
    // -----------------------------------------------------------------------
    async function apiRequest(endpoint, options = {}) {
        if (window.location.protocol === "file:") {
            throw new Error("StayFinder must be accessed through http://localhost:8080. Start backend/build/stayfinder.exe first.");
        }

        let response;
        try {
            response = await fetch(endpoint, options);
        } catch (err) {
            throw new Error("Unable to connect to StayFinder C++ server on port 8080. Please ensure the backend executable is running.");
        }

        const data = await response.json().catch(() => ({}));
        if (!response.ok) {
            throw new Error(data.error || `Server responded with status HTTP ${response.status}.`);
        }
        return data;
    }

    // -----------------------------------------------------------------------
    // Data Loading & Management
    // -----------------------------------------------------------------------
    async function loadProperties() {
        try {
            let url = "/api/properties";
            const params = new URLSearchParams();
            if (currentFilterCity) params.append("city", currentFilterCity);
            if (currentFilterGuests > 0) params.append("guests", currentFilterGuests);
            
            const queryStr = params.toString();
            if (queryStr) url += `?${queryStr}`;

            const properties = await apiRequest(url);
            allProperties = properties;

            renderPropertyGrid();
            populatePropertySelector();
        } catch (err) {
            propertyGrid.innerHTML = `
                <div class="empty-state">
                    <h3>Failed to load accommodations</h3>
                    <p>${err.message}</p>
                </div>
            `;
        }
    }

    async function loadBookings() {
        try {
            const bookings = await apiRequest("/api/bookings");
            allBookings = Array.isArray(bookings) ? bookings : [];
            renderBookings();
        } catch (err) {
            if (bookingsCardsGrid) {
                bookingsCardsGrid.hidden = true;
            }
            if (bookingsEmptyState) {
                bookingsEmptyState.hidden = false;
                if (bookingsEmptyTitle) bookingsEmptyTitle.textContent = "Could not load reservations";
                if (bookingsEmptyDesc) bookingsEmptyDesc.textContent = `Server communication error: ${err.message}`;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Rendering Logic
    // -----------------------------------------------------------------------
    function renderPropertyGrid() {
        propertyGrid.innerHTML = "";

        let items = [...allProperties];

        // Client-side Sorting
        if (currentSort === "price-asc") {
            items.sort((a, b) => a.price_per_night - b.price_per_night);
        } else if (currentSort === "price-desc") {
            items.sort((a, b) => b.price_per_night - a.price_per_night);
        } else if (currentSort === "capacity-desc") {
            items.sort((a, b) => b.max_guests - a.max_guests);
        }

        if (items.length === 0) {
            propertyGrid.innerHTML = `
                <div class="empty-state">
                    <svg width="48" height="48" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" style="color: var(--text-tertiary); margin-bottom: 12px;">
                        <circle cx="11" cy="11" r="8"></circle>
                        <line x1="21" y1="21" x2="16.65" y2="16.65"></line>
                    </svg>
                    <h3>No properties found</h3>
                    <p>No accommodations matched your criteria. Try adjusting destination, guest count, or reset filters.</p>
                </div>
            `;
            inventoryHeading.textContent = "No stays match your criteria";
            inventorySubheading.textContent = `0 stays found.`;
            return;
        }

        inventoryHeading.textContent = currentFilterCity ? `Stays in ${currentFilterCity}` : "Available accommodations";
        inventorySubheading.textContent = `Showing ${items.length} verified boutique ${items.length === 1 ? "stay" : "stays"} across India.`;

        items.forEach(property => {
            const card = document.createElement("article");
            card.className = "property-card";

            const imgSrc = `images/${property.image}`;

            card.innerHTML = `
                <div class="card-media-wrap">
                    <img src="${imgSrc}" alt="${property.name}" loading="lazy" onerror="this.onerror=null; this.src='images/${property.image.replace('.jpg', '.svg')}';">
                    <span class="card-badge">Up to ${property.max_guests} guests</span>
                </div>
                <div class="card-body">
                    <div class="card-location">📍 ${property.city}, India</div>
                    <h3 class="card-title">${property.name}</h3>
                    <p class="card-description">${property.description}</p>
                    <div class="card-amenities">
                        <span>✨ Boutique Interior</span>
                        <span>•</span>
                        <span>📶 High-Speed Wi-Fi</span>
                        <span>•</span>
                        <span>❄️ AC</span>
                    </div>
                    <div class="card-footer">
                        <div class="card-price-wrap">
                            <span class="card-price-amount">${formatRupees(property.price_per_night)}</span>
                            <span class="card-price-unit">per night</span>
                        </div>
                        <div class="card-actions">
                            <button type="button" class="btn-secondary-sm btn-details" data-id="${property.id}">View Details</button>
                            <button type="button" class="btn-primary-sm btn-book" data-id="${property.id}">Book Stay</button>
                        </div>
                    </div>
                </div>
            `;

            card.querySelector(".btn-details").addEventListener("click", () => openPropertyModal(property));
            card.querySelector(".btn-book").addEventListener("click", () => selectPropertyForBooking(property, true));

            propertyGrid.appendChild(card);
        });
    }

    function populatePropertySelector() {
        const currentVal = propertySelector.value;
        propertySelector.innerHTML = '<option value="" disabled selected>Choose an accommodation...</option>';

        allProperties.forEach(prop => {
            const opt = document.createElement("option");
            opt.value = prop.id;
            opt.textContent = `${prop.name} — ${prop.city} (${formatRupees(prop.price_per_night)}/night)`;
            propertySelector.appendChild(opt);
        });

        if (currentVal && allProperties.some(p => p.id === Number(currentVal))) {
            propertySelector.value = currentVal;
        }
    }

    function renderBookings() {
        if (!bookingsCardsGrid) return;

        // 1. Calculate Authoritative Counts
        const totalCount = allBookings.length;
        const confirmedCount = allBookings.filter(b => b.status === "CONFIRMED").length;
        const cancelledCount = allBookings.filter(b => b.status === "CANCELLED").length;

        if (bookingsTotalBadge) {
            bookingsTotalBadge.textContent = `${totalCount} ${totalCount === 1 ? "Reservation" : "Reservations"}`;
        }
        if (countAll) countAll.textContent = totalCount;
        if (countConfirmed) countConfirmed.textContent = confirmedCount;
        if (countCancelled) countCancelled.textContent = cancelledCount;

        // 2. Filter by Status Tab
        let filtered = [...allBookings];
        if (currentBookingStatus !== "all") {
            filtered = filtered.filter(b => b.status === currentBookingStatus);
        }

        // 3. Filter by Search Query (Property, City, Guest Name, Email, Ref #, Dates)
        if (bookingSearchQuery) {
            const q = bookingSearchQuery.toLowerCase();
            filtered = filtered.filter(b => {
                const idMatch = String(b.id).includes(q) || `#${b.id}`.includes(q);
                const propMatch = (b.property_name || "").toLowerCase().includes(q);
                const cityMatch = (b.city || "").toLowerCase().includes(q);
                const guestNameMatch = (b.guest_name || "").toLowerCase().includes(q);
                const guestEmailMatch = (b.guest_email || "").toLowerCase().includes(q);
                const dateMatch = (b.check_in || "").includes(q) || (b.check_out || "").includes(q);
                return idMatch || propMatch || cityMatch || guestNameMatch || guestEmailMatch || dateMatch;
            });
        }

        // 4. Handle Empty State
        if (filtered.length === 0) {
            bookingsCardsGrid.innerHTML = "";
            bookingsCardsGrid.hidden = true;
            if (bookingsExpandWrap) bookingsExpandWrap.hidden = true;

            if (bookingsEmptyState) {
                bookingsEmptyState.hidden = false;
                if (allBookings.length === 0) {
                    if (bookingsEmptyTitle) bookingsEmptyTitle.textContent = "No reservations recorded";
                    if (bookingsEmptyDesc) bookingsEmptyDesc.textContent = "You don't have any reservations yet. Complete a booking above to see your reservation card appear here.";
                    if (btnBookingsReset) btnBookingsReset.hidden = true;
                } else {
                    const statusLabel = currentBookingStatus === "all" ? "" : `${currentBookingStatus.toLowerCase()} `;
                    const searchLabel = bookingSearchQuery ? ` matching "${bookingSearchQuery}"` : "";
                    if (bookingsEmptyTitle) bookingsEmptyTitle.textContent = "No matching reservations";
                    if (bookingsEmptyDesc) bookingsEmptyDesc.textContent = `No ${statusLabel}reservations found${searchLabel}. Try adjusting your search query or reset your filters.`;
                    if (btnBookingsReset) btnBookingsReset.hidden = false;
                }
            }
            return;
        }

        // 5. Render Reservation Cards
        if (bookingsEmptyState) bookingsEmptyState.hidden = true;
        bookingsCardsGrid.hidden = false;
        bookingsCardsGrid.innerHTML = "";

        // Collapsible: show first 4 items by default, or all if expanded
        const visibleList = isBookingsExpanded ? filtered : filtered.slice(0, 4);

        visibleList.forEach(b => {
            const card = document.createElement("article");
            const isConfirmed = b.status === "CONFIRMED";
            card.className = `reservation-card ${isConfirmed ? "confirmed" : "cancelled"}`;

            const imgName = b.image || (allProperties.find(p => p.id === b.property_id)?.image) || "jaipur-haveli.jpg";
            const imgSrc = `images/${imgName}`;

            card.innerHTML = `
                <div class="res-card-media">
                    <img src="${imgSrc}" alt="${b.property_name}" class="res-card-img" loading="lazy" onerror="this.onerror=null; this.src='images/jaipur-haveli.jpg';">
                    <span class="res-ref-badge">Ref #${b.id}</span>
                    <span class="res-status-badge ${isConfirmed ? 'confirmed' : 'cancelled'}">
                        <span class="res-status-dot"></span>
                        ${isConfirmed ? 'Confirmed' : 'Cancelled'}
                    </span>
                </div>
                <div class="res-card-body">
                    <h3 class="res-card-title">${b.property_name}</h3>
                    <div class="res-card-location">📍 ${b.city || "India"}, India</div>
                    <div class="res-card-guest">
                        <div>
                            <strong>Guest:</strong> <span class="res-guest-name">${b.guest_name || "Guest"}</span>
                        </div>
                        <div class="res-guest-email">${b.guest_email || ""}</div>
                    </div>
                    <div class="res-meta-grid">
                        <div class="res-meta-item">
                            <span class="res-meta-label">Check-In</span>
                            <span class="res-meta-val">${b.check_in}</span>
                        </div>
                        <div class="res-meta-item">
                            <span class="res-meta-label">Check-Out</span>
                            <span class="res-meta-val">${b.check_out}</span>
                        </div>
                        <div class="res-meta-item">
                            <span class="res-meta-label">Duration</span>
                            <span class="res-meta-val">${b.nights} ${b.nights === 1 ? 'night' : 'nights'} (${b.guests || 1} ${(b.guests || 1) === 1 ? 'guest' : 'guests'})</span>
                        </div>
                        <div class="res-meta-item">
                            <span class="res-meta-label">Total Price</span>
                            <span class="res-meta-val price">${formatRupees(b.total_price)}</span>
                        </div>
                    </div>
                    <div class="res-card-footer">
                        ${isConfirmed ? `
                            <button type="button" class="btn-cancel-reservation" data-id="${b.id}" aria-label="Cancel reservation #${b.id}">
                                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                                    <circle cx="12" cy="12" r="10"></circle>
                                    <line x1="15" y1="9" x2="9" y2="15"></line>
                                    <line x1="9" y1="9" x2="15" y2="15"></line>
                                </svg>
                                Cancel Reservation
                            </button>
                        ` : `
                            <div class="res-released-tag">
                                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                                    <polyline points="20 6 9 17 4 12"></polyline>
                                </svg>
                                Dates Released
                            </div>
                        `}
                    </div>
                </div>
            `;

            if (isConfirmed) {
                const cancelBtn = card.querySelector(".btn-cancel-reservation");
                cancelBtn.addEventListener("click", () => handleCancelBooking(b.id, cancelBtn, b.property_name));
            }

            bookingsCardsGrid.appendChild(card);
        });

        // 6. Expand / Collapse Visibility and Button State
        if (bookingsExpandWrap) {
            if (filtered.length <= 4) {
                bookingsExpandWrap.hidden = true;
            } else {
                bookingsExpandWrap.hidden = false;
                const statusLabel = currentBookingStatus === "all" ? "" : `${currentBookingStatus.toLowerCase()} `;
                if (!isBookingsExpanded) {
                    if (bookingsVisibleHint) {
                        bookingsVisibleHint.textContent = `Showing 4 of ${filtered.length} ${statusLabel}reservations`;
                    }
                    if (btnToggleExpandText) {
                        btnToggleExpandText.textContent = `View All Reservations (${filtered.length})`;
                    }
                    if (btnToggleBookingsExpand) {
                        btnToggleBookingsExpand.classList.remove("expanded");
                        btnToggleBookingsExpand.setAttribute("aria-expanded", "false");
                    }
                } else {
                    if (bookingsVisibleHint) {
                        bookingsVisibleHint.textContent = `Showing all ${filtered.length} ${statusLabel}reservations`;
                    }
                    if (btnToggleExpandText) {
                        btnToggleExpandText.textContent = "Show Less";
                    }
                    if (btnToggleBookingsExpand) {
                        btnToggleBookingsExpand.classList.add("expanded");
                        btnToggleBookingsExpand.setAttribute("aria-expanded", "true");
                    }
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Booking Form & Price Estimation Interaction
    // -----------------------------------------------------------------------
    function selectPropertyForBooking(property, scrollTo = false) {
        selectedProperty = property;
        propertySelector.value = property.id;

        // Update guest count limits
        if (selectedGuestCount > property.max_guests) {
            selectedGuestCount = property.max_guests;
        }
        guestCountDisplay.textContent = selectedGuestCount;
        capacityHint.textContent = `Accommodates up to ${property.max_guests} guests.`;
        updateGuestButtonStates();

        // Update summary preview
        summaryPropThumb.src = `images/${property.image}`;
        summaryPropName.textContent = property.name;
        summaryPropCity.textContent = `📍 ${property.city}, India`;
        summaryPropRate.textContent = `${formatRupees(property.price_per_night)} / night`;

        hideAlert();
        bookingConfirmationPanel.hidden = true;
        if (summaryCard) summaryCard.hidden = false;
        updatePriceSummary();

        if (scrollTo) {
            document.getElementById("book").scrollIntoView({ behavior: "smooth" });
        }
    }

    function updateGuestButtonStates() {
        const max = selectedProperty ? selectedProperty.max_guests : 10;
        guestMinusBtn.disabled = selectedGuestCount <= 1;
        guestPlusBtn.disabled = selectedGuestCount >= max;
    }

    async function updatePriceSummary() {
        const checkIn = checkInDateInput.value;
        const checkOut = checkOutDateInput.value;
        const nights = countNights(checkIn, checkOut);

        if (!selectedProperty) {
            breakdownRateCalc.textContent = "Select an accommodation";
            breakdownRateSubtotal.textContent = "—";
            rowDiscount.hidden = true;
            breakdownFinalTotal.textContent = "—";
            return;
        }

        if (nights <= 0) {
            breakdownRateCalc.textContent = `${formatRupees(selectedProperty.price_per_night)} × 0 nights`;
            breakdownRateSubtotal.textContent = "₹0";
            rowDiscount.hidden = true;
            breakdownFinalTotal.textContent = "₹0";
            return;
        }

        // Query the C++ backend /api/price-estimate to enforce authoritative pricing
        try {
            const estimate = await apiRequest(`/api/price-estimate?property_id=${selectedProperty.id}&nights=${nights}&guests=${selectedGuestCount}`);

            breakdownRateCalc.textContent = `${formatRupees(estimate.nightly_rate)} × ${nights} ${nights === 1 ? "night" : "nights"}`;
            breakdownRateSubtotal.textContent = formatRupees(estimate.subtotal);

            if (estimate.discount > 0) {
                rowDiscount.hidden = false;
                breakdownDiscountLabel.textContent = `${estimate.strategy}`;
                breakdownDiscountAmount.textContent = `−${formatRupees(estimate.discount)}`;
            } else {
                rowDiscount.hidden = true;
            }

            breakdownFinalTotal.textContent = formatRupees(estimate.total_price);
        } catch {
            // Local fallback calculation matching C++ ExtendedStayPricingStrategy
            const subtotal = selectedProperty.price_per_night * nights;
            let discount = 0;
            if (nights >= 5) {
                discount = Math.floor((subtotal * 10) / 100);
            }
            const total = subtotal - discount;

            breakdownRateCalc.textContent = `${formatRupees(selectedProperty.price_per_night)} × ${nights} ${nights === 1 ? "night" : "nights"}`;
            breakdownRateSubtotal.textContent = formatRupees(subtotal);

            if (discount > 0) {
                rowDiscount.hidden = false;
                breakdownDiscountLabel.textContent = "Extended Stay Discount (10% off 5+ nights)";
                breakdownDiscountAmount.textContent = `−${formatRupees(discount)}`;
            } else {
                rowDiscount.hidden = true;
            }

            breakdownFinalTotal.textContent = formatRupees(total);
        }
    }

    // -----------------------------------------------------------------------
    // Booking Submission
    // -----------------------------------------------------------------------
    async function handleBookingSubmit(event) {
        event.preventDefault();
        hideAlert();

        if (!selectedProperty) {
            showAlert("Please select an accommodation first.");
            return;
        }

        const guestName = guestNameInput.value.trim();
        const guestEmail = guestEmailInput.value.trim();
        const checkIn = checkInDateInput.value;
        const checkOut = checkOutDateInput.value;

        // Validation Checks
        if (guestName.length < 2) {
            showAlert("Please provide your full name (minimum 2 characters).");
            guestNameInput.focus();
            return;
        }
        if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(guestEmail)) {
            showAlert("Please provide a valid email address (e.g., traveler@example.com).");
            guestEmailInput.focus();
            return;
        }
        if (!checkIn || !checkOut) {
            showAlert("Please choose valid arrival and departure dates.");
            return;
        }
        if (checkIn < todayText()) {
            showAlert("Check-in date cannot be in the past.");
            return;
        }
        if (checkOut <= checkIn) {
            showAlert("Check-out date must be strictly after the check-in date.");
            return;
        }
        if (selectedGuestCount > selectedProperty.max_guests) {
            showAlert(`Selected guest count (${selectedGuestCount}) exceeds maximum capacity (${selectedProperty.max_guests}).`);
            return;
        }

        const payload = {
            property_id: selectedProperty.id,
            name: guestName,
            email: guestEmail,
            check_in: checkIn,
            check_out: checkOut,
            guests: selectedGuestCount
        };

        btnSubmitBooking.disabled = true;
        btnSubmitBooking.textContent = "Securing reservation...";

        try {
            const result = await apiRequest("/api/bookings", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(payload)
            });

            // Display Confirmed Certificate
            const b = result.booking;
            confirmHeadline.textContent = result.confirmation;
            confirmId.textContent = `#${b.id}`;
            confirmProperty.textContent = `${b.property_name} (${b.city})`;
            confirmDates.textContent = `${b.check_in} to ${b.check_out} (${b.nights} nights)`;
            confirmGuest.textContent = `${b.guest_name} (${b.guests || selectedGuestCount} guests)`;
            confirmPrice.textContent = formatRupees(b.total_price);

            // Populate Stakeholder Notifications with clean customer-facing labels
            confirmMessagesList.innerHTML = "";
            (result.messages || []).forEach(msg => {
                const li = document.createElement("li");
                const roleLabel = msg.to === "Guest" ? "Guest Confirmation" : "Host Update";
                li.innerHTML = `<strong>${roleLabel}:</strong> ${msg.text}`;
                confirmMessagesList.appendChild(li);
            });

            // Cleanly swap out summary card for the confirmation certificate to prevent overlap
            if (summaryCard) summaryCard.hidden = true;
            bookingConfirmationPanel.hidden = false;
            bookingConfirmationPanel.scrollIntoView({ behavior: "smooth", block: "nearest" });

            // Wire up "Book Another Stay" button inside confirmation card
            const btnBookAnother = document.getElementById("btn-book-another");
            if (btnBookAnother) {
                btnBookAnother.onclick = () => {
                    bookingConfirmationPanel.hidden = true;
                    if (summaryCard) summaryCard.hidden = false;
                    bookingForm.reset();
                    selectedProperty = null;
                    selectedGuestCount = 1;
                    guestCountDisplay.textContent = "1";
                    capacityHint.textContent = "Select a property to view guest limits.";
                    updatePriceSummary();
                    document.getElementById("book").scrollIntoView({ behavior: "smooth" });
                };
            }

            // Refresh Bookings table
            loadBookings();
        } catch (err) {
            showAlert(err.message);
        } finally {
            btnSubmitBooking.disabled = false;
            btnSubmitBooking.textContent = "Confirm Reservation";
        }
    }

    // -----------------------------------------------------------------------
    // Booking Cancellation
    // -----------------------------------------------------------------------
    async function handleCancelBooking(bookingId, buttonElement, propertyName) {
        const propDesc = propertyName ? ` for "${propertyName}"` : "";
        if (!confirm(`Are you sure you want to cancel reservation #${bookingId}${propDesc}?\n\nThis will immediately release the calendar dates in SQLite for other guests.`)) {
            return;
        }

        buttonElement.disabled = true;
        buttonElement.innerHTML = `Cancelling...`;

        try {
            await apiRequest(`/api/bookings/${bookingId}/cancel`, {
                method: "POST"
            });

            if (bookingsActionAlert) {
                bookingsActionAlert.className = "alert-box alert-success";
                bookingsActionAlert.textContent = `Reservation #${bookingId} was successfully cancelled. Calendar dates have been released.`;
                bookingsActionAlert.hidden = false;
                setTimeout(() => {
                    bookingsActionAlert.hidden = true;
                }, 6000);
            }

            await loadBookings();
        } catch (err) {
            alert(`Cancellation error: ${err.message}`);
            buttonElement.disabled = false;
            buttonElement.innerHTML = `
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                    <circle cx="12" cy="12" r="10"></circle>
                    <line x1="15" y1="9" x2="9" y2="15"></line>
                    <line x1="9" y1="9" x2="15" y2="15"></line>
                </svg>
                Cancel Reservation
            `;
        }
    }

    // -----------------------------------------------------------------------
    // Property Details Modal
    // -----------------------------------------------------------------------
    function openPropertyModal(property) {
        modalPropName.textContent = property.name;
        modalPropCity.textContent = `📍 ${property.city}, India`;
        modalPropPrice.textContent = formatRupees(property.price_per_night);
        modalPropDesc.textContent = property.description;

        modalHostName.textContent = `Hosted by ${property.host_name || "Host"}`;
        modalHostEmail.textContent = `Direct contact: ${property.host_email || "host@stayfinder.in"} · Verified Host`;
        modalHostAvatar.textContent = (property.host_name || "H").charAt(0).toUpperCase();

        const gallery = galleryMapping[property.id] || [property.image, property.image];
        modalImgPrimary.src = `images/${gallery[0]}`;
        modalImgSecondary.src = `images/${gallery[1] || gallery[0]}`;

        modalBookNowBtn.onclick = () => {
            closePropertyModal();
            selectPropertyForBooking(property, true);
        };

        propertyModal.classList.add("open");
        document.body.style.overflow = "hidden";
    }

    function closePropertyModal() {
        propertyModal.classList.remove("open");
        document.body.style.overflow = "";
    }

    // -----------------------------------------------------------------------
    // Event Listeners & Startup
    // -----------------------------------------------------------------------
    // Property Selector Change
    propertySelector.addEventListener("change", () => {
        const propId = Number(propertySelector.value);
        const prop = allProperties.find(p => p.id === propId);
        if (prop) {
            selectPropertyForBooking(prop);
        } else {
            selectedProperty = null;
            capacityHint.textContent = "Select a property to view guest limits.";
            updateGuestButtonStates();
            updatePriceSummary();
        }
    });

    // Date Changes
    checkInDateInput.addEventListener("change", () => {
        const checkIn = checkInDateInput.value;
        if (checkIn) {
            checkOutDateInput.min = addDays(checkIn, 1);
            if (checkOutDateInput.value && checkOutDateInput.value <= checkIn) {
                checkOutDateInput.value = addDays(checkIn, 1);
            }
        }
        updatePriceSummary();
    });

    checkOutDateInput.addEventListener("change", updatePriceSummary);

    // Guest Counter
    guestMinusBtn.addEventListener("click", () => {
        if (selectedGuestCount > 1) {
            selectedGuestCount--;
            guestCountDisplay.textContent = selectedGuestCount;
            updateGuestButtonStates();
            updatePriceSummary();
        }
    });

    guestPlusBtn.addEventListener("click", () => {
        const max = selectedProperty ? selectedProperty.max_guests : 10;
        if (selectedGuestCount < max) {
            selectedGuestCount++;
            guestCountDisplay.textContent = selectedGuestCount;
            updateGuestButtonStates();
            updatePriceSummary();
        }
    });

    // Booking Submission
    bookingForm.addEventListener("submit", handleBookingSubmit);

    // City Filter Pills
    cityPills.forEach(pill => {
        pill.addEventListener("click", () => {
            cityPills.forEach(p => p.classList.remove("active"));
            pill.classList.add("active");
            currentFilterCity = pill.dataset.city || "";
            loadProperties();
        });
    });

    // Destination Cards Click
    document.querySelectorAll(".destination-card").forEach(card => {
        card.addEventListener("click", () => {
            const city = card.dataset.city;
            currentFilterCity = city;
            cityPills.forEach(p => {
                p.classList.toggle("active", p.dataset.city === city);
            });
            loadProperties();
            document.getElementById("stays").scrollIntoView({ behavior: "smooth" });
        });
    });

    // Filter Controls
    filterSortSelect.addEventListener("change", () => {
        currentSort = filterSortSelect.value;
        renderPropertyGrid();
    });

    filterGuestsSelect.addEventListener("change", () => {
        currentFilterGuests = Number(filterGuestsSelect.value);
        loadProperties();
    });

    btnResetFilters.addEventListener("click", () => {
        currentFilterCity = "";
        currentFilterGuests = 0;
        currentSort = "recommended";
        filterSortSelect.value = "recommended";
        filterGuestsSelect.value = "0";
        cityPills.forEach(p => p.classList.toggle("active", p.dataset.city === ""));
        loadProperties();
    });

    // Hero Search Form
    heroSearchForm.addEventListener("submit", event => {
        event.preventDefault();
        const dest = heroDestInput.value.trim();
        const checkIn = heroCheckInInput.value;
        const checkOut = heroCheckOutInput.value;
        const guests = Number(heroGuestsSelect.value);

        if (dest) {
            currentFilterCity = dest;
            cityPills.forEach(p => {
                p.classList.toggle("active", p.dataset.city.toLowerCase() === dest.toLowerCase());
            });
        }
        if (guests) {
            currentFilterGuests = guests;
            filterGuestsSelect.value = String(guests);
        }

        // Prepopulate booking form dates and travelers
        if (checkIn) checkInDateInput.value = checkIn;
        if (checkOut) checkOutDateInput.value = checkOut;
        if (guests) {
            selectedGuestCount = guests;
            guestCountDisplay.textContent = guests;
            updateGuestButtonStates();
        }

        loadProperties();
        document.getElementById("stays").scrollIntoView({ behavior: "smooth" });
    });

    // Modal Close Triggers
    modalCloseBtn.addEventListener("click", closePropertyModal);
    propertyModal.addEventListener("click", e => {
        if (e.target === propertyModal) closePropertyModal();
    });
    document.addEventListener("keydown", e => {
        if (e.key === "Escape" && propertyModal.classList.contains("open")) {
            closePropertyModal();
        }
    });

    // Initialize Default Dates
    const today = todayText();
    const tomorrow = addDays(today, 2);
    checkInDateInput.min = today;
    checkInDateInput.value = today;
    checkOutDateInput.min = addDays(today, 1);
    checkOutDateInput.value = tomorrow;

    heroCheckInInput.min = today;
    heroCheckInInput.value = today;
    heroCheckOutInput.min = addDays(today, 1);
    heroCheckOutInput.value = tomorrow;

    // Initialize Guest Button States
    updateGuestButtonStates();

    // -----------------------------------------------------------------------
    // Bookings Filter, Search, and Expand Listeners
    // -----------------------------------------------------------------------
    if (bookingStatusTabs) {
        bookingStatusTabs.forEach(tab => {
            tab.addEventListener("click", () => {
                bookingStatusTabs.forEach(t => {
                    t.classList.remove("active");
                    t.setAttribute("aria-selected", "false");
                });
                tab.classList.add("active");
                tab.setAttribute("aria-selected", "true");
                currentBookingStatus = tab.dataset.status || "all";
                isBookingsExpanded = false; // Reset to collapsed on filter change
                renderBookings();
            });
        });
    }

    if (bookingsSearchInput) {
        bookingsSearchInput.addEventListener("input", () => {
            bookingSearchQuery = bookingsSearchInput.value.trim();
            if (bookingsSearchClear) {
                bookingsSearchClear.hidden = !bookingSearchQuery;
            }
            isBookingsExpanded = false; // Reset to collapsed on search
            renderBookings();
        });
    }

    if (bookingsSearchClear) {
        bookingsSearchClear.addEventListener("click", () => {
            bookingsSearchInput.value = "";
            bookingSearchQuery = "";
            bookingsSearchClear.hidden = true;
            isBookingsExpanded = false;
            bookingsSearchInput.focus();
            renderBookings();
        });
    }

    if (btnBookingsReset) {
        btnBookingsReset.addEventListener("click", () => {
            currentBookingStatus = "all";
            bookingSearchQuery = "";
            if (bookingsSearchInput) bookingsSearchInput.value = "";
            if (bookingsSearchClear) bookingsSearchClear.hidden = true;
            bookingStatusTabs.forEach(t => {
                const isAll = t.dataset.status === "all";
                t.classList.toggle("active", isAll);
                t.setAttribute("aria-selected", String(isAll));
            });
            isBookingsExpanded = false;
            renderBookings();
        });
    }

    if (btnToggleBookingsExpand) {
        btnToggleBookingsExpand.addEventListener("click", () => {
            isBookingsExpanded = !isBookingsExpanded;
            renderBookings();
            if (!isBookingsExpanded) {
                const myBookingsSec = document.getElementById("my-bookings");
                if (myBookingsSec) {
                    myBookingsSec.scrollIntoView({ behavior: "smooth" });
                }
            }
        });
    }

    // Initial Data Fetch
    loadProperties();
    loadBookings();
})();
