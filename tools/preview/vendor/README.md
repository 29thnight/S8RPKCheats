# SDL backends for the standalone preview

These four files come from Dear ImGui **v1.90.9**. One compatibility change adapts
the SDL2 backend to the repository's Dear ImGui 1.91.0 WIP (19093) core:
`io.SetPlatformImeDataFn` is named `io.PlatformSetImeDataFn` in this core, and its
callback receives an additional `ImGuiContext*` argument.

The core fork's `CustomSliderScalar` test hook also referenced the removed
`window->DC.ItemFlags` member. It now reports `g.LastItemData.StatusFlags`, like
the standard widgets. This hook compiles away in the production build, which
does not enable `IMGUI_ENABLE_TEST_ENGINE`.

- https://github.com/ocornut/imgui/blob/v1.90.9/backends/imgui_impl_sdl2.cpp
- https://github.com/ocornut/imgui/blob/v1.90.9/backends/imgui_impl_sdl2.h
- https://github.com/ocornut/imgui/blob/v1.90.9/backends/imgui_impl_sdlrenderer2.cpp
- https://github.com/ocornut/imgui/blob/v1.90.9/backends/imgui_impl_sdlrenderer2.h

Dear ImGui is distributed under the MIT license. The existing complete license
is at `third_party/imgui_legacy/LICENSE.txt`. The preview shares that directory's
core implementation; only the SDL platform and renderer backends are added here.
