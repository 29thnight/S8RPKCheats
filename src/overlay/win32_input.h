#pragma once

#include "input_events.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace s8::overlay {

enum class InputMessageResult { Ignored, Queued, QueueFull };

InputMessageResult queue_win32_input(InputEvents& input, UINT message, WPARAM wparam, LPARAM lparam);
InputMessageResult queue_ime_result(InputEvents& input, HWND window, LPARAM lparam);

} // namespace s8::overlay
