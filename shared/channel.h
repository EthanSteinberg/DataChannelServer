#pragma once

#include <functional>

typedef std::function<void(const std::string& message)> MessageHandler;
typedef std::function<void()> CloseHandler;

class DataChannel {
 public:
    DataChannel(MessageHandler message_handler, CloseHandler close_handler) : send_message_(message_handler), close_channel_(close_handler) {}

    DataChannel(const DataChannel& other) = delete;
    DataChannel& operator=(const DataChannel&) = delete;

    ~DataChannel() {
        close_channel_();
    }

    void SendMessage(const std::string& message) {
        send_message_(message);
    }

    void SetOnMessageHandler(MessageHandler handler) {
        on_message_ = handler;
    }

    void SetOnCloseHandler(CloseHandler handler) {
        on_close_ = handler;
    }

    const MessageHandler& GetOnMessageHandler() {
        return on_message_;
    }

    const CloseHandler& GetOnCloseHandler() {
        return on_close_;
    }

  private:
    MessageHandler send_message_;
    CloseHandler close_channel_;

    MessageHandler on_message_;
    CloseHandler on_close_;
};