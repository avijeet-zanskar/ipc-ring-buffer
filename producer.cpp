#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "ring_buffer.hpp"

uint64_t next_data() {
    static uint64_t data = 0;
    return data++;
}

int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(14, &cpuset);
    int cpu_set_err = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (cpu_set_err != 0) {
        std::cerr << "Failed to set CPU affinity\n";
        return EXIT_FAILURE;
    }
    rb_producer<uint64_t> rb;
    while (true) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
        rb.push(next_data());
        //std::cout << count << '\n';
    }
    std::cout << "Producer exit\n";
}