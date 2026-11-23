// tests for DynamicArray
#include <string>

#include "core/dynamic_array.h"
#include "test_framework.h"

// anonymous namespace so these functions stay in this file only
namespace {

// push 10000 values and check all of them
void pushBackManyValues() {
    DynamicArray<int> a;
    for (int i = 0; i < 10000; ++i)
        a.pushBack(i * 2);
    CHECK_EQ(a.size(), 10000u);
    bool allRight = true;
    for (int i = 0; i < 10000; ++i)
        if (a[static_cast<std::size_t>(i)] != i * 2)
            allRight = false;
    CHECK(allRight);
    CHECK(a.capacity() >= a.size());
}

// capacity should double every time
void capacityDoubles() {
    DynamicArray<int> a;
    std::size_t growths = 0;
    std::size_t lastCap = a.capacity();
    for (int i = 0; i < 1000; ++i) {
        a.pushBack(i);
        if (a.capacity() != lastCap) {
            ++growths;
            if (lastCap != 0)
                CHECK_EQ(a.capacity(), lastCap * 2);
            lastCap = a.capacity();
        }
    }
    CHECK(growths <= 12);
}

// empty array should throw instead of crashing
void emptyArray() {
    DynamicArray<int> a;
    CHECK(a.isEmpty());
    CHECK_EQ(a.size(), 0u);
    CHECK_THROWS_AS(a.popBack(), std::out_of_range);
    CHECK_THROWS_AS(a.back(), std::out_of_range);
    CHECK_THROWS_AS(a.at(0), std::out_of_range);
}

void popBackAndBack() {
    DynamicArray<int> a;
    a.pushBack(1);
    a.pushBack(2);
    a.pushBack(3);
    CHECK_EQ(a.back(), 3);
    a.popBack();
    CHECK_EQ(a.back(), 2);
    CHECK_EQ(a.size(), 2u);
}

void atChecksBounds() {
    DynamicArray<int> a;
    a.pushBack(7);
    CHECK_EQ(a.at(0), 7);
    CHECK_THROWS_AS(a.at(1), std::out_of_range);
    const DynamicArray<int>& c = a;
    CHECK_EQ(c.at(0), 7);
    CHECK_EQ(c[0], 7);
}

// copy should have its own memory (deep copy)
void copyIsIndependent() {
    DynamicArray<int> a;
    for (int i = 0; i < 5; ++i)
        a.pushBack(i);
    DynamicArray<int> b(a);
    b[0] = 100;
    b.pushBack(99);
    CHECK_EQ(a[0], 0);
    CHECK_EQ(a.size(), 5u);
    CHECK_EQ(b.size(), 6u);

    DynamicArray<int> c;
    c.pushBack(42);
    c = a;
    c[1] = -1;
    CHECK_EQ(a[1], 1);
    CHECK_EQ(c.size(), 5u);
}

// a = a should not break the array
void selfAssignment() {
    DynamicArray<int> a;
    for (int i = 0; i < 5; ++i)
        a.pushBack(i);
    DynamicArray<int>& alias = a;
    a = alias;
    CHECK_EQ(a.size(), 5u);
    CHECK_EQ(a[4], 4);
}

void insertAtFrontMiddleEnd() {
    DynamicArray<int> a;
    a.pushBack(2);
    a.pushBack(4);
    a.insertAt(0, 1);
    a.insertAt(2, 3);
    a.insertAt(a.size(), 5);
    CHECK_EQ(a.size(), 5u);
    bool inOrder = true;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i] != static_cast<int>(i) + 1)
            inOrder = false;
    CHECK(inOrder);
    CHECK_THROWS_AS(a.insertAt(a.size() + 1, 0), std::out_of_range);
}

void removeAtFrontMiddleEnd() {
    DynamicArray<int> a;
    for (int i = 1; i <= 5; ++i)
        a.pushBack(i);
    a.removeAt(0);
    CHECK_EQ(a[0], 2);
    a.removeAt(1);
    CHECK_EQ(a[1], 4);
    a.removeAt(a.size() - 1);
    CHECK_EQ(a.size(), 2u);
    CHECK_EQ(a.back(), 4);
    CHECK_THROWS_AS(a.removeAt(2), std::out_of_range);
}

void clearKeepsWorking() {
    DynamicArray<int> a;
    for (int i = 0; i < 10; ++i)
        a.pushBack(i);
    a.clear();
    CHECK(a.isEmpty());
    a.pushBack(5);
    CHECK_EQ(a[0], 5);
}

// template should work with strings too
void worksWithStrings() {
    DynamicArray<std::string> a;
    a.pushBack("Sukkur");
    a.pushBack("Hyderabad");
    a.insertAt(1, "Larkana");
    CHECK(a[1] == "Larkana");
    DynamicArray<std::string> b = a;
    b[0] = "Karachi";
    CHECK(a[0] == "Sukkur");
}

void insertEmptyAndRemoveUntilEmpty() {
    DynamicArray<int> a;
    a.insertAt(0, 77);
    CHECK_EQ(a.size(), 1u);
    CHECK_EQ(a[0], 77);

    a.removeAt(0);
    CHECK(a.isEmpty());
    CHECK_EQ(a.size(), 0u);
    CHECK_THROWS_AS(a.popBack(), std::out_of_range);
}
}

// called from test_main.cpp
void runDynamicArrayTests() {
    test::section("DynamicArray");
    test::runCase("pushBackManyValues", pushBackManyValues);
    test::runCase("capacityDoubles", capacityDoubles);
    test::runCase("emptyArray", emptyArray);
    test::runCase("popBackAndBack", popBackAndBack);
    test::runCase("atChecksBounds", atChecksBounds);
    test::runCase("copyIsIndependent", copyIsIndependent);
    test::runCase("selfAssignment", selfAssignment);
    test::runCase("insertAtFrontMiddleEnd", insertAtFrontMiddleEnd);
    test::runCase("removeAtFrontMiddleEnd", removeAtFrontMiddleEnd);
    test::runCase("clearKeepsWorking", clearKeepsWorking);
    test::runCase("worksWithStrings", worksWithStrings);
    test::runCase("insertEmptyAndRemoveUntilEmpty", insertEmptyAndRemoveUntilEmpty);
}
