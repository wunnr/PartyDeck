
#include "shared.h"
#include "handler.cpp"
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
#define UI_HEIGHT_BTN 50
#define UI_BUTTON_FULLW ImVec2(UI_WIDTH_FULL, UI_HEIGHT_BTN)
#define UI_BUTTON_HALFW ImVec2(UI_WIDTH_HALF, UI_HEIGHT_BTN)
#define UI_BUTTON_THIRDW ImVec2(UI_WIDTH_THIRD, UI_HEIGHT_BTN)

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

class GUI {
private:
    SDL_Window* win;
    SDL_Renderer* rend;
    ImGuiWindowFlags window_flags = 0 | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground;

    Page cur_page = MainMenu;
    Page prev_page;

    bool show_popup = false;
    string popup_text;
    char input_box[1024] = {0};

    bool show_demo_window = true;

    strvector profiles_list;

    std::vector<Handler> handler_list;
    Handler* handler;

    ImVec4 bg_color = ImVec4(0.05f, 0.15f, 0.25f, 1.00f);
    std::vector<ImVec4> player_colors = {ImVec4(0.1f, 0.6f, 1.0f, 1.0f), ImVec4(1.0f, 0.1f, 0.5f, 1.0f), ImVec4(0.0f, 1.0f, 0.5f, 1.0f), ImVec4(1.0f, 0.8f, 0.0f, 1.0f)};
public:
    bool running = true;
    GUI(SDL_Window* window, SDL_Renderer* renderer) {

        win = window;
        rend = renderer;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.IniFilename = NULL;
        ImFont* font = io.Fonts->AddFontFromFileTTF("data/DroidSans.ttf", 36.0f, nullptr);
        // io.FontGlobalScale = 2;
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameRounding = 6;
        // style.ScaleAllSizes(2);
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

    void CreatePopup(std::string s) {
        popup_text = s;
        show_popup = true;
    }

    void DoPopup(){
        ResizeAndCenterNext(500, 200);
        ImGui::OpenPopup("Message");
        if (ImGui::BeginPopupModal("Message", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextWrapped("%s", popup_text.c_str());
            ImGui::Separator();
            if (ImGui::Button("OK", UI_BUTTON_FULLW)) { show_popup = false; }
            ImGui::EndPopup();
        }
    }

    void DoGameStuff(){
        bool ret;

        for (int i = 0; i < PLAYERS.size(); i++){
            // TEMP: make all players guests for now
            PLAYERS[i].profile = Profiles::guestnames[i];
            fs::path profilepath = fs::path(PATH_PARTY)/"profiles"/PLAYERS[i].profile;
            if (!fs::exists(profilepath)) Profiles::create(PLAYERS[i].profile);
        }

        try {
            handler->CreateProfileUniqueDirs(PLAYERS);
            handler->CreateGameSymlinkDir();
            if (handler->IsWin() && !fs::exists(PATH_PARTY/"compatdata")) { handler->CreateCompatdataDir(); }
            handler->WriteGameLaunchScript(PLAYERS);

            path path_f_run = PATH_PARTY/"run.sh";
            path kwinscriptpath = PATH_EXECDIR/"data/splitscreen_kwin.js";
            // Ideally we should be using some DBus library to do this instead of relying on qdbus but.... eh.....
            Util::Exec(string("qdbus org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript \"" + kwinscriptpath.string() + "\" \"splitscreen\""));
            Util::Exec(string("qdbus org.kde.KWin /Scripting org.kde.kwin.Scripting.start"));
            closeGamepads();
            int execret = Util::Exec(path_f_run.string());
            if (execret != 0) { throw std::runtime_error("Games may have closed unexpectedly. ");
            } else {running = false;}
        }
        catch (const std::runtime_error& e) {
            LOG(e.what());
            CreatePopup(string(e.what()) + "\nCheck log for more details");
            GamepadNav(true);
            cur_page = MainMenu;
            return;
        }
    }

    void showMainMenu(){
        ResizeAndCenterNext(500, 500);
        ImGui::Begin("PartyDeck", NULL);
        if (ImGui::Button("Play", UI_BUTTON_FULLW)){
            handler_list = scanHandlers();
            cur_page = SelectGame;
        }
        ImGui::SetItemDefaultFocus();
        if (ImGui::Button("Settings", UI_BUTTON_HALFW)) cur_page = SettingsMenu;
        ImGui::SameLine(); if (ImGui::Button("Profiles", UI_BUTTON_FULLW)) {
            profiles_list = Profiles::listAll(false);
            cur_page = ProfilesMenu;
        }
        if (ImGui::Button("DemoWindow", UI_BUTTON_FULLW)) { cur_page = DemoWindow; }
        if (ImGui::Button("popuptest", UI_BUTTON_FULLW)) { CreatePopup("hi!"); }
        if (ImGui::Button("Quit", UI_BUTTON_FULLW)) { running = false; }
        // if (ImGui::InputText("test", input_box, sizeof(input_box), ImGuiInputTextFlags_EnterReturnsTrue)){
        //     CreateMsg(string(input_box));
        // }
        ImGui::End();
    }

    void showDemoWindow(){
        ImGui::ShowDemoWindow(&show_demo_window);
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) cur_page = MainMenu;
    }

    void showSettingsMenu(){
        ResizeAndCenterNext(500, 500);
        ImGui::Begin("Settings");
        ImGui::Checkbox("Force latest SDL2 version", &SETTINGS["force_sdl2_version"].get_ref<bool&>());
        if (ImGui::Button("Erase Game Symlink Folders", UI_BUTTON_FULLW)){ Util::RemoveIfExists(PATH_PARTY/"game"); }
        if (ImGui::Button("Erase Profile Folders", UI_BUTTON_FULLW)){ Util::RemoveIfExists(PATH_PARTY/"profiles"); }
        if (ImGui::Button("Back", UI_BUTTON_FULLW)){
            try {
                Util::SaveJson(PATH_PARTY/"settings.json", SETTINGS);
            }
            catch (std::runtime_error& e){
                CreatePopup(e.what());
            }
            cur_page = MainMenu;
        }
        ImGui::End();
    }

    void showProfilesMenu(){
        ResizeAndCenterNext(500, 500);
        ImGui::Begin("Profiles");
        for (const string& profile : profiles_list){
            ImGui::Selectable(profile.c_str());
        }
        if (ImGui::Button("Back", UI_BUTTON_HALFW) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) cur_page = MainMenu;
        ImGui::SameLine(); if (ImGui::Button("New", UI_BUTTON_FULLW)) cur_page = CreateProfileMenu;
        ImGui::End();
    }

    void showSelectGame(){
        ResizeAndCenterNext(500, 500);
        ImGui::Begin("Select Game", NULL, window_flags);
        ImGui::Text("Select Game:");
        ImGui::BeginChild("GamesScrollableList", ImVec2(UI_WIDTH_FULL, 200), ImGuiChildFlags_Borders);
        for (Handler& h: handler_list){
            if (ImGui::Selectable(h.Name().c_str())){
                if (h.Test() == false) { CreatePopup("Handler is malformed or corrupt. Check log for more details"); continue; }
                else { openGamepads(); handler = &h; cur_page = PlayersMenu; GamepadNav(false); }
            }
        }
        ImGui::EndChild();
        if (ImGui::Button("Back", UI_BUTTON_FULLW) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) cur_page = MainMenu;
        ImGui::End();
    }

    void showPlayersMenu(){
        ResizeAndCenterNext(500, 500);
        ImGui::Begin("Players");
        if (GAMEPADS.size() == 0) { ImGui::TextWrapped("No gamepads detected. Connect a gamepad and click Rescan"); }
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
        if (ImGui::Button("Back", UI_BUTTON_THIRDW) || ret == -1){
            GamepadNav(true);
            cur_page = MainMenu;
        }
        ImGui::SameLine(); if (ImGui::Button("Rescan", UI_BUTTON_HALFW)) { GAMEPADS.clear(); openGamepads(); }
        ImGui::SameLine(); if (ImGui::Button("Start", UI_BUTTON_FULLW)) { if (PLAYERS.size() > 0) cur_page = GameLoading; }
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

        if (show_popup) DoPopup();

        switch (cur_page){
            case MainMenu: showMainMenu(); break;
            case DemoWindow: showDemoWindow(); break;
            case SettingsMenu: showSettingsMenu(); break;
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
