#pragma once

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "shared.h"
#include "util.cpp"

class Handler{
private:
    bool valid = false;
    string uid;
    fs::path path_handler;
    fs::path path_gameroot;
    fs::path path_goldberg;
    string fancyname;
    string steam_appdir;
    string steam_appid;
    string exec;
    string runtime;
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
    Handler(string name){
        path_handler = PATH_EXECDIR / "handlers" / name;
        path path_handler_json = path_handler / "info.json";
        std::ifstream f_game(path_handler_json.c_str());
        if (!f_game.is_open()){
            LOG("[party] Couldn't open " << path_handler_json);
            valid = false;
            return;
        }
        nlohmann::json json;
        f_game >> json;
        uid = name;
        fancyname = Util::JsonValue(json, "fancyname", string(""));
        steam_appdir = Util::JsonValue(json, "steam_appdir", string(""));
        exec = Util::JsonValue(json, "exec", string(""));
        runtime = Util::JsonValue(json, "runtime", string(""));
        path_goldberg = Util::JsonValue(json, "path_goldberg", string(""));
        steam_appid = Util::JsonValue(json, "steam_appid", string(""));
        is_32bit = Util::JsonValue(json, "is_32bit", false);
        is_win = (runtime == "proton" || runtime == "wine");
        win_unique_appdata = Util::JsonValue(json, "win_unique_appdata", false);
        win_unique_documents = Util::JsonValue(json, "win_unique_documents", false);
        linux_unique_localshare = Util::JsonValue(json, "linux_unique_localshare", false);
        game_unique_paths = Util::JsonArray<string>(json, "game_unique_paths");
        copy_paths = Util::JsonArray<string>(json, "copy_paths");
        remove_paths = Util::JsonArray<string>(json, "remove_paths");
        args = Util::JsonArray<string>(json, "args");
        valid = true;
    }

    string Name(){
        return (fancyname.empty()) ? uid : fancyname;
    }

    bool Valid(){
        return valid;
    }

    bool Test(){
        if (!steam_appdir.empty()){
            path_gameroot = PATH_STEAM / "steamapps/common" / steam_appdir;
        } else {
            // TODO: if game isn't a steam app, the client should ask the user somehow to locate the game's dir themselves
            // and save that information to a settings file somewhere
            return false;
        }

        if (exec.empty()){
            LOG("[Handler] no exec path!");
            return false;
        }

        // If game is proton-based, needs a valid appID to copy compat folders and such
        if (runtime == "proton" && (steam_appid.empty() || !Util::StrIsNum(steam_appid)) ){
            LOG("[Handler] Game is proton-based but has no valid appid! Exiting...");
            return false;
        }

        return true;
    }

    bool CreateGameSymlinkDir(){
        PATH_SYM = PATH_PARTY / "game" / uid;
        if (!fs::exists(PATH_SYM)){
            LOG("[Handler] Game sym directory already found, skipping");
            return true;
        }
        LOG("[Handler] Creating sym directory");
        if (Util::CopyDirRecursive(path_gameroot, PATH_SYM, true) != true){
            return false;
        }

        // If json file has goldbergpath set, copy goldberg files to the specified dir in the symlink path
        if (path_goldberg.empty()){
            path src;
            if (is_32bit) src = path(PATH_EXECDIR) / "data/goldberg/32/";
            else src = fs::path(PATH_EXECDIR) / "data/goldberg/64/";
            path dest = PATH_SYM / path_goldberg;

            if (Util::CopyDirRecursive(src, dest, false) == false){ return false; }

            // Set up local goldberg save dir to bind the profile saves onto using bwrap
            path path_f_localsave = dest / "local_save.txt";
            std::ofstream f_localsave(path_f_localsave.c_str());
            if (!f_localsave){
                LOG("[Handler] Couldn't open goldberg local_save.txt to edit.");
                return false;
            }
            f_localsave << "goldbergsave" << endl;
            f_localsave.close();

            path path_f_steam_appid = dest / "steam_appid.txt";
            if (fs::exists(path_f_steam_appid)) fs::remove(path_f_steam_appid);
            std::ofstream f_steam_appid(path_f_steam_appid.c_str());
            if (!f_steam_appid){
                LOG("[Handler] Couldn't open goldberg steam_appid.txt to edit.");
                return false;
            }
            f_steam_appid << steam_appid << endl;
            f_steam_appid.close();

            // Create find_interfaces file from game's original steam dll, put it in sym folder
            string steam_dll;
            if (is_win) { steam_dll = (is_32bit) ? string("steam_api.dll") : string("steam_api64.dll"); }
            else { steam_dll = string("libsteam_api.so"); }
            path path_find_interfaces = PATH_EXECDIR / "data/goldberg/find_interfaces.sh";
            src = path_gameroot / path_goldberg / steam_dll;
            dest = PATH_SYM / path_goldberg / "steam_interfaces.txt";
            int ret = Util::Exec(path_find_interfaces.string() + " \"" + src.string() + "\" > \"" + dest.string() + "\"");
            if (ret != 0) { LOG("Couldn't create goldberg steam_interfaces.txt"); return false; }
        }

        for (const auto& path : copy_paths){
            fs::path src = path_gameroot / path;
            fs::path dest = PATH_SYM / path;

            if (!fs::exists(src)) continue;
            try {
                if (fs::is_directory(src)){
                    if (Util::CopyDirRecursive(src, dest, false) == false) return false;
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

        for (const auto& path : remove_paths){
            fs::path dest = PATH_SYM / path;
            Util::RemoveIfExists(dest);
        }

        string copypath = PATH_EXECDIR / "handlers" / uid / "copy";
        if(fs::exists(copypath) && fs::is_directory(copypath)){
            if (Util::CopyDirRecursive(copypath, PATH_SYM, false) != true) return false;
        }

        return true;
    }

    bool CreateCompatdataDir(){
        path src = PATH_STEAM / "steamapps/compatdata" / steam_appid;
        path dest = PATH_PARTY / "compatdata";
        if (!fs::exists(dest)){
            LOG("[Handler] Compatdata already found, skipping");
            return true;
        }

        try {
            fs::copy(src, dest, fs::copy_options::recursive | fs::copy_options::copy_symlinks);
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            return false;
        }

        string wine_patch_hidraw = "WINEPREFIX=\"" + dest.string() + "/pfx" + "\" ";
        // TODO: Don't know if we can just call wine on a Steam Deck.
        // If not, replace this with a call to Proton/files/bin/wine
        wine_patch_hidraw += "wine reg add \"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Services\\winebus\" /v \"DisableHidraw\" /t REG_DWORD /d 1";
        LOG("Patching HIDraw from wine prefix for proper PlayStation controller support...");
        if (Util::Exec(wine_patch_hidraw) != 0){
            LOG("Failed to patch HIDraw from Wine prefix");
            return false;
        }

        // Wine seems to take a while to "close" a prefix after applying a regedit.
        // Probably related to the time needed in between opening Proton instances
        sleep(5);
        return true;
    }

    bool CreateProfileUniqueDirs(std::vector<Player>& players){
        for (const auto& p : players){
            string name = p.profile;
            fs::path profilepath = PATH_PARTY / "profiles" / name;
            for (const string& subdir : game_unique_paths){
                if (fs::exists(profilepath / uid / subdir)) continue;
                try {
                    fs::create_directories(profilepath / uid / subdir);
                }
                catch (const fs::filesystem_error& e) {
                    std::cerr << "Filesystem error: " << e.what() << std::endl;
                    return false;
                }
            }
        }
        return true;
    }

    bool WriteGameLaunchScript(std::vector<Player>& players){
        // Create the bash script to run all the games
        path path_f_run = PATH_PARTY / "run.sh";
        std::ofstream f_run(path_f_run.c_str());
        if (!f_run){
            LOG("[Handler] Couldn't open run.sh to edit. Exiting...");
            return false;
        }

        f_run << "#!/bin/bash" << endl;
        f_run << "export SDL_JOYSTICK_HIDAPI=0" << endl;
        if (is_32bit){ // SDL2 COMPAT
            f_run << "export SDL_DYNAMIC_API=" << PATH_STEAM << "/ubuntu12_32/steam-runtime/usr/lib/i386-linux-gnu/libSDL2-2.0.so.0" << endl;
        } else { f_run << "export SDL_DYNAMIC_API=" << PATH_STEAM << "/ubuntu12_32/steam-runtime/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0" << endl; }
        if (runtime == "proton"){
            f_run << "export STEAM_COMPAT_CLIENT_INSTALL_PATH=\"" << PATH_STEAM << "\"" << endl;
            f_run << "export STEAM_COMPAT_DATA_PATH=\"" << PATH_PARTY << "/compatdata\"" << endl;
            f_run << "export SteamAppId=\"" << steam_appid << "\"" << endl;
            f_run << "export SteamGameId=\"" << steam_appid << "\"" << endl;
        }
        f_run << "cd \"" << PATH_SYM << "\"" << endl;

        for (int i = 0; i < players.size(); i++){
            path path_profile = PATH_PARTY / "profiles" / players[i].profile;

            // Wine/Proton doesn't seem to like opening multiple instances rapidly in succession
            if (is_win && i > 0) f_run << "sleep 5" << endl;

            f_run << endl << "# Player " << i << endl;
            f_run << "gamescope ";
            f_run << "-W 1280 -H 720 -b "; // TODO: DYNAMIC RESOLUTIONS
            // f_run << "--prefer-vk-device 1002:1900 ";
            f_run << "-- \\" << endl;

            f_run << "bwrap \\" << endl;
            f_run << "--dev-bind / / \\" << endl;

            // Bind user's "steam" folder onto the symlink path's "goldbergsave" folder for individual saves
            if (!path_goldberg.empty()){
                f_run << "--bind \"" << path_profile << "/steam\" ";
                f_run << "\"" << PATH_SYM << "/" << path_goldberg << "/goldbergsave\" \\" << endl;
            }
            f_run << "--tmpfs /tmp \\" << endl;

            // TODO: These only work with proton runtime, not wine (non-steam games)! Find some way to make this plat
            if (is_win && win_unique_appdata){
                f_run << "--bind \"" << path_profile << "/windata/AppData\" ";
                f_run << "\"" << PATH_PARTY << "/compatdata/pfx/drive_c/users/steamuser/AppData\" \\" << endl;
            }
            if (is_win && win_unique_documents){
                f_run << "--bind \"" << path_profile << "/windata/Documents\" ";
                f_run << "\"" << PATH_PARTY << "/compatdata/pfx/drive_c/users/steamuser/Documents\" \\" << endl;
            }

            for (const string& subdir : game_unique_paths){
                f_run << "--bind \"" << path_profile << "/" << uid << "/" << subdir << "\" ";
                f_run << "\"" << PATH_SYM << "/" << subdir << "\" \\" << endl;
            }

            // TODO: idek if this actually works LOLLLL
            if (!is_win && linux_unique_localshare){
                f_run << "--bind \""<< PATH_PARTY << "/profiles/" << players[i].profile << "/share\" ";
                f_run << "\"" << PATH_LOCAL_SHARE << "\" \\" << endl;
            }

            // TODO: Make this dynamic, to allow for e.g. 2 controllers per instance
            for (auto& pad : GAMEPADS){ // Mask out all gamepad paths
                if (pad.path == players[i].pad.path) continue; // Except current player's pad path, of course
                f_run << "--bind /dev/null " << pad.path << " \\" << endl;
            }

            if (runtime == "scout"){
                f_run << "\"" << PATH_STEAM << "/ubuntu12_32/steam-runtime/run.sh\" ";
            } else if (runtime == "proton"){
                // TODO: Make Proton version configurable
                f_run << "\"" << PATH_STEAM << "/steamapps/common/Proton - Experimental/proton\" run ";
            } else if (runtime == "wine"){
                f_run << "wine ";
            }

            f_run << "\"" << PATH_SYM << "/" << exec << "\"";
            for (const string& arg : args){
                if (arg == "$PROFILENAME") { f_run << " " << players[i].profile; }
                else { f_run << " " << arg; }
            }

            if ((i + 1) < players.size()) { f_run << " &"; }

            f_run << endl;
        }

        f_run.close();

        if (chmod(path_f_run.c_str(), S_IRWXU) != 0) {  // S_IRWXU gives the user read, write, execute permissions
            LOG("[party] Failed changing file permissions");
            return false;
        }

        return true;
    }

};

std::vector<Handler> scanHandlers(){
    std::vector<Handler> out;
    path dir_games = PATH_EXECDIR / "handlers/";
    for (const auto& entry : fs::directory_iterator(dir_games)){
        if (fs::is_directory(entry) && fs::exists(entry.path() / "info.json")) {
            string entrystring = entry.path().string();
            Handler h(entrystring);
            if (h.Valid()) { out.push_back(h); }
        }
    }
    return out;
}
