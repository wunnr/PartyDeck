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
    path path_profile = PATH_PARTY / "profiles" / name;
    const strvector subdirs = {
        "/windata/Documents",
        "/windata/AppData",
        "/share",
        "/steam/settings"
    };

    for (const string& dir : subdirs) {
        try {
            fs::create_directories(path_profile / dir);
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            return false;
        }
    }

    std::ofstream f_acc((path_profile / "steam/settings/account_name.txt").c_str());
    f_acc << name << endl;
    f_acc.close();

    std::ofstream f_lang((path_profile / "steam/settings/language.txt").c_str());
    f_lang << "english" << endl;
    f_lang.close();

    std::ofstream f_port((path_profile / "steam/settings/listen_port.txt").c_str());
    f_port << "47584" << endl;
    f_port.close();

    // Generate a random 17-digit number as fake SteamID for goldberg
    std::ofstream f_id((path_profile / "steam/settings/user_steam_id.txt").c_str());
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1, 2147483647);
    f_id << std::setw(17) << std::setfill('0') << dist(gen) << endl;
    f_id.close();

    return true;
}

// For player profile selection we want to include guest but for the view profiles menu we don't
strvector listAll(bool include_guest){
    strvector out;
    for (const auto& entry : fs::directory_iterator(PATH_PARTY / "profiles/")) {
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
        string guestpath = PATH_PARTY / "profiles" / name;
        Util::RemoveIfExists(guestpath);
    }
}

}
