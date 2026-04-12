#pragma once

#include <cstddef>
#include "AppContext.hpp"

class TaskScheduler {
public:
  static constexpr size_t MAX_TASKS = 10;

  using TaskFunc = void(*)(AppContext &);

public:
  TaskScheduler() = default;
  ~TaskScheduler() = default;

  TaskFunc pop();
  bool schedule(TaskFunc task);

  void clear();
  bool empty() const;

private:
  TaskFunc queue[MAX_TASKS];
  uint head = 0;
  uint tail = 0;
  uint count = 0;
};