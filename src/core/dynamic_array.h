#pragma once

#include <cstddef>
#include <stdexcept>

template <typename T> class DynamicArray {
  private:
    T* data_;
    std::size_t size_;
    std::size_t capacity_;

    void grow() {
        std::size_t newCapacity;
        if (capacity_ == 0) {
            newCapacity = 1;
        } else {
            newCapacity = capacity_ * 2;
        }

        T* newData = new T[newCapacity];

        for (std::size_t i = 0; i < size_; ++i) {
            newData[i] = data_[i];
        }

        delete[] data_;
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

    // copy constructor
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

    // copy assignment operator
    DynamicArray& operator=(const DynamicArray& other) {
        if (this == &other) {
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
    void pushBack(const T& value) {
        if (size_ == capacity_) {
            grow();
        }
        data_[size_++] = value;
    }

    void popBack() {
        if (isEmpty()) {
            throw std::out_of_range("Array is empty!");
        } else {
            --size_;
        }
    }

    // operator overloading
    T& operator[](std::size_t index) { return data_[index]; }

    // operator overloading
    const T& operator[](std::size_t index) const { return data_[index]; }

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

    void insertAt(std::size_t index, const T& value) {
        if (index > size_) {
            throw std::out_of_range("Index out of bound!");
        }
        if (size_ == capacity_) {
            grow();
        }

        for (std::size_t i = size_; i > index; i--) {
            data_[i] = data_[i - 1];
        }
        data_[index] = value;
        ++size_;
    }

    void removeAt(std::size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Index out of bound!");
        }

        for (std::size_t i = index; i + 1 < size_; ++i) {
            data_[i] = data_[i + 1];
        }

        --size_;
    }

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

    void clear() { size_ = 0; }

    //============= getters ====================
    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }
    //===========================================

    bool isEmpty() const { return size_ == 0; }
};
