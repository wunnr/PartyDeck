#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <poll.h>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"
#include <libevdev-1.0/libevdev/libevdev.h>

#define LOG(text) do { std::cout << text << std::endl; f_log << text << std::endl; } while(0)

namespace fs = std::filesystem;
using std::cout;
using std::endl;
using string = std::string;
using strvector = std::vector<std::string>;
using path = fs::path;

typedef struct {
    libevdev* dev;
    string path;
} Gamepad;

typedef struct {
    Gamepad pad;
    int profilechoice;
    string profile;
} Player;

extern const path PATH_EXECDIR; // Folder where executable lives
extern const path PATH_LOCAL_SHARE; // $HOME/.local/share
extern const path PATH_PARTY; // PATH_DATA_HOME/partydeck
extern const path PATH_STEAM; // HOME/.local/share/Steam, or $STEAM_BASE_FOLDER
extern path PATH_SYM; // PARTY/game/[gamename]
extern std::vector<Gamepad> GAMEPADS;
extern std::vector<Player> PLAYERS;
extern nlohmann::json SETTINGS;
extern std::ofstream f_log;
