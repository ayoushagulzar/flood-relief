// Tests for DynamicArray<T>. These are the "spec": make them all pass.
#include <string>

#include "test_framework.h"
#include "core/dynamic_array.h"

namespace {

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

void capacityDoubles() {
    DynamicArray<int> a;
    std::size_t growths = 0;
    std::size_t lastCap = a.capacity();
    for (int i = 0; i < 1000; ++i) {
        a.pushBack(i);
        if (a.capacity() != lastCap) {
            ++growths;
            if (lastCap != 0)
                CHECK_EQ(a.capacity(), lastCap * 2); // doubling
            lastCap = a.capacity();
        }
    }
    CHECK(growths <= 12); // doubling to 1000 needs ~10 growths; +10 would need 100
}

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

void copyIsIndependent() {
    DynamicArray<int> a;
    for (int i = 0; i < 5; ++i)
        a.pushBack(i);
    DynamicArray<int> b(a); // copy constructor
    b[0] = 100;
    b.pushBack(99);
    CHECK_EQ(a[0], 0); // original unchanged
    CHECK_EQ(a.size(), 5u);
    CHECK_EQ(b.size(), 6u);

    DynamicArray<int> c;
    c.pushBack(42);
    c = a; // copy assignment
    c[1] = -1;
    CHECK_EQ(a[1], 1);
    CHECK_EQ(c.size(), 5u);
}

void selfAssignment() {
    DynamicArray<int> a;
    for (int i = 0; i < 5; ++i)
        a.pushBack(i);
    DynamicArray<int>& alias = a;
    a = alias; // a = a must not destroy the data
    CHECK_EQ(a.size(), 5u);
    CHECK_EQ(a[4], 4);
}

void insertAtFrontMiddleEnd() {
    DynamicArray<int> a;
    a.pushBack(2);
    a.pushBack(4);
    a.insertAt(0, 1);        // front  -> 1 2 4
    a.insertAt(2, 3);        // middle -> 1 2 3 4
    a.insertAt(a.size(), 5); // end    -> 1 2 3 4 5 (index == size is allowed)
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
        a.pushBack(i); // 1 2 3 4 5
    a.removeAt(0);     // 2 3 4 5
    CHECK_EQ(a[0], 2);
    a.removeAt(1); // 2 4 5
    CHECK_EQ(a[1], 4);
    a.removeAt(a.size() - 1); // 2 4
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
