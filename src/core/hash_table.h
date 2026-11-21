// HashTable.h - string -> int map with separate chaining.
//
// Used for: city name -> node id (scenario loader), request id -> index
// (relief manager), "z/x/y" -> tile texture index (GUI map).
//
//   index = djb2(key) % bucketCount
//   each bucket = a singly linked list of Nodes (your own struct below)
//   load factor = count / buckets; above 0.75 -> rehash() doubles the buckets
//
// Not a template, so the code goes in HashTable.cpp (normal header/source split).
// IMPORTANT for CMake: this is the first .cpp in src/core, so relief_core must be
// a STATIC library from now on (not INTERFACE). See the kit notes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/dynamic_array.h"

class HashTable {
public:
    // O(buckets). Starts with all buckets empty (nullptr).
    explicit HashTable(std::size_t initialBuckets = 16);

    // O(n + buckets). Frees every node.
    ~HashTable();

    // Copying is disabled on purpose: a copy would need to deep-copy every
    // chain. We never need that, so we forbid it (honest and safe).
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    // O(1) average, O(n) worst. Existing key -> value is UPDATED, size unchanged.
    void insert(const std::string& key, int value);

    // O(1) average. true + out set if found; false and out untouched if not.
    bool find(const std::string& key, int& out) const;

    // O(1) average.
    bool contains(const std::string& key) const;

    // O(1) average. true if something was removed.
    bool remove(const std::string& key);

    // O(1)
    std::size_t size() const { return count_; }
    // O(1)
    std::size_t bucketCount() const { return buckets_.size(); }

    // O(n + buckets). Deletes all nodes, keeps the buckets.
    void clear();

    // O(length of s). h = 5381; for each char: h = h * 33 + c (64-bit unsigned).
    static std::uint64_t djb2(const std::string& s);

private:
    struct Node {
        std::string key;
        int value;
        Node* next;
    };

    // O(length of key). Which bucket does key go in, for a table with `buckets` buckets?
    std::size_t indexFor(const std::string& key, std::size_t buckets) const;

    // O(n + buckets). Double the buckets and move every node to its NEW index.
    void rehash();

    DynamicArray<Node*> buckets_;
    std::size_t count_ = 0;
};
