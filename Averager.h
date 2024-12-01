#pragma once
#include <memory>

template <typename T>
class Averager {
    int pos;
    int sampleSize;
    std::unique_ptr<T[]> sum;
public:
    explicit Averager(int sampleSize);
    Averager(const Averager& other);
    Averager(Averager&& other) noexcept;
    Averager& operator=(const Averager& other);
    Averager& operator=(Averager&& other) noexcept;
    void add(T value);
    T getAverage();
};
