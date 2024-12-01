#pragma once
#include <vector>

template <typename T>
class ArrayAverager {
    size_t sampleSize;
    size_t arraySize;
    size_t pos;
    std::vector<T> sum;
public:
    ArrayAverager(size_t sampleSize, size_t arraySize);
    ArrayAverager(const ArrayAverager& other);
    ArrayAverager(ArrayAverager&& other) noexcept;
    ArrayAverager& operator=(const ArrayAverager& other);
    ArrayAverager& operator=(ArrayAverager&& other) noexcept;
    void add(const T* array);
    template<typename T2>
    void getAverage(T* average);
    size_t getArraySize() const { return arraySize; }
};