#include "DKUtil/Config.hpp"
#include "DKUtil/Hook.hpp"
#include "DKUtil/Logger.hpp"
#include "SFSE/SFSE.h"
#include "fmt/format.h"  // Ensure fmt is included

// For MCI
#include <Mmsystem.h>
#include <mciapi.h>
#pragma comment(lib, "Winmm.lib")

// Formatting, string and console
#include <codecvt>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

static bool gIsInitialized = false;

// For type aliases
using namespace DKUtil::Alias;
std::wstring to_wstring(const std::string& stringToConvert)
{
	std::wstring wideString =
		std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(stringToConvert);
	return wideString;
}

// Function to trim trailing commas and spaces
std::string trimTrailingCommas(const std::string& str) {
    std::string result = str;
    // Remove trailing spaces
    result.erase(result.find_last_not_of(" \t") + 1);
    // Remove trailing commas
    if (!result.empty() && result.back() == ',') {
        result.pop_back();
    }
    return result;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

// Function to trim all items in the playlist
void trimPlaylist(std::vector<std::string>& playlist) {
    for (auto& song : playlist) {
        song = trimTrailingCommas(song);
    }
}

int hexStringToInt(const std::string& hexStr) {
    int value = 0;
    std::stringstream ss;
    ss << std::hex << hexStr;  // Read the hex string
    ss >> value;               // Convert to integer
    return value;
}

void ConsoleExecute(std::string command)
{
	static REL::Relocation<void**>                       BGSScaleFormManager{ REL::ID(879512) };
	static REL::Relocation<void (*)(void*, const char*)> ExecuteCommand{ REL::ID(166307) };
	ExecuteCommand(*BGSScaleFormManager, command.data());
}

void Notification(std::string Message)
{
	std::string Command = fmt::format("cgf \"Debug.Notification\" \"{}\"", Message);
	ConsoleExecute(Command);
}

// Structure to hold the configuration data
struct Config {
    bool autoStartRadio = true;
    bool randomizeStartTime = true;
    std::vector<std::string> playlist;
    int toggleRadioKey = 0x60;
    int switchModeKey = 0x6D;
    int volumeUpKey = 0x69;
    int volumeDownKey = 0x66;
    int nextStationKey = 0x68;
    int previousStationKey = 0x67;
    int seekForwardKey = 0x6A;
    int seekBackwardKey = 0x6F;
};

// Function to load the configuration from the TOML file
void loadConfig(Config& config) {
    std::ifstream configFile(".\\Data\\SFSE\\Plugins\\StarfieldGalacticRadio.toml");
    if (!configFile.is_open()) {
        INFO("Could not open configuration file!");
        return;
    }

    std::string line;
    while (std::getline(configFile, line)) {
        std::istringstream lineStream(line);
        std::string key;
		line = trim(line);

        if (line.find("AutoStartRadio") != std::string::npos) {
            bool value;
            lineStream >> key >> value;
            config.autoStartRadio = value;
            INFO("AutoStartRadio: {}", config.autoStartRadio);
        } else if (line.find("RandomizeStartTime") != std::string::npos) {
            bool value;
            lineStream >> key >> value;
            config.randomizeStartTime = value;
            INFO("RandomizeStartTime: {}", config.randomizeStartTime);
        } else if (line.find("Playlist =") != std::string::npos) {
			config.playlist.clear();  // Clear any existing entries

			// Now read each playlist item until we find a line containing ']'
			while (std::getline(configFile, line)) {
				line = trim(line);  // Trim whitespace from line

				size_t closingBracketPos = line.find("]");
				if (closingBracketPos != std::string::npos) {
					// If ']' is found, take everything before it and break the loop
					line = line.substr(0, closingBracketPos);
				}

				if (!line.empty()) {
					line.erase(std::remove(line.begin(), line.end(), '"'), line.end()); // Remove quotes
					config.playlist.push_back(line);
					// INFO("Playlist item: {}", line);
				}

				// Break if we found ']' on this line
				if (closingBracketPos != std::string::npos) {
					break;
				}
			}
		} else if (line.find("ToggleRadioKey=") != std::string::npos) {
			config.toggleRadioKey = hexStringToInt(line.substr(line.find('=') + 1));
		} else if (line.find("SwitchModeKey=") != std::string::npos) {
			config.switchModeKey = hexStringToInt(line.substr(line.find('=') + 1));
		} else if (line.find("VolumeUpKey=") != std::string::npos) {
			config.volumeUpKey = hexStringToInt(line.substr(line.find('=') + 1));
		} else if (line.find("VolumeDownKey=") != std::string::npos) {
			config.volumeDownKey = hexStringToInt(line.substr(line.find('=') + 1));
		} else if (line.find("NextStationKey=") != std::string::npos) {
			config.nextStationKey = hexStringToInt(line.substr(line.find('=') + 1));
		} else if (line.find("PreviousStationKey=") != std::string::npos) {
			config.previousStationKey = hexStringToInt(line.substr(line.find('=') + 1));
		} else if (line.find("SeekForwardKey=") != std::string::npos) {
			config.seekForwardKey = hexStringToInt(line.substr(line.find('=') + 1));
		} else if (line.find("SeekBackwardKey=") != std::string::npos) {
			config.seekBackwardKey = hexStringToInt(line.substr(line.find('=') + 1));
		}
    }
}

// Function to print the loaded configuration
void printConfig(const Config& config) {
	
    INFO("{} - AutoStartRadio: {}", Plugin::NAME, config.autoStartRadio);
    INFO("{} - RandomizeStartTime: {}", Plugin::NAME, config.randomizeStartTime);
    INFO("{} - Playlist:", Plugin::NAME);
    for (const auto& song : config.playlist) {
        INFO("{} playlist item - {}", Plugin::NAME, song);
    }
	INFO("{} - ToggleRadioKey: 0x{:X}", Plugin::NAME, config.toggleRadioKey);
	INFO("{} - SwitchModeKey: 0x{:X}", Plugin::NAME, config.switchModeKey);
	INFO("{} - VolumeUpKey: 0x{:X}", Plugin::NAME, config.volumeUpKey);
	INFO("{} - VolumeDownKey: 0x{:X}", Plugin::NAME, config.volumeDownKey);
	INFO("{} - NextStationKey: 0x{:X}", Plugin::NAME, config.nextStationKey);
	INFO("{} - PreviousStationKey: 0x{:X}", Plugin::NAME, config.previousStationKey);
	INFO("{} - SeekForwardKey: 0x{:X}", Plugin::NAME, config.seekForwardKey);
	INFO("{} - SeekBackwardKey: 0x{:X}", Plugin::NAME, config.seekBackwardKey);
}


class RadioPlayer
{
public:
	RadioPlayer(const std::vector<std::string>& InStations, bool InAutoStart, bool InRandomizeStartTime) :
		AutoStart(InAutoStart),
		RandomizeStartTime(InRandomizeStartTime),
		Stations(InStations)
	{
	}

	~RadioPlayer()
	{
	}

	void Init()
	{
		srand(time(NULL));
		randStart = rand() % (3600);
		RadioStartTime = std::time(nullptr);

		INFO("{} v{} - Initializing Starfield Radio Sound System -", Plugin::NAME, Plugin::Version);

		if (Stations.size() <= 0) {
			INFO("{} v{} - No Stations Found, Starfield Radio Shutting Down -", Plugin::NAME, Plugin::Version);
			return;
		}

		INFO("{} v{} - Starting Starfield Radio -", Plugin::NAME, Plugin::Version);

		if (!Stations.empty()) {
			INFO("{} v{} - {} Stations Found, Starfield Radio operational -", Plugin::NAME, Plugin::Version, Stations.size());
			StationIndex = rand() % Stations.size();
			if (StationIndex >= Stations.size()) {
				INFO("{} - Invalid StationIndex: {}, unable to select station.", Plugin::NAME, StationIndex);
				return;
			}
		}

		std::pair<std::string, std::string> StationInfo = GetStationInfo(Stations[StationIndex]);

		if (!StationInfo.second.empty()) {
			INFO("{} - Attempt to load file - {}", Plugin::NAME, StationInfo.second);
			std::wstring OpenLocalFile;
			if (StationInfo.second.find("://") != std::string::npos) {
				OpenLocalFile = to_wstring(std::format("open {} type mpegvideo alias sfradio", StationInfo.second));
			} else {
				OpenLocalFile = to_wstring(std::format("open \".\\Data\\SFSE\\Plugins\\StarfieldGalacticRadio\\tracks\\{}\" type mpegvideo alias sfradio", StationInfo.second));
			}
			int result = mciSendString(OpenLocalFile.c_str(), NULL, 0, NULL);
			if (result != 0) {
				INFO("{} - mciSendString failed with code: {}", Plugin::NAME, result);
				return;
			}
		} else {
			// Лог или сообщение об ошибке
			INFO("{} v{} - Station path is empty, unable to start playback -", Plugin::NAME, Plugin::Version);
		}

		if (!StationInfo.first.empty())
			Notification(std::format("On Air - {}", StationInfo.first));

		INFO("{} v{} - Selected Station {}, AutoStart: {}, Mode: {} -", Plugin::NAME, Plugin::Version, Stations[StationIndex], AutoStart, Mode);

		if (AutoStart && Mode == 0) {
			IsStarted = true;
			//mciSendString(L"play sfradio repeat", NULL, 0, NULL);
			int trackLength = getTrackLength();
			INFO("{} - Track length: {}", Plugin::NAME, trackLength);

			// Проверка корректности значения trackLength
			if (trackLength > 0) {
				mciSendString(to_wstring(std::format("play sfradio from {} repeat", (randStart % trackLength))).c_str(), NULL, 0, NULL);
			} else {
				// Лог или сообщение об ошибке для отладки
				INFO("{} - Invalid track length: {}, playback cannot start", Plugin::NAME, trackLength);
			}
			//Notification(std::format("当前播放进度：{}%%", std::floor((static_cast<float>(randStart % getTrackLength()) * 100 / getTrackLength()) * 10) / 10.0f));
			Notification(std::format("Play at: {}%%", std::floor((static_cast<float>(randStart % getTrackLength())*100 / getTrackLength()) * 10) / 10.0f));
			mciSendString(L"setaudio sfradio volume to 0", NULL, 0, NULL);
		}

		Notification("银河电台初始化完成");
		//Notification("Starfield Radio Initialized");
	}

	int32_t getElapsedTimeInSec()
	{
		std::time_t currentTime = std::time(nullptr);
		return static_cast<int>(currentTime - RadioStartTime);
	}

	int32_t getTrackLength()
	{
		std::wstring StatusBuffer;
		StatusBuffer.reserve(128);

		mciSendString(L"status sfradio length", StatusBuffer.data(), 128, NULL);

		// Convert Length to int
		int32_t TrackLength = std::stoi(StatusBuffer);
		StatusBuffer.clear();
		return TrackLength;
	}

	void SelectStation(int InStationIndex)
	{
		// Index out of bounds.
		if (Stations.size() <= InStationIndex)
			return;

		std::pair<std::string, std::string> StationInfo = GetStationInfo(Stations[InStationIndex]);

		const std::string& StationName = StationInfo.first;
		const std::string& StationURL = StationInfo.second;

		mciSendString(L"close sfradio", NULL, 0, NULL);

		if (StationURL.contains("://")) {
			std::wstring OpenLocalFile = to_wstring(std::format("open {} type mpegvideo alias sfradio", StationURL));
			int result = mciSendString(OpenLocalFile.c_str(), NULL, 0, NULL);
			if (result != 0) {
				INFO("{} - mciSendString failed with code: {}", Plugin::NAME, result);
				return;
			}
			//Notification("正在连接至银河电台网络。由于跨星际传输，通讯可能存在延迟，请稍等。");
			Notification("Connecting to Galactic Radio Network. Delay expected because of the inter-stellar communication.");
		} else {
			std::wstring OpenLocalFile = to_wstring(std::format("open \".\\Data\\SFSE\\Plugins\\StarfieldGalacticRadio\\tracks\\{}\" type mpegvideo alias sfradio", StationURL));
			//Notification("在当前设备上检测到本地媒体文件，现在进行播放。");
			Notification("Local media on your device found, playing right now.");
			INFO("{} - Attempt to load file - {}", Plugin::NAME, StationURL);
			int result = mciSendString(OpenLocalFile.c_str(), NULL, 0, NULL);
			if (result != 0) {
				INFO("{} - mciSendString failed with code: {}", Plugin::NAME, result);
				return;
			}
		}

		int32_t NewPosition = (getElapsedTimeInSec() % getTrackLength() + randStart) * 1000 % getTrackLength();

		if (!StationInfo.first.empty())
			Notification(std::format("On Air - {}", StationInfo.first));

		// mciSendString(L"play sfradio repeat", NULL, 0, NULL);
		//Notification(std::format("当前播放进度：{}%%", std::floor((static_cast<float>(NewPosition % getTrackLength()) * 100 / getTrackLength()) * 10) / 10.0f));
		Notification(std::format("Play at: {}%%", std::floor((static_cast<float>(NewPosition % getTrackLength()) * 100 / getTrackLength()) * 10) / 10.0f));
		mciSendString(to_wstring(std::format("play sfradio from {} repeat", NewPosition)).c_str(), NULL, 0, NULL);

		std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)Volume));
		mciSendString(v.c_str(), NULL, 0, NULL);
	}

	void NextStation()
	{
		StationIndex = (StationIndex + 1) % Stations.size();

		SelectStation(StationIndex);
	}

	void PrevStation()
	{
		StationIndex = (StationIndex - 1);
		if (StationIndex < 0)
			StationIndex = Stations.size() - 1;
		SelectStation(StationIndex);
	}

	void SetVolume(float InVolume)
	{
		Volume = InVolume;
		std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)InVolume));
		mciSendString(v.c_str(), NULL, 0, NULL);
	}

	void DecreaseVolume()
	{
		Volume = Volume - 25.0f;
		std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)Volume));
		mciSendString(v.c_str(), NULL, 0, NULL);

		Notification(std::format("Volume {}", Volume));
	}

	void IncreaseVolume()
	{
		Volume = Volume + 25.0f;
		std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)Volume));
		mciSendString(v.c_str(), NULL, 0, NULL);

		Notification(std::format("Volume {}", Volume));
	}

	uint32_t GetTrackLength()
	{
		std::wstring StatusBuffer;
		StatusBuffer.reserve(128);

		mciSendString(L"status sfradio length", StatusBuffer.data(), 128, NULL);

		// Convert Length to int
		uint32_t TrackLength = std::stoi(StatusBuffer);

		return TrackLength;
	}

	void Seek(int32_t InSeconds)
	{
		// Get current position from MCI with mciSendString
		std::wstring StatusBuffer;
		StatusBuffer.reserve(128);

		mciSendString(L"status sfradio length", StatusBuffer.data(), 128, NULL);

		// Convert Length to int
		int32_t TrackLength = std::stoi(StatusBuffer);
		StatusBuffer.clear();
		StatusBuffer.reserve(128);
		mciSendString(L"status sfradio position", StatusBuffer.data(), 128, NULL);

		int32_t Position = 0;

		if (StatusBuffer.contains(L":")) {
			std::tm             t;
			std::wistringstream ss(StatusBuffer);
			ss >> std::get_time(&t, L"%H:%M:%S");
			Position = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
		} else {
			Position = std::stoi(StatusBuffer);
		}

		int32_t NewPosition = Position + (InSeconds * 1000);

		if (NewPosition >= TrackLength)
			NewPosition = TrackLength - 1;

		mciSendString(to_wstring(std::format("play sfradio from {} repeat", (int)NewPosition)).c_str(), NULL, 0, NULL);
	}

	void TogglePlayer()
	{
		ENABLE_DEBUG

		IsPlaying = !IsPlaying;
		if (!IsStarted && IsPlaying) {
			IsStarted = true;
			mciSendString(L"play sfradio repeat", NULL, 0, NULL);

			if (RandomizeStartTime) {
				uint32_t TrackLength = GetTrackLength();
				srand(time(NULL));
				uint32_t RandomTime = rand() % TrackLength;

				Seek(RandomTime);
			}
		}

		std::pair<std::string, std::string> StationInfo = GetStationInfo(Stations[StationIndex]);

		if (IsPlaying) {
			if (!StationInfo.first.empty())
				Notification(std::format("On Air - {}", StationInfo.first));
			else
				Notification("Radio On");
		} else
			Notification("Radio Off");

		if (Mode == 0) {
			std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)IsPlaying ? Volume : 0.0f));
			mciSendString(v.c_str(), NULL, 0, NULL);
		} else {
			if (!IsPlaying) {
				mciSendString(L"stop sfradio", NULL, 0, NULL);
			} else {
				mciSendString(L"play sfradio repeat", NULL, 0, NULL);
				if (RandomizeStartTime) {
					uint32_t TrackLength = GetTrackLength();
					srand(time(NULL));
					uint32_t RandomTime = rand() % TrackLength;

					Seek(RandomTime);
				}
				std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)Volume));
				mciSendString(v.c_str(), NULL, 0, NULL);
			}
		}
	}

	// 0 = Radio, 1 = Podcast
	void ToggleMode()
	{
		Mode = ~Mode;

		// Handle Podcast vs Radio mode.
		// Podcast mode will actually stop the stream, while Radio mode just mutes it so that time passes when not listened to.
	}

	std::pair<std::string, std::string> GetStationInfo(const std::string& StationConfig)
	{
		std::string Station = StationConfig;
		size_t      Separator = Station.find("|");

		std::string StationName = Separator != std::string::npos ? Station.substr(0, Station.find("|")) : "";
		std::string StationURL = Separator != std::string::npos ? Station.substr(Station.find("|") + 1) : Station;

		return std::make_pair(StationName, StationURL);
	}

private:
	int   Mode = 0;
	int   StationIndex = 0;
	float Volume = 700.0f;
	float Seconds = 0.0f;
	bool  RandomizeStartTime = false;
	bool  AutoStart = true;
	bool  IsStarted = false;
	bool  IsPlaying = false;

	std::vector<std::string> Stations;
	std::time_t              RadioStartTime;
	int32_t                  randStart;
};

const int    TimePerFrame = 50;
static DWORD MainLoop(void* unused)
{
	(void)unused;

	ENABLE_DEBUG

	DEBUG("Input Loop Starting");
	
	std::string configFilename = "StarfieldGalacticRadio.toml";

    Config config; // Create a Config instance
    loadConfig(config); // Load configuration from file
	
	trimPlaylist(config.playlist);
	// printConfig(config);


	DEBUG("Loaded config, waiting for player form...");
	while (!RE::TESForm::LookupByID(0x14))
		Sleep(1000);

	DEBUG("Pre-Initialize RadioPlayer.");

	RadioPlayer Radio(config.playlist, config.autoStartRadio, config.randomizeStartTime);
	Radio.Init();

	DEBUG("Post-Initialize RadioPlayer.")

	bool ToggleRadioHoldFlag = false;
	bool SwitchModeHoldFlag = false;
	bool VolumeUpHoldFlag = false;
	bool VolumeDownHoldFlag = false;
	bool NextStationHoldFlag = false;
	bool PrevStationHoldFlag = false;
	bool SeekForwardHoldFlag = false;
	bool SeekBackwardHoldFlag = false;

	for (;;) {
		// Get current key states
		short ToggleRadioKeyState = GetKeyState(config.toggleRadioKey);
		short SwitchModeKeyState = GetKeyState(config.switchModeKey);
		short VolumeUpKeyState = GetKeyState(config.volumeUpKey);
		short VolumeDownKeyState = GetKeyState(config.volumeDownKey);
		short NextStationKeyState = GetKeyState(config.nextStationKey);
		short PrevStationKeyState = GetKeyState(config.previousStationKey);
		short SeekForwardKeyState = GetKeyState(config.seekForwardKey);
		short SeekBackwardKeyState = GetKeyState(config.seekBackwardKey);
		
		// INFO("ToggleRadioKeyState: {}\nSwitchModeKeyState: {}\nVolumeUpKeyState: {}\nVolumeDownKeyState: {}\nNextStationKeyState: {}\nPrevStationKeyState: {}\nSeekForwardKeyState: {}\nSeekBackwardKeyState: {}", ToggleRadioKeyState, SwitchModeKeyState, VolumeUpKeyState, VolumeDownKeyState, NextStationKeyState, PrevStationKeyState, SeekForwardKeyState, SeekBackwardKeyState);

		// Handle toggle logic based on key states
		if (ToggleRadioKeyState < 0) { // Key is pressed
			if (!ToggleRadioHoldFlag) {
				ToggleRadioHoldFlag = 1; // Set the hold flag
				Radio.TogglePlayer(); // Perform action
			}
		} else {
			ToggleRadioHoldFlag = 0; // Reset hold flag when key is released
		}

		if (SwitchModeKeyState < 0) {
			if (!SwitchModeHoldFlag) {
				SwitchModeHoldFlag = 1;
				Radio.ToggleMode();
			}
		} else {
			SwitchModeHoldFlag = 0;
		}

		if (VolumeUpKeyState < 0) {
			if (!VolumeUpHoldFlag) {
				VolumeUpHoldFlag = 1;
				Radio.IncreaseVolume();
			}
		} else {
			VolumeUpHoldFlag = 0;
		}

		if (VolumeDownKeyState < 0) {
			if (!VolumeDownHoldFlag) {
				VolumeDownHoldFlag = 1;
				Radio.DecreaseVolume();
			}
		} else {
			VolumeDownHoldFlag = 0;
		}

		if (NextStationKeyState < 0) {
			if (!NextStationHoldFlag) {
				NextStationHoldFlag = 1;
				Radio.NextStation();
			}
		} else {
			NextStationHoldFlag = 0;
		}

		if (PrevStationKeyState < 0) {
			if (!PrevStationHoldFlag) {
				PrevStationHoldFlag = 1;
				Radio.PrevStation();
			}
		} else {
			PrevStationHoldFlag = 0;
		}

		if (SeekForwardKeyState < 0) {
			if (!SeekForwardHoldFlag) {
				SeekForwardHoldFlag = 1;
				Radio.Seek(10);
			}
		} else {
			SeekForwardHoldFlag = 0;
		}

		if (SeekBackwardKeyState < 0) {
			if (!SeekBackwardHoldFlag) {
				SeekBackwardHoldFlag = 1;
				Radio.Seek(-10);
			}
		} else {
			SeekBackwardHoldFlag = 0;
		}

		// Delay to control frame rate
		Sleep(TimePerFrame);
	}

	return 0;
}

class OpenCloseSink final :
	public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	static OpenCloseSink* GetSingleton()
	{
		static OpenCloseSink self;
		return std::addressof(self);
	}

	RE::BSEventNotifyControl ProcessEvent(RE::MenuOpenCloseEvent const& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource)
	{
		if (a_event.menuName == "HUDMenu" && a_event.opening && !gIsInitialized) {
			INFO("Creating Input Thread")
			CreateThread(NULL, 0, MainLoop, NULL, 0, NULL);

			gIsInitialized = true;
		}

		return RE::BSEventNotifyControl::kContinue;
	}
};

DLLEXPORT constinit auto SFSEPlugin_Version = []() noexcept {
	SFSE::PluginVersionData data{};

	data.PluginVersion(Plugin::Version);
	data.PluginName(Plugin::NAME);
	data.AuthorName(Plugin::AUTHOR);
	//data.UsesSigScanning(true);
	data.UsesAddressLibrary(true);
	//data.HasNoStructUse(true);
	data.IsLayoutDependent(true);
	data.CompatibleVersions({ SFSE::RUNTIME_LATEST });

	return data;
}();

namespace
{
	void MessageCallback(SFSE::MessagingInterface::Message* a_msg) noexcept
	{
		switch (a_msg->type) {
		case SFSE::MessagingInterface::kPostLoad:
			{
				if (const auto ui = RE::UI::GetSingleton(); ui) {
					ui->RegisterSink<RE::MenuOpenCloseEvent>(OpenCloseSink::GetSingleton());
				}

				break;
			}
		default:
			break;
		}
	}
}

/**
// for preload plugins
void SFSEPlugin_Preload(SFSE::LoadInterface* a_sfse);
/**/

DLLEXPORT bool SFSEAPI SFSEPlugin_Load(const SFSE::LoadInterface* a_sfse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) {
		Sleep(100);
	}
#endif

	SFSE::Init(a_sfse, false);

	DKUtil::Logger::Init(Plugin::NAME, std::to_string(Plugin::Version));

	INFO("{} v{} loaded", Plugin::NAME, Plugin::Version);

	// do stuff
	SFSE::AllocTrampoline(1 << 10);

	SFSE::GetMessagingInterface()->RegisterListener(MessageCallback);

	return true;
}

