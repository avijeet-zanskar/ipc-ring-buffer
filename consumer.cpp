#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
#include <fstream>

#include <x86intrin.h>

#include "ring_buffer.hpp"

void dump_csv(std::vector<uint64_t>& lag) {
    std::ofstream dump("lag.csv");
    dump << "cycles\n";
    for (auto i : lag) {
        dump << i << '\n';
    }
}

struct Data {
    uint64_t data[128];
    Data() = default;
    const Data& operator=(const Data& rhs) {
        std::memcpy(this, &rhs, sizeof(Data));
        return *this;
    }
    Data(const Data& rhs) {
        std::memcpy(this, &rhs, sizeof(Data));
    }
};


int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(15, &cpuset);
    int cpu_set_err = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (cpu_set_err != 0) {
        std::cerr << "Failed to set CPU affinity\n";
        return EXIT_FAILURE;
    }
    rb_consumer<Data> rb;
    uint64_t count = 1000000;
    std::vector<uint64_t> lag;
    lag.reserve(count);
    std::cout.imbue(std::locale(""));
    std::cout << "Count: " << count << '\n';
    Data data;
    bool dropped;
    int mis_read = 0;
    int drop_count = 0;
    rb.catchup();
    auto start = std::chrono::steady_clock::now();
    while (count--) {
        bool new_data = false;
        do {
            new_data = rb.pop(data, dropped);
        } while (new_data == false);
        if (dropped) ++drop_count;
        //if (!std::equal(data.data + 1, data.data + 128, data.data)) {
            //std::cout << data.val_copy - data.val << '\n';
            //++mis_read;
        //}
        lag.push_back(__rdtsc() - data.data[0]);
        //std::cout << data.val << '\n';
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Time elapsed: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << '\n';
    std::cout << "Dropped: " << drop_count << '\n';
    std::cout << "Data corrupted: " << mis_read << '\n';
    dump_csv(lag);
}