#include "sample_session.h"
#include "ui_probe.h"
#include "overlay/feature_menu.h"
#include "overlay/menu_sprites.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL.h>
#include <SDL_image.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace s8;

namespace {
constexpr std::array page_names{"overview", "officers", "traits", "domestic", "council", "battle", "scenario", "settings"};
overlay::MenuPage page_from_name(const std::string& name) {
    for (std::size_t i = 0; i < page_names.size(); ++i)
        if (name == page_names[i]) return static_cast<overlay::MenuPage>(i);
    throw std::runtime_error("Unknown page: " + name);
}

struct Options {
    std::string scenario = "ready", font, screenshot, dump, script, open;
    fs::path resources;
    overlay::MenuPage page = overlay::MenuPage::Overview;
    int frames = 0, width = 1440, height = 1000, delay_frames = 0;
    bool no_atlas = false;
    float scale = 1.0f;
};

Options parse_options(int argc, char** argv) {
    Options result;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--no-atlas") { result.no_atlas = true; continue; }
        if (key == "--help") {
            std::cout << "S8RPK shared ImGui UI preview (sample data only)\n"
                "  --scenario ready|title|loading|errors|external\n"
                "  --page overview|officers|traits|domestic|council|battle|scenario|settings\n"
                "  --open roster|traits|city|protagonist\n"
                "  --scale 1.0 --size 1440x1000 --font FONT --resources DIRECTORY\n"
                "  --frames 8 --screenshot FILE.png --dump-items FILE.txt --script FILE\n"
                "  --no-atlas --delay-frames 20 (fallback / pending-input checks)\n"
                "F2 saves preview.png; backtick reopens the main menu.\n";
            std::exit(0);
        }
        if (i + 1 == argc) throw std::runtime_error("Missing value for " + key);
        const std::string value = argv[++i];
        if (key == "--scenario") result.scenario = value;
        else if (key == "--page") result.page = page_from_name(value);
        else if (key == "--open") result.open = value;
        else if (key == "--font") result.font = value;
        else if (key == "--resources") result.resources = value;
        else if (key == "--screenshot") result.screenshot = value;
        else if (key == "--dump-items") result.dump = value;
        else if (key == "--script") result.script = value;
        else if (key == "--frames") result.frames = std::stoi(value);
        else if (key == "--delay-frames") result.delay_frames = std::stoi(value);
        else if (key == "--scale") result.scale = std::stof(value);
        else if (key == "--size") {
            const auto separator = value.find('x');
            if (separator == std::string::npos) throw std::runtime_error("Expected WIDTHxHEIGHT");
            result.width = std::stoi(value.substr(0, separator));
            result.height = std::stoi(value.substr(separator + 1));
        } else throw std::runtime_error("Unknown option: " + key);
    }
    if (result.scenario != "ready" && result.scenario != "title" && result.scenario != "loading"
        && result.scenario != "errors" && result.scenario != "external") throw std::runtime_error("Unknown scenario");
    if (!std::isfinite(result.scale) || result.scale < 0.5f || result.scale > 2.0f
        || result.width < 640 || result.height < 480 || result.frames < 0 || result.delay_frames < 0)
        throw std::runtime_error("Invalid scale or window size");
    return result;
}

fs::path resource_directory(const Options& options) {
    if (!options.resources.empty()) return options.resources;
    char* base = SDL_GetBasePath();
    const fs::path beside_executable = fs::path(base ? base : ".") / "resources";
    SDL_free(base);
    if (fs::exists(beside_executable / "ui")) return beside_executable;
    return S8_PREVIEW_RESOURCE_DIR;
}

void load_font(const Options& options) {
    std::vector<fs::path> candidates;
    if (!options.font.empty()) candidates.emplace_back(options.font);
    char* base = SDL_GetBasePath();
    if (base) candidates.emplace_back(fs::path(base) / "fonts/NotoSansCJK-Regular.ttc");
    SDL_free(base);
    candidates.emplace_back("C:/Windows/Fonts/malgun.ttf");
    candidates.emplace_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
    candidates.emplace_back("/usr/share/fonts/truetype/nanum/NanumGothic.ttf");
    auto& io = ImGui::GetIO();
    for (const auto& candidate : candidates) {
        if (!fs::exists(candidate)) continue;
        ImFontConfig font_config;
        font_config.OversampleH = 1;
        font_config.OversampleV = 1;
        if (io.Fonts->AddFontFromFileTTF(candidate.string().c_str(), 18.0f,
            &font_config, io.Fonts->GetGlyphRangesKorean())) return;
    }
    throw std::runtime_error("Korean font missing; use --font /path/to/font.ttf");
}

struct Textures {
    std::vector<SDL_Texture*> values;
    ~Textures() { for (auto* value : values) SDL_DestroyTexture(value); }
    SDL_Texture* load(SDL_Renderer* renderer, const fs::path& path, int* width = nullptr, int* height = nullptr) {
        SDL_Texture* texture = IMG_LoadTexture(renderer, path.string().c_str());
        if (!texture) {
            std::cerr << "Preview asset unavailable; using shared ImGui fallback: " << path.string() << '\n';
            return nullptr;
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        SDL_QueryTexture(texture, nullptr, nullptr, width, height);
        values.push_back(texture);
        return texture;
    }
};

void screenshot(SDL_Renderer* renderer, const fs::path& path) {
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
    fs::path temporary = path;
    temporary += ".writing";
    int width = 0, height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) throw std::runtime_error(SDL_GetError());
    const int read = SDL_RenderReadPixels(renderer, nullptr, surface->format->format, surface->pixels, surface->pitch);
    const int saved = read == 0 ? IMG_SavePNG(surface, temporary.string().c_str()) : -1;
    SDL_FreeSurface(surface);
    if (saved != 0) throw std::runtime_error("Screenshot failed: " + std::string(SDL_GetError()));
    // Consumers and packaging must never observe a partially written PNG.
    std::error_code error;
    fs::rename(temporary, path, error);
    if (error && fs::exists(path)) {
        // std::filesystem::rename does not replace an existing file on Windows.
        fs::remove(path);
        fs::rename(temporary, path);
    } else if (error) throw fs::filesystem_error("Screenshot publish failed", temporary, path, error);
    std::cout << "SCREENSHOT " << path.string() << '\n';
}

void render_shared_ui(overlay::FeatureMenu& menu, services::UiBridge& bridge) {
    overlay::update_feature_menu(menu, bridge);
    menu.dpi_scale = menu.snapshot.ui_scale;
    ImGui::GetIO().FontGlobalScale = menu.snapshot.ui_scale;
    overlay::menu_sprites::ThemeScope skin(menu.dpi_scale);
    overlay::draw_feature_menu(menu, bridge.commands);
    if (menu.open) {
        overlay::draw_officer_roster(menu, bridge);
        overlay::draw_protagonist_abilities(menu, bridge);
        overlay::draw_officer_tools(menu, bridge);
        const auto* names = menu.officer_names ? &*menu.officer_names : nullptr;
        overlay::draw_city_management(menu.city_management, bridge, menu.epoch, menu.next_request, names);
        overlay::draw_corps_deployment(menu.corps_deployment, bridge.corps_deployment,
            bridge.commands, menu.epoch, menu.next_request, names);
        overlay::draw_debug_tools(menu.debug_tools, bridge.debug_tools, menu.epoch);
        overlay::draw_specialty_tools(menu.specialty_tools, bridge, menu.epoch, names);
        overlay::draw_notifications(menu.notifications, bridge, menu.snapshot.notification_speed);
    }
    if (menu.officer_notices.epoch == menu.epoch)
        overlay::draw_officer_notice_panel(menu.officer_notices,
            menu.snapshot.features.get(domain::FeatureId::MonitorRonin),
            menu.officer_names ? &*menu.officer_names : nullptr, menu.dpi_scale);
}

void preview_menu(std::unique_ptr<overlay::FeatureMenu>& menu,
    std::unique_ptr<preview::SampleSession>& session, std::string& scenario, SDL_Window* window,
    bool& capture, int delay_frames) {
    ImGui::GetIO().FontGlobalScale = 1.0f;
    if (!ImGui::BeginMainMenuBar()) return;
    ImGui::TextDisabled("미리보기 · 샘플 데이터");
    ImGui::Separator();
    const bool popup_open = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    // Menus themselves are popups. Disable only while one of the shared UI's
    // modal editors is open, otherwise this host menu could disable itself.
    bool editor_open = false;
    if (popup_open) {
        for (const auto& popup : ImGui::GetCurrentContext()->OpenPopupStack)
            if (popup.Window && (popup.Window->Flags & ImGuiWindowFlags_Modal)) editor_open = true;
    }
    ImGui::BeginDisabled(editor_open);
    std::string next_scenario;
    if (ImGui::BeginMenu("상태")) {
        constexpr std::array modes{"ready", "title", "loading", "errors", "external"};
        constexpr std::array labels{"시나리오 연결", "타이틀 · 주인공 없음", "설정 불러오는 중", "대기 · 실패 혼합", "확장 기재 외부 관리"};
        for (std::size_t i = 0; i < modes.size(); ++i)
            if (ImGui::MenuItem(labels[i], nullptr, scenario == modes[i])) next_scenario = modes[i];
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("배율")) {
        for (const float scale : {1.0f, 1.25f, 1.5f}) {
            const auto label = std::to_string(static_cast<int>(scale * 100)) + "%";
            if (ImGui::MenuItem(label.c_str(), nullptr, session->config.ui_scale == scale)) session->config.ui_scale = scale;
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("메뉴 열기")) menu->open = true;
    if (ImGui::MenuItem("PNG 저장", "F2")) capture = true;
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::TextDisabled("게임 연결 없음 · 변경 사항은 현재 창에서만 유지");
    ImGui::EndMainMenuBar();
    if (!next_scenario.empty()) {
        const auto atlases = menu->ui_atlases;
        const auto ranges = menu->range_textures;
        const auto page = menu->page;
        const float scale = session->config.ui_scale;
        session = std::make_unique<preview::SampleSession>(next_scenario);
        session->config.ui_scale = scale;
        session->response_delay_frames = delay_frames;
        menu = std::make_unique<overlay::FeatureMenu>();
        menu->ui_atlases = atlases;
        menu->range_textures = ranges;
        menu->page = page;
        menu->open = true;
        menu->auto_load_applied = true;
        scenario = next_scenario;
        session->pump();
        const auto title = "S8RPK UI Preview | SAMPLE DATA | " + scenario + " | F2: screenshot";
        SDL_SetWindowTitle(window, title.c_str());
    }
}

struct Script {
    std::vector<std::string> lines;
    std::size_t cursor = 0;
    int wait_frames = 5;
    bool release_mouse = false;
    bool release_ctrl = false;
    SDL_Keycode release_key = SDLK_UNKNOWN;
    int checks = 0;
    explicit Script(const std::string& path) {
        if (path.empty()) return;
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Cannot read script " + path);
        std::string line;
        while (std::getline(input, line)) if (!line.empty() && line.front() != '#') lines.push_back(line);
    }
    bool finished() const { return !lines.empty() && cursor == lines.size() && !wait_frames && !release_mouse && release_key == SDLK_UNKNOWN; }
    void step(SDL_Window* window, SDL_Renderer* renderer, overlay::FeatureMenu& menu,
        preview::SampleSession& session) {
        if (lines.empty()) return;
        if (release_mouse) {
            SDL_Event event{};
            event.type = SDL_MOUSEBUTTONUP;
            event.button.windowID = SDL_GetWindowID(window);
            event.button.button = SDL_BUTTON_LEFT;
            SDL_PushEvent(&event);
            if (release_ctrl) {
                SDL_Event key{};
                key.type = SDL_KEYUP;
                key.key.windowID = SDL_GetWindowID(window);
                key.key.keysym.sym = SDLK_LCTRL;
                key.key.keysym.scancode = SDL_SCANCODE_LCTRL;
                SDL_PushEvent(&key);
                SDL_SetModState(KMOD_NONE);
                release_ctrl = false;
            }
            release_mouse = false;
            wait_frames = 3;
            return;
        }
        if (release_key != SDLK_UNKNOWN) {
            SDL_Event key{};
            key.type = SDL_KEYUP;
            key.key.windowID = SDL_GetWindowID(window);
            key.key.keysym.sym = release_key;
            key.key.keysym.scancode = SDL_GetScancodeFromKey(release_key);
            SDL_PushEvent(&key);
            release_key = SDLK_UNKNOWN;
            wait_frames = 3;
            return;
        }
        if (wait_frames > 0) { --wait_frames; return; }
        if (cursor == lines.size()) return;
        const auto& line = lines[cursor++];
        std::istringstream input(line);
        std::string verb, value;
        input >> verb >> std::quoted(value);
        if (verb == "wait") wait_frames = std::stoi(value);
        else if (verb == "click" || verb == "ctrl-click") {
            std::string window_filter;
            input >> std::quoted(window_filter);
            const auto item = preview::find_item(value, window_filter);
            if (!item || !item->enabled) throw std::runtime_error("Cannot click visible enabled item: " + value);
            const int x = static_cast<int>((item->min.x + item->max.x) * 0.5f);
            const int y = static_cast<int>((item->min.y + item->max.y) * 0.5f);
            SDL_WarpMouseInWindow(window, x, y);
            if (verb == "ctrl-click") {
                SDL_SetModState(KMOD_CTRL);
                SDL_Event key{};
                key.type = SDL_KEYDOWN;
                key.key.windowID = SDL_GetWindowID(window);
                key.key.keysym.sym = SDLK_LCTRL;
                key.key.keysym.scancode = SDL_SCANCODE_LCTRL;
                key.key.keysym.mod = KMOD_CTRL;
                SDL_PushEvent(&key);
                release_ctrl = true;
            }
            SDL_Event motion{};
            motion.type = SDL_MOUSEMOTION;
            motion.motion.windowID = SDL_GetWindowID(window);
            motion.motion.x = x;
            motion.motion.y = y;
            SDL_PushEvent(&motion);
            SDL_Event button{};
            button.type = SDL_MOUSEBUTTONDOWN;
            button.button.windowID = SDL_GetWindowID(window);
            button.button.button = SDL_BUTTON_LEFT;
            button.button.x = x;
            button.button.y = y;
            SDL_PushEvent(&button);
            release_mouse = true;
        } else if (verb == "text") {
            SDL_Event event{};
            event.type = SDL_TEXTINPUT;
            event.text.windowID = SDL_GetWindowID(window);
            if (value.size() >= sizeof(event.text.text)) throw std::runtime_error("Test text too long");
            std::copy(value.begin(), value.end(), event.text.text);
            SDL_PushEvent(&event);
            wait_frames = 3;
        } else if (verb == "key") {
            const SDL_Keycode keycode = value == "Enter" ? SDLK_RETURN : value == "Tab" ? SDLK_TAB
                : value == "Escape" ? SDLK_ESCAPE : value == "Space" ? SDLK_SPACE : SDLK_UNKNOWN;
            if (keycode == SDLK_UNKNOWN) throw std::runtime_error("Unsupported test key");
            SDL_Event event{};
            event.type = SDL_KEYDOWN;
            event.key.windowID = SDL_GetWindowID(window);
            event.key.keysym.sym = keycode;
            event.key.keysym.scancode = SDL_GetScancodeFromKey(keycode);
            SDL_PushEvent(&event);
            release_key = keycode;
        } else if (verb == "shot") screenshot(renderer, value);
        else if (verb == "dump") preview::dump_items(value);
        else if (verb == "expect-page") {
            if (menu.page != page_from_name(value)) throw std::runtime_error("Wrong active page: " + value);
            ++checks;
        } else if (verb == "expect-enabled" || verb == "expect-disabled") {
            const auto item = preview::find_item(value);
            if (!item || item->enabled != (verb == "expect-enabled")) throw std::runtime_error("Unexpected enabled state: " + value);
            ++checks;
        } else if (verb == "expect-open") {
            const bool open = value == "roster" ? menu.officer_list_open : value == "traits" ? menu.trait_text_editor.open
                : value == "city" ? menu.city_management.open : value == "protagonist" ? menu.protagonist_detail : false;
            if (!open) throw std::runtime_error("Subwindow not open: " + value);
            ++checks;
        } else if (verb == "expect-commands") {
            if (session.accepted_commands < static_cast<std::size_t>(std::stoi(value))) throw std::runtime_error("Command mailbox did not acknowledge expected operations");
            ++checks;
        } else if (verb == "expect-roster") {
            if (!menu.officer_roster->available || menu.officer_roster->entry_count != static_cast<std::size_t>(std::stoi(value)))
                throw std::runtime_error("Roster sample publication incomplete");
            ++checks;
        } else if (verb == "expect-scale") {
            if (std::abs(session.config.ui_scale - std::stof(value)) > 0.05f) throw std::runtime_error("UI scale was not committed");
            ++checks;
        } else if (verb == "expect-pending") {
            if (menu.pending.has_value() != (value == "yes")) throw std::runtime_error("Unexpected pending command state");
            ++checks;
        } else if (verb == "expect-modal") {
            bool open = false;
            for (const auto& popup : ImGui::GetCurrentContext()->OpenPopupStack)
                if (popup.Window && (popup.Window->Flags & ImGuiWindowFlags_Modal)) open = true;
            if (open != (value == "yes")) throw std::runtime_error("Unexpected modal state");
            ++checks;
        } else if (verb == "expect-window-fit") {
            const auto* workbench = ImGui::FindWindowByName("S8RPK · 편집 도구###S8RPK.Workbench");
            const auto display = ImGui::GetIO().DisplaySize;
            if (!workbench || workbench->Pos.x < -1 || workbench->Pos.y < -1
                || workbench->Pos.x + workbench->Size.x > display.x + 1
                || workbench->Pos.y + workbench->Size.y > display.y + 1)
                throw std::runtime_error("Workbench extends outside the resized display");
            ++checks;
        } else if (verb == "resize") {
            const auto split = value.find('x');
            SDL_SetWindowSize(window, std::stoi(value.substr(0, split)), std::stoi(value.substr(split + 1)));
            wait_frames = 4;
        } else if (verb == "scroll") {
            SDL_WarpMouseInWindow(window, static_cast<int>(ImGui::GetIO().DisplaySize.x * 0.75f),
                static_cast<int>(ImGui::GetIO().DisplaySize.y * 0.6f));
            SDL_Event event{};
            event.type = SDL_MOUSEWHEEL;
            event.wheel.windowID = SDL_GetWindowID(window);
            event.wheel.y = std::stoi(value);
            SDL_PushEvent(&event);
            wait_frames = 4;
        } else throw std::runtime_error("Unknown script operation: " + verb);
        std::cout << "STEP " << cursor << ' ' << line << '\n';
    }
};
}

int main(int argc, char** argv) {
    bool show_error_dialog = true;
    try {
        const auto options = parse_options(argc, argv);
        show_error_dialog = options.script.empty() && options.frames == 0;
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) throw std::runtime_error(SDL_GetError());
        if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) throw std::runtime_error(IMG_GetError());
        const auto title = "S8RPK UI Preview | SAMPLE DATA | " + options.scenario + " | F2: screenshot";
        SDL_Window* window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            options.width, options.height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) throw std::runtime_error(SDL_GetError());
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetCurrentContext()->TestEngineHookItems = true;
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        load_font(options);
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);
        {
            Textures textures;
            auto session = std::make_unique<preview::SampleSession>(options.scenario);
            auto menu = std::make_unique<overlay::FeatureMenu>();
            session->config.ui_scale = options.scale;
            session->response_delay_frames = options.delay_frames;
            menu->open = true;
            menu->auto_load_applied = true;
            menu->page = options.page;
            menu->officer_list_open = options.open == "roster";
            menu->trait_text_editor.open = options.open == "traits";
            menu->city_management.open = options.open == "city";
            menu->protagonist_detail = options.open == "protagonist";
            const auto resources = resource_directory(options);
            constexpr std::array atlas_files{"menu_controls.png", "status_badges.png", "panel_frame.png"};
            for (std::size_t i = 0; i < atlas_files.size(); ++i) {
                int width = 0, height = 0;
                auto* texture = textures.load(renderer, resources / "ui" / atlas_files[i], &width, &height);
                menu->ui_atlases[i] = {texture, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
            }
            if (options.no_atlas) menu->ui_atlases = {};
            for (std::size_t i = 0; i < menu->range_textures.size(); ++i)
                menu->range_textures[i] = textures.load(renderer, resources / "ranges" / (std::to_string(i + 1) + ".png"));
            Script script(options.script);
            std::string scenario = options.scenario;
            bool running = true;
            int frame = 0;
            while (running) {
                SDL_Event event;
                bool capture = false;
                while (SDL_PollEvent(&event)) {
                    ImGui_ImplSDL2_ProcessEvent(&event);
                    if (event.type == SDL_QUIT) running = false;
                    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2) capture = true;
                    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKQUOTE) menu->open = true;
                }
                session->pump();
                ImGui_ImplSDLRenderer2_NewFrame();
                ImGui_ImplSDL2_NewFrame();
                preview::begin_probe_frame();
                ImGui::NewFrame();
                preview_menu(menu, session, scenario, window, capture, options.delay_frames);
                render_shared_ui(*menu, session->bridge);
                ImGui::Render();
                SDL_SetRenderDrawColor(renderer, 19, 22, 26, 255);
                SDL_RenderClear(renderer);
                ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
                if (capture) screenshot(renderer, "preview.png");
                script.step(window, renderer, *menu, *session);
                ++frame;
                if ((options.frames > 0 && frame >= options.frames) || script.finished()) running = false;
                if (!running) {
                    if (!options.screenshot.empty()) screenshot(renderer, options.screenshot);
                    if (!options.dump.empty()) preview::dump_items(options.dump);
                }
                SDL_RenderPresent(renderer);
                SDL_Delay(16);
            }
            std::cout << "RESULT frames=" << frame << " checks=" << script.checks
                << " acknowledged_sample_commands=" << session->accepted_commands
                << " unsupported_game_commands=" << session->unsupported_commands << '\n';
            if (!options.script.empty() && !script.finished()) throw std::runtime_error("Script did not finish before the frame limit");
        }
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PREVIEW ERROR: " << error.what() << '\n';
#ifdef _WIN32
        if (show_error_dialog) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "S8RPK UI Preview", error.what(), nullptr);
#else
        (void)show_error_dialog;
#endif
        return 1;
    }
}
