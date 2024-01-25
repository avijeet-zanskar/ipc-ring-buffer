#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "ring_buffer.hpp"

int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(13, &cpuset);
    int cpu_set_err = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (cpu_set_err != 0) {
        std::cerr << "Failed to set CPU affinity\n";
        return EXIT_FAILURE;
    }
    rb_consumer<uint64_t> rb;
    int count = 10000;
    uint64_t data;
    bool dropped;
    int drop_count = 0;
    rb.catchup();
    while (count--) {
        bool new_data = false;
        do {
            new_data = rb.pop(data, dropped);
        } while (new_data == false);
        if (dropped) ++drop_count;
        std::cout << data << '\n';
    }
    std::cout << drop_count << '\n';
}