#pragma once

#include <functional>

/// \file shared/channel.h
/// The connection API for both the server and client.

namespace data_channel {

/// A class which represents a single peer connection
///
/// When this class is destroyed, the connection is closed.
/// You can also set on message and on close handlers.
/// As well as directly send messages.
class DataChannel {
private:
  typedef std::function<void(const std::string &message)> MessageHandler;
  typedef std::function<void()> CloseHandler;

public:
  /// \cond PRIVATE
  DataChannel(MessageHandler message_handler, CloseHandler close_handler)
      : send_message_(message_handler), close_channel_(close_handler) {}
  /// \endcond

  DataChannel(const DataChannel &other) = delete;
  DataChannel &operator=(const DataChannel &) = delete;

  /// Closes the channel
  ~DataChannel() { close_channel_(); }

  /// Send a message to the other side.
  /// \param message The provided message.
  void SendMessage(const std::string &message) { send_message_(message); }

  /// Set a handler for processing new messages.
  /// \param handler The callback when a new message arrives.
  void SetOnMessageHandler(std::function<void(const std::string &message)> handler) { on_message_ = handler; }

  /// Get a notification when the channel is closed.
  ///
  /// NOTE: You cannot call SendMessage() after receiving a CloseMessage.
  /// You should also free the DataChannel to avoid memory leaks after receiving
  /// a close.
  /// \param handler The callback for when the channel is closed.
  void SetOnCloseHandler(std::function<void()> handler) { on_close_ = handler; }

  /// \cond PRIVATE
  const MessageHandler &GetOnMessageHandler() { return on_message_; }

  const CloseHandler &GetOnCloseHandler() { return on_close_; }
  /// \endcond

private:
  MessageHandler send_message_;
  CloseHandler close_channel_;

  MessageHandler on_message_;
  CloseHandler on_close_;
};

}  // namespace data_channel