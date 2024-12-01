#pragma once
#include <vector>

template <typename T>
class ArrayAverager {
    std::vector<T> sum;
    size_t pos;
    size_t sampleSize;
    size_t arraySize;

public:
    ArrayAverager(size_t sampleSize, size_t arraySize);
    ArrayAverager(const ArrayAverager& other);
    ArrayAverager(ArrayAverager&& other) noexcept;
    ArrayAverager& operator=(const ArrayAverager& other);
    ArrayAverager& operator=(ArrayAverager&& other) noexcept;
    void add(const T* array);
    void getAverage(T* average);
    size_t getArraySize() const { return arraySize; }
};