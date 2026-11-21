// HashTable.cpp - separate chaining hash table with djb2 hashing and rehash at > 0.75 load factor.
#include "core/hash_table.h"

#include <stdexcept>

HashTable::HashTable(std::size_t initialBuckets) {
    std::size_t count = (initialBuckets == 0) ? 1 : initialBuckets;
    for (std::size_t i = 0; i < count; ++i) {
        buckets_.pushBack(nullptr);
    }
}

HashTable::~HashTable() {
    clear();
}

void HashTable::clear() {
    for (std::size_t i = 0; i < buckets_.size(); ++i) {
        Node* cur = buckets_[i];
        while (cur != nullptr) {
            Node* next = cur->next;
            delete cur;
            cur = next;
        }
        buckets_[i] = nullptr;
    }
    count_ = 0;
}

std::uint64_t HashTable::djb2(const std::string& s) {
    std::uint64_t h = 5381;
    for (unsigned char c : s) {
        h = h * 33 + c;
    }
    return h;
}

std::size_t HashTable::indexFor(const std::string& key, std::size_t buckets) const {
    return static_cast<std::size_t>(djb2(key) % buckets);
}

void HashTable::insert(const std::string& key, int value) {
    std::size_t idx = indexFor(key, buckets_.size());
    Node* cur = buckets_[idx];
    while (cur != nullptr) {
        if (cur->key == key) {
            cur->value = value;
            return;
        }
        cur = cur->next;
    }

    buckets_[idx] = new Node{key, value, buckets_[idx]};
    ++count_;

    double loadFactor = static_cast<double>(count_) / static_cast<double>(buckets_.size());
    if (loadFactor > 0.75) {
        rehash();
    }
}

bool HashTable::find(const std::string& key, int& out) const {
    std::size_t idx = indexFor(key, buckets_.size());
    Node* cur = buckets_[idx];
    while (cur != nullptr) {
        if (cur->key == key) {
            out = cur->value;
            return true;
        }
        cur = cur->next;
    }
    return false;
}

bool HashTable::contains(const std::string& key) const {
    int ignored = 0;
    return find(key, ignored);
}

bool HashTable::remove(const std::string& key) {
    std::size_t idx = indexFor(key, buckets_.size());
    Node* prev = nullptr;
    Node* cur = buckets_[idx];

    while (cur != nullptr) {
        if (cur->key == key) {
            if (prev == nullptr) {
                buckets_[idx] = cur->next;
            } else {
                prev->next = cur->next;
            }
            delete cur;
            --count_;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }

    return false;
}

void HashTable::rehash() {
    std::size_t newCount = buckets_.size() * 2;
    DynamicArray<Node*> fresh;
    for (std::size_t i = 0; i < newCount; ++i) {
        fresh.pushBack(nullptr);
    }

    for (std::size_t i = 0; i < buckets_.size(); ++i) {
        Node* cur = buckets_[i];
        while (cur != nullptr) {
            Node* next = cur->next;
            std::size_t idx = indexFor(cur->key, newCount);
            cur->next = fresh[idx];
            fresh[idx] = cur;
            cur = next;
        }
        buckets_[i] = nullptr;
    }

    buckets_ = fresh;
}
