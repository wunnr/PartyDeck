#pragma once

#include <cctype>
#include <climits>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include "shared.h"

namespace Util
{

bool StrIsAlnum(const string& str){
    for(const char& c : str) {
        if (!isalnum(c)){
            return false;
        }
    }
    return true;
}

bool StrIsNum(const string& str){
    for(const char& c : str) {
        if (!isdigit(c)){
            return false;
        }
    }
    return true;
}

// Executes a system command verbosely, and warns if return value isn't 0
int Exec(const string& command){
    LOG("[util] Exec: " << command);
    int ret = std::system(command.c_str());
    if (ret != 0){
        LOG("[util] Exec: Warn: Command returned " << ret << ". May be error?");
    }
    return ret;
}

template<typename T>
T JsonValue(nlohmann::json& json, const std::string& key, const T& defaultvalue) {
    if (json.contains(key)) {
        return json[key].get<T>();
    }
    return defaultvalue;
}

template<typename T>
std::vector<T> JsonArray(nlohmann::json& j, string key){
    strvector out;
    if (j.contains(key) && j[key].is_array()){
        for (const auto& i : j[key]){
            out.push_back(i.get<T>());
        }
    }
    return out;
}

string EnvVar(const string& key){
    const char* result = std::getenv(key.c_str());
    return (result == NULL) ? string("") : string(result);
}

void RemoveIfExists(const fs::path& path){
    if (!fs::exists(path)) return;
    try {
        if (fs::is_directory(path)) fs::remove_all(path);
        else fs::remove(path);
    }
    catch (const fs::filesystem_error& e) {
        LOG("Filesystem error: " << e.what() << ". Src: " << path);
    }
    return;
}

// Recursively copies files in a directory to another directory, preserves dir structure. Removes existing files in dest.
// If symlink = true, make symlinks to the src files instead of copying.
// This should be done with fs::copy(src, dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing ),
// But fs::copy_options::overwrite_existing doesn't seem to do anything, at least on my compiler?
bool CopyDirRecursive(const fs::path& src, const fs::path& dest, bool symlink) {
    LOG("[Util] copyDirRecursive:\n  src: " << src << "\n  dest: " << dest);
    if (!fs::exists(src) || !fs::is_directory(src)) {
        LOG("[Util] copyDirRecursive: src not valid");
        return false;
    }

    if (!fs::exists(dest)) {
        fs::create_directories(dest);
    }

    for (const auto& entry : fs::recursive_directory_iterator(src)) {
        try {
            const fs::path relative_path = fs::relative(entry.path(), src);
            fs::path destination_path = dest / relative_path;

            if (fs::is_regular_file(entry)) {
                // Create a symlink for files in the destination directory
                RemoveIfExists(destination_path);

                if (symlink == true) fs::create_symlink(entry.path(), destination_path);
                else fs::copy_file(entry.path(), destination_path, std::filesystem::copy_options::copy_symlinks);
                // LOG("Created symlink: " << destination_path << " -> " << entry.path() << std::endl;

            } else if (fs::is_directory(entry)) {
                // Ensure directory exists in the destination path
                if (!fs::exists(destination_path)) {
                    fs::create_directory(destination_path);
                    // LOG("Created directory: " << destination_path << std::endl;
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            return false;
        }
    }
    return true;
}

}
