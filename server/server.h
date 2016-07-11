#include <boost/asio.hpp>
#include <iostream>
#include <websocketpp/config/asio_no_tls.hpp>

#include <websocketpp/server.hpp>

#include <iostream>

#include <DataChannelServer/src/internal-api.h>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;

typedef std::function<void(const char*, int)> MessageHandler;

void SendMessageWrapper(const char* message, int message_length, void* data) {
  auto func = reinterpret_cast<MessageHandler*>(data);
  (*func)(message, message_length);
}

MessageCallback WrapFunction(MessageHandler* function) {
  MessageCallback result;
  result.user_data = reinterpret_cast<void*>(function);
  result.function = SendMessageWrapper;

  return result;
}

struct PeerConnectionDeleter {
  explicit PeerConnectionDeleter(ProcessingThread* a_thread)
      : thread(a_thread) {}

  void operator()(PeerConnection* peer) { DeletePeerConnection(thread, peer); }

  ProcessingThread* thread;
};

class Server;
class DataChannel {
 public:
  DataChannel(boost::asio::io_service& a_io_service,
              MessageHandler send_websocket_message,
              ProcessingThread* a_thread)
      : service(a_io_service),
        thread(a_thread),
        send_websocket_message_callback(
            new MessageHandler(send_websocket_message)),
        send_data_channel_message_callback(CreateSendDataChannelMessage()),
        peer(CreatePeerConnection(
                 thread,
                 WrapFunction(send_websocket_message_callback.get()),
                 WrapFunction(send_data_channel_message_callback.get())),
             PeerConnectionDeleter(thread)) {}

  void SendMessage(const char* message, int message_length) {
    OnDataChannelMessage(thread, peer.get(), message, message_length);
  }

  void SetMessageHandler(MessageHandler handler) {
    callback.reset(new MessageHandler(handler));
  }

 private:
  friend Server;
  std::unique_ptr<MessageHandler> CreateSendDataChannelMessage() {
    return std::make_unique<MessageHandler>(
        [this](const char* message, int message_length) {
          std::string data(message, message_length);
          service.dispatch(
              [this, data]() { (*callback)(data.data(), data.size()); });
        });
  }

  boost::asio::io_service& service;
  ProcessingThread* thread;

  std::unique_ptr<std::function<void(const char*, int)>>
      send_websocket_message_callback;
  std::unique_ptr<std::function<void(const char*, int)>>
      send_data_channel_message_callback;

  std::unique_ptr<PeerConnection, PeerConnectionDeleter> peer;

  std::unique_ptr<MessageHandler> callback;
};

class Server {
 public:
  Server(int port) : thread(CreateProcessingThread()) {
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

  std::unique_ptr<ProcessingThread, ProcessingThreadDeleter> thread;

  MessageHandler CreateSendWebsocketMessageCallback(
      websocketpp::connection_hdl hdl) {
    return [this, hdl](const char* message, int message_length) {
      std::string data(message, message_length);
      service_.dispatch([this, hdl, data]() {
        echo_server_.send(hdl, data, websocketpp::frame::opcode::text);
      });
    };
  }

  void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
    std::cout << "on_message called with hdl: " << hdl.lock().get()
              << " and message: " << msg->get_payload() << std::endl;

    // check for a special command to instruct the server to stop listening so
    // it can be cleanly exited.
    if (msg->get_payload() == "stop-listening") {
      echo_server_.stop_listening();
      return;
    }

    auto iter = connection_info_.find(hdl);
    if (iter == connection_info_.end()) {
      printf("I CAN'T BELIEVE THE CONNECTION INFO IS NOT IN THERE >>>>>\n");
    }

    std::shared_ptr<DataChannel> channel = iter->second.lock();

    OnWebsocketMessage(thread.get(), channel->peer.get(),
                       msg->get_payload().data(), msg->get_payload().size());
  }

  void on_open(websocketpp::connection_hdl hdl) {
    std::cout << "on_open called with hdl: " << hdl.lock().get() << std::endl;

    std::shared_ptr<DataChannel> channel = std::make_shared<DataChannel>(
        service_, CreateSendWebsocketMessageCallback(hdl), thread.get());
    connection_handler_(channel);
    connection_info_.emplace(hdl, channel);
  }

  void on_close(websocketpp::connection_hdl hdl) {
    std::cout << "on_close called with hdl: " << hdl.lock().get() << std::endl;

    connection_info_.erase(hdl);
  }

  std::map<websocketpp::connection_hdl,
           std::weak_ptr<DataChannel>,
           std::owner_less<websocketpp::connection_hdl>>
      connection_info_;

  std::function<void(std::shared_ptr<DataChannel>)> connection_handler_;

  boost::asio::io_service service_;
  server echo_server_;
};