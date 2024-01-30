#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

template<typename T>
class ring_buffer {
    struct chunk {
        uint64_t sequence_no;
        T data;
    };
    struct info {
        uint64_t current_sequence_no;
        volatile uint64_t head;
    };
    template<typename U> friend class rb_producer;
    template<typename U> friend class rb_consumer;
public:
    ring_buffer() : buffer_start_ptr(nullptr), info_ptr(nullptr) {}
private:
    template<int AccessFlag>
    void init();

    void init_producer();
    void init_consumer();

    void push(const T& data);

    chunk pop();

    void free_buffer();

    chunk* buffer_start_ptr;
    info* info_ptr;
    static constexpr size_t capacity = 128 * 1024;
};

template<typename T>
template<int AccessFlag>
void ring_buffer<T>::init() {
    auto fd_rb = shm_open("rb", O_RDWR | O_CREAT, S_IRWXU);
    if (fd_rb == -1) {
        std::cerr << "Couldn't create fd for ring buffer. shm_open failed with err " << std::strerror(errno) << '\n';
        return;
    }
    auto fd_info = shm_open("rb_info", O_RDWR | O_CREAT, S_IRWXU);
    if (fd_info == -1) {
        std::cerr << "Couldn't create fd for ring buffer info. shm_open failed with err " << std::strerror(errno) << '\n';
        return;
    }
    auto err = ftruncate(fd_rb, capacity * sizeof(chunk));
    if (err == -1) {
        std::cerr << "ftruncate failed with err " << std::strerror(errno) << '\n';
        return;
    }
    err = ftruncate(fd_info, sizeof(info));
    if (err == -1) {
        std::cerr << "ftruncate failed with err " << std::strerror(errno) << '\n';
        return;
    }
    // map address to info struct
    auto res = mmap(nullptr, sizeof(info), AccessFlag, MAP_SHARED, fd_info, 0);
    if (res == (void*)-1) {
        std::cerr << "Couldn't map address to info struct. mmap failed with err " << std::strerror(errno) << '\n';
        return;
    }
    info_ptr = reinterpret_cast<info*>(res);

    // map address to ring buffer
    auto buffer_start = mmap(nullptr, capacity * sizeof(chunk), AccessFlag, MAP_SHARED, fd_rb, 0);
    if (buffer_start == (void*)-1) {
        std::cerr << "Couldn't map address to ring buffer. mmap failed with err " << std::strerror(errno) << '\n';
        return;
    }
    buffer_start_ptr = reinterpret_cast<chunk*>(buffer_start);
}

template<typename T>
void ring_buffer<T>::init_producer() {
    init<PROT_READ | PROT_WRITE>();
    assert(info_ptr != nullptr);
    assert(buffer_start_ptr != nullptr);
    info_ptr->current_sequence_no = 0;
    info_ptr->head = 0;
}

template<typename T>
void ring_buffer<T>::init_consumer() {
    init<PROT_READ>();
    assert(info_ptr != nullptr);
    assert(buffer_start_ptr != nullptr);
}

template<typename T>
void ring_buffer<T>::push(const T& data) {

    auto write_offset = info_ptr->head + 1;
    buffer_start_ptr[write_offset % capacity] = chunk{ info_ptr->current_sequence_no, data };
    info_ptr->head = write_offset;
    ++(info_ptr->current_sequence_no);
}

template<typename T>
ring_buffer<T>::chunk ring_buffer<T>::pop() {
    return buffer_start_ptr[info_ptr->head % capacity];
}

template<typename T>
void ring_buffer<T>::free_buffer() {
    int res = munmap(info_ptr, sizeof(info));
    if (res == -1) {
        std::cerr << "munmap failed with err " << std::strerror(errno) << '\n';
    }
    res = munmap(buffer_start_ptr, capacity * sizeof(T));
    if (res == -1) {
        std::cerr << "munmap failed with err " << std::strerror(errno) << '\n';
    }
    res = shm_unlink("rb_info");
    if (res == -1) {
        std::cerr << "Unlink failed with err " << std::strerror(errno) << '\n';
    }
    res = shm_unlink("rb");
    if (res == -1) {
        std::cerr << "Unlink failed with err " << std::strerror(errno) << '\n';
    }
}

template<typename T>
class rb_producer {
public:
    rb_producer();
    ~rb_producer();

    void push(const T& data);
    size_t capacity();

private:
    ring_buffer<T> rb;
};

template<typename T>
rb_producer<T>::rb_producer() {
    rb.init_producer();
}

template<typename T>
rb_producer<T>::~rb_producer() {
    rb.free_buffer();
}

template<typename T>
void rb_producer<T>::push(const T& data) {
    rb.push(data);
}

template<typename T>
size_t rb_producer<T>::capacity() {
    return rb.capacity;
}

template<typename T>
class rb_consumer {
public:
    rb_consumer();
    bool pop(T& data, bool& dropped);
    size_t capacity();
    void catchup();

private:
    ring_buffer<T> rb;
    uint64_t current_sequence_no;
};

template<typename T>
rb_consumer<T>::rb_consumer() {
    rb.init_consumer();
    catchup();
}

template<typename T>
bool rb_consumer<T>::pop(T& data, bool& dropped) {
    typename ring_buffer<T>::chunk temp = rb.pop();
    dropped = temp.sequence_no > current_sequence_no;
    if (temp.sequence_no < current_sequence_no) {
        return false;
    } else {
        current_sequence_no = temp.sequence_no + 1;
        data = temp.data;
        return true;
    }
}

template<typename T>
size_t rb_consumer<T>::capacity() {
    return rb.capacity;
}

template<typename T>
void rb_consumer<T>::catchup() {
    current_sequence_no = rb.info_ptr->current_sequence_no;
}