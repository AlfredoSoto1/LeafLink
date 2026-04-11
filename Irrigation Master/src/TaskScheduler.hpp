#pragma once

#include <cstddef>
#include <cstdint>

// Forward-declare so TaskFunc signature can reference AppContext
struct AppContext;

// ----------------------------------------------------------------------------
// TaskScheduler
//
// Direct port of the Pico TaskScheduler, adapted for the ESP32-S3:
//   - AppContext is the ESP32 version (WiFiAP, WebServer, EventQueue…)
//   - Capacity bumped to 16 (more concurrent tasks on the master side)
//   - schedule() returns bool so callers can detect overflow
// -----------------------------------------------------------------------------
class TaskScheduler {
public:
    static constexpr size_t MAX_TASKS = 16;

    using TaskFunc = void (*)(AppContext &);

    TaskScheduler()  = default;
    ~TaskScheduler() = default;

    // Add a task to the back of the queue.
    // Returns false if the queue is full.
    bool schedule(TaskFunc task) {
        if (_count >= MAX_TASKS) return false;
        _queue[_tail] = task;
        _tail = (_tail + 1) % MAX_TASKS;
        ++_count;
        return true;
    }

    // Remove and return the next task. Returns nullptr when empty.
    TaskFunc pop() {
        if (empty()) return nullptr;
        TaskFunc t = _queue[_head];
        _head = (_head + 1) % MAX_TASKS;
        --_count;
        return t;
    }

    bool   empty() const { return _count == 0; }
    bool   full()  const { return _count >= MAX_TASKS; }
    size_t size()  const { return _count; }
    void   clear()       { _head = _tail = _count = 0; }

private:
    TaskFunc _queue[MAX_TASKS] = {};
    size_t   _head  = 0;
    size_t   _tail  = 0;
    size_t   _count = 0;
};
