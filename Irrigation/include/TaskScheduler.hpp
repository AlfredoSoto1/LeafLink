#pragma once

#include <cstddef>
#include <functional>
#include "pico/stdlib.h"

class TaskScheduler {
public:
  static constexpr size_t MAX_TASKS = 10;

  using TaskFunc = std::function<void()>;

public:
  TaskScheduler() = default;
  ~TaskScheduler() = default;

  void execute();
  void schedule(TaskFunc task);

  void clear();
  bool empty() const;

private:
  TaskFunc queue[MAX_TASKS];
  uint head = 0;
  uint tail = 0;
  uint count = 0;
};