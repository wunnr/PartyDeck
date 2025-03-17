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

typedef struct {
    string name;
    string fancyname;
    string steam_appdir;
    string steam_appid;
    string rootpath;
    string exec;
    string runtime;
    string goldbergpath;
    strvector args;
    strvector game_unique_paths;
    strvector copy_paths;
    strvector remove_paths;
    bool is_32bit;
    bool is_win;
    bool win_unique_appdata;
    bool win_unique_documents;
    bool linux_unique_localshare;
} Game;

typedef struct {
    libevdev* dev;
    string path;
} Gamepad;

typedef struct {
    Gamepad pad;
    int profilechoice;
    string profile;
} Player;

extern const string PATH_EXECDIR; // Folder where executable lives
extern const string PATH_LOCAL_SHARE; // $HOME/.local/share
extern const string PATH_PARTY; // PATH_DATA_HOME/partydeck
extern const string PATH_STEAM; // HOME/.local/share/Steam, or $STEAM_BASE_FOLDER
extern string PATH_SYM; // PARTY/game/[gamename]
extern std::vector<Gamepad> GAMEPADS;
extern std::vector<Player> PLAYERS;
extern nlohmann::json SETTINGS;
extern std::ofstream f_log;
