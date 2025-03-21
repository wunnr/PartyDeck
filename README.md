
<img src=".github/assets/icon.png" align="left" width="48" height="48">

# `PartyDeck`

A proof-of-concept split-screen game launcher for Linux/SteamOS.

> [!IMPORTANT]
> This is the first serious software project I've ever done. It surely contains many violations of software best practices and security flaws, and has not been audited at all. Use at your own discretion! If you are experienced in software I would love to know what aspects of the codebase could be improved and how I can do better.

## Installing

This app is not yet ready for public usage; No releases will be provided until the app is more stable and feature-complete.

## Building

Clone the repository with submodules included:

```bash
git clone --recursive https://github.com/wunnr/PartyDeck.git
```

You will need Libevdev and SDL2 installed to build the project. On Arch, these are `libevdev` and `sdl2`. Then simply enter the directory (`cd PartyDeck`) and run `make`.

## Usage

This project uses KWin (included with KDE Plasma), Gamescope, qdbus and Bubblewrap; SteamOS already includes these, but if you're on a desktop Linux distro you may need to install these yourself. On Arch, these packages are `kwin`, `gamescope`, `qt5-tools`/`qt6-tools`, and `bubblewrap`.

If you're running a KDE Plasma session, you can simply run the executable `PartyDeck` to get started. If you're on Steam Deck and want to access PartyDeck from Gaming Mode, simply add `SteamDeckLaunch.sh` as a non-Steam game by right-clicking that file and selecting "Add to Steam". This is a simple script that launches a KWin session from within Gaming Mode, then runs PartyDeck inside of that session

> [!NOTE]
> **SteamOS Users:** This app requires KDE Plasma 6 for the KWin split-screen. The current stable version of SteamOS still uses Plasma 5, but for now you can update to the Preview channel in the system settings to get Plasma 6.

Note that you'll also need a Handler to actually run a game; These will be uploaded to a separate repository, and eventually the project will include a program that helps you generate your own Handler.

## The Tech Stack

PartyDeck uses a few software layers to provide a (relatively) seamless, console-like split-screen gaming experience

- **KWin Session:** Running a script to automatically resize and reposition the Gamescope windows

This KWin Session displays all running game instances. Each instance runs these:

- **Gamescope:** Contains each instance of the game to its own window. Also has the neat side effect of receiving controller input even when the window is not currently active, meaning multiple Gamescope instances can all receive input simultaneously
- **Bubblewrap:** Uses bindings to mask out evdev input files from the instances, so each instance only receives input from one specific gamepad (pre-defined by the launcher). Also uses directory binding to give each player their own save data and settings within the games.
- **Runtime (Steam Runtime/Proton/Wine)**
- **Goldberg Steam Emu:** On games that use the Steam API for multiplayer, Goldberg is used to allow the game instances to connect to each other, as well as other devices running on the same LAN.
- **The game**

## Known Issues, Limitations and Todos

- The app only works correctly with a single display. Running with multiple displays has not been tested at all, and will most likely not work. Modifying the KWin script to support multiple displays is possible but will require further investigation into the KWin API.
- The app has only been tested on Arch Linux and SteamOS 3.7.0+. Your mileage may vary if using other Linux distributions.
- By default, the app reads from `~/.local/share/Steam/` for Steam things like games, Proton, and the Steam Runtime. This can be changed by using the environment variable `STEAM_BASE_FOLDER=/path/to/steam/`.
- The app can only locate Steam games installed in Steam's default installation folder (`Steam/steamapps/common/`). Support for reading from separate Steam library folders is planned.
- The only runtimes available right now are the default Steam Linux "scout" runtime, and Proton Experimental. Support for other runtimes (ProtonGE, regular Wine, etc) is planned.

## Disclaimer
This software has been created purely for the purposes of academic research. It is not intended to be used to attack other systems. Project maintainers are not responsible or liable for misuse of the software. Use responsibly.
