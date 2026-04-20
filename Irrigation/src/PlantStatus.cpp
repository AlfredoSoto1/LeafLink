#include "PlantStatus.hpp"

#include <cstring>

PlantStatus::PlantStatus() {
  clear();
}

void PlantStatus::clear() {
  m_kind = PlantStatusKind::Status;
  m_dirty = false;
}

bool PlantStatus::is_dirty() const {
  return m_dirty;
}

const PlantStatusKind PlantStatus::kind() const {
  return m_kind;
}

const PlantStatus::Payload &PlantStatus::payload() const {
  return m_payload;
}

const PlantStatus::StatusData &PlantStatus::status() const {
  return m_payload.status;
}

const PlantStatus::MessageData &PlantStatus::message() const {
  return m_payload.message;
}

void PlantStatus::fill_message(MessageData &record, const char *message,
                               size_t length) {
  std::memset(&record, 0, sizeof(record));

  if (message == nullptr || length == 0) {
    return;
  }

  const size_t copy_length = length < (MAX_MESSAGE_LENGTH - 1)
                               ? length
                               : (MAX_MESSAGE_LENGTH - 1);
  std::memcpy(record.text, message, copy_length);
  record.text[copy_length] = '\0';
  record.length = static_cast<uint8_t>(copy_length);
  record.truncated = length >= MAX_MESSAGE_LENGTH;
}

void PlantStatus::write_message(ErrorType error, const char *message) {
  size_t length = 0;

  // Calculate the length of the message up to the null terminator
  if (message != nullptr) {
    while (message[length] != '\0') {
      ++length;
    }
  }

  fill_message(m_payload.message, message, length);
  m_payload.message.error = error;
  m_kind = PlantStatusKind::Message;
  m_dirty = true;
}

PlantStatus::StatusData& PlantStatus::write_status() {
  m_kind = PlantStatusKind::Status;
  m_dirty = true;
  return m_payload.status;
}
