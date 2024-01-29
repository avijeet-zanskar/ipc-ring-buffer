#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include <signal.h>

#include <cpuid.h>
#include <x86intrin.h>

#include "ring_buffer.hpp"

double get_tsc_period() {
    unsigned int eax_denominator, ebx_numerator, ecx_hz, edx;
    __get_cpuid(0x15, &eax_denominator, &ebx_numerator, &ecx_hz, &edx);
    return (eax_denominator * 1e9) / (static_cast<double>(ecx_hz) * ebx_numerator);
}

bool exit_flag = false;

void handle_interrupt(int) {
    exit_flag = true;
}

struct Data {
    uint64_t val;
    uint8_t data[1280];
    uint64_t val_copy;
    const Data& operator=(const Data& rhs) {
        std::memcpy(this, &rhs, sizeof(Data));
        return *this;
    }
};

Data next_data() {
    static uint64_t val = 0;
    val++;
    return Data{ .val = val, .val_copy = val };
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
    int cyc = 1000 / get_tsc_period();
    auto start = __rdtsc();
    while (true and !exit_flag) {
        if (__rdtsc() - start < cyc) {
            continue;
        }
        start = __rdtsc();
        //std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
        rb.push(next_data());
        //std::cout << count << '\n';
    }
    std::cout << "Producer exit\n";
}