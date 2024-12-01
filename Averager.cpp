#include <stdexcept>
#include "Averager.h"

template <typename T>
Averager<T>::Averager(int sampleSize) : pos(0), sampleSize(sampleSize) {
    if (sampleSize <= 0) {
        throw std::invalid_argument("sampleSize must be greater than 0");
    }
    sum = std::make_unique<T[]>(sampleSize);
}

template <typename T>
Averager<T>::Averager(const Averager& other)
        : pos(other.pos), sampleSize(other.sampleSize), sum(std::make_unique<T[]>(other.sampleSize)) {
    std::copy(other.sum.get(), other.sum.get() + other.sampleSize, sum.get());
}

template <typename T>
Averager<T>::Averager(Averager&& other) noexcept
        : pos(other.pos), sampleSize(other.sampleSize), sum(std::move(other.sum)) {
    other.pos = 0;
    other.sampleSize = 0;
}

template <typename T>
Averager<T>& Averager<T>::operator=(const Averager& other) {
    if (this != &other) {
        pos = other.pos;
        sampleSize = other.sampleSize;
        sum = std::make_unique<T[]>(other.sampleSize);
        std::copy(other.sum.get(), other.sum.get() + other.sampleSize, sum.get());
    }
    return *this;
}

template <typename T>
Averager<T>& Averager<T>::operator=(Averager&& other) noexcept {
    if (this != &other) {
        pos = other.pos;
        sampleSize = other.sampleSize;
        sum = std::move(other.sum);
        other.pos = 0;
        other.sampleSize = 0;
    }
    return *this;
}

template <typename T>
void Averager<T>::add(T value) {
    sum[pos++] = value;
    if (pos >= sampleSize) {
        pos = 0;
    }
}

template <typename T>
T Averager<T>::getAverage() {
    T average = 0;
    for (int i = 0; i < sampleSize; i++) {
        average += sum[i];
    }
    return average / sampleSize;
}

template class Averager<long>;
