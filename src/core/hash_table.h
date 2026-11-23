// this file declares HashTable (string -> int) using separate chaining.
// used for: city name -> node id, request id -> index, map tile "z/x/y" -> tile index

#pragma once // this prevents multiple inclusions of the header file.

#include <cstddef>
#include <cstdint> // for std::uint64_t
#include <string>

#include "core/dynamic_array.h"

class HashTable {
public:
    // starts with 16 empty buckets by default
    explicit HashTable(std::size_t initialBuckets = 16);

    ~HashTable(); // deletes all nodes

    // = delete -> copying is not allowed (we never need a copy of a table)
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    // adds key, if key already exists then its value is updated. O(1) average
    void insert(const std::string& key, int value);

    // returns true and puts the value in out if key is found
    bool find(const std::string& key, int& out) const;

    bool contains(const std::string& key) const;

    // returns true if the key was removed
    bool remove(const std::string& key);

    std::size_t size() const { return count_; }
    std::size_t bucketCount() const { return buckets_.size(); }

    void clear(); // removes all keys but keeps buckets

    // static -> belongs to the class, can be called without an object
    static std::uint64_t djb2(const std::string& s);

private:
    // one node of the linked list inside a bucket
    struct Node {
        std::string key;
        int value;
        Node* next;
    };

    // which bucket the key goes in -> hash % no. of buckets
    std::size_t indexFor(const std::string& key, std::size_t buckets) const;

    void rehash(); // doubles the buckets when load factor > 0.75

    DynamicArray<Node*> buckets_; // each bucket = head of a linked list
    std::size_t count_ = 0;       // no. of keys stored
};
