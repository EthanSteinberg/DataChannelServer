console.log('I WIN');

var port = 9026;
var server = 'localhost';

function connectToServer(server, port) {
  return new Promise(function(resolve, reject) {
    window.RTCPeerConnection = window.RTCPeerConnection ||
        window.mozRTCPeerConnection || window.webkitRTCPeerConnection;
    window.RTCIceCandidate = window.RTCIceCandidate ||
        window.mozRTCIceCandidate || window.webkitRTCIceCandidate;
    window.RTCSessionDescription = window.RTCSessionDescription ||
        window.mozRTCSessionDescription || window.webkitRTCSessionDescription;

    var peerConnectionConfig = {
      'optional': [{'DtlsSrtpKeyAgreement': true}],
      'iceServers': [{'url': 'stun:stun.l.google.com:19302'}]
    };

    var peerConnection = new RTCPeerConnection(peerConnectionConfig);
    var socket = new WebSocket('ws://localhost:' + port);

    peerConnection.ondatachannel =
        function(event) {
      console.log('Got channel ', event);
      resolve(event.channel);
    }

        peerConnection.onicecandidate =
            function(event) {
      console.log("GOT ICE CANDIDATE", event);
      if (event.candidate != null) {
        var response = {type: 'icecandidate', ice: event.candidate.toJSON()};
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

                      var response = {type: 'answer', sdi: sdi.toJSON()};
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