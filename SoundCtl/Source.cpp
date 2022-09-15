#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <bitset>

#include "deps/DeviceManager.h"

#undef max
#undef min

const std::string version = "V1.0.0";

bool remake_terminal();
void message_timer(const unsigned int);

int main(int argc, char* argv[])
{
	try {
#ifdef _DEBUG
		remake_terminal();
		std::cout << "This is a DEBUG build. Do not use it as final version.\n\n";
		std::cout << "SoundCtl " << version << " by Lohk, 2022\n";
		std::cout << "Compiled " << __DATE__ << " @ " << __TIME__ << " GMT-3\n\n";
		std::cout << "Arguments from call (" << argc << "):\n";
		for (int a = 0; a < argc; ++a) {
			std::cout << "[" << a << "] => '" << argv[a] << "'\n";
		}
		std::cout << "\n";
#endif

		if (argc == 2 && strcmp(argv[1], "-help") == 0)
		{
			remake_terminal();
			std::cout << "SoundCtl " << version << " by Lohk, 2022\n";
			std::cout << "Compiled " << __DATE__ << " @ " << __TIME__ << " GMT-3\n\n";

			std::cout << "Call: <app.exe> <device kind> <device name to find> <modifiers> (number)\n\n";
			std::cout << "Device kind: IN or OUT (defaults IN if something else)\n";
			std::cout << "Device name: hint or * for default console one\n";
			std::cout << "Number: depends on flag\n";
			std::cout << "Flags:\n";
			std::cout << "- M: mute\n";
			std::cout << "- m: unmute\n";
			std::cout << "- T: toggle mute\n";
			std::cout << "- i: increase volume by (number: [0.0..1.0])\n";
			std::cout << "- d: decrease volume by (number: [0.0..1.0]\n";
			std::cout << "- s: set volume (number: [0.0..1.0])\n\n";

			std::cout << "Examples:\n";
			std::cout << "app.exe OUT Yeti T <- Switch mute on a OUTPUT device with Yeti in the name\n";
			std::cout << "app.exe IN Line ms 1.0 <- Unmute a output with Line in the name and set its volume to 100%\n";
			message_timer(10);
			return 0;
		}
		if (argc < 4) {
			remake_terminal();
			std::cout << "Invalid parameters. Try -help.\n";
			std::cout << "Got " << argc << " parameters.\n";
			for (int a = 0; a < argc; ++a) std::cout << "- '" << argv[a] << "'" << std::endl;
			message_timer(5);
			return 0;
		}

		const bool is_device_mic = (strcmp(argv[1], "OUT") != 0);
		const std::string device_search = argv[2];
		const std::bitset<6> flags = {
			(((uint32_t)(strchr(argv[3], 'M') != nullptr))     ) | // mute
			(((uint32_t)(strchr(argv[3], 'm') != nullptr)) << 1) | // unmute
			(((uint32_t)(strchr(argv[3], 'T') != nullptr)) << 2) | // toggle mute
			(((uint32_t)(strchr(argv[3], 'i') != nullptr)) << 3) | // increase vol
			(((uint32_t)(strchr(argv[3], 'd') != nullptr)) << 4) | // decrease vol
			(((uint32_t)(strchr(argv[3], 's') != nullptr)) << 5)   // set vol
		};
		const float device_change = (argc > 4 ? std::stof(argv[4]) : -1.0f);

#ifdef _DEBUG
		std::cout << "Parameters read:\n";
		std::cout << "- Mic? " << (is_device_mic ? "Yes" : "No") << "\n";
		std::cout << "- Search for? " << device_search << "\n";
		std::cout << "- Flags? " << flags << "\n";
		std::cout << "- Volume (optional)? " << device_change << "\n";
#endif

		DeviceList devl;
		Device dev = is_device_mic ?
			((device_search.empty() || device_search.find('*') == 0) ? devl.get_default_rec(AudioType::CONSOLE) : devl.get_rec(device_search)) :
			((device_search.empty() || device_search.find('*') == 0) ? devl.get_default_play(AudioType::CONSOLE) : devl.get_play(device_search));

#ifdef _DEBUG
		std::cout << "Device selected: " << dev.get_friendly_name() << std::endl;
#endif

		if (flags[2]) { // toggle mute
			dev.set_mute(!dev.get_mute());
		}
		else if (flags[0]) { // mute
			dev.set_mute(true);
		}
		else if (flags[1]) { // unmute
			dev.set_mute(false);
		}

		if (flags[5]) { // set volume
			if (device_change < 0.0f || device_change > 1.0f) {
				remake_terminal();
				std::cout << "Invalid volume\n";
				message_timer(5);
				return 0;
			}
			dev.set_volume(device_change);
		}
		else if (flags[3]) { // increase
			if (device_change < 0.0f || device_change > 1.0f) {
				remake_terminal();
				std::cout << "Invalid volume\n";
				message_timer(5);
				return 0;
			}
			const float sum = std::min(1.0f, device_change + dev.get_volume());
			dev.set_volume(sum);
		}
		else if (flags[4]) { // decrease
			if (device_change < 0.0f || device_change > 1.0f) {
				remake_terminal();
				std::cout << "Invalid volume\n";
				message_timer(5);
				return 0;
			}
			const float sum = std::max(0.0f, device_change + dev.get_volume());
			dev.set_volume(sum);
		}

#ifdef _DEBUG
		std::cout << "- Muted: " << (dev.get_mute() ? "Yes" : "No") << std::endl;
		std::cout << "- Volume: " << (dev.get_volume() * 100.0f) << "%" << std::endl;
#endif
	}
	catch (const std::exception& e) {
		remake_terminal();
		std::cout << "Exception: " << e.what() << std::endl;
		message_timer(5);
	}
	catch (...) {
		remake_terminal();
		std::cout << "Exception: UNHANDLED" << std::endl;
		message_timer(5);
	}

#ifdef _DEBUG
	std::cout << "\nThis is a debug build. Terminal will keep itself open for at least 30 seconds now.\n";
	message_timer(30);
#endif

	return 0;
}

bool remake_terminal()
{
	static bool result = false;
	if (result) return true;

	AllocConsole();
	FILE* fp;

	// Redirect STDIN if the console has an input handle
	if (GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE)
		if (freopen_s(&fp, "CONIN$", "r", stdin) != 0)
			result = false;
		else
			setvbuf(stdin, NULL, _IONBF, 0);

	// Redirect STDOUT if the console has an output handle
	if (GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE)
		if (freopen_s(&fp, "CONOUT$", "w", stdout) != 0)
			result = false;
		else
			setvbuf(stdout, NULL, _IONBF, 0);

	// Redirect STDERR if the console has an error handle
	if (GetStdHandle(STD_ERROR_HANDLE) != INVALID_HANDLE_VALUE)
		if (freopen_s(&fp, "CONOUT$", "w", stderr) != 0)
			result = false;
		else
			setvbuf(stderr, NULL, _IONBF, 0);

	// Make C++ standard streams point to console as well.
	std::ios::sync_with_stdio(true);

	// Clear the error state for each of the C++ standard streams.
	std::wcout.clear();
	std::cout.clear();
	std::wcerr.clear();
	std::cerr.clear();
	std::wcin.clear();
	std::cin.clear();

	result = true;

	return result;
}

void message_timer(const unsigned int t)
{
	std::cout << "\nClosing app in " << t << " second(s)..." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(t));
	return;
}