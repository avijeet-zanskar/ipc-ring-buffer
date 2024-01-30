#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include <signal.h>

#include <cpuid.h>
#include <x86intrin.h>

#include "ring_buffer.hpp"

double get_tsc_freq() {
    unsigned int eax_denominator, ebx_numerator, ecx_hz, edx;
    __get_cpuid(0x15, &eax_denominator, &ebx_numerator, &ecx_hz, &edx);
    return (static_cast<double>(ecx_hz) * ebx_numerator) / (1e9 * static_cast<double>(eax_denominator));
}

bool exit_flag = false;

void handle_interrupt(int) {
    exit_flag = true;
}

struct Data {
    uint64_t data[128];
    const Data& operator=(const Data& rhs) {
        std::memcpy(this, &rhs, sizeof(Data));
        return *this;
    }
};

Data next_data() {
    static uint64_t val = 0;
    val++;
    Data data;
    data.data[0] = __rdtsc();
    return data;
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
    rb_producer<Data> rb;
    signal(SIGINT, &handle_interrupt);
    int cyc = 1000 * get_tsc_freq();
    uint64_t data_count = 0;
    auto start = __rdtsc();
    auto start_time = std::chrono::steady_clock::now();
    while (!exit_flag) {
        if (auto end = __rdtsc(); end - start < cyc) {
            continue;
        } else {
            start = end;
            rb.push(next_data());
            ++data_count;
        }
    }
    auto end_time = std::chrono::steady_clock::now();
    std::cout << "Producer exit\n";
    std::cout.imbue(std::locale(""));
    std::cout << "Time elapsed: " << std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count() << '\n';
    std::cout << "Packets sent: " << data_count << '\n';
    std::cout << "Time per packet: " << (end_time - start_time).count() / data_count << '\n';
}