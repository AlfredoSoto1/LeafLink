#pragma once

#include <cstddef>
#include "AppContext.hpp"

// ----------------------------------------------------------------------
// TaskScheduler — a simple fixed-size queue for scheduling 
// tasks to run in the main loop
// ----------------------------------------------------------------------
class TaskScheduler {
public:
  using TaskFunc = void(*)(AppContext &);

public:
  TaskScheduler() = default;
  
  TaskFunc pop();
  bool schedule(TaskFunc task);
  
  void clear();
  bool empty() const;
  
private:
  static constexpr size_t MAX_TASKS = 10;
  TaskFunc queue[MAX_TASKS];
  uint head = 0;
  uint tail = 0;
  uint count = 0;
};