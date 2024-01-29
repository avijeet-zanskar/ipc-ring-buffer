#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include <signal.h>

#include "ring_buffer.hpp"
bool exit_flag = false;

void handle_interrupt(int) {
    exit_flag = true;
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

Data next_data() {
    static uint64_t val = 0;
    val++;
    return Data{ val };
}

int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(12, &cpuset);
    int cpu_set_err = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (cpu_set_err != 0) {
        std::cerr << "Failed to set CPU affinity\n";
        return EXIT_FAILURE;
    }
    rb_producer<Data> rb;
    signal(SIGINT, &handle_interrupt);
    while (true and !exit_flag) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
        rb.push(next_data());
        //std::cout << count << '\n';
    }
    std::cout << "Producer exit\n";
}