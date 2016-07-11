#include <boost/asio.hpp>
#include <iostream>
#include <websocketpp/config/asio_no_tls.hpp>

#include <websocketpp/server.hpp>

#include <iostream>

#include <simplepeerlibrary/impl/foo.h>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;

void SendMessageWrapper(const char* message, int message_length, void* data) {
  auto func = reinterpret_cast<std::function<void(const char*, int)>*>(data);
  (*func)(message, message_length);
}

class DataChannel {
public:
  void SendMessage(const char* message, int message_length) {
    OnDataChannelMessage(peer, message, )
  }

  void SetMessageHandler(std::function<void(const char* message, int message_length)> handler) {
    callback = handler;
  }
private:
  std::unique_ptr<std::function<void(const char* message, int message_length)>> callback;
  PeerConnection* peer;
};

class Server {
 public:
  Server(int port) : thread(create_processing_thread()) {
    try {
      printf("Actually starting ...\n");
      // Set logging settings
      echo_server.set_access_channels(websocketpp::log::alevel::all);
      echo_server.clear_access_channels(
          websocketpp::log::alevel::frame_payload);

      // Initialize Asio
      echo_server.init_asio(&service);

      // Register our message handler
      echo_server.set_message_handler(
          [this](websocketpp::connection_hdl hdl, message_ptr msg) {
            on_message(hdl, msg);
          });
      echo_server.set_open_handler(
          [this](websocketpp::connection_hdl hdl) { on_open(hdl); });
      echo_server.set_close_handler(
          [this](websocketpp::connection_hdl hdl) { on_close(hdl); });

      // Listen on port 9002
      echo_server.listen(port);

      // Start the server accept loop
      echo_server.start_accept();
    } catch (websocketpp::exception const& e) {
      std::cout << e.what() << std::endl;
    } catch (...) {
      std::cout << "other exception" << std::endl;
    }
  }

  void SetConnectionHandler(std::function<void(std::unique_ptr<DataChannel>)> handler) {

  }

  void Start() {
    // Start the ASIO io_service run loop
    service.run();
  }

 private:
  struct PeerConnectionDeleter {
    explicit PeerConnectionDeleter(ProcessingThread* a_thread)
        : thread(a_thread) {}

    void operator()(PeerConnection* peer) {
      delete_peer_connection(thread, peer);
    }

    ProcessingThread* thread;
  };

  struct ProcessingThreadDeleter {
    void operator()(ProcessingThread* peer) { delete_processing_thread(peer); }
  };

  std::unique_ptr<ProcessingThread, ProcessingThreadDeleter> thread;

  void SendMessage(websocketpp::connection_hdl hdl,
                   const char* message,
                   int message_length) {
    std::string data(message, message_length);
    service.dispatch([this, hdl, data]() {
      echo_server.send(hdl, data, websocketpp::frame::opcode::text);
    });
  }

  std::unique_ptr<std::function<void(const char*, int)>>
  CreateSendMessageCallback(websocketpp::connection_hdl hdl) {
    auto lambda = [this, hdl](const char* message, int message_length) {
      SendMessage(hdl, message, message_length);
    };

    return std::unique_ptr<std::function<void(const char*, int)>>(
        new std::function<void(const char*, int)>(lambda));
  }

  struct ConnectionData {
    ConnectionData(Server& server,
                   websocketpp::connection_hdl hdl,
                   ProcessingThread* thread)
        : send_message_callback(server.CreateSendMessageCallback(hdl)),
          peer(create_peer_connection(thread,
                                      SendMessageWrapper,
                                      send_message_callback.get()),
               PeerConnectionDeleter(thread)) {}
    std::unique_ptr<std::function<void(const char*, int)>>
        send_message_callback;
    std::unique_ptr<PeerConnection, PeerConnectionDeleter> peer;
  };

  void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
    std::cout << "on_message called with hdl: " << hdl.lock().get()
              << " and message: " << msg->get_payload() << std::endl;

    // check for a special command to instruct the server to stop listening so
    // it can be cleanly exited.
    if (msg->get_payload() == "stop-listening") {
      echo_server.stop_listening();
      return;
    }

    auto iter = connection_info.find(hdl);
    if (iter == connection_info.end()) {
      printf("I CAN'T BELIEVE THE CONNECTION INFO IS NOT IN THERE >>>>>\n");
    }

    on_peer_message(thread.get(), iter->second.peer.get(),
                    msg->get_payload().data(), msg->get_payload().size());
  }

  void on_open(websocketpp::connection_hdl hdl) {
    std::cout << "on_open called with hdl: " << hdl.lock().get() << std::endl;

    connection_info.emplace(hdl, ConnectionData(*this, hdl, thread.get()));
  }

  void on_close(websocketpp::connection_hdl hdl) {
    std::cout << "on_close called with hdl: " << hdl.lock().get() << std::endl;

    connection_info.erase(hdl);
  }

  std::map<websocketpp::connection_hdl,
           ConnectionData,
           std::owner_less<websocketpp::connection_hdl>>
      connection_info;

  boost::asio::io_service service;
  server echo_server;
};