#ifndef LIB_WEBRTC_RTC_AUDIO_DEVICE_HXX
#define LIB_WEBRTC_RTC_AUDIO_DEVICE_HXX

#include "rtc_types.h"

namespace libwebrtc {

class RTCAudioDevice : public RefCountInterface {
 public:
  static const int kAdmMaxDeviceNameSize = 128;
  static const int kAdmMaxFileNameSize = 512;
  static const int kAdmMaxGuidSize = 128;

  enum WindowsDeviceType {
    kDefaultCommunicationDevice = -1,
    kDefaultDevice = -2
  };

 public:
  // Main initialization and termination
  virtual int32_t Init() = 0;
  virtual int32_t Terminate() = 0;
  virtual bool Initialized() const = 0;

  // Device enumeration
  virtual int16_t PlayoutDevices() = 0;
  virtual int16_t RecordingDevices() = 0;
  virtual int32_t PlayoutDeviceName(uint16_t index,
                                    char name[kAdmMaxDeviceNameSize],
                                    char guid[kAdmMaxGuidSize]) = 0;

  virtual int32_t RecordingDeviceName(uint16_t index,
                                      char name[kAdmMaxDeviceNameSize],
                                      char guid[kAdmMaxGuidSize]) = 0;

  // Device selection
  virtual int32_t SetPlayoutDevice(uint16_t index) = 0;
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device) = 0;
  virtual int32_t SetRecordingDevice(uint16_t index) = 0;
  virtual int32_t SetRecordingDevice(WindowsDeviceType device) = 0;

  // Audio transport initialization
  virtual int32_t PlayoutIsAvailable(bool* available) = 0;
  virtual int32_t InitPlayout() = 0;
  virtual bool PlayoutIsInitialized() const = 0;
  virtual int32_t RecordingIsAvailable(bool* available) = 0;
  virtual int32_t InitRecording() = 0;
  virtual bool RecordingIsInitialized() const = 0;

  // Audio transport control
  virtual int32_t StartPlayout() = 0;
  virtual int32_t StopPlayout() = 0;
  virtual bool Playing() const = 0;
  virtual int32_t StartRecording() = 0;
  virtual int32_t StopRecording() = 0;
  virtual bool Recording() const = 0;

  // Audio mixer initialization
  virtual int32_t InitSpeaker() = 0;
  virtual bool SpeakerIsInitialized() const = 0;
  virtual int32_t InitMicrophone() = 0;
  virtual bool MicrophoneIsInitialized() const = 0;

  // Speaker volume controls
  virtual int32_t SpeakerVolumeIsAvailable(bool* available) = 0;
  virtual int32_t SetSpeakerVolume(uint32_t volume) = 0;
  virtual int32_t SpeakerVolume(uint32_t* volume) const = 0;
  virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const = 0;
  virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const = 0;

  // Microphone volume controls
  virtual int32_t MicrophoneVolumeIsAvailable(bool* available) = 0;
  virtual int32_t SetMicrophoneVolume(uint32_t volume) = 0;
  virtual int32_t MicrophoneVolume(uint32_t* volume) const = 0;
  virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const = 0;
  virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const = 0;

  // Speaker mute control
  virtual int32_t SpeakerMuteIsAvailable(bool* available) = 0;
  virtual int32_t SetSpeakerMute(bool enable) = 0;
  virtual int32_t SpeakerMute(bool* enabled) const = 0;

  // Microphone mute control
  virtual int32_t MicrophoneMuteIsAvailable(bool* available) = 0;
  virtual int32_t SetMicrophoneMute(bool enable) = 0;
  virtual int32_t MicrophoneMute(bool* enabled) const = 0;

  // Stereo support
  virtual int32_t StereoPlayoutIsAvailable(bool* available) const = 0;
  virtual int32_t SetStereoPlayout(bool enable) = 0;
  virtual int32_t StereoPlayout(bool* enabled) const = 0;
  virtual int32_t StereoRecordingIsAvailable(bool* available) const = 0;
  virtual int32_t SetStereoRecording(bool enable) = 0;
  virtual int32_t StereoRecording(bool* enabled) const = 0;

 protected:
  virtual ~RTCAudioDevice() {}
};

}  // namespace libwebrtc

#endif  // LIB_WEBRTC_RTC_AUDIO_DEVICE_HXX
