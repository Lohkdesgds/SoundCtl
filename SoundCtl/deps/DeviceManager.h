#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <functiondiscoverykeys_devpkey.h>
#include <endpointvolume.h>
#include <stdexcept>
#include <vector>

enum class AudioType {CONSOLE = eConsole, MULTIMEDIA = eMultimedia, COMMUNICATIONS = eCommunications};

class VolumeDevice {
	IAudioVolumeLevel* level = nullptr;
	const std::string& name;

	void _unload();
public:
	VolumeDevice(IAudioVolumeLevel*, const std::string&);
	VolumeDevice(const VolumeDevice&) = delete;
	VolumeDevice(VolumeDevice&&) noexcept;
	void operator=(const VolumeDevice&) = delete;
	void operator=(VolumeDevice&&) = delete;
	~VolumeDevice();

	float get_level(const size_t = static_cast<size_t>(-1)) const;
	void set_level(const float, const size_t = static_cast<size_t>(-1));

	const std::string& get_name() const;
};

class Device {
	IMMDevice* device = nullptr;
	IPropertyStore* pProps = nullptr;
	IAudioEndpointVolume* vol = nullptr;
	IDeviceTopology* topo = nullptr;

	void _unload();
public:
	Device(IMMDevice*);
	Device(const Device&) = delete;
	Device(Device&&) noexcept;
	void operator=(const Device&) = delete;
	void operator=(Device&&) = delete;
	~Device();

	std::string get_friendly_name() const;
	void set_volume(const float);
	float get_volume() const;
	void set_mute(const bool);
	bool get_mute() const;

	VolumeDevice get_underlying_volume(const size_t = 0);
};

class DeviceList {
	IMMDeviceEnumerator *devenum = nullptr;
	IMMDeviceCollection *rec = nullptr, *play = nullptr;
	static bool coinit;

	template<typename T> inline void __funky_release(T*& dev) { if ((dev) != nullptr) { dev->Release(); dev = nullptr; } }

	void _delete_all();
public:
	DeviceList();
	~DeviceList();

	size_t get_num_rec() const;
	size_t get_num_play() const;

	Device get_rec(const std::string&) const;
	Device get_play(const std::string&) const;

	Device get_rec(const size_t) const;
	Device get_play(const size_t) const;

	Device get_default_rec(const AudioType) const;
	Device get_default_play(const AudioType) const;

	Device find_default_rec() const;
	Device find_default_play() const;
};

void _test();