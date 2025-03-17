#pragma once
#include <poll.h>
#include <algorithm>
#include <cerrno>
#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <unistd.h>

#include "shared.h"

#define UI_NONE 0
#define UI_ACCEPT 1
#define UI_BACK 2
#define UI_ALT1 3
#define UI_ALT2 4
#define UI_START 5
#define UI_UP 10
#define UI_DOWN 11
#define UI_LEFT 12
#define UI_RIGHT 13

int pollGamepad(libevdev* dev){
    // struct pollfd fd[1];
    // fd[0].fd = libevdev_get_fd(dev);
    // fd[0].events = POLLIN;
    // int ret = poll(fd, 1, 0);
    int ret = libevdev_has_event_pending(dev);
    if (ret < 1){
        return 0;
    }
    // if (fd[0].revents & POLLIN){
    struct input_event ev;
    ret = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    if (ret == -EAGAIN){
        return 0;
    }
    if (ret == LIBEVDEV_READ_STATUS_SYNC){
        ret = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
    }
    if (ev.type == EV_KEY && ev.value == 1){
        switch (ev.code){
        case BTN_SOUTH: return UI_ACCEPT;
        case BTN_EAST: return UI_BACK;
        case BTN_NORTH: return UI_ALT1; // BTN_NORTH is actually the West button?
        case BTN_WEST: return UI_ALT2; // And vice versa
        case BTN_START: return UI_START;
        }
    } else if (ev.type == EV_ABS){
        switch (ev.code){
        case ABS_HAT0X:
            switch (ev.value){
            case -1: return UI_LEFT;
            case 1: return UI_RIGHT;
            }
        case ABS_HAT0Y:
            switch (ev.value){
            case -1: return UI_UP;
            case 1: return UI_DOWN;
            }
        }
    }
    // }
    return 0;
}

int pollAllGamepads(){
    for (const auto& pad : GAMEPADS ){
        int ret = pollGamepad(pad.dev);
        if (ret != UI_NONE) return ret;
    }
    return UI_NONE;
}

void openGamepads() {
    for (const auto& entry : fs::directory_iterator("/dev/input")) {
        string filename = entry.path().filename().string();
        string path = entry.path().string();

        if (filename.rfind("event", 0) != 0) { // Only read files starting with "event"
            continue;
        }
        struct libevdev *dev = nullptr;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }
        if (libevdev_new_from_fd(fd, &dev) < 0) {
            close(fd);
            LOG("[input][Error] Failed to initialize libevdev");
            continue;
        }
        // Check for BTN_SOUTH event to see if device is game controller
        if (!libevdev_has_event_code(dev, EV_KEY, BTN_SOUTH)) {
            libevdev_free(dev);
            close(fd);
            continue;
        }
        GAMEPADS.push_back({dev, path});
        LOG("found controller " << libevdev_get_name(dev) << " at " << path);
    }
}

void closeGamepads(){
    for (auto& pad : GAMEPADS){
        close(libevdev_get_fd(pad.dev));
        libevdev_free(pad.dev);
    }
}

bool isAlreadyInPlayers(string path){
    if (PLAYERS.size() == 0) return false;
    for (const auto& player : PLAYERS){
        if (path == player.pad.path) return true;
    }
    return false;
}

int doPlayersMenu(){
    for (auto& pad : GAMEPADS){
        if (isAlreadyInPlayers(pad.path)) continue;
        switch (pollGamepad(pad.dev)){
        case UI_ACCEPT:
            PLAYERS.push_back({pad, 0, ""});
            break;
        case UI_BACK:
            if (PLAYERS.size() == 0) return -1; // Return empty if a controller presses B
            break;
        case UI_START:
            if (PLAYERS.size() > 0) return 1; // If there's at least one player, continue when controller presses Start
            break;
        }
    }
    return 0;
}
