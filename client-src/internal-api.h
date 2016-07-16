#pragma once

extern "C" {
struct PeerConnection;

typedef void (*ConnectCallback)(PeerConnection *peer, void *data);

typedef void (*MessageCallback)(const char *message, int message_length,
                                void *data);

typedef void (*CloseCallback)(void *data);

extern void CreatePeerConnection(const char *server, int port,
                                 ConnectCallback callback, void *user_data,
                                 MessageCallback error_callback,
                                 void *error_data);

void DeletePeerConnection(PeerConnection *peer);

void SetOnMessageCallback(PeerConnection *peer, MessageCallback callback,
                          void *user_data);
void SetOnCloseCallback(PeerConnection *peer, CloseCallback callback,
                        void *user_data);

void SendPeerConnectionMessage(PeerConnection *peer, const char *message,
                               int message_length);
}