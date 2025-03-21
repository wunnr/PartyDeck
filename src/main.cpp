#include "shared.h"
#include "gui.cpp"
#include "util.cpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL2/SDL.h>

const path PATH_EXECDIR = fs::read_symlink("/proc/self/exe").parent_path();
const path PATH_LOCAL_SHARE = path(Util::EnvVar("HOME")) / ".local/share";
const path PATH_PARTY = PATH_LOCAL_SHARE / "partydeck";
const path PATH_STEAM = (Util::EnvVar("STEAM_BASE_FOLDER").empty()) ? (PATH_LOCAL_SHARE / "Steam") : path(Util::EnvVar("STEAM_BASE_FOLDER"));
path PATH_SYM;
std::vector<Gamepad> GAMEPADS{};
std::vector<Player> PLAYERS{};
nlohmann::json SETTINGS;
nlohmann::json GAMEPATHS;
std::ofstream f_log((PATH_EXECDIR/"log.txt").string().data());

int main(int, char**)
{
    if (!f_log){
        cout << "[Party] Couldn't open logging file. Quitting" << endl;
        return 1;
    }

    if (Util::EnvVar("HOME").empty()) { LOG("[Party] HOME env var isn't set!"); return 1; }

    nlohmann::json defaultsettings = {
        {"force_sdl2_version", false},
    };
    SETTINGS = Util::LoadJson(PATH_PARTY/"settings.json", defaultsettings);
    GAMEPATHS = Util::LoadJson(PATH_PARTY/"gamepaths.json", nlohmann::json::object());

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0){
        LOG("[Party] SDL_Init(): " << SDL_GetError());
        return -1;
    }

    // Create window with SDL_Renderer graphics context
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("PartyDeck", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (window == nullptr)
    {
        LOG("[Party] SDL_CreateWindow(): " << SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
    {
        LOG("[Party] Error creating SDL_Renderer!");
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_SetWindowFullscreen(window, true);
    SDL_ShowWindow(window);

    GUI gui(window, renderer);
    while (gui.running){
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                gui.running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                gui.running = false;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        gui.doNewFrame();
    }

    Util::Exec("qdbus org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript \"splitscreen\"");

    // Cleanup
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    f_log.close();

    return 0;
}
