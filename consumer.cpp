#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "ring_buffer.hpp"

void work() {
    volatile int x;
    for (int i = 0; i < 100000; ++i) {
        x = i;
    }
}

struct Data {
    uint64_t val;
    uint8_t data[10000];
    uint64_t val_copy;
    const Data& operator=(const Data& rhs) {
        std::memcpy(this, &rhs, sizeof(Data));
        val_copy = val;
        return *this;
    }
};

int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(13, &cpuset);
    int cpu_set_err = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (cpu_set_err != 0) {
        std::cerr << "Failed to set CPU affinity\n";
        return EXIT_FAILURE;
    }
    rb_consumer<Data> rb;
    int count = 1000000;
    Data data;
    bool dropped;
    int mis_read = 0;
    int drop_count = 0;
    rb.catchup();
    while (count--) {
        bool new_data = false;
        do {
            new_data = rb.pop(data, dropped);
        } while (new_data == false);
        if (dropped) ++drop_count;
        if (data.val != data.val_copy) ++mis_read;
        //work();
        //std::cout << data.val << '\n';
    }
    std::cout << drop_count << '\n';
    std::cout << mis_read << '\n';
}