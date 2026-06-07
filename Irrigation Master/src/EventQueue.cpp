#include "EventQueue.hpp"

bool EventQueue::push(const Event &event) {
  bool accepted_without_drop = true;

  portENTER_CRITICAL(&_mux);
  if (_count == EVENT_QUEUE_CAPACITY) {
    _head = (_head + 1) % EVENT_QUEUE_CAPACITY;
    --_count;
    accepted_without_drop = false;
  }

  _buf[_tail] = event;
  _tail = (_tail + 1) % EVENT_QUEUE_CAPACITY;
  ++_count;
  portEXIT_CRITICAL(&_mux);

  return accepted_without_drop;
}

bool EventQueue::pop(Event &out) {
  bool has_event = false;

  portENTER_CRITICAL(&_mux);
  if (_count > 0) {
    out = _buf[_head];
    _head = (_head + 1) % EVENT_QUEUE_CAPACITY;
    --_count;
    has_event = true;
  }
  portEXIT_CRITICAL(&_mux);

  return has_event;
}

bool EventQueue::peek(Event &out) const {
  bool has_event = false;

  portENTER_CRITICAL(&_mux);
  if (_count > 0) {
    out = _buf[_head];
    has_event = true;
  }
  portEXIT_CRITICAL(&_mux);

  return has_event;
}

bool EventQueue::empty() const {
  portENTER_CRITICAL(&_mux);
  bool is_empty = (_count == 0);
  portEXIT_CRITICAL(&_mux);
  return is_empty;
}

bool EventQueue::full() const {
  portENTER_CRITICAL(&_mux);
  bool is_full = (_count == EVENT_QUEUE_CAPACITY);
  portEXIT_CRITICAL(&_mux);
  return is_full;
}

size_t EventQueue::size() const {
  portENTER_CRITICAL(&_mux);
  size_t count = _count;
  portEXIT_CRITICAL(&_mux);
  return count;
}

void EventQueue::clear() {
  portENTER_CRITICAL(&_mux);
  _head = 0;
  _tail = 0;
  _count = 0;
  portEXIT_CRITICAL(&_mux);
}
