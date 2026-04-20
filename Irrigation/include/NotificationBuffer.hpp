#pragma once

// ---------------------------------------------------------------------------
// NotificationBuffer
//
// ---------------------------------------------------------------------------
class NotificationBuffer {
public:
  NotificationBuffer();

  void write(const char *message, size_t length);
private:
  
};