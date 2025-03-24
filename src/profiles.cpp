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
    path path_profile = PATH_PARTY/"profiles"/name;
    const std::vector<path> subdirs = {
        path_profile/"windata/Documents",
        path_profile/"windata/AppData",
        path_profile/"share",
        path_profile/"steam/settings"
    };

    for (const path& dir : subdirs) {
        try {
            fs::create_directories(dir);
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
    if (!fs::exists(PATH_PARTY/"profiles")) { fs::create_directories(PATH_PARTY/"profiles"); }
    for (const auto& entry : fs::directory_iterator(PATH_PARTY/"profiles/")) {
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

bool alreadyInUse(int p, int num){
    if (num == 0) { return false; } // 0 is guest, so allow multiple players to be 0
    for (int i = 0; i < PLAYERS.size(); i++){
        if (i == p) { continue; }
        if (PLAYERS[i].profilechoice == num) { return true; }
    }
    return false;
}

enum CycleDirection { Next, Prev };
// Cycles a player (PLAYERS[p]) through the list of available profiles, forward or backwards depending on enum
void ChoiceCycle(int p, CycleDirection dir, int max){
    LOG("max is: " << max);
    LOG("choice was: " << PLAYERS[p].profilechoice);
    while (true){
        // ++ or -- depending on direction
        if (dir == Next) { LOG("++"); PLAYERS[p].profilechoice++; }
        else if (dir == Prev) { LOG("--"); PLAYERS[p].profilechoice--; }
        LOG("choice is: " << PLAYERS[p].profilechoice);
        // Wrap around if choice goes outside the range [ 0 to (max-1) ]
        if (PLAYERS[p].profilechoice >= max) { LOG("wrapped to 0"); PLAYERS[p].profilechoice = 0; }
        else if (PLAYERS[p].profilechoice < 0) { LOG("wrapped to end"); PLAYERS[p].profilechoice = (max - 1); }

        // Finally, check if the spot is open. If not, keep cycling.
        if (!alreadyInUse(p, PLAYERS[p].profilechoice)) { break; }
        LOG(PLAYERS[p].profilechoice << "already in use!");
    }
    LOG("choice is now: " << PLAYERS[p].profilechoice);
    return;
}

}
