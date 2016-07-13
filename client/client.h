#pragma once

#include <memory>
#include <functional>
#include <string>
#include <DataChannelServer/shared/channel.h>
#include <DataChannelServer/client-src/internal-api.h>
#include <emscripten.h>

typedef std::function<void(std::shared_ptr<DataChannel>)> ConnectHandler;

struct PeerConnectionDeleter {
  void operator()(PeerConnection* peer) { DeletePeerConnection(peer); }
};

class ClientDataChannel : public DataChannel {
public:
    ClientDataChannel(PeerConnection* a_peer): peer(a_peer) {
        SetOnMessageCallback(peer.get(), [](const char* message,
                            int message_length,
                            void* data) {
            ClientDataChannel* self = reinterpret_cast<ClientDataChannel*>(data);
            self->callback(message, message_length);
        }, this);
    }

    void SendMessage(const char* message, int message_length) override {
        SendPeerConnectionMessage(peer.get(), message, message_length);
    }

    void SetMessageHandler(MessageHandler handler) override {
        callback = handler;
    }
private:
    std::unique_ptr<PeerConnection, PeerConnectionDeleter> peer;
    MessageHandler callback;
};


inline void Connect(const std::string& server, int port, ConnectHandler handler) {
    CreatePeerConnection(server.c_str(), port, [](PeerConnection* peer, void* data) {
        ConnectHandler* handler = reinterpret_cast<ConnectHandler*>(data);
        std::shared_ptr<ClientDataChannel> channel = std::make_shared<ClientDataChannel>(peer);
        (*handler)(channel);
        delete handler;
    }, new ConnectHandler(handler));
    emscripten_exit_with_live_runtime();
}