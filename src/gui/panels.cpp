// panels.cpp - the right panel pages (Overview, Queue, Depots, Why, Events)
// and the Add-request form. All read straight from ReliefManager every
// frame (immediate mode).
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include "gui/app.h"
#include "gui/theme.h"
#include "gui/widgets.h"

using namespace theme;

namespace {

constexpr float kPagePad = 18;
constexpr float kRowH = 68;

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
        case EventType::Retry: return kAccent;
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

std::string lowerWord(const char* s) {
    std::string out = s;
    for (char& ch : out) ch = static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
    return out;
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
    ui::roundRect({area.x + area.width - 5, y, 3, h}, 1.5f, kAccentSoft);
}

void beginClip(Rectangle r) {
    BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width), static_cast<int>(r.height));
}

// Page title + a short muted line under it. Returns the y below.
float pageTitle(Rectangle r, const std::string& title, const std::string& sub) {
    ui::text(title, {r.x + kPagePad, r.y + 16}, kBody, kText, Weight::SemiBold);
    if (!sub.empty()) ui::text(sub, {r.x + kPagePad, r.y + 39}, kSmall, kMuted);
    return r.y + (sub.empty() ? 48 : 70);
}

// One "Last decision" tile: a bold top line and a small second line.
bool tile(Rectangle r, const std::string& top, const std::string& bottom, Color fill, bool filled) {
    bool hot = ui::hovered(r);
    if (filled) {
        ui::roundRect(r, 10, hot ? Color{static_cast<unsigned char>(fill.r + 14), static_cast<unsigned char>(fill.g + 10),
                                         static_cast<unsigned char>(fill.b + 12), 255}
                                 : fill);
    } else {
        ui::roundRect(r, 10, hot ? kHover : kCard);
        ui::roundBorder(r, 10, kRule);
    }
    Color fg = filled ? kBackground : kText;
    Color fg2 = filled ? Color{255, 255, 255, 215} : kMuted;
    std::string a = ui::fit(top, r.width - 16, kBody, Weight::SemiBold);
    std::string b = ui::fit(bottom, r.width - 16, kSmall);
    float aw = ui::measure(a, kBody, Weight::SemiBold), bw = ui::measure(b, kSmall);
    ui::text(a, {r.x + (r.width - aw) / 2, r.y + 13}, kBody, fg, Weight::SemiBold);
    ui::text(b, {r.x + (r.width - bw) / 2, r.y + 39}, kSmall, fg2);
    return ui::clickedIn(r);
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
    switch (page_) {
        case Page::Overview: drawOverviewPage(r); break;
        case Page::Queue: drawQueuePage(r); break;
        case Page::Depots: drawDepotsPage(r); break;
        case Page::Why: drawWhyPage(r); break;
        case Page::Events: drawEventsPage(r); break;
    }
}

// The heap AS STORED (array order: slot 0 is the root = served next), as
// bars. It is not sorted: a heap only promises parent >= children, so
// e.g. slot 3 can be bigger than slot 2. Then the last decision as 4 tiles.
void App::drawOverviewPage(Rectangle r) {
    const DynamicArray<Request>& rq = manager_->requests();
    float x = r.x + kPagePad, w = r.width - kPagePad * 2;
    ui::text("Priority heap", {x, r.y + 16}, kBody, kText, Weight::SemiBold);
    ui::textRight("key", x + w, r.y + 19, kSmall, kMuted);
    ui::text("array order · slot 0 is served next", {x, r.y + 39}, kSmall, kFaint);

    DynamicArray<int> heap = manager_->queueSnapshot();
    float y = r.y + 70;
    const std::size_t shown = heap.size() < 8 ? heap.size() : 8;
    double maxKey = 1;
    for (std::size_t i = 0; i < shown; ++i) {
        double k = ReliefManager::heapKey(rq[static_cast<std::size_t>(heap[i])]);
        if (k > maxKey) maxKey = k;
    }
    if (heap.isEmpty()) {
        ui::paragraph("The heap is empty: every request has been handled.", {x, y}, w, kSmall, kMuted);
        y += 40;
    }
    for (std::size_t i = 0; i < shown; ++i) {
        const Request& q = rq[static_cast<std::size_t>(heap[i])];
        Rectangle row{x - 6, y, w + 12, 30};
        bool hot = ui::hovered(row);
        if (hot) {
            ui::roundRect(row, 6, kHover);
            panelHoverNode_ = q.locationId;
            tooltip_ = q.id + " · " + manager_->graph().node(q.locationId).name + " · " + lowerWord(supplyName(q.supply)) +
                       " · urgency " + std::to_string(q.urgency);
            tooltipPos_ = {r.x - 250, row.y};
            if (ui::clickedIn(row)) {
                selNode_ = q.locationId;
                selU_ = selV_ = -1;
            }
        }
        double key = ReliefManager::heapKey(q);
        ui::text(q.id, {x, y + 6}, kSmall, kMuted);
        float bx = x + 54, bw = w - 54 - 50;
        ui::roundRect({bx, y + 12, bw, 7}, 3.5f, kAccentTint);
        float frac = static_cast<float>(key / maxKey);
        if (frac < 0.04f) frac = 0.04f;
        ui::roundRect({bx, y + 12, bw * frac, 7}, 3.5f, i == 0 ? kBrand : kAccent);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f", key);
        ui::textRight(buf, x + w, y + 6, kSmall, q.urgency >= 5 ? kAlert : kText, Weight::SemiBold);
        y += 32;
    }
    if (heap.size() > shown) {
        ui::text("+ " + std::to_string(heap.size() - shown) + " more in the heap  ·  Queue page", {x, y + 2}, kSmall,
                 kFaint);
        y += 22;
    }

    y += 10;
    ui::hline(x, x + w, y, kRule);
    y += 16;
    ui::text("Last decision", {x, y}, kBody, kText, Weight::SemiBold);
    y += 32;

    const Decision& d = manager_->lastDecision();
    float tw = (w - 8) / 2, th = 64;
    if (!d.valid) {
        Rectangle box{x, y, w, th * 2 + 8};
        ui::roundRect(box, 10, kField);
        ui::roundBorder(box, 10, kRule);
        ui::paragraph("Press Next request (N). The top of the heap is served, and the decision shows up here.",
                      {box.x + 14, box.y + 16}, box.width - 28, kSmall, kMuted);
        return;
    }
    const Request& q = rq[static_cast<std::size_t>(d.requestIndex)];
    const Graph& g = manager_->graph();

    // 1: what was delivered
    double longest = 0;
    for (std::size_t i = 0; i < d.legs.size(); ++i)
        if (d.legs[i].minutes > longest) longest = d.legs[i].minutes;
    std::string t1 = d.requestId + " " + g.node(q.locationId).name;
    std::string b1 = d.deliveredNow > 0 ? formatNumber(d.deliveredNow) + " " + lowerWord(supplyName(q.supply)) + " · " +
                                              formatMinutes(longest)
                                        : std::string(statusName(d.status)) + " · nothing sent";
    Color c1 = d.status == Status::Deferred ? kAlert : kAccent;
    // 2: from where
    std::string t2, b2;
    if (d.legs.size() == 1) {
        t2 = "from " + g.node(d.legs[0].depotNodeId).name;
        b2 = "via " + (d.legs[0].via.empty() ? std::string("local roads") : d.legs[0].via);
    } else if (d.legs.size() > 1) {
        t2 = "from " + std::to_string(d.legs.size()) + " depots";
        for (std::size_t i = 0; i < d.legs.size(); ++i) b2 += (i ? ", " : "") + depotLabel(g.node(d.legs[i].depotNodeId).name);
    } else {
        t2 = "no depot reached it";
        b2 = d.skipped.isEmpty() ? "no stock" : d.skipped[0].reason;
    }
    // 3: the key and where it comes from
    char t3[32], b3[96];
    std::snprintf(t3, sizeof(t3), "Key %.0f", ReliefManager::heapKey(q));
    std::snprintf(b3, sizeof(b3), "%d + %.0f + %d − %.0f", q.urgency * 100, q.people / 100.0, q.vulnerable ? 50 : 0,
                  ReliefManager::kAgingPerMinute * q.arrivalTime);
    // 4: what comes next
    std::string t4 = "Heap empty", b4 = "all requests handled";
    if (!heap.isEmpty()) {
        const Request& n = rq[static_cast<std::size_t>(heap[0])];
        t4 = "Next: " + n.id;
        b4 = g.node(n.locationId).name + " · " + lowerWord(supplyName(n.supply));
    }

    bool open = false;
    open |= tile({x, y, tw, th}, t1, b1, c1, true);
    open |= tile({x + tw + 8, y, tw, th}, t2, b2, kAccent, false);
    open |= tile({x, y + th + 8, tw, th}, t3, b3, kAccent, false);
    open |= tile({x + tw + 8, y + th + 8, tw, th}, t4, b4, kAccent, true);
    ui::text("Click a tile for the full explanation", {x, y + th * 2 + 20}, kSmall, kFaint);
    if (open) {
        page_ = Page::Why;
        whyScroll_ = 0;
    }
}

// Every request, sorted by priority: open requests first (highest heap key
// first), fulfilled ones at the bottom. Insertion sort on indices: n = 28,
// and it is stable, so equal keys keep their file order.
void App::drawQueuePage(Rectangle r) {
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

    float top = pageTitle(r, "Queue", std::to_string(rq.size()) + " requests · most urgent first");
    Rectangle list{r.x, top, r.width, r.y + r.height - top};
    float contentH = static_cast<float>(order.size()) * kRowH + 8;
    scrollWith(list, listScroll_, contentH);
    beginClip(list);
    for (std::size_t k = 0; k < order.size(); ++k) {
        const Request& q = rq[static_cast<std::size_t>(order[k])];
        Rectangle row{list.x + 10, list.y + static_cast<float>(k) * kRowH - listScroll_, list.width - 20, kRowH - 4};
        if (row.y + row.height < list.y || row.y > list.y + list.height) continue;
        bool hot = ui::hovered(row) && ui::hovered(list);
        bool done = q.status == Status::Fulfilled;
        Color c = statusColor(q.status);
        if (q.locationId == selNode_) ui::roundRect(row, 10, kPressed);
        else if (hot) ui::roundRect(row, 10, kHover);
        if (hot) panelHoverNode_ = q.locationId;
        ui::roundRect({row.x + 8, row.y + 10, 3, row.height - 20}, 1.5f, c);  // status strip

        float x = row.x + 20, right = row.x + row.width - 10;
        Color main = done ? kMuted : kText;
        ui::text(q.id, {x, row.y + 9}, kSmall, main, Weight::SemiBold);
        std::string place = manager_->graph().node(q.locationId).name;
        float idW = ui::measure(q.id, kSmall, Weight::SemiBold) + 8;
        std::string rem = done ? "done" : formatNumber(q.remaining());
        float remW = ui::measure(rem, kBody, Weight::SemiBold);
        ui::text(ui::fit(place, right - x - idW - remW - 12, kSmall), {x + idW, row.y + 9}, kSmall, main);
        ui::text(rem, {right - remW, row.y + 7}, kBody, done ? kFulfilled : kText, Weight::SemiBold);

        float y2 = row.y + 33;
        std::string sup = supplyName(q.supply);
        ui::text(sup, {x, y2}, kSmall, kMuted);
        float ux = x + ui::measure(sup, kSmall) + 10;
        ui::text("urgency " + std::to_string(q.urgency), {ux, y2}, kSmall, done ? kFaint : urgencyColor(q.urgency),
                 q.urgency >= 5 && !done ? Weight::SemiBold : Weight::Regular);
        std::string status = statusName(q.status);
        ui::textRight(status, right, y2, kSmall, c);

        // delivered / asked
        float bw = right - x;
        ui::roundRect({x, row.y + row.height - 8, bw, 3}, 1.5f, kRuleSoft);
        float frac = q.quantity > 0 ? static_cast<float>(q.delivered) / static_cast<float>(q.quantity) : 0;
        if (frac > 0) ui::roundRect({x, row.y + row.height - 8, bw * frac, 3}, 1.5f, c);

        if (hot && ui::clickedIn(row)) {
            selNode_ = q.locationId;
            selU_ = selV_ = -1;
        }
    }
    EndScissorMode();
    scrollbar(list, listScroll_, contentH);
}

void App::drawDepotsPage(Rectangle r) {
    const DynamicArray<Depot>& ds = manager_->depots();
    float top = pageTitle(r, "Depots", std::to_string(ds.size()) + " depots · stock left");
    const float blockH = 34 + 4 * 22 + 16;
    Rectangle list{r.x, top, r.width, r.y + r.height - top};
    float contentH = static_cast<float>(ds.size()) * blockH;
    scrollWith(list, depotScroll_, contentH);
    beginClip(list);
    for (std::size_t i = 0; i < ds.size(); ++i) {
        const Depot& d = ds[i];
        Rectangle block{list.x + 10, list.y + static_cast<float>(i) * blockH - depotScroll_, list.width - 20, blockH - 6};
        if (block.y + block.height < list.y || block.y > list.y + list.height) continue;
        bool hot = ui::hovered(block) && ui::hovered(list);
        if (d.nodeId == selNode_) ui::roundRect(block, 10, kPressed);
        else if (hot) ui::roundRect(block, 10, kHover);
        if (hot) {
            panelHoverNode_ = d.nodeId;
            if (ui::clickedIn(block)) {
                selNode_ = d.nodeId;
                selU_ = selV_ = -1;
            }
        }
        float x = block.x + 10, right = block.x + block.width - 10;
        DrawRectangleRec({x, block.y + 15, 9, 9}, kDepot);
        ui::text(depotLabel(manager_->graph().node(d.nodeId).name), {x + 18, block.y + 8}, kSmall, kText,
                 Weight::SemiBold);
        for (int s = 0; s < kSupplyCount; ++s) {
            float y = block.y + 35 + static_cast<float>(s) * 22;
            ui::text(supplyName(static_cast<Supply>(s)), {x, y}, kSmall, kMuted);
            float bx = x + 86, bw = right - bx - 60;
            ui::roundRect({bx, y + 7, bw, 6}, 3, kAccentTint);
            float frac = static_cast<float>(d.stock[s]) / static_cast<float>(initialMax_[s]);
            if (frac > 1) frac = 1;
            if (frac > 0) ui::roundRect({bx, y + 7, bw * frac, 6}, 3, kAccent);
            ui::textRight(formatNumber(d.stock[s]), right, y, kSmall, d.stock[s] > 0 ? kText : kAlert,
                          d.stock[s] > 0 ? Weight::Regular : Weight::SemiBold);
        }
    }
    EndScissorMode();
    scrollbar(list, depotScroll_, contentH);
}

// The last Decision in full: sentence, legs, skipped depots and the
// priority maths that put this request at the top of the heap.
void App::drawWhyPage(Rectangle r) {
    const Decision& d = manager_->lastDecision();
    float x = r.x + kPagePad, w = r.width - kPagePad * 2;
    if (!d.valid) {
        float top = pageTitle(r, "Why", "the reasoning behind the last decision");
        ui::paragraph("Press Next request (N) to serve the most urgent request. This page then explains which depots "
                      "were used, which were skipped, and why this request came first.",
                      {x, top}, w, kSmall, kText);
        return;
    }

    Rectangle area{r.x, r.y, r.width, r.height};
    beginClip(area);
    float y = r.y + 16 - whyScroll_;
    const Request& q = manager_->requests()[static_cast<std::size_t>(d.requestIndex)];
    const Graph& g = manager_->graph();

    ui::text(d.requestId, {x, y - 4}, kTitle, kText, Weight::SemiBold);
    std::string st = statusName(d.status);
    float sw = ui::measure(st, kSmall, Weight::SemiBold) + 18;
    Rectangle pill{x + w - sw, y, sw, 26};
    ui::roundRect(pill, 13, Fade(statusColor(d.status), 0.15f));
    ui::text(st, {pill.x + 9, pill.y + 5}, kSmall, statusColor(d.status), Weight::SemiBold);
    y += 36;
    std::string sub = g.node(q.locationId).name + " · " + supplyName(q.supply) + " · " + formatNumber(q.people) + " people";
    ui::text(ui::fit(sub, w, kSmall), {x, y}, kSmall, kMuted);
    y += 28;
    y += ui::paragraph(d.explanation, {x, y}, w, kSmall, kText) + 14;

    ui::text("Deliveries", {x, y}, kSmall, kText, Weight::SemiBold);
    y += 25;
    if (d.legs.isEmpty()) {
        ui::text("No delivery this time.", {x, y}, kSmall, kFaint);
        y += 22;
    }
    for (std::size_t i = 0; i < d.legs.size(); ++i) {
        const DecisionLeg& leg = d.legs[i];
        ui::roundRect({x, y + 3, 3, 16}, 1.5f, kAccent);
        std::string head = depotLabel(g.node(leg.depotNodeId).name) + "  ›  " + formatNumber(leg.quantity);
        ui::text(head, {x + 12, y}, kSmall, kText, Weight::SemiBold);
        ui::textRight(formatMinutes(leg.minutes), x + w, y, kSmall, kText);
        y += 23;
        y += ui::paragraph("via " + (leg.via.empty() ? std::string("local roads") : leg.via), {x + 12, y}, w - 12, kSmall,
                           kMuted);
        std::string path;
        for (std::size_t k = 0; k < leg.path.size(); ++k) {
            if (k) path += " › ";
            path += g.node(leg.path[k]).name;
        }
        y += ui::paragraph(path, {x + 12, y}, w - 12, kSmall, kFaint) + 10;
    }

    if (!d.skipped.isEmpty()) {
        ui::text("Skipped depots", {x, y}, kSmall, kText, Weight::SemiBold);
        y += 25;
        for (std::size_t i = 0; i < d.skipped.size(); ++i) {
            ui::text(depotLabel(g.node(d.skipped[i].depotNodeId).name), {x, y}, kSmall, kText, Weight::SemiBold);
            y += ui::paragraph(d.skipped[i].reason, {x + 100, y}, w - 100, kSmall, kMuted) + 4;
        }
        y += 8;
    }

    // Priority maths (the aging formula from relief_manager.h), in a card.
    ui::text("Why it went first", {x, y}, kSmall, kText, Weight::SemiBold);
    y += 25;
    Rectangle mathCard{x - 4, y - 6, w + 8, 6 * 23 + 14};
    ui::card(mathCard, 10);
    y += 2;
    char buf[64];
    auto row = [&](const std::string& label, double value, Color c, bool strong) {
        ui::text(label, {x + 8, y}, kSmall, strong ? kText : kMuted, strong ? Weight::SemiBold : Weight::Regular);
        std::snprintf(buf, sizeof(buf), "%.0f", value == 0 ? 0.0 : value);  // never "-0"
        ui::textRight(buf, x + w - 8, y, kSmall, c, strong ? Weight::SemiBold : Weight::Regular);
        y += 23;
    };
    row("urgency " + std::to_string(q.urgency) + " × 100", q.urgency * 100.0, kText, false);
    row("people " + formatNumber(q.people) + " ÷ 100", q.people / 100.0, kText, false);
    row(q.vulnerable ? "vulnerable" : "not vulnerable", q.vulnerable ? 50.0 : 0.0, kText, false);
    row("base score", ReliefManager::baseScore(q), kText, true);
    row("aging − 2 × arrival " + std::to_string(q.arrivalTime), -ReliefManager::kAgingPerMinute * q.arrivalTime, kText,
        false);
    row("priority key (what the heap sorts)", ReliefManager::heapKey(q), kAccent, true);
    y += kGap * 2;

    float contentH = y + whyScroll_ - r.y;
    EndScissorMode();
    scrollWith(area, whyScroll_, contentH);
    scrollbar(area, whyScroll_, contentH);
}

void App::drawEventsPage(Rectangle r) {
    const DynamicArray<Event>& ev = manager_->events();
    float top = pageTitle(r, "Events", std::to_string(ev.size()) + " events · newest first");
    Rectangle list{r.x, top, r.width, r.y + r.height - top};
    float x = r.x + kPagePad, w = r.width - kPagePad * 2;

    // Rows have different heights (wrapped text), so measure while drawing.
    beginClip(list);
    float y = list.y - logScroll_;
    for (std::size_t k = 0; k < ev.size(); ++k) {
        const Event& e = ev[ev.size() - 1 - k];  // newest first
        DynamicArray<std::string> lines = ui::wrap(e.text, w - 14, kSmall);
        float h = 24 + static_cast<float>(lines.size()) * 20 + 12;
        if (y + h >= list.y && y <= list.y + list.height) {
            DrawCircleV({x + 3, y + 10}, 4, eventColor(e.type));
            std::string name = eventTypeName(e.type);
            if (!name.empty() && name[0] >= 'a' && name[0] <= 'z') name[0] = static_cast<char>(name[0] - 'a' + 'A');
            ui::text(name, {x + 14, y}, kSmall, eventColor(e.type), Weight::SemiBold);
            ui::textRight(formatClock(e.time), x + w, y, kSmall, kFaint);
            for (std::size_t i = 0; i < lines.size(); ++i)
                ui::text(lines[i], {x + 14, y + 23 + static_cast<float>(i) * 20}, kSmall, k == 0 ? kText : kMuted);
        }
        y += h;
    }
    float contentH = y + logScroll_ - list.y;
    EndScissorMode();
    scrollWith(list, logScroll_, contentH);
    scrollbar(list, logScroll_, contentH);
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
    float x = r.x + kPagePad, w = r.width - kPagePad * 2;
    float y = pageTitle(r, "Add request", "Esc to cancel");

    ui::text("Location", {x, y}, kSmall, kMuted);
    y += 21;
    Rectangle loc{x, y, w, 44};
    ui::roundRect(loc, 9, form_.locationId < 0 ? kField : kAccentTint);
    ui::roundBorder(loc, 9, form_.locationId < 0 ? kRule : kAccent);
    if (form_.locationId < 0) {
        ui::text("Click a city or camp on the map", {loc.x + 12, loc.y + 12}, kSmall, kAccent, Weight::SemiBold);
    } else {
        const Node& n = manager_->graph().node(form_.locationId);
        ui::text(ui::fit(n.name, w - 100, kBody, Weight::SemiBold), {loc.x + 12, loc.y + 12}, kBody, kText,
                 Weight::SemiBold);
        ui::textRight(nodeTypeName(n.type), loc.x + loc.width - 12, loc.y + 14, kSmall, kMuted);
    }
    y += 44 + 14;

    static const char* const kSupplies[kSupplyCount] = {"Food", "Water", "Medicine", "Shelter"};
    const float fieldH = 42, labelGap = 21, rowGap = 12;
    ui::text("Supply", {x, y}, kSmall, kMuted);
    ui::cycler({x, y + labelGap, w, fieldH}, kSupplies, kSupplyCount, form_.supply);
    y += labelGap + fieldH + rowGap;

    float half = (w - 12) / 2;
    ui::text("Quantity", {x, y}, kSmall, kMuted);
    ui::text("Urgency (1-5)", {x + half + 12, y}, kSmall, kMuted);
    ui::stepper({x, y + labelGap, half, fieldH}, form_.quantity, 50, 50, 5000);
    ui::stepper({x + half + 12, y + labelGap, half, fieldH}, form_.urgency, 1, 1, 5);
    ui::roundRect({x + half + 12 + 14, y + labelGap + fieldH - 4, half - 28, 3}, 1.5f, urgencyColor(form_.urgency));
    y += labelGap + fieldH + rowGap;

    ui::text("People affected", {x, y}, kSmall, kMuted);
    ui::stepper({x, y + labelGap, half, fieldH}, form_.people, 500, 0, 50000);
    ui::toggle({x + half + 12, y + labelGap, half, fieldH}, "Vulnerable", form_.vulnerable);
    y += labelGap + fieldH + rowGap + 4;

    // live preview of the priority this request will get
    Request preview;
    preview.urgency = form_.urgency;
    preview.people = form_.people;
    preview.vulnerable = form_.vulnerable;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.0f", ReliefManager::baseScore(preview));
    ui::text("Base score", {x, y}, kSmall, kMuted);
    ui::textRight(buf, x + w, y - 2, kBody, kAccent, Weight::SemiBold);
    y += 24;
    if (!form_.error.empty()) {
        y += ui::paragraph(form_.error, {x, y}, w, kSmall, kAlert) + 4;
    }

    std::string id = nextRequestId();
    float by = r.y + r.height - kPagePad - fieldH;
    if (ui::button({x, by, half, fieldH}, "Cancel")) setTool(Tool::Select);
    if (ui::button({x + half + 12, by, half, fieldH}, "Add " + id, true, form_.locationId >= 0)) {
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
            page_ = Page::Queue;
        } catch (const std::exception& e) {
            form_.error = e.what();
        }
    }
}
