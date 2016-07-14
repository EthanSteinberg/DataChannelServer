var LibraryDataChannelClient = {
    $DataChannelClient: {
        connections: {},
        counter: 1,

        connectToServer: function(server, port) {
            return new Promise(function(resolve, reject) {
                window.RTCPeerConnection = window.RTCPeerConnection ||
                    window.mozRTCPeerConnection || window.webkitRTCPeerConnection;
                window.RTCIceCandidate = window.RTCIceCandidate ||
                    window.mozRTCIceCandidate || window.webkitRTCIceCandidate;
                window.RTCSessionDescription = window.RTCSessionDescription ||
                    window.mozRTCSessionDescription || window.webkitRTCSessionDescription;

                var peerConnectionConfig = {
                    'optional': [{
                        'DtlsSrtpKeyAgreement': true
                    }],
                    'iceServers': [{
                        'url': 'stun:stun.l.google.com:19302'
                    }]
                };

                var peerConnection = new RTCPeerConnection(peerConnectionConfig);
                var socket = new WebSocket('ws://' + server + ':' + port);

                socket.onclose = function(close) {
                    reject('Closed with reason "' + close.reason + '" and code ' + close.code);
                }

                peerConnection.ondatachannel =
                    function(event) {
                        console.log('Got channel ', event);
                        resolve(event.channel);
                    }

                peerConnection.onicecandidate =
                    function(event) {
                        console.log("GOT ICE CANDIDATE", event);
                        if (event.candidate != null) {
                            var response = {
                                type: 'icecandidate',
                                ice: event.candidate.toJSON()
                            };
                            socket.send(JSON.stringify(response));
                        }
                    }

                function processOffer(sdi) {
                    var actual_sdi = new RTCSessionDescription(sdi);
                    peerConnection.setRemoteDescription(actual_sdi)
                        .then(function() {
                            console.log('sdi is now set, creating answer');
                            return peerConnection.createAnswer();
                        })
                        .then(function(sdi) {
                            console.log('have answer, setting');
                            peerConnection.setLocalDescription(sdi).then(function() {
                                console.log('sending answer back');

                                var response = {
                                    type: 'answer',
                                    sdi: sdi.toJSON()
                                };
                                socket.send(JSON.stringify(response));
                            });
                        });
                }

                function processIceCandidate(ice) {
                    var actual_ice = new RTCIceCandidate(ice);
                    peerConnection.addIceCandidate(actual_ice).then(function() {
                        console.log('ice added');
                    });
                }

                socket.onmessage = function(event) {
                    console.log('got', event);
                    var message = JSON.parse(event.data);
                    console.log('GOT', message);
                    if (message.type === 'offer') {
                        processOffer(message.sdi);
                    } else if (message.type === 'icecandidate') {
                        processIceCandidate(message.ice);
                    } else {
                        console.log('INVALID TYPE ', message.type);
                    }
                }
            });
        }
    },

    CreatePeerConnection: function(server, port, callback, user_data, error_callback, error_data) {
        server = Pointer_stringify(server);
        DataChannelClient.connectToServer(server, port).then(function(channel) {
            var id = DataChannelClient.counter++;
            DataChannelClient.connections[id] = channel;
            channel.binaryType = 'arraybuffer';
            Runtime.dynCall('vii', callback, [id, user_data]);
        }, function(error) {
            console.log('got error', error);

            var ptr = Module._malloc(error.length);
            writeStringToMemory(error, ptr, true);
            Runtime.dynCall('viii', error_callback, [ptr, error.length, error_data]);
            Module._free(ptr);
        });
    },

    DeletePeerConnection: function(peer_handle) {
        var channel = DataChannelClient.connections[peer_handle];
        console.log("CLOSE ME");
        channel.onmessage = null;
        channel.onclose = null;
        channel.close();
        DataChannelClient.connections[peer_handle] = null;
    },

    SetOnMessageCallback: function(peer_handle, callback, user_data) {
        var channel = DataChannelClient.connections[peer_handle];
        channel.onmessage = function(event) {
            console.log('Got message', event);
            var ptr = Module._malloc(event.data.length);

            writeStringToMemory(event.data, ptr, true);

            // I thought it was supposed to be an arraybuffer? WTF?
            // Module.HEAPU8.set(event.data, ptr);

            Runtime.dynCall('viii', callback, [ptr, event.data.length, user_data]);
            Module._free(ptr);
        }
    },

    SetOnCloseCallback: function(peer_handle, callback, user_data) {
        var channel = DataChannelClient.connections[peer_handle];
        channel.onclose = function(event) {
            console.log('Got close', event);
            Runtime.dynCall('vi', callback, [user_data]);
        }
    },

    SendPeerConnectionMessage: function(peer_handle, message, message_length) {
        var channel = DataChannelClient.connections[peer_handle];
        channel.send(Module.HEAPU8.slice(message, message + message_length));
    },
};

autoAddDeps(LibraryDataChannelClient, '$DataChannelClient');
mergeInto(LibraryManager.library, LibraryDataChannelClient);