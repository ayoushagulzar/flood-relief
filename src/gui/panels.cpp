// panels.cpp - right panel (Requests | Depots | Why | Add form) and event log.
// All read straight from ReliefManager every frame (immediate mode).
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include "gui/app.h"
#include "gui/theme.h"
#include "gui/widgets.h"

using namespace theme;

namespace {

constexpr float kTabsH = 44;
constexpr float kRowH = 60;

Color statusColor(Status s) {
    switch (s) {
        case Status::Fulfilled: return kFulfilled;
        case Status::PartiallyFulfilled: return kPartial;
        case Status::Deferred: return kDeferred;
        case Status::Pending: return kPending;
    }
    return kPending;
}

Color eventColor(EventType t) {
    switch (t) {
        case EventType::Served: return kFulfilled;
        case EventType::Partial: return kPartial;
        case EventType::Deferred: return kDeferred;
        case EventType::RoadBlocked: return kBlocked;
        case EventType::RoadReopened: return kFulfilled;
        case EventType::Retry: return kRoute;
        case EventType::ConditionChanged: return kFlooded;
        case EventType::RequestAdded: return kText;
        default: return kMuted;
    }
}

std::string depotLabel(const std::string& name) {
    const std::string suffix = " Depot";
    if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
        return name.substr(0, name.size() - suffix.size());
    return name;
}

// Wheel scrolling for a panel area: only when the mouse is over it.
void scrollWith(Rectangle area, float& scroll, float contentH) {
    if (ui::hovered(area)) scroll -= GetMouseWheelMove() * 48;
    float maxScroll = contentH - area.height;
    if (maxScroll < 0) maxScroll = 0;
    if (scroll > maxScroll) scroll = maxScroll;
    if (scroll < 0) scroll = 0;
}

void scrollbar(Rectangle area, float scroll, float contentH) {
    if (contentH <= area.height) return;
    float h = area.height * area.height / contentH;
    float y = area.y + (area.height - h) * scroll / (contentH - area.height);
    DrawRectangleRec({area.x + area.width - 3, y, 2, h}, kFaint);
}

void beginClip(Rectangle r) {
    BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width), static_cast<int>(r.height));
}

}  // namespace

// ---------------------------------------------------------------- right panel

void App::drawRightPanel(Rectangle r) {
    DrawRectangleRec(r, kPanel);
    ui::vline(r.x, r.y, r.y + r.height, kRule);
    if (tool_ == Tool::AddRequest) {
        drawAddForm(r);
        return;
    }

    const char* names[3] = {"Requests", "Depots", "Why"};
    Tab tabs[3] = {Tab::Requests, Tab::Depots, Tab::Why};
    float x = r.x + kGap + 8;
    for (int i = 0; i < 3; ++i) {
        float w = ui::measureCaps(names[i], kSmall);
        if (ui::tab({x, r.y, w, kTabsH}, names[i], tab_ == tabs[i])) tab_ = tabs[i];
        x += w + kGap * 1.5f;
    }
    ui::hline(r.x, r.x + r.width, r.y + kTabsH, kRule);

    Rectangle body{r.x + 1, r.y + kTabsH + 1, r.width - 1, r.height - kTabsH - 1};
    if (tab_ == Tab::Requests) drawRequestsTab(body);
    else if (tab_ == Tab::Depots) drawDepotsTab(body);
    else drawWhyTab(body);
}

// Cards sorted by priority: open requests first (highest heap key first),
// fulfilled ones at the bottom. Insertion sort on indices: n = 28, and it
// is stable, so equal keys keep their file order.
void App::drawRequestsTab(Rectangle r) {
    const DynamicArray<Request>& rq = manager_->requests();
    DynamicArray<int> order;
    auto before = [&](int a, int b) {
        bool da = rq[a].status == Status::Fulfilled, db = rq[b].status == Status::Fulfilled;
        if (da != db) return !da;
        return ReliefManager::heapKey(rq[a]) > ReliefManager::heapKey(rq[b]);
    };
    for (std::size_t i = 0; i < rq.size(); ++i) {
        int v = static_cast<int>(i);
        std::size_t j = order.size();
        order.pushBack(v);
        while (j > 0 && before(v, order[j - 1])) {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = v;
    }

    ui::caps(std::to_string(rq.size()) + " requests", {r.x + kGap + 8, r.y + 14}, kSmall, kMuted, Weight::Regular);
    ui::textRight("Sorted by priority", r.x + r.width - kGap, r.y + 14, kSmall, kFaint);

    Rectangle list{r.x, r.y + 40, r.width, r.height - 40};
    float contentH = static_cast<float>(order.size()) * kRowH;
    scrollWith(list, listScroll_, contentH);
    beginClip(list);
    for (std::size_t k = 0; k < order.size(); ++k) {
        const Request& q = rq[static_cast<std::size_t>(order[k])];
        Rectangle row{list.x, list.y + static_cast<float>(k) * kRowH - listScroll_, list.width, kRowH};
        if (row.y + row.height < list.y || row.y > list.y + list.height) continue;
        bool hot = ui::hovered(row) && ui::hovered(list);
        bool done = q.status == Status::Fulfilled;
        Color c = statusColor(q.status);
        if (hot) {
            DrawRectangleRec(row, kHover);
            panelHoverNode_ = q.locationId;
        }
        if (q.locationId == selNode_) DrawRectangleRec(row, kPressed);
        DrawRectangleRec({row.x + kGap, row.y + 10, 3, row.height - 20}, c);  // status strip

        float x = row.x + kGap + 15, right = row.x + row.width - kGap;
        Color main = done ? kMuted : kText;
        ui::text(q.id, {x, row.y + 10}, kSmall, main, Weight::SemiBold);
        std::string place = manager_->graph().node(q.locationId).name;
        float idW = ui::measure(q.id, kSmall, Weight::SemiBold) + 10;
        std::string rem = done ? "0" : formatNumber(q.remaining());
        float remW = ui::measure(rem, kBody, Weight::SemiBold);
        ui::text(ui::fit(place, right - x - idW - remW - 12, kSmall), {x + idW, row.y + 10}, kSmall, main);
        ui::text(rem, {right - remW, row.y + 8}, kBody, done ? kFulfilled : kText, Weight::SemiBold);

        float y2 = row.y + 30;
        std::string sup = supplyName(q.supply);
        ui::caps(sup, {x, y2}, kSmall, kMuted, Weight::Regular);
        float ux = x + ui::measureCaps(sup, kSmall, Weight::Regular) + 12;
        ui::caps("U" + std::to_string(q.urgency), {ux, y2}, kSmall, done ? kFaint : urgencyColor(q.urgency));
        std::string status = statusName(q.status);
        ui::caps(status, {right - ui::measureCaps(status, kSmall), y2}, kSmall, c);

        // delivered / asked
        float bw = right - x;
        DrawRectangleRec({x, row.y + row.height - 9, bw, 2}, kRuleSoft);
        float frac = q.quantity > 0 ? static_cast<float>(q.delivered) / static_cast<float>(q.quantity) : 0;
        DrawRectangleRec({x, row.y + row.height - 9, bw * frac, 2}, c);
        ui::hline(row.x + kGap, row.x + row.width, row.y + row.height - 1, kRuleSoft);

        if (hot && ui::clickedIn(row)) {
            selNode_ = q.locationId;
            selU_ = selV_ = -1;
        }
    }
    EndScissorMode();
    scrollbar(list, listScroll_, contentH);
}

void App::drawDepotsTab(Rectangle r) {
    const DynamicArray<Depot>& ds = manager_->depots();
    ui::caps(std::to_string(ds.size()) + " depots · stock", {r.x + kGap + 8, r.y + 14}, kSmall, kMuted, Weight::Regular);
    ui::textRight("Bar = share of largest", r.x + r.width - kGap, r.y + 14, kSmall, kFaint);

    const float blockH = 28 + 4 * 18 + 14;
    Rectangle list{r.x, r.y + 40, r.width, r.height - 40};
    float contentH = static_cast<float>(ds.size()) * blockH;
    scrollWith(list, depotScroll_, contentH);
    beginClip(list);
    for (std::size_t i = 0; i < ds.size(); ++i) {
        const Depot& d = ds[i];
        Rectangle block{list.x, list.y + static_cast<float>(i) * blockH - depotScroll_, list.width, blockH};
        if (block.y + block.height < list.y || block.y > list.y + list.height) continue;
        bool hot = ui::hovered(block) && ui::hovered(list);
        if (hot) {
            DrawRectangleRec(block, kHover);
            panelHoverNode_ = d.nodeId;
            if (ui::clickedIn(block)) {
                selNode_ = d.nodeId;
                selU_ = selV_ = -1;
            }
        }
        float x = block.x + kGap + 8, right = block.x + block.width - kGap;
        Vector2 sq{x + 4, block.y + 18};
        DrawRectangleLinesEx({sq.x - 4, sq.y - 4, 8, 8}, 1.6f, kDepot);
        ui::caps(depotLabel(manager_->graph().node(d.nodeId).name), {x + 16, block.y + 11}, kSmall, kText);
        for (int s = 0; s < kSupplyCount; ++s) {
            float y = block.y + 34 + static_cast<float>(s) * 18;
            ui::text(supplyName(static_cast<Supply>(s)), {x, y}, kSmall, kMuted);
            float bx = x + 76, bw = right - bx - 56;
            DrawRectangleRec({bx, y + 6, bw, 4}, kRuleSoft);
            float frac = static_cast<float>(d.stock[s]) / static_cast<float>(initialMax_[s]);
            if (frac > 1) frac = 1;
            DrawRectangleRec({bx, y + 6, bw * frac, 4}, d.stock[s] > 0 ? kText : kBlocked);
            ui::textRight(formatNumber(d.stock[s]), right, y, kSmall, d.stock[s] > 0 ? kText : kBlocked);
        }
        ui::hline(block.x + kGap, block.x + block.width, block.y + block.height - 1, kRuleSoft);
    }
    EndScissorMode();
    scrollbar(list, depotScroll_, contentH);
}

// The last Decision in full: sentence, legs, skipped depots and the
// priority maths that put this request at the top of the heap.
void App::drawWhyTab(Rectangle r) {
    const Decision& d = manager_->lastDecision();
    float x = r.x + kGap + 8, w = r.width - kGap * 2 - 8;
    if (!d.valid) {
        ui::caps("No decision yet", {x, r.y + kGap}, kSmall, kMuted);
        ui::paragraph("Press NEXT (N) to serve the most urgent request. This tab then explains which depots were "
                      "used, which were skipped, and why this request came first.",
                      {x, r.y + kGap + 26}, w, kSmall, kText);
        return;
    }

    Rectangle area{r.x, r.y, r.width, r.height};
    beginClip(area);
    float y = r.y + kGap - whyScroll_;
    const Request& q = manager_->requests()[static_cast<std::size_t>(d.requestIndex)];
    const Graph& g = manager_->graph();

    ui::text(d.requestId, {x, y}, kTitle, kText, Weight::SemiBold);
    std::string st = statusName(d.status);
    ui::caps(st, {x + w - ui::measureCaps(st, kSmall), y + 6}, kSmall, statusColor(d.status));
    y += 32;
    std::string sub = g.node(q.locationId).name + " · " + supplyName(q.supply) + " · " + formatNumber(q.people) + " people";
    ui::text(ui::fit(sub, w, kSmall), {x, y}, kSmall, kMuted);
    y += 26;
    y += ui::paragraph(d.explanation, {x, y}, w, kSmall, kText) + 12;

    ui::caps("Legs", {x, y}, kSmall, kMuted);
    y += 22;
    if (d.legs.isEmpty()) {
        ui::text("No delivery this time.", {x, y}, kSmall, kFaint);
        y += 22;
    }
    for (std::size_t i = 0; i < d.legs.size(); ++i) {
        const DecisionLeg& leg = d.legs[i];
        DrawRectangleRec({x, y + 2, 3, 14}, kRoute);
        std::string head = depotLabel(g.node(leg.depotNodeId).name) + "  →  " + formatNumber(leg.quantity);
        ui::text(head, {x + 12, y}, kSmall, kText, Weight::SemiBold);
        ui::textRight(formatMinutes(leg.minutes), x + w, y, kSmall, kText);
        y += 20;
        y += ui::paragraph("via " + (leg.via.empty() ? std::string("local roads") : leg.via), {x + 12, y}, w - 12, kSmall,
                           kMuted);
        std::string path;
        for (std::size_t k = 0; k < leg.path.size(); ++k) {
            if (k) path += " → ";
            path += g.node(leg.path[k]).name;
        }
        y += ui::paragraph(path, {x + 12, y}, w - 12, kSmall, kFaint) + 10;
    }

    if (!d.skipped.isEmpty()) {
        ui::caps("Skipped depots", {x, y}, kSmall, kMuted);
        y += 22;
        for (std::size_t i = 0; i < d.skipped.size(); ++i) {
            ui::text(depotLabel(g.node(d.skipped[i].depotNodeId).name), {x, y}, kSmall, kText, Weight::SemiBold);
            y += ui::paragraph(d.skipped[i].reason, {x + 96, y}, w - 96, kSmall, kMuted) + 4;
        }
        y += 8;
    }

    // Priority maths (the aging formula from relief_manager.h)
    ui::caps("Why it went first", {x, y}, kSmall, kMuted);
    y += 22;
    char buf[64];
    auto row = [&](const std::string& label, double value, Color c, bool strong) {
        ui::text(label, {x, y}, kSmall, strong ? kText : kMuted, strong ? Weight::SemiBold : Weight::Regular);
        std::snprintf(buf, sizeof(buf), "%.0f", value);
        ui::textRight(buf, x + w, y, kSmall, c, strong ? Weight::SemiBold : Weight::Regular);
        y += 20;
    };
    row("urgency " + std::to_string(q.urgency) + " × 100", q.urgency * 100.0, kText, false);
    row("people " + formatNumber(q.people) + " / 100", q.people / 100.0, kText, false);
    row(q.vulnerable ? "vulnerable" : "not vulnerable", q.vulnerable ? 50.0 : 0.0, kText, false);
    row("base score", ReliefManager::baseScore(q), kText, true);
    row("aging − 2 × arrival " + std::to_string(q.arrivalTime), -ReliefManager::kAgingPerMinute * q.arrivalTime, kText,
        false);
    row("heap key", ReliefManager::heapKey(q), kAccent, true);
    y += kGap;

    float contentH = y + whyScroll_ - r.y;
    EndScissorMode();
    scrollWith(area, whyScroll_, contentH);
    scrollbar(area, whyScroll_, contentH);
}

// ---------------------------------------------------------------- add request

std::string App::nextRequestId() const {
    int best = 0;
    const DynamicArray<Request>& rq = manager_->requests();
    for (std::size_t i = 0; i < rq.size(); ++i) {
        const std::string& id = rq[i].id;
        if (id.size() > 2 && id[0] == 'R' && id[1] == '-') {
            int n = std::atoi(id.c_str() + 2);
            if (n > best) best = n;
        }
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "R-%02d", best + 1);
    return buf;
}

void App::drawAddForm(Rectangle r) {
    float x = r.x + kGap + 8, w = r.width - kGap * 2 - 8;
    float y = r.y + 14;
    ui::caps("Add request", {x, y}, kSmall, kText);
    ui::textRight("Esc: cancel", x + w, y, kSmall, kFaint);
    DrawRectangleRec({x, r.y + kTabsH - 2, ui::measureCaps("Add request", kSmall), 2}, kAccent);
    ui::hline(r.x, r.x + r.width, r.y + kTabsH, kRule);
    y = r.y + kTabsH + kGap;

    ui::caps("Location", {x, y}, kSmall, kMuted, Weight::Regular);
    y += 20;
    if (form_.locationId < 0) {
        ui::text("Click a city or shelter on the map.", {x, y}, kBody, kAccent);
    } else {
        const Node& n = manager_->graph().node(form_.locationId);
        ui::text(ui::fit(n.name, w - 80, kBody, Weight::SemiBold), {x, y}, kBody, kText, Weight::SemiBold);
        ui::caps(nodeTypeName(n.type), {x + w - ui::measureCaps(nodeTypeName(n.type), kSmall, Weight::Regular), y + 3},
                 kSmall, kMuted, Weight::Regular);
    }
    y += 36;

    static const char* const kSupplies[kSupplyCount] = {"Food", "Water", "Medicine", "Shelter"};
    const float fieldH = 40, labelGap = 20, rowGap = 14;
    ui::caps("Supply", {x, y}, kSmall, kMuted, Weight::Regular);
    ui::cycler({x, y + labelGap, w, fieldH}, kSupplies, kSupplyCount, form_.supply);
    y += labelGap + fieldH + rowGap;

    float half = (w - kGap) / 2;
    ui::caps("Quantity", {x, y}, kSmall, kMuted, Weight::Regular);
    ui::caps("Urgency", {x + half + kGap, y}, kSmall, kMuted, Weight::Regular);
    ui::stepper({x, y + labelGap, half, fieldH}, form_.quantity, 50, 50, 5000);
    ui::stepper({x + half + kGap, y + labelGap, half, fieldH}, form_.urgency, 1, 1, 5);
    DrawRectangleRec({x + half + kGap, y + labelGap + fieldH - 2, half, 2}, urgencyColor(form_.urgency));
    y += labelGap + fieldH + rowGap;

    ui::caps("People affected", {x, y}, kSmall, kMuted, Weight::Regular);
    ui::stepper({x, y + labelGap, half, fieldH}, form_.people, 500, 0, 50000);
    ui::toggle({x + half + kGap, y + labelGap, half, fieldH}, "Vulnerable", form_.vulnerable);
    y += labelGap + fieldH + rowGap;

    // live preview of the priority this request will get
    Request preview;
    preview.urgency = form_.urgency;
    preview.people = form_.people;
    preview.vulnerable = form_.vulnerable;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Base score %.0f", ReliefManager::baseScore(preview));
    ui::text(buf, {x, y}, kSmall, kMuted);
    y += 24;
    if (!form_.error.empty()) {
        y += ui::paragraph(form_.error, {x, y}, w, kSmall, kBlocked) + 4;
    }

    std::string id = nextRequestId();
    float by = r.y + r.height - kGap - fieldH;
    if (ui::button({x, by, half, fieldH}, "Cancel")) setTool(Tool::Select);
    if (ui::button({x + half + kGap, by, half, fieldH}, "Add " + id, true, form_.locationId >= 0)) {
        Request q;
        q.id = id;
        q.locationId = form_.locationId;
        q.supply = static_cast<Supply>(form_.supply);
        q.quantity = form_.quantity;
        q.urgency = form_.urgency;
        q.people = form_.people;
        q.vulnerable = form_.vulnerable;
        try {
            manager_->addRequest(q);  // validates, logs the event, queues it
            selNode_ = form_.locationId;
            form_.locationId = -1;
            setTool(Tool::Select);
            tab_ = Tab::Requests;
        } catch (const std::exception& e) {
            form_.error = e.what();
        }
    }
}

// ---------------------------------------------------------------- event log

void App::drawEventLog(Rectangle r) {
    DrawRectangleRec(r, kPanel);
    ui::hline(r.x, r.x + r.width, r.y, kRule);
    const DynamicArray<Event>& ev = manager_->events();
    ui::caps("Event log", {r.x + kGap, r.y + 12}, kSmall, kMuted);
    ui::textRight(std::to_string(ev.size()) + " events · newest first", r.x + r.width - kGap, r.y + 12, kSmall, kFaint);

    const float rowH = 20;
    Rectangle list{r.x, r.y + 34, r.width, r.height - 40};
    float contentH = static_cast<float>(ev.size()) * rowH;
    scrollWith(list, logScroll_, contentH);
    beginClip(list);
    for (std::size_t k = 0; k < ev.size(); ++k) {
        float y = list.y + static_cast<float>(k) * rowH - logScroll_;
        if (y + rowH < list.y || y > list.y + list.height) continue;
        const Event& e = ev[ev.size() - 1 - k];  // newest first
        if (k == 0 && logScroll_ < 1) DrawRectangleRec({r.x, y - 2, 3, rowH}, eventColor(e.type));
        ui::text(formatClock(e.time), {r.x + kGap, y}, kSmall, k == 0 ? kText : kMuted);
        ui::caps(eventTypeName(e.type), {r.x + 120, y}, kSmall, eventColor(e.type));
        ui::text(ui::fit(e.text, r.width - 240 - kGap, kSmall), {r.x + 228, y}, kSmall, k == 0 ? kText : Color{200, 200, 194, 255});
    }
    EndScissorMode();
    scrollbar(list, logScroll_, contentH);
}
