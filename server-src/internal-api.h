#pragma once

#define EXPORT __attribute__((visibility("default")))

extern "C" {
struct PeerConnection;
struct ProcessingThread;

typedef void (*SendMessage)(const char* message,
                            int message_length,
                            void* data);

typedef void (*OnOpen)(void* data);

struct MessageCallback {
  void* user_data;
  SendMessage function;
};

EXPORT PeerConnection* CreatePeerConnection(
    ProcessingThread* thread,
    MessageCallback send_websocket_message,
    MessageCallback send_data_channel_message,
    OnOpen on_open,
    void* on_open_data);

EXPORT void DeletePeerConnection(ProcessingThread* thread,
                                 PeerConnection* peer);

EXPORT void OnWebsocketMessage(ProcessingThread* thread,
                               PeerConnection* peer,
                               const char* message,
                               int message_length);

EXPORT void OnDataChannelMessage(ProcessingThread* thread,
                                 PeerConnection* peer,
                                 const char* message,
                                 int message_length);

EXPORT ProcessingThread* CreateProcessingThread();
EXPORT void DeleteProcessingThread(ProcessingThread* thread);
}