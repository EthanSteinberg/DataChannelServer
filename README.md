DataChannelServer
============
<!--- {#mainpage} -->

DataChannelServer is a C++ library for creating WebRTC DataChannel servers.

It consists of two parts: a native C++ server API, and an emscripten C++ client API.

See client/client.h and server/server.h for the corresponding APIs and documentation.
The corresponding doxygen documentation can be located at https://lalaland.github.io/DataChannelServer.

### Server example

```cpp
int main() {
  std::map<int, std::shared_ptr<data_channel::DataChannel>> channels;
  int next_id = 0;

  data_channel::Server server(9014);
  server.SetConnectionHandler([&channels](std::shared_ptr<data_channel::DataChannel> channel) {
    int id = next_id++;

    channels[id] = channel;

    channel->SendMessage("foo");
    channel->SetOnMessageHandler([&clients](const std::string& message_str) {
      std::cout << "Got message " << message_str << std::endl;
    });
    channel->SetOnCloseHandler([&clients,id](){
      clients.erase(id);
    });
  });

  server.Start();
}
```

### Client example

```cpp
std::shared_ptr<data_channel::DataChannel> saved;

int main() {
  data_channel::Connect(std::string("localhost"), 9014, [](std::shared_ptr<data_channel::DataChannel> channel) {
    std::cout << "Connected " << std::endl;
    channel->SetOnMessageHandler([](const std::string& message_str) {
      std::cout << "Got message " << message_str << std::endl;
    });
    channel->SetOnCloseHandler([](){
      std::cout << "Closed" << std::endl;
    });

    // Have to keep alive
    saved = channel;
  }, [](const std::string& error) {
    std::cout << "Could not connect " << error << std::endl;
  });
}
```

### Installation and setup

TODO