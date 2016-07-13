#pragma once

typedef std::function<void(const char*, int)> MessageHandler;

class DataChannel {
 public:
  virtual void SendMessage(const char* message, int message_length) = 0;

  virtual void SetMessageHandler(MessageHandler handler) = 0;

  virtual ~DataChannel() {}
};