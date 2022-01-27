#include "flutter_webrtc_base.h"

#include "flutter_data_channel.h"
#include "flutter_peerconnection.h"
#include "DeviceListener.h"

namespace flutter_webrtc_plugin {
std::mutex g_mutex;

bool SkipDefaultDevice(const char* name) {
  const auto utfName = std::string(name);
  return (utfName.rfind("Default - ", 0) == 0) ||
         (utfName.rfind("Communication - ", 0) == 0);
}

static inline std::string wide_to_cp(const std::wstring& s, UINT codepage) {
  int in_length = (int)s.length();
  int out_length =
      WideCharToMultiByte(codepage, 0, s.c_str(), in_length, 0, 0, 0, 0);
  std::vector<char> buffer(out_length);
  if (out_length)
    WideCharToMultiByte(codepage, 0, s.c_str(), in_length, &buffer[0],
                        out_length, 0, 0);
  std::string result(buffer.begin(), buffer.end());
  return result;
}

static inline std::string wide_to_ansi(const std::wstring& s) {
  return wide_to_cp(s, CP_ACP);
}

FlutterWebRTCBase::FlutterWebRTCBase(BinaryMessenger *messenger,
                                     TextureRegistrar *textures)
    : messenger_(messenger), textures_(textures) {
  LibWebRTC::Initialize();
  factory_ = LibWebRTC::CreateRTCPeerConnectionFactory();
  audio_device_ = factory_->GetAudioDevice();
  video_device_ = factory_->GetVideoDevice();
  pNotifyClient_ = new DeviceListener(this);
  pNotifyClient_->onRegister();
  bool hasInit = audio_device_->Initialized();
  if (hasInit) {
    std::cout << "audio_device_ hasInit" << std::endl;
  }
}

FlutterWebRTCBase::~FlutterWebRTCBase() {
  LibWebRTC::Terminate();
  if (pNotifyClient_) {
    pNotifyClient_->onUnregister();
    delete pNotifyClient_;
  }
  
}

std::string FlutterWebRTCBase::GenerateUUID() {
  return uuidxx::uuid::Generate().ToString(false);
}

RTCPeerConnection *FlutterWebRTCBase::PeerConnectionForId(
    const std::string &id) {
  auto it = peerconnections_.find(id);

  if (it != peerconnections_.end()) return (*it).second.get();

  return nullptr;
}

void FlutterWebRTCBase::RemovePeerConnectionForId(const std::string &id) {
  auto it = peerconnections_.find(id);
  if (it != peerconnections_.end()) peerconnections_.erase(it);
}

RTCMediaTrack* FlutterWebRTCBase ::MediaTrackForId(const std::string& id) {
  auto it = local_tracks_.find(id);

  if (it != local_tracks_.end())
    return (*it).second.get();

  for (auto kv : peerconnection_observers_) {
      auto pco = kv.second.get();
      auto track = pco->MediaTrackForId(id);
      if (track != nullptr) return track;
  }

  return nullptr;
}

void FlutterWebRTCBase::RemoveMediaTrackForId(const std::string& id) {
  auto it = local_tracks_.find(id);
  if (it != local_tracks_.end())
    local_tracks_.erase(it);
}

FlutterPeerConnectionObserver* FlutterWebRTCBase::PeerConnectionObserversForId(
    const std::string& id) {
  auto it = peerconnection_observers_.find(id);

  if (it != peerconnection_observers_.end())
    return (*it).second.get();

  return nullptr;
}

void FlutterWebRTCBase::RemovePeerConnectionObserversForId(
    const std::string& id) {
  auto it = peerconnection_observers_.find(id);
  if (it != peerconnection_observers_.end())
    peerconnection_observers_.erase(it);
}

scoped_refptr<RTCMediaStream> FlutterWebRTCBase::MediaStreamForId(
    const std::string& id) {
  auto it = local_streams_.find(id);
  if (it != local_streams_.end()) {
    return (*it).second;
  }

  for (auto kv : peerconnection_observers_) {
    auto pco = kv.second.get();
    auto stream = pco->MediaStreamForId(id);
    if (stream != nullptr) return stream;
  }

  return nullptr;
}

void FlutterWebRTCBase::RemoveStreamForId(const std::string &id) {
  auto it = local_streams_.find(id);
  if (it != local_streams_.end()) local_streams_.erase(it);
}

bool FlutterWebRTCBase::ParseConstraints(const EncodableMap &constraints,
                                         RTCConfiguration *configuration) {
  memset(&configuration->ice_servers, 0, sizeof(configuration->ice_servers));
  return false;
}

void FlutterWebRTCBase::ParseConstraints(
    const EncodableMap &src,
    scoped_refptr<RTCMediaConstraints> mediaConstraints,
    ParseConstraintType type /*= kMandatory*/) {
  for (auto kv : src) {
    EncodableValue k = kv.first;
    EncodableValue v = kv.second;
    std::string key = GetValue<std::string>(k);
    std::string value;
    if (TypeIs<EncodableList>(v) || TypeIs<EncodableMap>(v)) {
    } else if (TypeIs<std::string>(v)) {
      value = GetValue<std::string>(v);
    } else if (TypeIs<double>(v)) {
      value = std::to_string(GetValue<double>(v));
    } else if (TypeIs<int>(v)) {
      value = std::to_string(GetValue<int>(v));
    } else if (TypeIs<bool>(v)) {
      value = GetValue<bool>(v) ? RTCMediaConstraints::kValueTrue
                                : RTCMediaConstraints::kValueFalse;
    } else {
      value = std::to_string(GetValue<int>(v));
    }
    if (type == kMandatory) {
      mediaConstraints->AddMandatoryConstraint(key.c_str(), value.c_str());
    } else {
      mediaConstraints->AddOptionalConstraint(key.c_str(), value.c_str());
      if (key == "DtlsSrtpKeyAgreement") {
        configuration_.srtp_type = GetValue<bool>(v)
                                       ? MediaSecurityType::kDTLS_SRTP
                                       : MediaSecurityType::kSDES_SRTP;
      }
    }
  }
}

scoped_refptr<RTCMediaConstraints> FlutterWebRTCBase::ParseMediaConstraints(
    const EncodableMap &constraints) {
  scoped_refptr<RTCMediaConstraints> media_constraints =
      RTCMediaConstraints::Create();

  if (constraints.find(EncodableValue("mandatory")) != constraints.end()) {
    auto it = constraints.find(EncodableValue("mandatory"));
    const EncodableMap mandatory = GetValue<EncodableMap>(it->second);
    ParseConstraints(mandatory, media_constraints, kMandatory);
  } else {
    // Log.d(TAG, "mandatory constraints are not a map");
  }

  auto it = constraints.find(EncodableValue("optional"));
  if (it != constraints.end()) {
    const EncodableValue optional = it->second;
    if (TypeIs<EncodableMap>(optional)) {
      ParseConstraints(GetValue<EncodableMap>(optional), media_constraints,
                       kOptional);
    } else if (TypeIs<EncodableList>(optional)) {
      const EncodableList list = GetValue<EncodableList>(optional);
      for (size_t i = 0; i < list.size(); i++) {
        ParseConstraints(GetValue<EncodableMap>(list[i]), media_constraints, kOptional);
      }
    }
  } else {
    // Log.d(TAG, "optional constraints are not an array");
  }

  return media_constraints;
}

bool FlutterWebRTCBase::CreateIceServers(const EncodableList &iceServersArray,
                                         IceServer *ice_servers) {
  size_t size = iceServersArray.size();
  for (size_t i = 0; i < size; i++) {
    IceServer &ice_server = ice_servers[i];
    EncodableMap iceServerMap = GetValue<EncodableMap>(iceServersArray[i]);
    bool hasUsernameAndCredential =
        iceServerMap.find(EncodableValue("username")) != iceServerMap.end() &&
        iceServerMap.find(EncodableValue("credential")) != iceServerMap.end();
    auto it = iceServerMap.find(EncodableValue("url"));
    if (it != iceServerMap.end() && TypeIs<std::string>(it->second)) {
      if (hasUsernameAndCredential) {
        std::string username =
             GetValue<std::string>(iceServerMap.find(EncodableValue("username"))->second);
        std::string credential =
             GetValue<std::string>(iceServerMap.find(EncodableValue("credential"))
                ->second);
        std::string uri =  GetValue<std::string>(it->second);
        ice_server.username = username;
        ice_server.password = credential;
        ice_server.uri = uri;
      } else {
        std::string uri = GetValue<std::string>(it->second);
        ice_server.uri = uri;
      }
    }
    it = iceServerMap.find(EncodableValue("urls"));
    if (it != iceServerMap.end()) {
      if (TypeIs<std::string>(it->second)) {
        if (hasUsernameAndCredential) {
          std::string username =  GetValue<std::string>(iceServerMap.find(EncodableValue("username"))
                                     ->second);
          std::string credential =
               GetValue<std::string>(iceServerMap.find(EncodableValue("credential"))
                  ->second);
          std::string uri =  GetValue<std::string>(it->second);
          ice_server.username = username;
          ice_server.password = credential;
          ice_server.uri = uri;
        } else {
          std::string uri =  GetValue<std::string>(it->second);
          ice_server.uri = uri;
        }
      }
      if (TypeIs<EncodableList>(it->second)) {
        const EncodableList urls = GetValue<EncodableList>(it->second);
        for (auto url : urls) {
          if (TypeIs<EncodableMap>(url)) {
            const EncodableMap map = GetValue<EncodableMap>(url);
            std::string value;
            auto it2 = map.find(EncodableValue("url"));
            if (it2 != map.end()) {
              value =  GetValue<std::string>(it2->second);
              if (hasUsernameAndCredential) {
                std::string username =
                     GetValue<std::string>(iceServerMap.find(EncodableValue("username"))
                        ->second);
                std::string credential =
                    GetValue<std::string>(iceServerMap.find(EncodableValue("credential"))
                        ->second);
                ice_server.username = username;
                ice_server.password = credential;
                ice_server.uri = value;
              } else {
                ice_server.uri = value;
              }
            }
          }
          else if (TypeIs<std::string>(url)) {
              std::string urlString = GetValue<std::string>(url);
              ice_server.uri = urlString;
          }
        }
      }
    }
  }
  return size > 0;
}

bool FlutterWebRTCBase::ParseRTCConfiguration(const EncodableMap &map,
                                              RTCConfiguration &conf) {
  auto it = map.find(EncodableValue("iceServers"));
  if (it != map.end()) {
    const EncodableList iceServersArray = GetValue<EncodableList>(it->second);
    CreateIceServers(iceServersArray, conf.ice_servers);
  }
  // iceTransportPolicy (public API)
  it = map.find(EncodableValue("iceTransportPolicy"));
  if (it != map.end() && TypeIs<std::string>(it->second)) {
    std::string v = GetValue<std::string>(it->second);
    if (v == "all")  // public
      conf.type = IceTransportsType::kAll;
    else if (v == "relay")
      conf.type = IceTransportsType::kRelay;
    else if (v == "nohost")
      conf.type = IceTransportsType::kNoHost;
    else if (v == "none")
      conf.type = IceTransportsType::kNone;
  }

  // bundlePolicy (public api)
  it = map.find(EncodableValue("bundlePolicy"));
  if (it != map.end() && TypeIs<std::string>(it->second)) {
    std::string v = GetValue<std::string>(it->second);
    if (v == "balanced")  // public
      conf.bundle_policy = kBundlePolicyBalanced;
    else if (v == "max-compat")  // public
      conf.bundle_policy = kBundlePolicyMaxCompat;
    else if (v == "max-bundle")  // public
      conf.bundle_policy = kBundlePolicyMaxBundle;
  }

  // rtcpMuxPolicy (public api)
  it = map.find(EncodableValue("rtcpMuxPolicy"));
  if (it != map.end() && TypeIs<std::string>(it->second)) {
    std::string v = GetValue<std::string>(it->second);
    if (v == "negotiate")  // public
      conf.rtcp_mux_policy = RtcpMuxPolicy::kRtcpMuxPolicyNegotiate;
    else if (v == "require")  // public
      conf.rtcp_mux_policy = RtcpMuxPolicy::kRtcpMuxPolicyRequire;
  }

  // FIXME: peerIdentity of type DOMString (public API)
  // FIXME: certificates of type sequence<RTCCertificate> (public API)
  // iceCandidatePoolSize of type unsigned short, defaulting to 0
  it = map.find(EncodableValue("iceCandidatePoolSize"));
  if (it != map.end()) {
    conf.ice_candidate_pool_size = GetValue<int>(it->second);
  }

  // sdpSemantics (public api)
  it = map.find(EncodableValue("sdpSemantics"));
  if (it != map.end() && TypeIs<std::string>(it->second)) {
    std::string v = GetValue<std::string>(it->second);
    if (v == "plan-b")  // public
      conf.sdp_semantics = SdpSemantics::kPlanB;
    else if (v == "unified-plan")  // public
      conf.sdp_semantics = SdpSemantics::kUnifiedPlan;
  }
  return true;
}


scoped_refptr<RTCMediaTrack> FlutterWebRTCBase::MediaTracksForId(
    const std::string& id) {
  auto it = local_tracks_.find(id);
  if (it != local_tracks_.end()) {
    return (*it).second;
  }

  return nullptr;
}

void FlutterWebRTCBase::RemoveTracksForId(const std::string& id) {
  auto it = local_tracks_.find(id);
  if (it != local_tracks_.end())
    local_tracks_.erase(it);
}

void FlutterWebRTCBase::switchToAudioOutput(std::string id) {
    if (audio_device_->Playing()) {
        audio_device_->StopPlayout();
    }
    auto specific = false;
    const auto finish = [&]() {
        if (!specific) {
            if (const auto result = audio_device_->SetPlayoutDevice(
                RTCAudioDevice::kDefaultCommunicationDevice)) {
                std::cout
                    << "[webrtc_base] setAudioOutputDevice(" << id
                    << "): SetPlayoutDevice(kDefaultCommunicationDevice) failed: "
                    << result << "." << std::endl;
            } else {
                std::cout
                    << "[webrtc_base] setAudioOutputDevice(" << id
                    << "): SetPlayoutDevice(kDefaultCommunicationDevice) success." << std::endl;
            }
        }
        if (audio_device_->InitPlayout() == 0) {
            audio_device_->StartPlayout();
        }
    };

    if (id == "default" || id.empty()) {
        return finish();
    }
    const auto count = audio_device_ ? audio_device_->PlayoutDevices() : int16_t(-666);
    if (count <= 0) {
        std::cout << "[webrtc_base] setAudioOutputDevice(" << id
                    << "): Could not get playout devices count: " << count << "." << std::endl;
        return finish();
    }
    int16_t order = !id.empty() && id[0] == '#'
                    ? static_cast<int16_t>(std::stoi(id.substr(1)))
                    : -1;
    for (uint16_t i = 0; i != count; ++i) {
        char name[RTCAudioDevice::kAdmMaxDeviceNameSize + 1] = {0};
        char guid[RTCAudioDevice::kAdmMaxGuidSize + 1] = {0};
        audio_device_->PlayoutDeviceName(i, name, guid);
        if ((!SkipDefaultDevice(name) && id == guid) || order == i) {
            const auto result = audio_device_->SetPlayoutDevice(i);
            if (result != 0) {
                std::cout << "[webrtc_base] setAudioOutputDevice(" << id << ") name '"
                        << std::string(name) << "' failed: " << result << "." << std::endl;
            } else {
                std::cout << "[webrtc_base] setAudioOutputDevice(" << id << ") name '"
                            << std::string(name) << "' success." << std::endl;
                specific = true;
            }
            return finish();
        }
    }
    std::cout << "[webrtc_base] setAudioOutputDevice(" << id
            << "): Could not find playout device." << std::endl;
    return finish();
}

void FlutterWebRTCBase::setAudioOutput(int index) {
  std::thread thObj = std::thread([this, index]() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Sleep(500);
    int32_t ret = 0;
    ret = audio_device_->StopPlayout();
    if (0 != ret) {
      std::cout << "==== Failed to set Playout." << std::endl;
      return;
    }

    uint16_t indexU = static_cast<uint16_t>(index);
    ret = audio_device_->SetPlayoutDevice(indexU);
    if (0 != ret) {
      std::cout << "==== Failed to set Playout Device." << std::endl;
      return;
    }

    if (audio_device_->InitSpeaker() != 0) {
      std::cout << "Unable to access speaker." << std::endl;
    }
    bool available = false;
    if (audio_device_->StereoPlayoutIsAvailable(&available) != 0) {
      std::cout << "Failed to query stereo playout." << std::endl;
    }
    if (audio_device_->SetStereoPlayout(available) != 0) {
      std::cout << "Failed to set stereo playout mode." << std::endl;
    }
    ret = audio_device_->InitPlayout();
    if (0 != ret) {
      std::cout << "==== Failed to Init Playout." << std::endl;
    }
    ret = audio_device_->StartPlayout();
    if (0 != ret) {
      std::cout << "==== Failed to Start Playout." << std::endl;
    }
    std::cout << "==== setAudioOutput thread ====="
              << std::this_thread::get_id() << " index = " << indexU
              << std::endl;
  });
  thObj.detach();
}

void FlutterWebRTCBase::switchToAudioInput(const std::string& id) {
  const auto recording =
      audio_device_->Recording() || audio_device_->RecordingIsInitialized();
  if (recording) {
    audio_device_->StopRecording();
  }
  auto specific = false;
  const auto finish = [&] {
    if (!specific) {
        if (const auto result = audio_device_->SetRecordingDevice(
                RTCAudioDevice::kDefaultCommunicationDevice)) {
        std::cout << "[webrtc_base] setAudioInputDevice(" << id.c_str()
            << "): SetRecordingDevice(kDefaultCommunicationDevice) failed: "
            << result << "." << std::endl;
        } else {
        std::cout << "[webrtc_base] setAudioInputDevice(" << id.c_str()
            << "): SetRecordingDevice(kDefaultCommunicationDevice) success." << std::endl;
        }
    }
    if (recording && audio_device_->InitRecording() == 0) {
        audio_device_->StartRecording();
    }
  };
  if (id == "default" || id.empty()) {
    return finish();
  }

  const auto count = audio_device_ ? audio_device_->RecordingDevices() : int16_t(-666);
  if (count <= 0) {
    std::cout << "[webrtc_base] setAudioInputDevice(" << id
                      << "): Could not get recording devices count: " << count << "." << std::endl;
    return finish();
  }

  int16_t order = !id.empty() && id[0] == '#'
                      ? static_cast<int16_t>(std::stoi(id.substr(1)))
                      : -1;
  for (uint16_t i = 0; i != count; ++i) {
    char name[RTCAudioDevice::kAdmMaxDeviceNameSize + 1] = {0};
    char guid[RTCAudioDevice::kAdmMaxGuidSize + 1] = {0};
    audio_device_->RecordingDeviceName(i, name, guid);
    if ((!SkipDefaultDevice(name) && id == guid) || order == i) {
        const auto result = audio_device_->SetRecordingDevice(i);
        if (result != 0) {
            std::cout << "[webrtc_base] setAudioInputDevice(" << id << ") name '"
                    << std::string(name) << "' failed: " << result << "." << std::endl;
        } else {
            std::cout << "[webrtc_base] setAudioInputDevice(" << id << ") name '"
                    << std::string(name) << "' success." << std::endl;
            specific = true;
        }
        return finish();
    }
  }
  std::cout << "[webrtc_base] setAudioInputDevice(" << id
            << "): Could not find recording device." << std::endl;
  return finish();
}

void FlutterWebRTCBase::activeDefaultAudioInput() {
  std::thread thObj = std::thread([this]() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Sleep(500);
    int32_t ret = 0;
    const auto recording = audio_device_->Recording() || audio_device_->RecordingIsInitialized();
    if (recording) {
      ret = audio_device_->StopRecording();
      if (0 != ret) {
        std::cout << "[webrtc_base] StopRecording failed" <<std::endl;
      }
    }

    if (const auto result = audio_device_->SetRecordingDevice(
            RTCAudioDevice::kDefaultCommunicationDevice)) {
    std::cout
        << "[webrtc_base] setAudioInputDevice(): SetRecordingDevice(kDefaultCommunicationDevice) failed: "
        << result << "." << std::endl;
    } else {
    std::cout
        << "[webrtc_base] setAudioInputDevice(): SetRecordingDevice(kDefaultCommunicationDevice) success."
        << std::endl;
    }
    if (recording && audio_device_->InitRecording() == 0) {
      ret = audio_device_->StartRecording();
      if (0 != ret) {
        std::cout << "[webrtc_base] StartRecording failed" << std::endl;
      }
    }
    std::cout << "==== activeDefaultAudioInput thread ====="
              << std::this_thread::get_id() <<  std::endl;
  });

  thObj.detach();
}

void FlutterWebRTCBase::setAudioInput(int index) {
  std::thread thObj = std::thread([this, index]() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Sleep(500);
    const auto recording = audio_device_->Recording() || audio_device_->RecordingIsInitialized();
    if (recording) {
      audio_device_->StopRecording();
    }
    uint16_t indexU = static_cast<uint16_t>(index);
    if (const auto result = audio_device_->SetRecordingDevice(indexU)) {
      std::cout << "[webrtc_base] setAudioInputDevice(): index = " << indexU <<
                   "SetRecordingDevice(kDefaultCommunicationDevice) failed: "
                << result << "." << std::endl;
    } else {
      std::cout << "[webrtc_base] setAudioInputDevice(): "
                   "SetRecordingDevice(kDefaultCommunicationDevice) success."
                << std::endl;
    }
    if (recording && audio_device_->InitRecording() == 0) {
      audio_device_->StartRecording();
    }
    std::cout << "==== setAudioInput thread ====="
              << std::this_thread::get_id() << std::endl;
  });

  thObj.detach();
}

void FlutterWebRTCBase::activeAudioOutputDevice(LPCWSTR pwstrDeviceId) {
  std::string strDeviceId = wide_to_ansi(pwstrDeviceId);
  std::thread thObj = std::thread([this, strDeviceId]() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Sleep(380);
    char szNameUTF8[libwebrtc::RTCAudioDevice::kAdmMaxDeviceNameSize + 1] = {
        0};
    char szGuidUTF8[libwebrtc::RTCAudioDevice::kAdmMaxGuidSize + 1] = {0};
    std::cout << "\n================ active output = " << strDeviceId << std::endl;
    const int16_t iaudioDeviceCount = audio_device_->PlayoutDevices();
    std::cout << "================ PlayoutDevices = " << iaudioDeviceCount
              << std::endl;
    for (uint16_t index = 0; index < iaudioDeviceCount; index++) {
      audio_device_->PlayoutDeviceName(index, szNameUTF8, szGuidUTF8);
      std::string strNameUTF8 = szNameUTF8;
      std::string strGuidUTF8 = szGuidUTF8;
//      std::cout << "================ Playout strGuidUTF8 = " << strGuidUTF8 << std::endl;
      if (strGuidUTF8 == strDeviceId) {
        std::cout << "[webrtc_base] ACTIVE***********************************" << std::endl
                  << "PlayoutDevice name: " << strNameUTF8 << std::endl
                  << " guid: " << strGuidUTF8 << std::endl
                  << "*****************************************" << std::endl;
        switchToAudioOutput(strDeviceId);
        break;
      }
    }
  });

  thObj.detach();
}

void FlutterWebRTCBase::activeAudioInputDevice(LPCWSTR pwstrDeviceId) {
  std::string strDeviceId = wide_to_ansi(pwstrDeviceId);
  std::thread thObj = std::thread([this, strDeviceId]() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Sleep(380);
    char szNameUTF8[libwebrtc::RTCAudioDevice::kAdmMaxDeviceNameSize + 1] = { 0 };
    char szGuidUTF8[libwebrtc::RTCAudioDevice::kAdmMaxGuidSize + 1] = { 0 };
    std::cout << "\n================ active input = " << strDeviceId
              << std::endl;
    const int16_t iaudioDeviceCount = audio_device_->RecordingDevices();
    std::cout << "================ RecordingDevices = " << iaudioDeviceCount
              << std::endl;
    for (uint16_t index = 0; index < iaudioDeviceCount; index++) {
      audio_device_->RecordingDeviceName(index, szNameUTF8, szGuidUTF8);
      std::string strNameUTF8 = szNameUTF8;
      std::string strGuidUTF8 = szGuidUTF8;
//      std::cout << "================ Recording strGuidUTF8 = " << strGuidUTF8 << std::endl;
      if (strGuidUTF8 == strDeviceId) {
        std::cout << "[webrtc_base] ACTIVE***********************************"
                  << std::endl
                  << "RecordingDeviceName name: " << strNameUTF8 << std::endl
                  << " guid: " << strGuidUTF8 << std::endl
                  << "*****************************************" << std::endl;
        switchToAudioInput(strDeviceId);
        break;
      }
    }
  });

  thObj.detach();
}

}  // namespace flutter_webrtc_plugin
