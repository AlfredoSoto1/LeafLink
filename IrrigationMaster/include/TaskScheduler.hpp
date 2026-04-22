#pragma once

#include <cstddef>

struct AppContext;

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
  unsigned int head = 0;
  unsigned int tail = 0;
  unsigned int count = 0;
};