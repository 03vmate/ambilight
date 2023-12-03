#include <iostream>

template <typename T>
class Averager {
    T* sum;
    int pos;
    int sampleSize;
public:
    Averager(int sampleSize) {
        this->sampleSize = sampleSize;
        pos = 0;
        sum = new T[sampleSize];
    }

    ~Averager() {
        delete[] sum;
    }

    void add(T value) {
        sum[pos++] = value;
        if (pos >= sampleSize) {
            pos = 0;
        }
    }

    T getAverage() {
        T average = 0;
        for (int i = 0; i < sampleSize; i++) {
            average += sum[i];
        }
        return average / sampleSize;
    }
};