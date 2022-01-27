#ifndef _CMMNOTIFICATIONCLIENT_H_
#define _CMMNOTIFICATIONCLIENT_H_

#include <mmdeviceapi.h>

namespace flutter_webrtc_plugin {
class FlutterWebRTCBase;

class DeviceListener : public IMMNotificationClient 
{
public:
  DeviceListener(FlutterWebRTCBase* base);
  virtual ~DeviceListener();
  //开始注册监听
  HRESULT onRegister();
  //注销设备监听
  HRESULT onUnregister();

  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface);

  // Callback methods for device-event notifications.
  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow,
                                                   ERole role,
                                                   LPCWSTR pwstrDeviceId);
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId);
  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId);
  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId,
                                                 DWORD dwNewState);
  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId,
                                                   const PROPERTYKEY key);
  LPWSTR getDefaultAudioEndpoint();

 private:
  // Private function to print device-friendly name
  HRESULT _PrintDeviceName(LPCWSTR pwstrId);

 private:
  LONG _cRef;
  IMMDeviceEnumerator* _pEnumerator;
  bool _hasRegister;
  FlutterWebRTCBase* base_;
};
}  // namespace flutter_webrtc_plugin
#endif  //_DEVICELISTENER_H_