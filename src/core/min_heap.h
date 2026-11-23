// this file defines MinHeap (binary heap) stored inside my DynamicArray.
// the element that should come out first is always at index 0.
// used by Dijkstra (closest node first) and ReliefManager (most urgent request first)

#pragma once // this prevents multiple inclusions of the header file.

#include <cstddef>
#include <stdexcept>

#include "core/dynamic_array.h"

// default comparator -> smaller value comes first (normal min heap)
template <typename T>
struct Less {
    bool operator()(const T& a, const T& b) const { return a < b; }
};

// Compare decides who comes first. it answers: should a come out before b?
// we pass our own Compare for requests / dijkstra items
template <typename T, typename Compare = Less<T>>
class MinHeap {
public:
    MinHeap() = default;
    //explicit = stops the compiler from using this constructor for automatic conversions
    explicit MinHeap(Compare cmp) : cmp_(cmp) {}

    // add at the end then move it up. O(log n)
    void push(const T& value) {
        data_.pushBack(value);
        siftUp(data_.size() - 1);
    }

    // removes and returns the top. O(log n)
    // last element goes to the top and then moves down
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

    // look at the top without removing it. O(1)
    const T& top() const {
        if (data_.isEmpty()) {
            throw std::out_of_range("MinHeap::top on empty heap");
        }
        return data_[0];
    }

    std::size_t size() const { return data_.size(); }
    // read only access to slot i (cli "queue" command uses it to print the heap)
    const T& at(std::size_t i) const { return data_.at(i); }
    bool isEmpty() const { return data_.isEmpty(); }
    void clear() { data_.clear(); }

private:
    // index formulas -> parent = (i-1)/2, left = 2i+1, right = 2i+2

    // moves element i up while it is better than its parent
    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t parent = (i - 1) / 2;
            if (!cmp_(data_[i], data_[parent])) {
                break; // parent is already better so stop
            }
            swapAt(i, parent);
            i = parent;
        }
    }

    // moves element i down while a child is better.
    // always swap with the BETTER child, otherwise the heap order breaks
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
                break; // already in right place
            }
            swapAt(i, best);
            i = best;
        }
    }

    // swaps two elements
    void swapAt(std::size_t a, std::size_t b) {
        T tmp = data_[a];
        data_[a] = data_[b];
        data_[b] = tmp;
    }

    DynamicArray<T> data_; // the heap is stored level by level in this array
    Compare cmp_{};
};
