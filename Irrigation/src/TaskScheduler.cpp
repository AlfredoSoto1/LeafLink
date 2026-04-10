#include "TaskScheduler.hpp"

void TaskScheduler::schedule(TaskFunc task) {
  if (count >= MAX_TASKS) {
    return;
  }
  queue[tail] = task;
  tail = (tail + 1) % MAX_TASKS;
  ++count;
}

void TaskScheduler::execute() {
  if (count == 0) {
    return;
  }
  TaskFunc task = queue[head];
  head = (head + 1) % MAX_TASKS;
  --count;
  task();
}

void TaskScheduler::clear() {
  head = 0;
  tail = 0;
  count = 0;
}

bool TaskScheduler::empty() const {
  return count == 0;
}
