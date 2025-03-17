#pragma once

#include <filesystem>
#include <fstream>
#include <random>

#include "shared.h"
#include "util.cpp"

namespace Profiles
{

const strvector guestnames = {"Player1", "Player2", "Player3", "Player4", "Player5", "Player6"};

bool create(string name) {
    string profilepath = PATH_PARTY + "/profiles/" + name;
    const strvector subdirs = {
        "/windata/Documents",
        "/windata/AppData",
        "/share",
        "/steam/settings"
    };

    for (const string& dir : subdirs) {
        try {
            fs::create_directories(profilepath + dir);
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            return false;
        }
    }

    std::ofstream f_acc(profilepath + "/steam/settings/account_name.txt");
    f_acc << name << endl;
    f_acc.close();

    std::ofstream f_lang(profilepath + "/steam/settings/language.txt");
    f_lang << "english" << endl;
    f_lang.close();

    std::ofstream f_port(profilepath + "/steam/settings/listen_port.txt");
    f_port << "47584" << endl;
    f_port.close();

    // Generate a random 17-digit number as fake SteamID for goldberg
    std::ofstream f_id(profilepath + "/steam/settings/user_steam_id.txt");
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1, 2147483647);
    f_id << std::setw(17) << std::setfill('0') << dist(gen) << endl;
    f_id.close();

    return true;
}

bool gameUniqueDirs(string name, Game game){
    fs::path profilepath = fs::path(PATH_PARTY) / "profiles" / name;
    for (const string& subdir : game.game_unique_paths){
        if (fs::exists(profilepath / game.name / subdir)) continue;
        try {
            fs::create_directories(profilepath / game.name / subdir);
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            return false;
        }
    }
    return true;
}

// For player profile selection we want to include guest but for the view profiles menu we don't
strvector listAll(bool include_guest){
    strvector out;
    for (const auto& entry : fs::directory_iterator(PATH_PARTY + "/profiles/")) {
        if (fs::is_directory(entry)) {
            out.push_back(entry.path().filename().string());
        }
    }
    std::sort(out.begin(), out.end());
    if (include_guest) { out.insert(out.begin(), "Guest"); }
    return out;
}

void removeGuests(){
    for (const string& name : guestnames) {
        string guestpath = PATH_PARTY + "/profiles/" + name;
        Util::RemoveIfExists(guestpath);
    }
}

}
