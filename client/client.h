#pragma once

#include <iostream>
#include <memory>
#include <functional>
#include <string>
#include <DataChannelServer/shared/channel.h>
#include <DataChannelServer/client-src/internal-api.h>
#include <emscripten.h>

typedef std::function<void(std::shared_ptr<DataChannel>)> ConnectHandler;
typedef std::function<void(const std::string& error)> ErrorHandler;

inline void Connect(const std::string& server, int port, ConnectHandler handler, ErrorHandler error_handler) {
    CreatePeerConnection(server.c_str(), port, [](PeerConnection* peer, void* data) {
        ConnectHandler* handler = reinterpret_cast<ConnectHandler*>(data);
        auto close_handler = [peer]() {
            std::cout<<"LOL SHOULD HAVE QUIT"<<std::endl;
            DeletePeerConnection(peer);
        };

        auto message_handler = [peer](const std::string& message) {
            SendPeerConnectionMessage(peer, message.data(), message.size());
        };

        std::shared_ptr<DataChannel> channel = std::make_shared<DataChannel>(message_handler, close_handler);

        SetOnMessageCallback(peer, [](const char* message,
                            int message_length,
                            void* data) {
            DataChannel* channel = reinterpret_cast<DataChannel*>(data);
            std::string message_str(message, message_length);
            channel->GetOnMessageHandler()(message_str);
        }, channel.get());

        SetOnCloseCallback(peer, [](void* data) {
            DataChannel* channel = reinterpret_cast<DataChannel*>(data);
            channel->GetOnCloseHandler()();
        }, channel.get());

        (*handler)(channel);
        delete handler;
    }, new ConnectHandler(handler), [](const char* error, int error_length, void *data) {
        ErrorHandler* handler = reinterpret_cast<ErrorHandler*>(data);
        std::string error_str(error, error_length);
        (*handler)(error_str);
    }, new ErrorHandler(error_handler));
    emscripten_exit_with_live_runtime();
}