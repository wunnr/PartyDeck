#pragma once

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "shared.h"
#include "util.cpp"

Game setupGame(nlohmann::json& json){
    Game game;

    game.fancyname = Util::JsonValue(json, "fancyname", string(""));
    game.steam_appdir = Util::JsonValue(json, "steam_appdir", string(""));
    game.exec = Util::JsonValue(json, "exec", string(""));
    game.runtime = Util::JsonValue(json, "runtime", string(""));
    game.goldbergpath = Util::JsonValue(json, "goldbergpath", string(""));
    game.steam_appid = Util::JsonValue(json, "steam_appid", string(""));
    game.is_32bit = Util::JsonValue(json, "is_32bit", false);
    game.is_win = (game.runtime == "proton" || game.runtime == "wine");
    game.win_unique_appdata = Util::JsonValue(json, "win_unique_appdata", false);
    game.win_unique_documents = Util::JsonValue(json, "win_unique_documents", false);
    game.linux_unique_localshare = Util::JsonValue(json, "linux_unique_localshare", false);
    game.game_unique_paths = Util::JsonArray(json, "game_unique_paths");
    game.copy_paths = Util::JsonArray(json, "copy_paths");
    game.remove_paths = Util::JsonArray(json, "remove_paths");
    game.args = Util::JsonArray(json, "args");

    return game;
}

bool testGame(Game& game){
    if (game.name.empty() || !Util::StrIsAlnum(game.name)){
        LOG("[game] game.name field not found or invalid.");
        return false;
    }

    if (!game.steam_appdir.empty()){
        game.rootpath = string(PATH_STEAM + "/steamapps/common/" + game.steam_appdir);
    } else {
        // TODO: if game isn't a steam app, the client should ask the user somehow to locate the game's dir themselves
        // and save that information to a settings file somewhere
        return false;
    }

    if (game.exec.empty()){
        LOG("[game] no exec path!");
        return false;
    }

    // If game is proton-based, needs a valid appID to copy compat folders and such
    if ( game.runtime == "proton" && (game.steam_appid.empty() || !Util::StrIsNum(game.steam_appid)) ){
        LOG("[game] Game is proton-based but has no valid appid! Exiting...");
        return false;
    }

    return true;
}

bool createGameSym(Game& game){
    LOG("[game] Creating sym directory");
    if (Util::CopyDirRecursive(game.rootpath, PATH_SYM, true) != true){
        return false;
    }

    // If json file has goldbergpath set, copy goldberg files to the specified dir in the symlink path
    if (!game.goldbergpath.empty()){
        string src;
        if (game.is_32bit) src = PATH_EXECDIR + "/data/goldberg/32/";
        else src = PATH_EXECDIR + "/data/goldberg/64/";
        string dest = PATH_SYM + "/" + game.goldbergpath;

        if (Util::CopyDirRecursive(src, dest, false) != true){
            return false;
        }

        // Set up local goldberg save dir to bind the profile saves onto using bwrap
        string path_f_localsave = dest + "/local_save.txt";
        std::ofstream f_localsave(path_f_localsave.c_str());
        if (!f_localsave){
            LOG("[game] Couldn't open goldberg local_save.txt to edit.");
            return false;
        }
        f_localsave << "goldbergsave" << endl;
        f_localsave.close();

        string path_f_steam_appid = dest + "/steam_appid.txt";
        if (fs::exists(path_f_steam_appid)) fs::remove(path_f_steam_appid);
        std::ofstream f_steam_appid(path_f_steam_appid.c_str());
        if (!f_steam_appid){
            LOG("[game] Couldn't open goldberg steam_appid.txt to edit.");
            return false;
        }
        f_localsave << game.steam_appid << endl;
        f_localsave.close();

        // Create find_interfaces file from game's original steam dll, put it in sym folder
        string steam_dll;
        if (game.is_win) { steam_dll = (game.is_32bit) ? string("steam_api.dll") : string("steam_api64.dll"); }
        else { steam_dll = string("libsteam_api.so"); }
        string path_find_interfaces = PATH_EXECDIR + "/data/goldberg/find_interfaces.sh";
        src = game.rootpath + "/" + game.goldbergpath + "/" + steam_dll;
        dest = PATH_SYM + "/" + game.goldbergpath + "/steam_interfaces.txt";
        int ret = Util::Exec(path_find_interfaces + "\"" + src + "\" > \"" + dest + "\"");
        if (ret != 0) { LOG("Couldn't create goldberg steam_interfaces.txt"); return false; }
    }

    for (const auto& path : game.copy_paths){
        fs::path src = fs::path(game.rootpath) / path;
        fs::path dest = fs::path(PATH_SYM) / path;

        if (!fs::exists(src)) continue;
        try {
            if (fs::is_directory(src)){
                if (Util::CopyDirRecursive(src, dest, false) != true) return false;
            } else {
                // fs::copy_options::overwrite_existing doesn't seem to do its job, on my gcc at least
                Util::RemoveIfExists(dest);
                fs::copy_file(src, dest);
            }
        }
        catch (const fs::filesystem_error& e) {
            LOG("Filesystem error: " << e.what() << ". Src: " << src << ", Dest: " << dest);
            return false;
        }
    }

    for (const auto& path : game.remove_paths){
        fs::path dest = fs::path(PATH_SYM) / path;
        Util::RemoveIfExists(dest);
    }

    string copypath = PATH_EXECDIR + "/handlers/" + game.name + "/copy";
    if(fs::exists(copypath) && fs::is_directory(copypath)){
        if (Util::CopyDirRecursive(copypath, PATH_SYM, false) != true) return false;
    }

    return true;
}

bool createProtonCompatdata(Game& game){
    string src = PATH_STEAM + "/steamapps/compatdata/" + game.steam_appid;
    string dest = PATH_PARTY + "/compatdata";

    try {
        fs::copy(src, dest, fs::copy_options::recursive | fs::copy_options::copy_symlinks);
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return false;
    }
    //if (Util::copyDirRecursive(src, dest, false) != true) return false;

    string wine_patch_hidraw = "WINEPREFIX=\"" + dest + "/pfx" + "\" ";
    // TODO: Don't know if we can just call wine on a Steam Deck.
    // If not, replace this with a call to Proton/files/bin/wine
    wine_patch_hidraw += "wine reg add \"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Services\\winebus\" /v \"DisableHidraw\" /t REG_DWORD /d 1";
    LOG("Patching HIDraw from wine prefix for proper PlayStation controller support...");
    if (Util::Exec(wine_patch_hidraw) != 0){
        LOG("Failed patching HIDraw?");
        return false;
    }

    // Wine seems to take a while to "close" a prefix after applying a regedit.
    // Probably related to the time needed in between opening Proton instances
    sleep(5);
    return true;
}

bool writeGameLaunchScript(Game& game, std::vector<Player>& players){
    // Create the bash script to run all the games
    string path_f_run = PATH_PARTY + "/run.sh";
    std::ofstream f_run(path_f_run.c_str());
    if (!f_run){
        LOG("[game] Couldn't open run.sh to edit. Exiting...");
        return false;
    }

    f_run << "#!/bin/bash" << endl;
    f_run << "export SDL_JOYSTICK_HIDAPI=0" << endl;
    if (game.is_32bit){ // SDL2 COMPAT
        f_run << "export SDL_DYNAMIC_API=" << PATH_STEAM << "/ubuntu12_32/steam-runtime/usr/lib/i386-linux-gnu/libSDL2-2.0.so.0" << endl;
    } else { f_run << "export SDL_DYNAMIC_API=" << PATH_STEAM << "/ubuntu12_32/steam-runtime/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0" << endl; }
    if (game.runtime == "proton"){
        f_run << "export STEAM_COMPAT_CLIENT_INSTALL_PATH=\"" << PATH_STEAM << "\"" << endl;
        f_run << "export STEAM_COMPAT_DATA_PATH=\"" << PATH_PARTY << "/compatdata\"" << endl;
        f_run << "export SteamAppId=\"" << game.steam_appid << "\"" << endl;
        f_run << "export SteamGameId=\"" << game.steam_appid << "\"" << endl;
    }
    f_run << "cd \"" << PATH_SYM << "\"" << endl;

    for (int i = 0; i < players.size(); i++){
        string profilepath = PATH_PARTY + "/profiles/" + players[i].profile;

        // Wine/Proton doesn't seem to like opening multiple instances rapidly in succession
        if (game.is_win && i > 0) f_run << "sleep 5" << endl;

        f_run << endl << "# Player " << i << endl;
        f_run << "gamescope ";
        f_run << "-W 1280 -H 720 -b "; // TODO: DYNAMIC RESOLUTIONS
        // f_run << "--prefer-vk-device 1002:1900 ";
        f_run << "-- \\" << endl;

        f_run << "bwrap \\" << endl;
        f_run << "--dev-bind / / \\" << endl;

        // Bind user's "steam" folder onto the symlink path's "goldbergsave" folder for individual saves
        if (!game.goldbergpath.empty()){
            f_run << "--bind \"" << profilepath << "/steam\" ";
            f_run << "\"" << PATH_SYM << "/" << game.goldbergpath << "/goldbergsave\" \\" << endl;
        }
        f_run << "--tmpfs /tmp \\" << endl;

        // TODO: These only work with proton runtime, not wine (non-steam games)! Find some way to make this plat
        if (game.is_win && game.win_unique_appdata){
            f_run << "--bind \"" << profilepath << "/windata/AppData\" ";
            f_run << "\"" << PATH_PARTY << "/compatdata/pfx/drive_c/users/steamuser/AppData\" \\" << endl;
        }
        if (game.is_win && game.win_unique_documents){
            f_run << "--bind \"" << profilepath << "/windata/Documents\" ";
            f_run << "\"" << PATH_PARTY << "/compatdata/pfx/drive_c/users/steamuser/Documents\" \\" << endl;
        }

        for (const string& subdir : game.game_unique_paths){
            f_run << "--bind \"" << profilepath << "/" << game.name << "/" << subdir << "\" ";
            f_run << "\"" << PATH_SYM << "/" << subdir << "\" \\" << endl;
        }

        // idek if this works LOLLLL
        if (!game.is_win && game.linux_unique_localshare){
            f_run << "--bind \""<< PATH_PARTY << "/profiles/" << players[i].profile << "/share\" ";
            f_run << "\"" << PATH_LOCAL_SHARE << "\" \\" << endl;
        }

        for (auto& pad : GAMEPADS){ // Mask out all gamepad paths
            if (pad.path == players[i].pad.path) continue; // Except current player's pad path, of course
            f_run << "--bind /dev/null " << pad.path << " \\" << endl;
        }

        if (game.runtime == "scout"){
            f_run << "\"" << PATH_STEAM << "/ubuntu12_32/steam-runtime/run.sh\" ";
        } else if (game.runtime == "proton"){
            f_run << "\"" << PATH_STEAM << "/steamapps/common/Proton - Experimental/proton\" run ";
        } else if (game.runtime == "wine"){
            f_run << "wine ";
        }

        f_run << "\"" << PATH_SYM << "/" << game.exec << "\"";
        for (const string& arg : game.args){
            if (arg == "$PROFILENAME") f_run << " " << players[i].profile;
            else f_run << " " << arg;
        }

        if ((i + 1) < players.size()) f_run << " &";

        f_run << endl;
    }

    f_run.close();

    if (chmod(path_f_run.c_str(), S_IRWXU) != 0) {  // S_IRWXU gives the user read, write, execute permissions
        LOG("[party] Failed changing file permissions");
        return false;
    }

    return true;
}

std::vector<Game> scanGames(){
    std::vector<Game> out;
    strvector candidates;
    string dir_games = PATH_EXECDIR + "/handlers/";
    for (const auto& entry : fs::directory_iterator(dir_games)){
        if (fs::is_directory(entry) && fs::exists(entry.path() / "info.json")) {
            candidates.push_back(entry.path().filename().string());
        }
    }
    for (const string& candidate : candidates){
        string path_f_game = PATH_EXECDIR + "/handlers/" + candidate + "/info.json";
        std::ifstream f_game(path_f_game.c_str());
        if (!f_game.is_open()){
            LOG("[party] Couldn't open " << path_f_game);
            continue;
        }
        nlohmann::json j;
        f_game >> j;
        f_game.close();
        Game game = setupGame(j);
        game.name = candidate;
        out.push_back(game);
    }
    return out;
}

// Early work into refactoring the "game" struct as a class
class Handler{
private:
    string name;
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
public:
    const string fancyname;
    bool Test(){
        if (name.empty() || !Util::StrIsAlnum(name)){
            LOG("[game] game.name field not found or invalid.");
            return false;
        }

        if (!steam_appdir.empty()){
            rootpath = string(PATH_STEAM + "/steamapps/common/" + steam_appdir);
        } else {
            // TODO: if game isn't a steam app, the client should ask the user somehow to locate the game's dir themselves
            // and save that information to a settings file somewhere
            return false;
        }

        if (exec.empty()){
            LOG("[game] no exec path!");
            return false;
        }

        // If game is proton-based, needs a valid appID to copy compat folders and such
        if (runtime == "proton" && (steam_appid.empty() || !Util::StrIsNum(steam_appid)) ){
            LOG("[game] Game is proton-based but has no valid appid! Exiting...");
            return false;
        }

        return true;
    }
};
