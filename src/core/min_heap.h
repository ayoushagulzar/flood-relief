// MinHeap.h - a binary heap stored in your DynamicArray (no pointers).
//
// "Min" means: the element that should come out FIRST sits at index 0.
// What "first" means is decided by the Compare type:
//   Less<int>          -> smallest number first (normal min-heap)
//   a custom struct    -> e.g. highest priority request first
// Compare must have:  bool operator()(const T& a, const T& b) const
// answering "should a come out before b?".
//
// Index maths for a complete binary tree in an array:
//   parent(i) = (i - 1) / 2      left(i) = 2i + 1      right(i) = 2i + 2
//
// Used later by: Dijkstra (closest node first) and ReliefManager (most
// urgent request first). Templates -> everything stays in this .h file.
#pragma once

#include <cstddef>
#include <stdexcept>

#include "core/dynamic_array.h"

// Default comparator: a comes before b if a < b. (Finished, nothing to do.)
template <typename T>
struct Less {
    bool operator()(const T& a, const T& b) const { return a < b; }
};

template <typename T, typename Compare = Less<T>>
class MinHeap {
public:
    MinHeap() = default;
    explicit MinHeap(Compare cmp) : cmp_(cmp) {}

    // O(log n). Append at the end, then sift up.
    void push(const T& value) {
        data_.pushBack(value);
        siftUp(data_.size() - 1);
    }

    // O(log n). Removes and returns the best element. Throws if empty.
    T pop() {
        if (data_.isEmpty()) {
            throw std::out_of_range("MinHeap::pop on empty heap");
        }
        T topValue = data_[0];
        data_[0] = data_[data_.size() - 1];
        data_.popBack();
        if (!data_.isEmpty()) {
            siftDown(0);
        }
        return topValue;
    }

    // O(1). Best element without removing it. Throws if empty.
    const T& top() const {
        if (data_.isEmpty()) {
            throw std::out_of_range("MinHeap::top on empty heap");
        }
        return data_[0];
    }

    // O(1)
    std::size_t size() const { return data_.size(); }
    // Read-only look at array slot i (0 = top). Used by the CLI `queue`
    // command to print the heap as it is stored. Throws if i is out of range.
    const T& at(std::size_t i) const { return data_.at(i); }
    // O(1)
    bool isEmpty() const { return data_.isEmpty(); }
    // O(1)
    void clear() { data_.clear(); }

private:
    // O(log n). Move element i UP while it should come before its parent.
    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t parent = (i - 1) / 2;
            if (!cmp_(data_[i], data_[parent])) {
                break;
            }
            swapAt(i, parent);
            i = parent;
        }
    }

    // O(log n). Move element i DOWN while a child should come before it.
    void siftDown(std::size_t i) {
        while (true) {
            std::size_t left = 2 * i + 1;
            std::size_t right = 2 * i + 2;
            std::size_t best = i;

            if (left < data_.size() && cmp_(data_[left], data_[best])) {
                best = left;
            }
            if (right < data_.size() && cmp_(data_[right], data_[best])) {
                best = right;
            }
            if (best == i) {
                break;
            }
            swapAt(i, best);
            i = best;
        }
    }

    // O(1). Finished: swaps two slots.
    void swapAt(std::size_t a, std::size_t b) {
        T tmp = data_[a];
        data_[a] = data_[b];
        data_[b] = tmp;
    }

    DynamicArray<T> data_;
    Compare cmp_{};
};
