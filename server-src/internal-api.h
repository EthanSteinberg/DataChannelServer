#pragma once

#define EXPORT __attribute__((visibility("default")))

extern "C" {
struct PeerConnection;
struct ProcessingThread;

struct PeerConnectionObserver {
  void (*Deleter)(void* data);

  void (*OnOpen)(void* data);
  void (*OnClose)(void* data);

  void (*ProcessWebsocketMessage)(void* data, const char* message, int message_length);
  void (*ProcessDataChannelMessage)(void* data, const char* message, int message_length);

  void* data;
};

struct DataChannelOptions {
  bool ordered;
  int maxRetransmitTime;
  int maxRetransmits;
};

EXPORT PeerConnection* CreatePeerConnection(
    ProcessingThread* thread,
    PeerConnectionObserver observer,
    DataChannelOptions options);

EXPORT void DeletePeerConnection(ProcessingThread* thread,
                                 PeerConnection* peer);

EXPORT void SendWebsocketMessage(ProcessingThread* thread,
                               PeerConnection* peer,
                               const char* message,
                               int message_length);

EXPORT void SendDataChannelMessage(ProcessingThread* thread,
                                 PeerConnection* peer,
                                 const char* message,
                                 int message_length);

EXPORT ProcessingThread* CreateProcessingThread();
EXPORT void DeleteProcessingThread(ProcessingThread* thread);
}