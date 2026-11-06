// Host stand-in for pico/util/queue.h. Single-threaded in the browser build (the
// AudioWorklet runs the ML tick and the audio on one thread), so no locking.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

struct queue_t {
    size_t elementSize = 0;
    size_t capacity = 0;
    size_t head = 0, count = 0;
    std::vector<uint8_t> data;
};

inline void queue_init(queue_t* q, unsigned elementSize, unsigned elementCount) {
    q->elementSize = elementSize;
    q->capacity = elementCount ? elementCount : 1;
    q->head = q->count = 0;
    q->data.assign(q->elementSize * q->capacity, 0);
}

inline bool queue_try_add(queue_t* q, const void* item) {
    if (q->count == q->capacity) return false;
    const size_t slot = (q->head + q->count) % q->capacity;
    std::memcpy(&q->data[slot * q->elementSize], item, q->elementSize);
    q->count++;
    return true;
}

inline bool queue_try_remove(queue_t* q, void* item) {
    if (q->count == 0) return false;
    std::memcpy(item, &q->data[q->head * q->elementSize], q->elementSize);
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return true;
}
