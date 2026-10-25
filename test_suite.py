"""
StayFinder Comprehensive Automated Test Suite
Exercises backend validation, C++ OOP business logic, pricing strategies,
database persistence, atomic overlap detection, and lifecycle cancellation.
"""

import urllib.request
import urllib.error
import json
import sys
import time

BASE_URL = "http://localhost:8080"

def request_json(method, path, data=None):
    url = BASE_URL + path
    headers = {"Content-Type": "application/json", "Connection": "close"}
    body = json.dumps(data).encode("utf-8") if data is not None else None
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            content = resp.read().decode("utf-8")
            return resp.status, json.loads(content) if content else {}
    except urllib.error.HTTPError as e:
        content = e.read().decode("utf-8")
        try:
            parsed = json.loads(content)
        except:
            parsed = {"raw": content}
        return e.code, parsed

def run_tests():
    print("=" * 70, flush=True)
    print("  STAYFINDER C++17 BACKEND AUTOMATED VERIFICATION SUITE", flush=True)
    print("=" * 70, flush=True)
    
    passed = 0
    failed = 0

    def assert_test(name, condition, details=""):
        nonlocal passed, failed
        if condition:
            print(f" [PASS] {name}", flush=True)
            passed += 1
        else:
            print(f" [FAIL] {name} - Details: {details}", flush=True)
            failed += 1

    # 1. Health check
    code, res = request_json("GET", "/api/health")
    assert_test("1. Health Check", code == 200 and res.get("status") == "ok", str(res))

    # 2. Property listings
    code, properties = request_json("GET", "/api/properties")
    assert_test("2. Property Listing Count", code == 200 and len(properties) >= 8, f"Count: {len(properties)}")

    # 3. Overloaded Search: Multi-criteria filter
    code, goa_stays = request_json("GET", "/api/properties?city=Goa&guests=4")
    assert_test("3. Overloaded Search (Goa, minGuests=4)", 
                code == 200 and len(goa_stays) >= 1 and "Goa" in goa_stays[0]["city"], str(goa_stays))

    # 4. Property Detail Lookup by ID
    code, prop6 = request_json("GET", "/api/properties/6")
    assert_test("4. Property Lookup by ID (#6)", code == 200 and prop6.get("name") == "Calangute Beach Villa", str(prop6))

    # 5. Price Estimate: Standard Linear Rate
    code, est_std = request_json("GET", "/api/price-estimate?property_id=6&nights=3&guests=2")
    assert_test("5. Price Estimate - Standard Rate (3 nights * 6000 = 18000)", 
                code == 200 and est_std.get("total_price") == 18000 and est_std.get("discount") == 0, str(est_std))

    # 6. Price Estimate: Extended Stay Strategy (10% discount on 5+ nights)
    code, est_ext = request_json("GET", "/api/price-estimate?property_id=6&nights=5&guests=2")
    # Base: 5 * 6000 = 30000; Discount: 3000; Total: 27000
    assert_test("6. Price Estimate - Extended Stay Strategy (5 nights: 30000 - 3000 = 27000)",
                code == 200 and est_ext.get("total_price") == 27000 and est_ext.get("discount") == 3000, str(est_ext))

    # Generate unique test date block for property 7 (Manali) to ensure idempotency across multiple runs
    # Base date: 2027-04-10 to 2027-04-13
    d1 = "2027-04-10"
    d2 = "2027-04-13"
    d3 = "2027-04-16"

    # Pre-clean: cancel any existing confirmed bookings on property 7 for these test dates if previously created
    _, existing_bookings = request_json("GET", "/api/bookings")
    for b in existing_bookings:
        if b.get("property_id") == 7 and b.get("check_in") in [d1, d2] and b.get("status") == "CONFIRMED":
            request_json("POST", f"/api/bookings/{b['id']}/cancel")

    # 7. Valid Booking Creation
    booking_payload = {
        "property_id": 7,
        "name": "Prof. Vikram Sen",
        "email": "vikram.sen@iit.ac.in",
        "check_in": d1,
        "check_out": d2,
        "guests": 3
    }
    code, book_res = request_json("POST", "/api/bookings", booking_payload)
    new_booking_id = book_res.get("booking", {}).get("id")
    has_polymorphic_msgs = len(book_res.get("messages", [])) == 2
    assert_test("7. Valid Booking Creation & Polymorphic Messages", 
                code == 201 and new_booking_id is not None and has_polymorphic_msgs, str(book_res))

    # 8. Overlapping Booking Rejection (Dates: 2027-04-11 to 2027-04-14, overlaps d1-d2)
    overlap_payload = {
        "property_id": 7,
        "name": "Ananya Roy",
        "email": "ananya@example.com",
        "check_in": "2027-04-11",
        "check_out": "2027-04-14",
        "guests": 2
    }
    code, overlap_res = request_json("POST", "/api/bookings", overlap_payload)
    assert_test("8. Overlapping Booking Rejection (HTTP 409 Conflict)", 
                code == 409 and "already reserved" in overlap_res.get("error", "").lower(), str(overlap_res))

    # 9. Adjacent Booking Acceptance (Check-in starts exactly on d2 when previous checks out)
    adjacent_payload = {
        "property_id": 7,
        "name": "Devendra Mehta",
        "email": "devendra@example.com",
        "check_in": d2,
        "check_out": d3,
        "guests": 2
    }
    code, adj_res = request_json("POST", "/api/bookings", adjacent_payload)
    adj_booking_id = adj_res.get("booking", {}).get("id")
    assert_test("9. Adjacent Stay Allowed (Starts on Checkout Day)", code == 201 and adj_booking_id is not None, str(adj_res))

    # 10. Past Check-in Date Rejection
    past_payload = {
        "property_id": 7,
        "name": "Traveler One",
        "email": "traveler@example.com",
        "check_in": "2020-01-01",
        "check_out": "2020-01-05",
        "guests": 2
    }
    code, past_res = request_json("POST", "/api/bookings", past_payload)
    assert_test("10. Past Date Rejection (HTTP 400 Bad Request)", 
                code == 400 and "past" in past_res.get("error", "").lower(), str(past_res))

    # 11. Checkout before or equal to Check-in
    invalid_dates_payload = {
        "property_id": 7,
        "name": "Traveler Two",
        "email": "traveler2@example.com",
        "check_in": "2027-12-10",
        "check_out": "2027-12-10",
        "guests": 2
    }
    code, inv_dates_res = request_json("POST", "/api/bookings", invalid_dates_payload)
    assert_test("11. Checkout <= Check-in Rejection (HTTP 400 Bad Request)", 
                code == 400 and "after" in inv_dates_res.get("error", "").lower(), str(inv_dates_res))

    # 12. Guest Count Exceeding Capacity
    # Property 2 (Marine Drive Flat) max_guests = 2
    capacity_payload = {
        "property_id": 2,
        "name": "Family Group",
        "email": "family@example.com",
        "check_in": "2027-05-01",
        "check_out": "2027-05-04",
        "guests": 6
    }
    code, cap_res = request_json("POST", "/api/bookings", capacity_payload)
    assert_test("12. Capacity Exceeded Rejection (HTTP 400 Bad Request)", 
                code == 400 and "exceeds" in cap_res.get("error", "").lower(), str(cap_res))

    # 13. Invalid Customer Details (Empty name / Bad email)
    bad_cust_payload = {
        "property_id": 1,
        "name": "X",
        "email": "not_an_email",
        "check_in": "2027-05-01",
        "check_out": "2027-05-04",
        "guests": 1
    }
    code, bad_cust_res = request_json("POST", "/api/bookings", bad_cust_payload)
    assert_test("13. Invalid Guest Name/Email Rejection (HTTP 400)", code == 400, str(bad_cust_res))

    # 14. Invalid Property ID
    bad_prop_payload = {
        "property_id": 9999,
        "name": "Traveler Three",
        "email": "t3@example.com",
        "check_in": "2027-05-01",
        "check_out": "2027-05-04",
        "guests": 1
    }
    code, bad_prop_res = request_json("POST", "/api/bookings", bad_prop_payload)
    assert_test("14. Non-existent Property ID (HTTP 404 Not Found)", code == 404, str(bad_prop_res))

    # 15. Booking Cancellation
    if new_booking_id:
        code, cancel_res = request_json("POST", f"/api/bookings/{new_booking_id}/cancel")
        is_cancelled = cancel_res.get("booking", {}).get("status") == "CANCELLED"
        assert_test("15. Booking Cancellation Lifecycle", code == 200 and is_cancelled, str(cancel_res))

        # 16. Availability Restoration after Cancellation
        # The previously blocked dates (d1 to d2) should now be AVAILABLE!
        code, rebook_res = request_json("POST", "/api/bookings", booking_payload)
        rebook_id = rebook_res.get("booking", {}).get("id")
        assert_test("16. Availability Restored on Cancelled Dates", code == 201 and rebook_id is not None, str(rebook_res))

        # 17. Re-cancellation of Already Cancelled Booking Rejection
        code, recancel_res = request_json("POST", f"/api/bookings/{new_booking_id}/cancel")
        assert_test("17. Re-cancellation of Cancelled Stay Rejected", code == 400, str(recancel_res))

        # Clean up adjacent booking 
        if adj_booking_id:
            request_json("POST", f"/api/bookings/{adj_booking_id}/cancel")
        if rebook_id:
            request_json("POST", f"/api/bookings/{rebook_id}/cancel")

    # 18. List Bookings
    code, all_b = request_json("GET", "/api/bookings")
    assert_test("18. List Bookings API", code == 200 and len(all_b) >= 1, f"Total bookings: {len(all_b)}")

    print("=" * 70, flush=True)
    print(f"  RESULTS: {passed} PASSED, {failed} FAILED (Total: {passed + failed})", flush=True)
    print("=" * 70, flush=True)
    return failed == 0

if __name__ == "__main__":
    success = run_tests()
    sys.exit(0 if success else 1)
