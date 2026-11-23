// tests for HashTable
#include <string>

#include "test_framework.h"
#include "core/hash_table.h"

namespace {

void insertAndFind() {
    HashTable t;
    t.insert("Sukkur", 3);
    t.insert("Karachi", 0);
    int v = -1;
    CHECK(t.find("Sukkur", v));
    CHECK_EQ(v, 3);
    CHECK(t.find("Karachi", v));
    CHECK_EQ(v, 0);
    CHECK_EQ(t.size(), 2u);
}

// wrong key -> false and out is not changed
void missingKeyReturnsFalse() {
    HashTable t;
    t.insert("Dadu", 5);
    int v = 123;
    CHECK(!t.find("Sukur", v));
    CHECK_EQ(v, 123);
    CHECK(!t.contains("Larkana"));
}

// same key again -> value updated, size stays 1
void duplicateKeyUpdates() {
    HashTable t;
    t.insert("Hyderabad", 1);
    t.insert("Hyderabad", 9);
    int v = 0;
    CHECK(t.find("Hyderabad", v));
    CHECK_EQ(v, 9);
    CHECK_EQ(t.size(), 1u);
}

// checked by hand: "" = 5381, "a" = 5381*33 + 97
void djb2KnownValues() {
    CHECK_EQ(HashTable::djb2(""), 5381u);
    CHECK_EQ(HashTable::djb2("a"), 5381u * 33u + 97u);
}

// starts with 2 buckets so it has to rehash many times
void tenThousandForcesRehash() {
    HashTable t(2);
    for (int i = 0; i < 10000; ++i) t.insert("key" + std::to_string(i), i);
    CHECK_EQ(t.size(), 10000u);
    bool allFound = true;
    for (int i = 0; i < 10000; ++i) {
        int v = -1;
        if (!t.find("key" + std::to_string(i), v) || v != i) allFound = false;
    }
    CHECK(allFound);
    CHECK(static_cast<double>(t.size()) / static_cast<double>(t.bucketCount()) <= 0.75);
}

// remove every 2nd key, the others must still be there
void removeManyKeepsTheRest() {
    HashTable t;
    for (int i = 0; i < 1000; ++i) t.insert("k" + std::to_string(i), i);
    bool allRemoved = true;
    for (int i = 0; i < 1000; i += 2)
        if (!t.remove("k" + std::to_string(i))) allRemoved = false;
    CHECK(allRemoved);
    CHECK_EQ(t.size(), 500u);
    bool rightKeysLeft = true;
    for (int i = 0; i < 1000; ++i) {
        bool shouldExist = (i % 2 == 1);
        if (t.contains("k" + std::to_string(i)) != shouldExist) rightKeysLeft = false;
    }
    CHECK(rightKeysLeft);
}

void removeInAnyOrder() {
    HashTable t;
    for (int i = 0; i < 6; ++i) t.insert("n" + std::to_string(i), i);
    CHECK(t.remove("n0"));
    CHECK(t.remove("n3"));
    CHECK(t.remove("n5"));
    CHECK(!t.remove("n5"));
    CHECK_EQ(t.size(), 3u);
    CHECK(t.contains("n1"));
    CHECK(t.contains("n2"));
    CHECK(t.contains("n4"));
    CHECK(!t.contains("n0"));
}

// 2 buckets -> lots of collisions
void removeFromSmallTable() {
    HashTable t(2);
    t.insert("a", 1);
    t.insert("b", 2);
    t.insert("c", 3);
    t.insert("d", 4);
    for (const char* k : {"a", "b", "c", "d"}) CHECK(t.remove(k));
    CHECK_EQ(t.size(), 0u);
    t.insert("a", 7);
    CHECK(t.contains("a"));
}

void clearFreesEverything() {
    HashTable t;
    for (int i = 0; i < 100; ++i) t.insert(std::to_string(i), i);
    t.clear();
    CHECK_EQ(t.size(), 0u);
    CHECK(!t.contains("5"));
}

// keys like the GUI tile cache uses
void tileCacheKeys() {
    HashTable t;
    t.insert("6/43/25", 101);
    t.insert("6/43/26", 102);
    t.insert("7/86/51", 205);

    int v = -1;
    CHECK(t.find("6/43/25", v));
    CHECK_EQ(v, 101);
    CHECK(t.find("6/43/26", v));
    CHECK_EQ(v, 102);
    CHECK(t.find("7/86/51", v));
    CHECK_EQ(v, 205);
    CHECK(!t.contains("6/43/27"));
    CHECK_EQ(t.size(), 3u);
}

}

// called from test_main.cpp
void runHashTableTests() {
    test::section("HashTable");
    test::runCase("insertAndFind", insertAndFind);
    test::runCase("missingKeyReturnsFalse", missingKeyReturnsFalse);
    test::runCase("duplicateKeyUpdates", duplicateKeyUpdates);
    test::runCase("djb2KnownValues", djb2KnownValues);
    test::runCase("tenThousandForcesRehash", tenThousandForcesRehash);
    test::runCase("removeManyKeepsTheRest", removeManyKeepsTheRest);
    test::runCase("removeInAnyOrder", removeInAnyOrder);
    test::runCase("removeFromSmallTable", removeFromSmallTable);
    test::runCase("clearFreesEverything", clearFreesEverything);
    test::runCase("tileCacheKeys", tileCacheKeys);
}
