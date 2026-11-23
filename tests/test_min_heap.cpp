// tests for MinHeap
#include <cstdlib>
#include <string>

#include "core/min_heap.h"
#include "test_framework.h"

namespace {

// makes it a max heap (bigger first)
struct Greater {
    bool operator()(int a, int b) const { return a > b; }
};

// small struct to test a custom comparator
struct Job {
    std::string name;
    int score;
};
struct HigherScoreFirst {
    bool operator()(const Job& a, const Job& b) const { return a.score > b.score; }
};

void basicPushPopOrder() {
    MinHeap<int> h;
    h.push(5);
    h.push(3);
    h.push(8);
    h.push(1);
    CHECK_EQ(h.top(), 1);
    CHECK_EQ(h.pop(), 1);
    CHECK_EQ(h.pop(), 3);
    CHECK_EQ(h.size(), 2u);
    CHECK_EQ(h.top(), 5);
}

// 1000 random numbers should come out sorted
void thousandRandomComeOutSorted() {
    MinHeap<int> h;
    std::srand(42);
    for (int i = 0; i < 1000; ++i)
        h.push(std::rand() % 10000);
    CHECK_EQ(h.size(), 1000u);
    int prev = -1;
    bool sorted = true;
    while (!h.isEmpty()) {
        int x = h.pop();
        if (x < prev)
            sorted = false;
        prev = x;
    }
    CHECK(sorted);
}

void duplicates() {
    MinHeap<int> h;
    for (int i = 0; i < 5; ++i)
        h.push(7);
    h.push(3);
    CHECK_EQ(h.pop(), 3);
    for (int i = 0; i < 5; ++i)
        CHECK_EQ(h.pop(), 7);
    CHECK(h.isEmpty());
}

// with Greater the biggest number comes first
void maxHeapWithComparator() {
    MinHeap<int, Greater> h;
    for (int x : {4, 9, 1, 7, 3})
        h.push(x);
    CHECK_EQ(h.pop(), 9);
    CHECK_EQ(h.pop(), 7);
    CHECK_EQ(h.pop(), 4);
}

void structWithComparator() {
    MinHeap<Job, HigherScoreFirst> h;
    h.push({"water", 250});
    h.push({"food", 510});
    h.push({"medicine", 330});
    CHECK(h.pop().name == "food");
    CHECK(h.pop().name == "medicine");
}

// pop / top on empty heap should throw
void emptyThrows() {
    MinHeap<int> h;
    CHECK_THROWS_AS(h.pop(), std::out_of_range);
    CHECK_THROWS_AS(h.top(), std::out_of_range);
    h.push(1);
    h.pop();
    CHECK_THROWS_AS(h.pop(), std::out_of_range);
}

void clearEmpties() {
    MinHeap<int> h;
    for (int i = 0; i < 10; ++i)
        h.push(i);
    h.clear();
    CHECK(h.isEmpty());
    h.push(5);
    CHECK_EQ(h.top(), 5);
}

// push 10..1 so every item has to move up
void descendingPushSiftUp() {
    MinHeap<int> h;
    for (int i = 10; i >= 1; --i)
        h.push(i);
    CHECK_EQ(h.size(), 10u);
    for (int expected = 1; expected <= 10; ++expected) {
        CHECK_EQ(h.pop(), expected);
    }
    CHECK(h.isEmpty());
}
}

// called from test_main.cpp
void runMinHeapTests() {
    test::section("MinHeap");
    test::runCase("basicPushPopOrder", basicPushPopOrder);
    test::runCase("thousandRandomComeOutSorted", thousandRandomComeOutSorted);
    test::runCase("duplicates", duplicates);
    test::runCase("maxHeapWithComparator", maxHeapWithComparator);
    test::runCase("structWithComparator", structWithComparator);
    test::runCase("emptyThrows", emptyThrows);
    test::runCase("clearEmpties", clearEmpties);
    test::runCase("descendingPushSiftUp", descendingPushSiftUp);
}
