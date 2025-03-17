
#include "shared.h"
#include "game.cpp"
#include "gamepad.cpp"
#include "profiles.cpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <SDL2/SDL.h>

#include <cstddef>
#include <imgui.h>
#include <string>

#define UI_WIDTH_FULL ImGui::GetContentRegionAvail().x
#define UI_WIDTH_HALF (ImGui::GetContentRegionAvail().x * 0.5f)
#define UI_WIDTH_THIRD (ImGui::GetContentRegionAvail().x * 0.333333333333333333f)

enum Page {
    MainMenu,
    DemoWindow,
    SettingsMenu,
    ProfilesMenu,
    CreateProfileMenu,
    SelectGame,
    PlayersMenu,
    GameLoading
};

class UI {
private:
    SDL_Window* win;
    SDL_Renderer* rend;
    ImGuiWindowFlags window_flags = 0 | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground;

    Page cur_page = MainMenu;
    Page prev_page;

    bool show_msg = false;
    string msg_text;
    char input_box[1024] = {0};

    bool show_demo_window = true;

    strvector profiles_list;

    std::vector<Game> gameslist;
    Game cur_game;

    ImVec4 bg_color = ImVec4(0.05f, 0.15f, 0.25f, 1.00f);
    std::vector<ImVec4> player_colors = {ImVec4(0.1f, 0.6f, 1.0f, 1.0f), ImVec4(1.0f, 0.1f, 0.5f, 1.0f), ImVec4(0.0f, 1.0f, 0.5f, 1.0f), ImVec4(1.0f, 0.8f, 0.0f, 1.0f)};
public:
    bool running = true;
    UI(SDL_Window* window, SDL_Renderer* renderer) {

        win = window;
        rend = renderer;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.IniFilename = NULL;
        ImFont* font = io.Fonts->AddFontFromFileTTF("data/DroidSans.ttf", 36.0f, nullptr);
        IM_ASSERT(font != nullptr);

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        // Setup Platform/Renderer backends
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);
    }

    void ResizeAndCenterNext(float width, float height){
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(width, height));
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f,0.5f));
    }

    void GamepadNav(bool enable){
        ImGuiIO& io = ImGui::GetIO();
        if (enable) io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        else io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    }

    void CreateMsg(std::string s) {
        show_msg = true;
        msg_text = s;
    }

    void ShowMsg(){
        ImGui::SetNextWindowFocus();
        ImGui::Begin("Message");
        ImGui::TextWrapped("%s", msg_text.c_str());
        if (ImGui::Button("OK")) show_msg = false;
        ImGui::End();
    }

    void DoGameStuff(){
        bool ret;

        for (int i = 0; i < PLAYERS.size(); i++){
            // TEMP: make all players guests for now
            PLAYERS[i].profile = Profiles::guestnames[i];
            fs::path profilepath = fs::path(PATH_PARTY) / "profiles" / PLAYERS[i].profile;
            if (!fs::exists(profilepath)) Profiles::create(PLAYERS[i].profile);
            ret = Profiles::gameUniqueDirs(PLAYERS[i].profile, cur_game);
            if (!ret){
                LOG("Failed setting up game dirs for profile " << PLAYERS[i].profile);
                CreateMsg("Failed setting up game dirs for profile" + PLAYERS[i].profile + ". Check log for more details ");
                cur_page = MainMenu;
                return;
            }
        }

        PATH_SYM = PATH_PARTY + "/game/" + cur_game.name;
        if (!fs::exists(PATH_SYM)){
            ret = createGameSym(cur_game);
            if (!ret){
                LOG("[party] Failed creating game symlink folder!");
                CreateMsg("Failed to create game symlink folder. Check log for more details");
                cur_page = MainMenu;
                Util::RemoveIfExists(PATH_SYM);
                return;
            }
        }

        string compatdatapath = PATH_PARTY + "/compatdata";
        if (cur_game.runtime == "proton" && !fs::exists(compatdatapath)){
            ret = createProtonCompatdata(cur_game);
            if (!ret){
                LOG("[party] Failed creating compatdata folder!");
                CreateMsg("Failed to create Wine/Proton data folder. Check log for more details");
                cur_page = MainMenu;
                Util::RemoveIfExists(compatdatapath);
                return;
            }
        }

        ret = writeGameLaunchScript(cur_game, PLAYERS);
        if (!ret){
            LOG("[party] Failed writing launch script!");
            CreateMsg("Failed to write game launch script. Check log for more details");
            cur_page = MainMenu;
            return;
        }

        string path_f_run = PATH_PARTY + "/run.sh";
        string kwinscriptpath = PATH_EXECDIR + "/data/splitscreen_kwin.js";
        // Ideally we should be using some DBus library to do this instead of relying on qdbus but.... eh.....
        Util::Exec(string("qdbus org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript \"" + kwinscriptpath + "\" \"splitscreen\""));
        Util::Exec(string("qdbus org.kde.KWin /Scripting org.kde.kwin.Scripting.start"));
        closeGamepads();
        int execret = Util::Exec(path_f_run);
        if (execret != 0) {
            CreateMsg("Games closed unexpectedly. Check log for more details");
            GamepadNav(true);
            cur_page = MainMenu;
        } else {running = false;}
        Util::Exec("qdbus org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript \"splitscreen\"");
    }

    void showMainMenu(){
        ResizeAndCenterNext(400, 400);
        ImGui::Begin("PartyDeck", NULL, window_flags);
        if (ImGui::Button("Play", ImVec2(-FLT_MIN, 40))){
            gameslist = scanGames();
            cur_page = SelectGame;
        }
        if (ImGui::Button("Settings", ImVec2(UI_WIDTH_HALF, 40))) cur_page = SettingsMenu;
        ImGui::SameLine(); if (ImGui::Button("Profiles", ImVec2(UI_WIDTH_FULL, 40))) {
            profiles_list = Profiles::listAll(false);
            cur_page = ProfilesMenu;
        }
        // if (ImGui::Button("DemoWindow", ImVec2(-FLT_MIN, 40))) cur_page = DemoWindow;
        // if (ImGui::Button("Test Players", ImVec2(-FLT_MIN, 40))) {
        //     PLAYERS.clear();
        //     GamepadNav(false);
        //     cur_page = PlayersMenu;
        // }
        if (ImGui::Button("Quit", ImVec2(-FLT_MIN, 40))) running = false;
        // if (ImGui::InputText("test", input_box, sizeof(input_box), ImGuiInputTextFlags_EnterReturnsTrue)){
        //     CreateMsg(string(input_box));
        // }
        ImGui::End();
    }

    void showDemoWindow(){
        ImGui::ShowDemoWindow(&show_demo_window);
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) cur_page = MainMenu;
    }

    void showProfilesMenu(){
        ResizeAndCenterNext(400, 400);
        ImGui::Begin("Profiles");
        for (const string& profile : profiles_list){
            ImGui::Selectable(profile.c_str());
        }
        if (ImGui::Button("Back", ImVec2(UI_WIDTH_HALF, 40)) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) cur_page = MainMenu;
        ImGui::SameLine(); if (ImGui::Button("New", ImVec2(UI_WIDTH_FULL, 40))) cur_page = CreateProfileMenu;
        ImGui::End();
    }

    void showSelectGame(){
        ResizeAndCenterNext(400, 400);
        ImGui::Begin("Select Game", NULL, window_flags);
        for (Game& game: gameslist){
            string name = game.fancyname.empty() ? game.name : game.fancyname;
            if (ImGui::Selectable(name.c_str())){
                bool ret = testGame(game);
                if (!ret) { CreateMsg("Handler is malformed or corrupt. Check log for more details"); continue; }
                else { openGamepads(); cur_game = game; cur_page = PlayersMenu; GamepadNav(false); }
            }
        }
        if (ImGui::Button("Back", ImVec2(-FLT_MIN, 40)) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) cur_page = MainMenu;
        ImGui::End();
    }

    void showPlayersMenu(){
        ResizeAndCenterNext(400, 400);
        ImGui::Begin("Players");
        if (GAMEPADS.size() == 0) { ImGui::Text("No gamepads detected. Connect a gamepad and click Rescan"); }
        else { ImGui::TextWrapped("A/X: Add gamepad\nB/Circle: Remove gamepad/Go back\nStart: Begin game"); }
        for (int i = 0; i < PLAYERS.size(); i++){
            const char* padname = libevdev_get_name(PLAYERS[i].pad.dev);
            ImGui::TextColored(player_colors[i], "P%i: ", i + 1);
            ImGui::SameLine(); ImGui::TextColored(player_colors[i], "%s", padname);
            ImGui::SameLine(); ImGui::TextColored(player_colors[i], "%i", PLAYERS[i].profilechoice);
            switch (pollGamepad(PLAYERS[i].pad.dev)){
                // case UI_LEFT: { PLAYERS[i].profilechoice--; break; }
                // case UI_RIGHT: { PLAYERS[i].profilechoice++; break;}
                case UI_BACK: { PLAYERS.erase(PLAYERS.begin() + i); break; }
                case UI_START: { cur_page = GameLoading; break; }
            }
        }
        int ret = doPlayersMenu();
        if (ImGui::Button("Back", ImVec2(UI_WIDTH_THIRD, 40)) || ret == -1){
            GamepadNav(true);
            cur_page = MainMenu;
        }
        ImGui::SameLine(); if (ImGui::Button("Rescan", ImVec2(UI_WIDTH_HALF, 40))) { GAMEPADS.clear(); openGamepads(); }
        ImGui::SameLine(); if (ImGui::Button("Start", ImVec2(UI_WIDTH_FULL, 40))) { if (PLAYERS.size() > 0) cur_page = GameLoading; }
        ImGui::End();
        if (cur_page == GameLoading){
            ResizeAndCenterNext(300, 200);
            ImGui::Begin("Loading");
            ImGui::Text("Starting games...");
            ImGui::End();
        }
    }

    void doNewFrame(){
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (show_msg) ShowMsg();
        switch (cur_page){
            case MainMenu: showMainMenu(); break;
            case DemoWindow: showDemoWindow(); break;
            case ProfilesMenu: showProfilesMenu(); break;
            case SelectGame: showSelectGame(); break;
            case PlayersMenu: showPlayersMenu(); break;
            default: break;
        }

        // Rendering
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Render();
        SDL_RenderSetScale(rend, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(rend, (Uint8)(bg_color.x * 255), (Uint8)(bg_color.y * 255), (Uint8)(bg_color.z * 255), (Uint8)(bg_color.w * 255));
        SDL_RenderClear(rend);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), rend);
        SDL_RenderPresent(rend);

        if (cur_page == GameLoading) DoGameStuff();
    }
};
