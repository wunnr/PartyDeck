// Dear ImGui: standalone example application for SDL2 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important to understand: SDL_Renderer is an _optional_ component of SDL2.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#include "shared.h"
#include "ui.cpp"
#include "util.cpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <stdio.h>
#include <SDL2/SDL.h>

const string PATH_EXECDIR = fs::read_symlink("/proc/self/exe").parent_path().string();
const string PATH_LOCAL_SHARE = Util::EnvVar("HOME") + "/.local/share";
const string PATH_PARTY = PATH_LOCAL_SHARE + "/partydeck";
const string PATH_STEAM = (Util::EnvVar("STEAM_BASE_FOLDER").empty()) ? (PATH_LOCAL_SHARE + "/Steam") : Util::EnvVar("STEAM_BASE_FOLDER");
string PATH_SYM;
std::vector<Gamepad> GAMEPADS{};
std::vector<Player> PLAYERS{};
nlohmann::json SETTINGS;
std::ofstream f_log;

int main(int, char**)
{
    string path_f_log = PATH_EXECDIR + "/log.txt";
    std::ofstream f_log(path_f_log.c_str());
    if (!f_log){
        cout << "[Party] Couldn't open logging file. Quitting" << endl;
        return 1;
    }

    if (Util::EnvVar("HOME").empty()) { LOG("HOME env var isn't set!"); return 1; }

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

    UI ui(window, renderer);
    while (ui.running){
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                ui.running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                ui.running = false;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        ui.doNewFrame();
    }

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
