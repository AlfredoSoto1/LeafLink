#include "TaskScheduler.hpp"

bool TaskScheduler::schedule(TaskFunc task) {
  if (count >= MAX_TASKS) {
    return false;
  }
  queue[tail] = task;
  tail = (tail + 1) % MAX_TASKS;
  ++count;
  return true;
}

TaskScheduler::TaskFunc TaskScheduler::pop() {
  if (count == 0) {
    return nullptr;
  }
  TaskFunc task = queue[head];
  head = (head + 1) % MAX_TASKS;
  --count;
  return task;
}

void TaskScheduler::clear() {
  head = 0;
  tail = 0;
  count = 0;
}

bool TaskScheduler::empty() const {
  return count == 0;
}
