#include <stdexcept>
#include <cstdint>
#include "ArrayAverager.h"

template <typename T>
ArrayAverager<T>::ArrayAverager(size_t sampleSize, size_t arraySize) : sampleSize(sampleSize), arraySize(arraySize), pos(0) {
    if (sampleSize <= 0 || arraySize <= 0) {
        throw std::invalid_argument("sampleSize and arraySize must be greater than 0");
    }
    sum.resize(sampleSize * arraySize);
}

template <typename T>
ArrayAverager<T>::ArrayAverager(const ArrayAverager& other)
        : sampleSize(other.sampleSize), arraySize(other.arraySize), pos(other.pos) {
    sum = other.sum;
}

template <typename T>
ArrayAverager<T>::ArrayAverager(ArrayAverager&& other) noexcept
        : sampleSize(other.sampleSize), arraySize(other.arraySize), pos(other.pos), sum(std::move(other.sum)) {
    other.pos = 0;
    other.sampleSize = 0;
    other.arraySize = 0;
}

template <typename T>
ArrayAverager<T>& ArrayAverager<T>::operator=(const ArrayAverager& other) {
    if (this != &other) {
        sampleSize = other.sampleSize;
        arraySize = other.arraySize;
        pos = other.pos;
        sum = other.sum;
    }
    return *this;
}

template <typename T>
ArrayAverager<T>& ArrayAverager<T>::operator=(ArrayAverager&& other) noexcept {
    if (this != &other) {
        sampleSize = other.sampleSize;
        arraySize = other.arraySize;
        pos = other.pos;
        sum = std::move(other.sum);
        other.pos = 0;
        other.sampleSize = 0;
        other.arraySize = 0;
    }
    return *this;
}

template <typename T>
void ArrayAverager<T>::add(const T* array) {
    for (size_t i = 0; i < arraySize; ++i) {
        sum[pos * arraySize + i] = array[i];
    }
    pos++;
    if (pos >= sampleSize) {
        pos = 0;
    }
}

template <typename T>
template <typename T2> // Type of summing array - double would be almost universally appropriate, but it's slow
void ArrayAverager<T>::getAverage(T* average) {
    std::vector<T2> temp(arraySize, T2(0));

    // Sum all the values
    for (size_t i = 0; i < sampleSize; ++i) {
        for (size_t j = 0; j < arraySize; ++j) {
            temp[j] += sum[i * arraySize + j];
        }
    }

    for (size_t i = 0; i < arraySize; ++i) {
        average[i] = static_cast<T>(temp[i] / sampleSize);
    }
}

template class ArrayAverager<uint8_t>;
template void ArrayAverager<uint8_t>::getAverage<uint64_t>(uint8_t*);

