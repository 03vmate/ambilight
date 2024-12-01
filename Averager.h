#pragma once
#include <memory>

template <typename T>
class Averager {
    std::unique_ptr<T[]> sum;
    int pos;
    int sampleSize;
public:
    explicit Averager(int sampleSize);
    Averager(const Averager& other);
    Averager(Averager&& other) noexcept;
    Averager& operator=(const Averager& other);
    Averager& operator=(Averager&& other) noexcept;
    void add(T value);
    T getAverage();
};
