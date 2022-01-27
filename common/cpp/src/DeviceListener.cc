#include "DeviceListener.h"
#include <Functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>
#include <iostream>
#include "flutter_webrtc_base.h"
//#include "AudioInterfaceActivator.h"
using namespace std;
using namespace flutter;
namespace flutter_webrtc_plugin {

#define SAFE_RELEASE(punk) \
  if ((punk) != NULL) {    \
    (punk)->Release();     \
    (punk) = NULL;         \
  }

static inline std::wstring cp_to_wide(const std::string& s, UINT codepage) {
  int in_length = (int)s.length();
  int out_length = MultiByteToWideChar(codepage, 0, s.c_str(), in_length, 0, 0);
  std::vector<wchar_t> buffer(out_length);
  if (out_length)
    MultiByteToWideChar(codepage, 0, s.c_str(), in_length, &buffer[0],
                        out_length);
  std::wstring result(buffer.begin(), buffer.end());
  return result;
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

static inline std::string utf8_to_cp(const std::string& s, UINT codepage) {
  if (codepage == CP_UTF8)
    return s;
  std::wstring wide = cp_to_wide(s, CP_UTF8);
  return wide_to_cp(wide, codepage);
}

static inline std::string wide_to_ansi(const std::wstring& s) {
  return wide_to_cp(s, CP_ACP);
}

static inline std::string utf8_to_ansi(const std::string& s) {
  return utf8_to_cp(s, CP_ACP);
}

void printHResult(const char* errorMessage, HRESULT hr) {
  DWORD error = GetLastError();
  printf("%s (hresult %lu 0x%x lasterror %d 0x%x)\n", errorMessage, hr, hr,
         error, error);
}

DeviceListener::DeviceListener(FlutterWebRTCBase* base)
    : base_(base), _cRef(1), _pEnumerator(NULL), _hasRegister(false) {}

DeviceListener::~DeviceListener(){SAFE_RELEASE(_pEnumerator)}

HRESULT DeviceListener::onRegister() {
  HRESULT hr = S_OK;
  // 初始化COM
  hr = ::CoInitialize(NULL);
  if (FAILED(hr)) {
    cout << "CoInitialize Error" << endl;
  }
  // 创建接口
  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                        __uuidof(IMMDeviceEnumerator), (void**)&_pEnumerator);

  if (hr == S_OK) {
    cout << "Create Instance Ok" << endl;
  } else {
    cout << "Create Instance Failed" << endl;
  }

  // 注册事件
  hr = _pEnumerator->RegisterEndpointNotificationCallback(
      (IMMNotificationClient*)this);
  if (hr == S_OK) {
    _hasRegister = true;
    cout << "DeviceListener Register Ok" << endl;
  } else {
    cout << "DeviceListener Register Failed" << endl;
  }
  CoUninitialize();
  return hr;
}

HRESULT DeviceListener::onUnregister() {
  HRESULT hr = S_OK;
  if (_pEnumerator && _hasRegister) {
    hr = _pEnumerator->UnregisterEndpointNotificationCallback(
        (IMMNotificationClient*)this);
    if (hr == S_OK) {
      _hasRegister = false;
      cout << "DeviceListener Unregister Ok" << endl;
      SAFE_RELEASE(_pEnumerator)
    } else {
      cout << "DeviceListener Unregister Failed" << endl;
    }
  }
  return hr;
}

ULONG STDMETHODCALLTYPE DeviceListener::AddRef() {
  return InterlockedIncrement(&_cRef);
}

ULONG STDMETHODCALLTYPE DeviceListener::Release() {
  ULONG ulRef = InterlockedDecrement(&_cRef);
  if (0 == ulRef) {
    delete this;
  }
  return ulRef;
}

HRESULT __stdcall DeviceListener::QueryInterface(REFIID riid,
                                                 VOID** ppvInterface) {
  if (IID_IUnknown == riid) {
    AddRef();
    *ppvInterface = (IUnknown*)this;
  } else if (__uuidof(IMMNotificationClient) == riid) {
    AddRef();
    *ppvInterface = (IMMNotificationClient*)this;
  } else {
    *ppvInterface = NULL;
    return E_NOINTERFACE;
  }
  return S_OK;
}

HRESULT __stdcall DeviceListener::OnDefaultDeviceChanged(
    EDataFlow flow,
    ERole role,
    LPCWSTR pwstrDeviceId) {
  const char* pszFlow = "?????";
  const char* pszRole = "?????";

  //_PrintDeviceName(pwstrDeviceId);

  switch (flow) {
    case eRender:
      pszFlow = "eRender";
      break;
    case eCapture:
      pszFlow = "eCapture";
      break;
  }

  std::string strDeviceId = wide_to_ansi(pwstrDeviceId);
  switch (role) {
    case eConsole:
      pszRole = "eConsole";
      break;
    case eMultimedia:
      pszRole = "eMultimedia";
      break;
    case eCommunications: {
      pszRole = "eCommunications";
    }
      break;
  }

  if (role == eConsole && flow == eRender) {
    base_->activeAudioOutputDevice(pwstrDeviceId);
  }

  if (role == eConsole && flow == eCapture) {
    base_->activeAudioInputDevice(pwstrDeviceId);
  }
  cout << "  ====>New default device: flow = " << pszFlow
       << ", role = " << pszRole 
      << ", id = " << strDeviceId << endl;

  return S_OK;
}

HRESULT __stdcall DeviceListener::OnDeviceAdded(LPCWSTR pwstrDeviceId) {
  //_PrintDeviceName(pwstrDeviceId);

  printf("  ---->Added device\n");
  return S_OK;
}

HRESULT __stdcall DeviceListener::OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
  //_PrintDeviceName(pwstrDeviceId);

  printf("  ---->Removed device\n");
  return S_OK;
}

HRESULT __stdcall DeviceListener::OnDeviceStateChanged(LPCWSTR pwstrDeviceId,
                                                       DWORD dwNewState) {
  const char* pszState = "Unkown";

  //_PrintDeviceName(pwstrDeviceId);
  std::string strDeviceId = wide_to_ansi(pwstrDeviceId);
  switch (dwNewState) {
    case DEVICE_STATE_ACTIVE:
    {
      //插入音频设备 切换到该设备
      //base_->activeAudioOutputDevice(pwstrDeviceId);
      //base_->activeAudioInputDevice(pwstrDeviceId);
      pszState = "ACTIVE";
    }
      break;
    case DEVICE_STATE_DISABLED:
      pszState = "DISABLED";
      break;
    case DEVICE_STATE_NOTPRESENT:
      pszState = "NOTPRESENT";
      break;
    case DEVICE_STATE_UNPLUGGED:
    {
      pszState = "UNPLUGGED";
      ////拔出设备 判断只剩下一个设备 切换到索引0
      //const int16_t iAudioDeviceCount = base_->audio_device_->PlayoutDevices();
      //if (iAudioDeviceCount == 1) {
      //  base_->setAudioOutput(0);
      //}
      //const int16_t iInputDeviceCount = base_->audio_device_->RecordingDevices();
      //if (iInputDeviceCount == 1) {
      //  base_->setAudioInput(0);
      //}
      //std::cout << "==== iaudioDeviceCount = " << iAudioDeviceCount << std::endl;
    }
      break;
    case DEVICE_STATEMASK_ALL:
      pszState = "MASK_ALL";
    //default:
    //  break;
  }

  printf("  ====>New device state is DEVICE_STATE_%s (0x%8.8x) id = %s\n",
         pszState, dwNewState, strDeviceId.c_str());

  return S_OK;
}

HRESULT __stdcall DeviceListener::OnPropertyValueChanged(
    LPCWSTR pwstrDeviceId,
    const PROPERTYKEY key) {
  // printf("  -->Changed device property "
  //    "{%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x}#%d\n",
  //    key.fmtid.Data1, key.fmtid.Data2, key.fmtid.Data3,
  //    key.fmtid.Data4[0], key.fmtid.Data4[1],
  //    key.fmtid.Data4[2], key.fmtid.Data4[3],
  //    key.fmtid.Data4[4], key.fmtid.Data4[5],
  //    key.fmtid.Data4[6], key.fmtid.Data4[7],
  //    key.pid);
  return S_OK;
}
//###################### GET DEFAULT DEVICE ######################
LPWSTR DeviceListener::getDefaultAudioEndpoint() {
  HRESULT hr;
  LPWSTR id;
  IMMDevice* device;

  EDataFlow dataFlow = eRender;
  ERole role = eConsole;

  hr = _pEnumerator->GetDefaultAudioEndpoint(dataFlow, role, &device);
  if (hr) {
    printHResult("GetDefaultAudioEndpoint failed", hr);
  }

  // printf("  DefaultDevice: ");

  hr = device->GetId(&id);
  if (hr) {
    printHResult("GetDefaultAudioEndpoint failed", hr);
  }

  IPropertyStore* pProps = NULL;

  hr = device->OpenPropertyStore(STGM_READ, &pProps);
  if (hr) {
    printHResult("GetDefaultAudioEndpoint failed", hr);
  }

  PROPVARIANT varName;

  PropVariantInit(&varName);

  hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
  if (hr) {
    printHResult("GetDefaultAudioEndpoint failed", hr);
  }

  printf("GetDefaultAudioEndpoint: %ls\n", varName.pwszVal);

  device->Release();

  return id;
}

HRESULT DeviceListener::_PrintDeviceName(LPCWSTR pwstrId) {
  HRESULT hr = S_OK;
  IMMDevice* pDevice = NULL;
  IPropertyStore* pProps = NULL;
  PROPVARIANT varString;

  CoInitialize(NULL);
  PropVariantInit(&varString);

  if (_pEnumerator == NULL) {
    // Get enumerator for audio endpoint devices.
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                          CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
                          (void**)&_pEnumerator);
  }
  if (hr == S_OK) {
  }
  if (hr == S_OK) {
    hr = _pEnumerator->GetDevice(pwstrId, &pDevice);
  }
  if (hr == S_OK) {
    hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
  }
  if (hr == S_OK) {
    // Get the endpoint device's friendly-name property.
    PropVariantInit(&varString);
    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varString);
  }
  cout << "----------------------\nDevice name: "
       << ((hr == S_OK) ? wide_to_cp(varString.pwszVal, CP_ACP) : "null device")
       << endl
        << "  Endpoint ID string: "
       << ((pwstrId != NULL) ? wide_to_cp(pwstrId, CP_ACP) : "null ID")
      << "---------------------------\n" << endl;
  if (hr == S_OK) {
    PropVariantClear(&varString);
  }

  SAFE_RELEASE(pProps)
  SAFE_RELEASE(pDevice)
  CoUninitialize();
  return hr;
}

}  // namespace flutter_webrtc_plugin