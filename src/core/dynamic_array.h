// this file defines DynamicArray (my version of std::vector).
// it is used almost everywhere in the project to store things.

#pragma once // this prevents multiple inclusions of the header file.

#include <cstddef>   // for std::size_t
#include <stdexcept> // for std::out_of_range

// template -> same class works for any type (int, string, Edge ...)
// that's why all the code is in the .h file and there is no .cpp
template <typename T> class DynamicArray {
  private:
    T* data_;              // pointer to the array on the heap
    std::size_t size_;     // no. of elements stored right now
    std::size_t capacity_; // how many elements fit before we need to grow

    // makes the array bigger when it is full.
    // capacity doubles (0 -> 1 -> 2 -> 4 ...) so pushBack is O(1) amortized
    void grow() {
        std::size_t newCapacity;
        if (capacity_ == 0) {
            newCapacity = 1;
        } else {
            newCapacity = capacity_ * 2;
        }

        T* newData = new T[newCapacity]; // new bigger array

        //copy old elements into the new array
        for (std::size_t i = 0; i < size_; ++i) {
            newData[i] = data_[i];
        }

        delete[] data_; // free the old array (no memory leak)
        data_ = newData;
        capacity_ = newCapacity;
    }

  public:
    // constructor
    DynamicArray() {
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    // destructor
    ~DynamicArray() { delete[] data_; }

    // copy constructor (deep copy)
    DynamicArray(const DynamicArray& other) {
        size_ = other.size_;
        capacity_ = other.capacity_;

        if (capacity_ > 0) {
            data_ = new T[capacity_];
        } else {
            data_ = nullptr;
        }

        for (std::size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    // copy assignment operator (a = b)
    DynamicArray& operator=(const DynamicArray& other) {
        if (this == &other) { // self assignment check
            return *this;
        }

        T* temp = nullptr;
        if (other.capacity_ > 0) {
            temp = new T[other.capacity_];
        }
        for (std::size_t i = 0; i < other.size_; ++i) {
            temp[i] = other.data_[i];
        }

        delete[] data_;

        data_ = temp;
        size_ = other.size_;
        capacity_ = other.capacity_;

        return *this;
    }

    // adds value at the end, grows first if full
    void pushBack(const T& value) {
        if (size_ == capacity_) {
            grow();
        }
        data_[size_++] = value;
    }

    // removes the last element (only size changes, memory stays)
    void popBack() {
        if (isEmpty()) {
            throw std::out_of_range("Array is empty!");
        } else {
            --size_;
        }
    }

    // operator overloading -> so we can write arr[i] (no bounds check)
    T& operator[](std::size_t index) { return data_[index]; }

    // const version for read only arrays
    const T& operator[](std::size_t index) const { return data_[index]; }

    // same as [] but throws if index is out of range
    T& at(std::size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Index out of bound!");
        }

        return data_[index];
    }

    const T& at(std::size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Index out of bound!");
        }

        return data_[index];
    }

    // inserts value at index and shifts the rest to the right. O(n)
    void insertAt(std::size_t index, const T& value) {
        if (index > size_) {
            throw std::out_of_range("Index out of bound!");
        }
        if (size_ == capacity_) {
            grow();
        }

        // start from the end so nothing gets overwritten
        for (std::size_t i = size_; i > index; i--) {
            data_[i] = data_[i - 1];
        }
        data_[index] = value;
        ++size_;
    }

    // removes element at index and shifts the rest to the left. O(n)
    void removeAt(std::size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Index out of bound!");
        }

        for (std::size_t i = index; i + 1 < size_; ++i) {
            data_[i] = data_[i + 1];
        }

        --size_;
    }

    // returns the last element
    T& back() {
        if (isEmpty()) {
            throw std::out_of_range("Array is empty!");
        }
        return data_[size_ - 1];
    }

    const T& back() const {
        if (isEmpty()) {
            throw std::out_of_range("Array is empty!");
        }
        return data_[size_ - 1];
    }

    // makes size 0 but keeps the memory for reuse
    void clear() { size_ = 0; }

    //============= getters ====================
    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }
    //===========================================

    bool isEmpty() const { return size_ == 0; }
};
