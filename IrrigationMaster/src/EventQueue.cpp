#include "EventQueue.hpp"

// Returns false if the queue is full (event is dropped).
bool EventQueue::push(const Event &evt) {
  if (full()) {
    return false;
  }
  _buf[_tail] = evt;
  _tail = (_tail + 1) % EVENT_QUEUE_CAPACITY;
  ++_count;
  return true;
}

// Pops into `out`. Returns false if empty.
bool EventQueue::pop(Event &out) {
  if (empty()) {
    return false;
  }
  out   = _buf[_head];
  _head = (_head + 1) % EVENT_QUEUE_CAPACITY;
  --_count;
  return true;
}

// Peek without consuming.
bool EventQueue::peek(Event &out) const {
  if (empty()) {
    return false;
  }
  out = _buf[_head];
  return true;
}

bool   EventQueue::empty() const { return _count == 0; }
bool   EventQueue::full()  const { return _count == EVENT_QUEUE_CAPACITY; }
size_t EventQueue::size()  const { return _count; }
void   EventQueue::clear()       { _head = _tail = _count = 0; }