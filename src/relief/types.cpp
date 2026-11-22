// types.cpp - names and formatting helpers. (Finished plumbing.)
#include "relief/types.h"

#include <cctype>
#include <cmath>

const char* supplyName(Supply s) {
    switch (s) {
        case Supply::Food: return "food";
        case Supply::Water: return "water";
        case Supply::Medicine: return "medicine";
        case Supply::Shelter: return "shelter";
    }
    return "?";
}

const char* statusName(Status s) {
    switch (s) {
        case Status::Pending: return "pending";
        case Status::Fulfilled: return "fulfilled";
        case Status::PartiallyFulfilled: return "partial";
        case Status::Deferred: return "deferred";
    }
    return "?";
}

const char* eventTypeName(EventType t) {
    switch (t) {
        case EventType::Info: return "info";
        case EventType::RequestAdded: return "request";
        case EventType::Served: return "served";
        case EventType::Partial: return "partial";
        case EventType::Deferred: return "deferred";
        case EventType::RoadBlocked: return "blocked";
        case EventType::RoadReopened: return "reopened";
        case EventType::ConditionChanged: return "condition";
        case EventType::StockAdded: return "stock";
        case EventType::Retry: return "retry";
    }
    return "?";
}

bool parseSupply(const std::string& text, Supply& out) {
    std::string t;
    for (char c : text) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (int i = 0; i < kSupplyCount; ++i) {
        if (t == supplyName(static_cast<Supply>(i))) {
            out = static_cast<Supply>(i);
            return true;
        }
    }
    return false;
}

std::string formatMinutes(double minutes) {
    int total = static_cast<int>(std::lround(minutes));
    int h = total / 60;
    int m = total % 60;
    if (h == 0) return std::to_string(m) + " min";
    if (m == 0) return std::to_string(h) + " h";
    return std::to_string(h) + " h " + std::to_string(m) + " min";
}

std::string formatNumber(int n) {
    std::string digits = std::to_string(n < 0 ? -n : n);
    std::string out;
    int count = 0;
    for (std::size_t i = digits.size(); i > 0; --i) {
        out.insert(out.begin(), digits[i - 1]);
        if (++count % 3 == 0 && i > 1) out.insert(out.begin(), ',');
    }
    return n < 0 ? "-" + out : out;
}

std::string formatClock(int minutes) {
    // Scenario starts at Day 1, 08:00.
    int t = minutes + 8 * 60;
    int day = t / (24 * 60) + 1;
    int h = (t / 60) % 24;
    int m = t % 60;
    std::string hh = (h < 10 ? "0" : "") + std::to_string(h);
    std::string mm = (m < 10 ? "0" : "") + std::to_string(m);
    return "Day " + std::to_string(day) + " " + hh + ":" + mm;
}
