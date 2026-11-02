cmake_minimum_required(VERSION 3.21)
project(S8RPKUiPreview LANGUAGES CXX)

# Independent UI executable: never link the injected runtime or game-memory layer.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(S8_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_executable(s8_ui_preview
    main.cpp sample_session.cpp ui_probe.cpp
    vendor/imgui_impl_sdl2.cpp vendor/imgui_impl_sdlrenderer2.cpp
    ${S8_ROOT}/third_party/imgui_legacy/imgui.cpp
    ${S8_ROOT}/third_party/imgui_legacy/imgui_draw.cpp
    ${S8_ROOT}/third_party/imgui_legacy/imgui_tables.cpp
    ${S8_ROOT}/third_party/imgui_legacy/imgui_widgets.cpp
    ${S8_ROOT}/src/overlay/feature_menu.cpp
    ${S8_ROOT}/src/overlay/trait_text_editor.cpp
    ${S8_ROOT}/src/overlay/notifications.cpp
    ${S8_ROOT}/src/overlay/debug_tools_panel.cpp
    ${S8_ROOT}/src/overlay/city_management_panel.cpp
    ${S8_ROOT}/src/overlay/corps_deployment_panel.cpp
    ${S8_ROOT}/src/overlay/officer_tools_panel.cpp
    ${S8_ROOT}/src/overlay/officer_notice_panel.cpp
    ${S8_ROOT}/src/overlay/specialty_tools_panel.cpp
    ${S8_ROOT}/src/services/ui_bridge.cpp
    ${S8_ROOT}/src/services/ui_bridge_city.cpp
    ${S8_ROOT}/src/services/ui_bridge_specialty.cpp
    ${S8_ROOT}/src/services/command_mailbox.cpp
    ${S8_ROOT}/src/services/file_log.cpp
    ${S8_ROOT}/src/services/officer_tools_bridge.cpp
    ${S8_ROOT}/src/services/debug_bridge.cpp
    ${S8_ROOT}/src/services/city_bridge.cpp
    ${S8_ROOT}/src/services/corps_bridge.cpp
    ${S8_ROOT}/src/services/specialty_bridge.cpp
    ${S8_ROOT}/src/services/native_officer_name_bridge.cpp
    ${S8_ROOT}/src/domain/stratagem5.cpp
    ${S8_ROOT}/src/domain/feature_flags.cpp
    ${S8_ROOT}/src/domain/secondary_jewel.cpp
    ${S8_ROOT}/src/domain/auto_affinity.cpp
    ${S8_ROOT}/src/domain/ai_officer_growth.cpp
    ${S8_ROOT}/src/domain/special_ability_assignment.cpp
    ${S8_ROOT}/src/domain/trait_text_edit.cpp
    ${S8_ROOT}/src/domain/debug_tools.cpp
)
if(EXISTS "${S8_ROOT}/src/overlay/menu_sprites.cpp")
    target_sources(s8_ui_preview PRIVATE ${S8_ROOT}/src/overlay/menu_sprites.cpp)
endif()
target_include_directories(s8_ui_preview PRIVATE
    ${S8_ROOT}/src ${S8_ROOT}/third_party/imgui_legacy vendor)
target_compile_definitions(s8_ui_preview PRIVATE SDL_MAIN_HANDLED IMGUI_ENABLE_TEST_ENGINE
    S8_PREVIEW_RESOURCE_DIR="${S8_ROOT}/resources")
target_link_libraries(s8_ui_preview PRIVATE SDL2::SDL2 SDL2_image::SDL2_image Threads::Threads)
if(WIN32)
    target_sources(s8_ui_preview PRIVATE ${S8_ROOT}/src/services/game_font.cpp)
    set_target_properties(s8_ui_preview PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(s8_ui_preview PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()
if(MSVC)
    target_compile_options(s8_ui_preview PRIVATE /utf-8 /W4 /EHsc)
else()
    target_compile_options(s8_ui_preview PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
if(NOT WIN32)
    set_source_files_properties(${S8_ROOT}/src/services/file_log.cpp PROPERTIES
        COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/portable_time.h")
endif()

install(TARGETS s8_ui_preview RUNTIME DESTINATION .)
install(DIRECTORY ${S8_ROOT}/resources/ranges DESTINATION resources)

include(CTest)
if(BUILD_TESTING)
    add_executable(s8_sprite_controls_test
        tests/sprite_controls.cpp
        ${S8_ROOT}/third_party/imgui_legacy/imgui.cpp
        ${S8_ROOT}/third_party/imgui_legacy/imgui_draw.cpp
        ${S8_ROOT}/third_party/imgui_legacy/imgui_tables.cpp
        ${S8_ROOT}/third_party/imgui_legacy/imgui_widgets.cpp)
    target_include_directories(s8_sprite_controls_test PRIVATE
        ${S8_ROOT}/src ${S8_ROOT}/third_party/imgui_legacy)
    if(MSVC)
        target_compile_options(s8_sprite_controls_test PRIVATE /utf-8)
    endif()
    add_test(NAME sprite_controls COMMAND s8_sprite_controls_test)
endif()
