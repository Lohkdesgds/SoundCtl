#include "DeviceManager.h"

bool DeviceList::coinit = false;


void VolumeDevice::_unload()
{
    if (level) { level->Release(); level = nullptr; }
}

VolumeDevice::VolumeDevice(IAudioVolumeLevel* lvl, const std::string& s)
    : level(lvl), name(s)
{
    if (!level) throw std::invalid_argument("NULL LEVEL");
}

VolumeDevice::VolumeDevice(VolumeDevice&& v) noexcept
    : name(v.name)
{
    level = std::exchange(v.level, nullptr);
}

VolumeDevice::~VolumeDevice()
{
    _unload();
}

float VolumeDevice::get_level(const size_t ch) const
{
    HRESULT hr{};
    float _f = 0;

    if (ch != static_cast<size_t>(-1)) {
        hr = level->GetLevel(static_cast<UINT>(ch), &_f);
        if (FAILED(hr)) throw std::runtime_error("Failed to get level of vol");
    }
    else {
        UINT _c;
        hr = level->GetChannelCount(&_c);
        if (FAILED(hr) || _c == 0) throw std::runtime_error("Failed to get channel count");

        for (UINT a = 0; a < _c; ++a)
        {
            float __f;
            level->GetLevel(a, &__f);
            _f += __f;
        }

        _f *= 1.0f / _c;
    }

    return powf(10.0f, _f / 20.0f);
}

void VolumeDevice::set_level(const float vol, const size_t ch)
{
    HRESULT hr{};
    const float dbvol = 20.0f * log10f(vol);

    if (ch == static_cast<size_t>(-1)) {
        UINT _c;
        hr = level->GetChannelCount(&_c);
        if (FAILED(hr) || _c == 0) throw std::runtime_error("Failed to get channel count");

        for (UINT a = 0; a < _c; ++a)
            level->SetLevel(a, dbvol, NULL);
    }
    else {
        level->SetLevel(static_cast<UINT>(ch), dbvol, NULL);
    }
}

const std::string& VolumeDevice::get_name() const
{
    return name;
}

void Device::_unload()
{
    if (vol) { vol->Release(); vol = nullptr; }
    if (device) { device->Release(); device = nullptr; }
    if (pProps) { pProps->Release(); pProps = nullptr; }
    if (topo) { topo->Release(); topo = nullptr; }
}

Device::Device(IMMDevice* dev)
    : device(dev)
{
    if (!dev) throw std::invalid_argument("NULL DEVICE");

    HRESULT hr{};

    hr = device->OpenPropertyStore(STGM_READ, &pProps);
    if (FAILED(hr)) {
        _unload();
        throw std::runtime_error("CANNOT LOAD PROPERTIES OF DEVICE!");
    }

    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (LPVOID*)&vol);
    if (FAILED(hr)) {
        _unload();
        throw std::runtime_error("CANNOT LOAD VOLUME PROPERTY OF DEVICE!");
    }

    hr = device->Activate(__uuidof(IDeviceTopology), CLSCTX_INPROC_SERVER, NULL, (LPVOID*)&topo);
    if (FAILED(hr)) {
        _unload();
        throw std::runtime_error("CANNOT LOAD TOPOLOGY PROPERTY OF DEVICE!");
    }    
}

Device::Device(Device&& d) noexcept
{
    device = std::exchange(d.device, nullptr);
    pProps = std::exchange(d.pProps, nullptr);
    vol = std::exchange(d.vol, nullptr);
    topo = std::exchange(d.topo, nullptr);
}

Device::~Device()
{
    _unload();
}

std::string Device::get_friendly_name() const
{
    HRESULT hr{};
    PROPVARIANT varName;

    PropVariantInit(&varName);
    hr = pProps->GetValue(
        PKEY_Device_FriendlyName, &varName);

    std::wstring wstr(varName.pwszVal);

    PropVariantClear(&varName);

    std::string hardcast;
    for (const auto& i : wstr) hardcast += (char)i;
    return hardcast;
}

void Device::set_volume(const float f)
{
    if (f < 0.0f || f > 1.0f) return;
    vol->SetMasterVolumeLevelScalar(f, NULL);    
}

float Device::get_volume() const
{
    float f;
    vol->GetMasterVolumeLevelScalar(&f);
    return f;
}

void Device::set_mute(const bool b)
{
    vol->SetMute(b, NULL);
}

bool Device::get_mute() const
{
    BOOL b;
    vol->GetMute(&b);
    return b;
}

VolumeDevice Device::get_underlying_volume(const size_t undr)
{
    HRESULT hr{};    

    // get the single connector for that endpoint
    IConnector* pConnEndpoint = NULL;
    hr = topo->GetConnector(static_cast<UINT>(undr), &pConnEndpoint);
    if (FAILED(hr)) {
        throw std::runtime_error("Could not get connector 0");
    }

    // get the connector on the device that is
    // connected to
    // the connector on the endpoint
    IConnector* pConnDevice = NULL;
    hr = pConnEndpoint->GetConnectedTo(&pConnDevice);
    if (FAILED(hr)) {
        pConnEndpoint->Release();
        throw std::runtime_error("Could not get connectedto");
    }
    pConnEndpoint->Release();

    // QI on the device's connector for IPart
    IPart* pPart = NULL;
    hr = pConnDevice->QueryInterface(__uuidof(IPart), (void**)&pPart);
    if (FAILED(hr)) {
        pConnDevice->Release();
        throw std::runtime_error("Could not query interface");
    }

    pConnDevice->Release();

    std::string _tmpnam;
    {
        LPWSTR pwszPartName = NULL;
        hr = pPart->GetName(&pwszPartName);
        std::wstring wstr(pwszPartName);
        CoTaskMemFree(pwszPartName);
        for (const auto& i : wstr) _tmpnam += (char)i;
    }

    // see if this is a volume node part
    IAudioVolumeLevel* pVolume = NULL;
    hr = pPart->Activate(CLSCTX_ALL, __uuidof(IAudioVolumeLevel), (void**)&pVolume);
    if (E_NOINTERFACE == hr) {
        pPart->Release();
        throw std::runtime_error("Invalid number or no audio interface here.");
    }
    else if (FAILED(hr)) {
        pPart->Release();
        throw std::runtime_error("Could not get sub device");
    }

    pPart->Release();
    return VolumeDevice{ pVolume, _tmpnam };
}


void DeviceList::_delete_all()
{
    __funky_release(devenum);
    __funky_release(rec);
    __funky_release(play);
}

DeviceList::DeviceList()
{
    if (!coinit) {
        if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
            _delete_all();
            throw std::runtime_error("COINIT FAILED!");
        }
        coinit = true;
    }

    HRESULT hr;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&devenum);

    if (FAILED(hr)) {
        _delete_all();
        throw std::runtime_error("COCREATEINSTANCE FAILED!");
    }

    hr = devenum->EnumAudioEndpoints(
        eRender, DEVICE_STATE_ACTIVE,
        &play);

    if (FAILED(hr)) {
        _delete_all();
        throw std::runtime_error("EnumAudioEndpoints eRender FAILED!");
    }

    hr = devenum->EnumAudioEndpoints(
        eCapture, DEVICE_STATE_ACTIVE,
        &rec);

    if (FAILED(hr)) {
        _delete_all();
        throw std::runtime_error("EnumAudioEndpoints eCapture FAILED!");
    }
}

DeviceList::~DeviceList()
{
    _delete_all();
}

size_t DeviceList::get_num_rec() const
{
    UINT num{};
    rec->GetCount(&num);
    return static_cast<size_t>(num);
}

size_t DeviceList::get_num_play() const
{
    UINT num{};
    play->GetCount(&num);
    return static_cast<size_t>(num);
}

Device DeviceList::get_rec(const size_t p) const
{
    IMMDevice* ptr;
    rec->Item(static_cast<UINT>(p), &ptr);
    return Device{ ptr };
}

Device DeviceList::get_play(const size_t p) const
{
    IMMDevice* ptr;
    play->Item(static_cast<UINT>(p), &ptr); 
    return Device{ ptr };
}

Device DeviceList::get_rec(const std::string& fin) const
{
    UINT num{};
    rec->GetCount(&num);
    IMMDevice* ptr;
    for (UINT p = 0; p < num; ++p) {
        rec->Item(p, &ptr);
        Device dd{ ptr };
        if (dd.get_friendly_name().find(fin) != std::string::npos) return Device{ ptr };
    }
    return Device{ nullptr }; // fails
}

Device DeviceList::get_play(const std::string& fin) const
{
    UINT num{};
    play->GetCount(&num);
    IMMDevice* ptr;
    for (UINT p = 0; p < num; ++p) {
        play->Item(p, &ptr);
        Device dd{ ptr };
        if (dd.get_friendly_name().find(fin) != std::string::npos) return Device{ ptr };
    }
    return Device{ nullptr }; // fails
}

Device DeviceList::get_default_rec(const AudioType t) const
{
    IMMDevice* ptr = nullptr;
    HRESULT hr = devenum->GetDefaultAudioEndpoint(eCapture, static_cast<ERole>(t), &ptr);

    if (!ptr && get_num_rec()) {
        return get_rec(0);
    }

    if (FAILED(hr)) {        
        throw std::runtime_error("FAILED TO GET DEFAULT DEVICE FOR REC");
    }

    return Device{ ptr };
}

Device DeviceList::get_default_play(const AudioType t) const
{
    IMMDevice* ptr = nullptr;
    HRESULT hr = devenum->GetDefaultAudioEndpoint(eRender, static_cast<ERole>(t), &ptr);

    if (FAILED(hr)) {
        throw std::runtime_error("FAILED TO GET DEFAULT DEVICE FOR REC");
    }

    return Device{ ptr };
}

Device DeviceList::find_default_rec() const
{
    IMMDevice* ptr = nullptr;
    HRESULT hr = devenum->GetDefaultAudioEndpoint(eCapture, eConsole, &ptr);
    if (FAILED(hr)) throw std::runtime_error("FAILED FIND DEFAULT 1 REC");
    if (ptr) return Device{ ptr };
    hr = devenum->GetDefaultAudioEndpoint(eCapture, eMultimedia, &ptr);
    if (FAILED(hr)) throw std::runtime_error("FAILED FIND DEFAULT 2 REC");
    if (ptr) return Device{ ptr };
    hr = devenum->GetDefaultAudioEndpoint(eCapture, eCommunications, &ptr);
    if (FAILED(hr)) throw std::runtime_error("FAILED FIND DEFAULT 3 REC");
    if (ptr) return Device{ ptr };
    if (get_num_rec()) return get_rec(0);
    throw std::runtime_error("Cannot find a valid default rec device");
}

Device DeviceList::find_default_play() const
{
    IMMDevice* ptr = nullptr;
    HRESULT hr = devenum->GetDefaultAudioEndpoint(eRender, eConsole, &ptr);
    if (FAILED(hr)) throw std::runtime_error("FAILED FIND DEFAULT 1 PLAY");
    if (ptr) return Device{ ptr };
    hr = devenum->GetDefaultAudioEndpoint(eRender, eMultimedia, &ptr);
    if (FAILED(hr)) throw std::runtime_error("FAILED FIND DEFAULT 2 PLAY");
    if (ptr) return Device{ ptr };
    hr = devenum->GetDefaultAudioEndpoint(eRender, eCommunications, &ptr);
    if (FAILED(hr)) throw std::runtime_error("FAILED FIND DEFAULT 3 PLAY");
    if (ptr) return Device{ ptr };
    if (get_num_play()) return get_play(0);
    throw std::runtime_error("Cannot find a valid default play device");
}



#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }
void _test()
{
    HRESULT hr = S_OK;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDeviceCollection* pCollection = NULL;
    IMMDevice* pEndpoint = NULL;
    IPropertyStore* pProps = NULL;
    LPWSTR pwszID = NULL;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator);

    EXIT_ON_ERROR(hr)

    hr = pEnumerator->EnumAudioEndpoints(
            eRender, DEVICE_STATE_ACTIVE,
            &pCollection);
    EXIT_ON_ERROR(hr)

    UINT  count;
    hr = pCollection->GetCount(&count);
    EXIT_ON_ERROR(hr)

        if (count == 0)
        {
            printf("No endpoints found.\n");
        }

    // Each loop prints the name of an endpoint device.
    for (ULONG i = 0; i < count; i++)
    {
        // Get pointer to endpoint number i.
        hr = pCollection->Item(i, &pEndpoint);
        EXIT_ON_ERROR(hr)

            // Get the endpoint ID string.
            hr = pEndpoint->GetId(&pwszID);
        EXIT_ON_ERROR(hr)

            hr = pEndpoint->OpenPropertyStore(
                STGM_READ, &pProps);
        EXIT_ON_ERROR(hr)

            PROPVARIANT varName;
        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        hr = pProps->GetValue(
            PKEY_Device_FriendlyName, &varName);
        EXIT_ON_ERROR(hr)

            // Print endpoint friendly name and endpoint ID.
            printf("Endpoint %d: \"%S\" (%S)\n",
                i, varName.pwszVal, pwszID);

        CoTaskMemFree(pwszID);
        pwszID = NULL;
        PropVariantClear(&varName);
        SAFE_RELEASE(pProps)
            SAFE_RELEASE(pEndpoint)
    }
    SAFE_RELEASE(pEnumerator)
        SAFE_RELEASE(pCollection)
        return;
Exit:
    printf("Error!\n");
    CoTaskMemFree(pwszID);
    SAFE_RELEASE(pEnumerator)
        SAFE_RELEASE(pCollection)
        SAFE_RELEASE(pEndpoint)
        SAFE_RELEASE(pProps)
}
