DataChannelServer
============

WARNING: This library is still a work in progress. Feel free to use the code, but the api might change at any time and there are lots of bugs.


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

# Server

In order to use server/server.h, you must have [websocketpp](https://github.com/zaphoyd/websocketpp) and the asio library of  [boost](http://www.boost.org/) installed and linked in your application. See the corresponding library sites for more details.

You must also link to libDataChannelServer.so, which is the C API binding layer between DataChannelServer and webrtc.
I provide a linux binary in the dist folder that was build on my Debain machine. It might or might not work for you.

In order to build libDataChannelServer.so, you must first setup the webrtc native libraries from https://webrtc.org/native-code/development. You then must copy the DataChannelServer folder into webrtc-checkout/src.

webrtc-checkout/src/all.gyp must be patched to include DataChannelServer.

The following indicates where to insert: 'DataChannelServer/build.gyp:*' inside webrtc-checkout/src/all.gyp.
```
# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'include_examples%': 1,
    'include_tests%': 1,
  },
  'includes': [
    'webrtc/build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
>>>     'DataChannelServer/build.gyp:*',
        'webrtc/api/api.gyp:*',
        'webrtc/base/base.gyp:*',
        'webrtc/common.gyp:*',
        'webrtc/common_audio/common_audio.gyp:*',
...
```

You can then build by running `gclient runhooks` followong by `ninja -C out/Debug -v`.
The resulting library can be found in webrtc-checkout/src/out/Debug/lib/libDataChannelServer.so.

# Client

In order to use the client, you must have emscripten installed.

You must also link to the DataChannelServer/client-src/nternal-api.js using the --js-library option.
For example: `--js-library libs/DataChannelServer/client-src/internal-api.js`

# Example

A fully complete example can be found at https://github.com/Lalaland/SimpleGame
