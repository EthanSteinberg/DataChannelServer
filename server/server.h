#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <websocketpp/config/asio_no_tls.hpp>

#include <websocketpp/server.hpp>

#include <iostream>

#include <DataChannelServer/server-src/internal-api.h>
#include <DataChannelServer/shared/channel.h>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;


struct PeerConnectionDeleter {
  explicit PeerConnectionDeleter(ProcessingThread* a_thread)
      : thread(a_thread) {}

  void operator()(PeerConnection* peer) { DeletePeerConnection(thread, peer); }

  ProcessingThread* thread;
};

template <typename T>
class Inner {
public:
  Inner(boost::asio::io_service& service, std::unique_ptr<T> impl) : service_(service), impl_(std::move(impl)) {

  }

  template <typename F>
  void Wrap(F functor) {
    service_.dispatch([this, functor]() { functor(impl_.get()); });
  }


private:
  boost::asio::io_service& service_;
  std::unique_ptr<T> impl_;
};

template <typename T>
PeerConnectionObserver WrapObserverImplementation(boost::asio::io_service& service, std::unique_ptr<T> impl) {
  PeerConnectionObserver result;

  result.data = new Inner<T>(service, std::move(impl));

  result.Deleter = [](void *data) {
    Inner<T>* inner = (Inner<T>*)(data);
    inner->Wrap([inner](T*) { delete inner; });
  };

  result.OnOpen = [](void* data) {
    Inner<T>* inner = (Inner<T>*)(data);
    inner->Wrap([](T* impl) { impl->OnOpen(); });
  };

  result.OnClose = [](void* data) {
    Inner<T>* inner = (Inner<T>*)(data);
    inner->Wrap([](T* impl) { impl->OnClose(); });
  };

  result.ProcessWebsocketMessage = [](void* data, const char* message, int message_length) {
    Inner<T>* inner = (Inner<T>*)(data);
    std::string message_str(message, message_length);
    inner->Wrap([message_str](T* impl) { impl->ProcessWebsocketMessage(message_str); });
  };

  result.ProcessDataChannelMessage = [](void* data, const char* message, int message_length) {
    Inner<T>* inner = (Inner<T>*)(data);
    std::string message_str(message, message_length);
    inner->Wrap([message_str](T* impl) { impl->ProcessDataChannelMessage(message_str); });
  };

  return result;
}

struct DataChannelSettings {
  DataChannelSettings() : ordered(true),  maxRetransmits(-1), maxRetransmitTime(-1) {}

  bool ordered; // True to force ordered delivery.
  int maxRetransmits; // How many retransmits before giving up.
  int maxRetransmitTime; // How long before giving up on retransmission.
};

DataChannelOptions ConvertSettings(DataChannelSettings settings) {
  DataChannelOptions options;
  options.ordered = settings.ordered;
  options.maxRetransmits = settings.maxRetransmits;
  options.maxRetransmitTime = settings.maxRetransmitTime;

  return options;
}


struct ServerData {
  ServerData(ProcessingThread* a_thread,
            PeerConnectionObserver observer,
            std::shared_ptr<std::weak_ptr<DataChannel>> a_channel_holder,
            DataChannelOptions options)
      : thread(a_thread),
        peer(CreatePeerConnection(
                 thread,
                  observer,
                  options
                ),
             PeerConnectionDeleter(thread)),
             channel_holder(a_channel_holder) {}

  ProcessingThread* thread;

  std::unique_ptr<PeerConnection, PeerConnectionDeleter> peer;
  std::shared_ptr<std::weak_ptr<DataChannel>> channel_holder;
};

class Server {

 public:
  Server(int port, DataChannelSettings settings = DataChannelSettings() ) : thread(CreateProcessingThread()), settings_(settings) {
    try {
      printf("Actually starting ...\n");
      // Set logging settings
      echo_server_.set_access_channels(websocketpp::log::alevel::all);
      echo_server_.clear_access_channels(
          websocketpp::log::alevel::frame_payload);

      // Initialize Asio
      echo_server_.init_asio(&service_);

      // Register our message handler
      echo_server_.set_message_handler(
          [this](websocketpp::connection_hdl hdl, message_ptr msg) {
            on_message(hdl, msg);
          });
      echo_server_.set_open_handler(
          [this](websocketpp::connection_hdl hdl) { on_open(hdl); });
      echo_server_.set_close_handler(
          [this](websocketpp::connection_hdl hdl) { on_close(hdl); });

      // Listen on port 9002
      echo_server_.listen(port);

      // Start the server accept loop
      echo_server_.start_accept();
    } catch (websocketpp::exception const& e) {
      std::cout << e.what() << std::endl;
    } catch (...) {
      std::cout << "other exception" << std::endl;
    }
  }

  void SetConnectionHandler(
      std::function<void(std::shared_ptr<DataChannel>)> handler) {
    connection_handler_ = handler;
  }

  void Start() {
    // Start the ASIO io_service run loop
    service_.run();
  }

 private:
  struct ProcessingThreadDeleter {
    void operator()(ProcessingThread* peer) { DeleteProcessingThread(peer); }
  };

  void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
    ServerData& channel = get_data(hdl);

    SendWebsocketMessage(thread.get(), channel.peer.get(),
                       msg->get_payload().data(), msg->get_payload().size());
  }

  ServerData& get_data(websocketpp::connection_hdl hdl) {
    auto iter = connection_info_.find(hdl);
    if (iter == connection_info_.end()) {
      std::cout << "Internal Error: Unable to find connection " << hdl.lock();
      abort();
    }

    return iter->second;
  }

  void on_open(websocketpp::connection_hdl hdl) {
    class PeerConnectionObserverImplementation {
      public:
        PeerConnectionObserverImplementation(Server *server, websocketpp::connection_hdl hdl, std::shared_ptr<std::weak_ptr<DataChannel>> channel_holder):
        server_(server), hdl_(hdl), channel_holder_(channel_holder) {}

        void OnOpen() {
            auto send_message_callback = [this](const std::string& message) {
              ServerData& channel = server_->get_data(hdl_);
              SendDataChannelMessage(server_->thread.get(), channel.peer.get(), message.data(), message.size());
            };

            auto close_channel_callback = [this]() {
              std::error_code error_code;
              server_->echo_server_.close(hdl_, websocketpp::close::status::normal, "server requested to quit", error_code);
              if (error_code) {
                std::cout << "Had error while closing " << error_code.message() << std::endl;
              }
              server_->connection_info_.erase(hdl_);
            };

            auto channel = std::make_shared<DataChannel>(send_message_callback, close_channel_callback);
            *channel_holder_ = channel;
            server_->connection_handler_(channel);
        }

        void OnClose() {
            auto actual_channel = channel_holder_->lock();
            if (actual_channel != nullptr) {
              actual_channel->GetOnCloseHandler()();
            }
        }

        void ProcessWebsocketMessage(const std::string& message) {
            std::error_code error_code;
            server_->echo_server_.send(hdl_, message, websocketpp::frame::opcode::text, error_code);
            if (error_code) {
                std::cout << "Had error while sending " << error_code.message() << std::endl;
              }
        }

        void ProcessDataChannelMessage(const std::string& message) {
            auto actual_channel = channel_holder_->lock();
            if (actual_channel != nullptr) {
              actual_channel->GetOnMessageHandler()(message);
            }
        }
      private:
        Server* server_;
        websocketpp::connection_hdl hdl_;
        std::shared_ptr<std::weak_ptr<DataChannel>> channel_holder_;
    };

    auto channel_holder = std::make_shared<std::weak_ptr<DataChannel>>();

    ServerData data(
      thread.get(),
      WrapObserverImplementation(service_, std::make_unique<PeerConnectionObserverImplementation>(this, hdl, channel_holder)),
      channel_holder,
      ConvertSettings(settings_)
    );

    connection_info_.emplace(hdl, std::move(data));
  }

  void on_close(websocketpp::connection_hdl hdl) {
    std::cout << "on_close called with hdl: " << hdl.lock().get() << std::endl;
    auto iter = connection_info_.find(hdl);
    if (iter == connection_info_.end()) {
      std::cout<<"Already closed it" << std::endl;
      return;
    }
    ServerData& channel = iter->second;
    auto actual_channel = channel.channel_holder->lock();
    if (actual_channel != nullptr) {
      actual_channel->GetOnCloseHandler()();
    } else {
      connection_info_.erase(hdl);
    }
  }

  std::unique_ptr<ProcessingThread, ProcessingThreadDeleter> thread;

  DataChannelSettings settings_;

  std::map<websocketpp::connection_hdl,
           ServerData,
           std::owner_less<websocketpp::connection_hdl>>
      connection_info_;

  std::function<void(std::shared_ptr<DataChannel>)> connection_handler_;

  boost::asio::io_service service_;
  server echo_server_;
};