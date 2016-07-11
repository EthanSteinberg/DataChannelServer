#Copyright(c) 2012 The WebRTC Project Authors.All rights reserved.
#
#Use of this source code is governed by a BSD - style license
#that can be found in the LICENSE file in the root of the source
#tree.An additional intellectual property rights grant can be found
#in the file PATENTS.All contributing project authors may
#be found in the AUTHORS file in the root of the source tree.
{
  'includes' :
      [
        '../talk/build/common.gypi',
      ],
      'targets' : [
        {
          'target_name' : 'DataChannelServer',
          'type' : 'shared_library',
          'sources' : [
            'src/internal-api.cc',
          ],
          'dependencies' : [
            '../webrtc/api/api.gyp:libjingle_peerconnection',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
            '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
          ]
        },
      ]
}