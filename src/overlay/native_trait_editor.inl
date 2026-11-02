// Included only by trait_text_editor.cpp. These bindings target the executable
// revision already checked by native_launcher; no engine calls run in Present.
namespace {

struct TraitListRow {
    std::wstring label;
    std::wstring text;
    std::size_t action;
};

struct TraitListCell {
    std::int32_t kind = 1, reserved = 0;
    const wchar_t* text = nullptr;
    std::int32_t color = 0x7FFFFFFF, padding = 0;
    void* extra = nullptr;
    std::uint64_t flags = 0;
};
static_assert(sizeof(TraitListCell) == 0x28);

const wchar_t* trait_list_label(const TraitListRow* row) { return row->label.c_str(); }
const wchar_t* trait_list_text(const TraitListRow* row) { return row->text.c_str(); }
TraitListCell* trait_label_cell(TraitListCell* output, const TraitListRow* row) {
    *output = {};
    output->text = row->label.c_str();
    return output;
}
TraitListCell* trait_text_cell(TraitListCell* output, const TraitListRow* row) {
    *output = {};
    output->text = row->text.c_str();
    return output;
}

// CCmnInfoList's column descriptor: use editor-owned getters and formatters
// rather than the original PersonalityData column functions.
struct TraitListColumn {
    std::int32_t id, reserved;
    const wchar_t* heading;
    std::int32_t width, kind;
    const wchar_t* (*get)(const TraitListRow*);
    void* unused1;
    TraitListCell* (*format)(TraitListCell*, const TraitListRow*);
    void* unused2;
    void* unused3;
    void* count;
    std::uint64_t options;
    std::uint64_t unused4;
};
static_assert(sizeof(TraitListColumn) == 0x58);
std::array<TraitListColumn, 2> trait_list_columns{{
    {0, 0, L"항목", 260, 1, trait_list_label, nullptr, trait_label_cell,
        nullptr, nullptr, nullptr, 0x10000FFFF, 0},
    {1, 0, L"설명", 1140, 1, trait_list_text, nullptr, trait_text_cell,
        nullptr, nullptr, nullptr, 0x10000FFFF, 0}
}};
const std::array<int, 3> trait_column_order{0, 1, -1};
struct TraitListTab { const int* columns; const wchar_t* label; std::uint64_t options; };
// The dialog resolves message IDs in-place; 0xFFFF keeps the supplied label.
std::array<TraitListTab, 2> trait_list_tabs{{{trait_column_order.data(), L"기본", 0xFFFF}, {}}};

struct NativeTraitPublication {
    own::shared_owner<const domain::TraitTextEdits> rows;
    own::shared_owner<const domain::TraitEffectDefinitions> effects;
    domain::GameEpoch epoch{};
    bool external = false;
    bool ready = false;
};
struct NativeTraitCommit {
    domain::TraitTextEdits rows;
    domain::GameEpoch epoch;
    bool external;
};
std::mutex native_trait_mutex;
NativeTraitPublication native_trait_publication;
std::optional<NativeTraitCommit> native_trait_commit;
std::atomic<bool> native_trait_active{false};
std::uintptr_t native_trait_module = 0;
platform::windows::Hook native_trait_select_hook{};
platform::windows::Hook native_trait_dialog_hook{};
platform::windows::Hook native_trait_close_hook{};
platform::windows::Hook native_trait_input_hook{};
platform::windows::Hook native_trait_input_event_hook{};
platform::windows::Hook native_trait_input_click_hook{};
platform::windows::Hook native_trait_destroy_hook{};
platform::windows::Hook native_trait_state_close_hook{};

enum class TraitPage { Catalog, Detail, Effects, Parameters, Name, Description, Message };
struct NativeTraitFrame {
    std::vector<TraitListRow> visible;
    std::array<std::uintptr_t, 2> native_rows{};
    std::optional<std::size_t> selection;
    std::array<void*, 8> text_nodes{};
    void* dialog = nullptr;
    TraitPage page = TraitPage::Catalog;
    domain::TraitEffect effect_before{};
};
struct NativeTraitSession {
    NativeTraitPublication source;
    std::vector<std::unique_ptr<NativeTraitFrame>> frames;
    NativeTraitFrame& frame() { return *frames.back(); }
    std::optional<NativeTraitDraft> draft;
    bool text_input_open = false;
    void* text_input_owner = nullptr;
    void* text_input_layout = nullptr;
    std::size_t row = 0, slot = 0;
    domain::TraitEffect effect{};
    std::string message;
} native_trait_session;

std::wstring native_trait_wide(const std::string& text) {
    std::wstring result(text.size(), L'\0');
    if (!text.empty()) {
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), static_cast<int>(result.size()));
        result.resize(static_cast<std::size_t>(length));
    }
    return result;
}

template<class T>
T& trait_native_field(void* object, std::size_t offset) {
    return *reinterpret_cast<T*>(static_cast<std::byte*>(object) + offset);
}

void show_native_trait_page(TraitPage page);

void restore_native_trait_dialog(void* dialog) {
    auto* vtable = trait_native_field<std::uintptr_t*>(dialog, 0);
    void* controller = trait_native_field<void*>(dialog, 0x238);
    // The retained parent's controller is already on the native modal stack.
    // Show without pushing it again: a duplicate push hides that same parent.
    const bool managed = trait_native_field<bool>(dialog, 0x593);
    trait_native_field<bool>(dialog, 0x593) = false;
    reinterpret_cast<void(*)(void*, bool)>(vtable[0x2C0 / 8])(dialog, true);
    trait_native_field<bool>(dialog, 0x593) = managed;
    trait_native_field<bool>(controller, 0x1CC) = managed;
    reinterpret_cast<void(*)(void*, bool)>(vtable[0x120 / 8])(dialog, true);
}

void native_trait_state_closed(void* state, void* event) {
    if (trait_native_field<std::uintptr_t>(state, 0) == native_trait_module + 0x257E0B8) {
        void* dialog = trait_native_field<void*>(state, 0x478);
        for (const auto& frame : native_trait_session.frames) {
            if (frame->dialog != dialog) continue;
            // Pushing a child also finishes the parent's hide animation and
            // emits event 100. That event must not pop the child state.
            if (frame.get() != &native_trait_session.frame() ||
                trait_native_field<int>(dialog, 0x230) == -1) return;
            break;
        }
    }
    reinterpret_cast<void(*)(void*, void*)>(native_trait_state_close_hook.original)(state, event);
}

void native_trait_state_destroyed(void* state, void* context) {
    void* dialog = trait_native_field<void*>(state, 0x478);
    for (const auto& frame : native_trait_session.frames) {
        if (frame->dialog != dialog) continue;
        // CDataSelect's destructor deletes its list before the base layout
        // unregisters. Remove this popup while its children are still alive.
        auto* vtable = trait_native_field<std::uintptr_t*>(dialog, 0);
        reinterpret_cast<void(*)(void*)>(vtable[0x50 / 8])(dialog);
        break;
    }
    reinterpret_cast<void(*)(void*, void*)>(native_trait_destroy_hook.original)(state, context);
}

void native_trait_dialog_closed(void* dialog, int result) {
    auto& session = native_trait_session;
    for (const auto& frame : session.frames) {
        if (frame->dialog != dialog) continue;
        // Only the top popup may close. Native keyboard/back events also use
        // this entry point, independently of our button callbacks.
        if (frame.get() != &session.frame()) return;
        if (session.text_input_open) {
            // The native footer also maps Backspace to cancel. While a text
            // popup owns input, leave that key to CInputString's deletion path.
            if (session.text_input_layout && result == 0x7FFFFFFE &&
                (GetKeyState(VK_BACK) & 0x8000) != 0) return;
            if (session.text_input_layout) {
                trait_native_field<int>(session.text_input_layout, 0x208) = result == 0x7FFFFFFD;
                trait_native_field<int>(session.text_input_layout, 0x204) = 1;
            }
            return;
        }
        break;
    }
    reinterpret_cast<void(*)(void*, int)>(native_trait_close_hook.original)(dialog, result);
}

bool native_trait_input_initialized(void* state, void* context) {
    const bool result = reinterpret_cast<bool(*)(void*, void*)>(native_trait_input_hook.original)(state, context);
    auto& session = native_trait_session;
    if (session.text_input_open &&
        (session.frame().page == TraitPage::Name || session.frame().page == TraitPage::Description)) {
        session.text_input_layout = trait_native_field<void*>(state, 0x478);
        // The input state is the textbox inside our popup. Restore the owning
        // dialog after the engine suspends it so its backdrop/footer remain.
        restore_native_trait_dialog(session.text_input_owner);
    }
    return result;
}

bool native_trait_input_event(void* layout, void* event) {
    if (layout == native_trait_session.text_input_layout) {
        // CInputString normally cancels on a click outside the textbox. Here
        // the popup footer owns acceptance/cancellation, so let it receive
        // that click without first popping the input state as cancelled.
        return reinterpret_cast<bool(*)(void*, void*)>(native_trait_module + 0x1EDFC50)(layout, event);
    }
    return reinterpret_cast<bool(*)(void*, void*)>(native_trait_input_event_hook.original)(layout, event);
}

void native_trait_input_clicked(void* layout) {
    auto& session = native_trait_session;
    if (layout == session.text_input_layout) {
        void* mouse = reinterpret_cast<void*(*)()>(native_trait_module + 0xD4D40)();
        const int x = trait_native_field<int>(mouse, 0x48);
        const int y = trait_native_field<int>(mouse, 0x4C);
        const int left = trait_native_field<int>(layout, 0x20C);
        const int top = trait_native_field<int>(layout, 0x210);
        if (x < left || x > left + trait_native_field<int>(layout, 0x214) ||
            y < top || y > top + trait_native_field<int>(layout, 0x218)) {
            // The textbox captures mouse release. Route releases over its own
            // footer using the controls' live logical positions, not screen pixels.
            void* footer = trait_native_field<void*>(session.text_input_owner, 0x258);
            void* buttons = trait_native_field<void*>(footer, 0x1A8);
            for (int index = 0; index < 6; ++index) {
                void* button = trait_native_field<void*>(buttons, 0x1A0 + index * 8);
                if (!button || !reinterpret_cast<int(*)(void*)>(native_trait_module + 0x1EDE760)(button)) continue;
                float bx = 0, by = 0;
                for (void* parent = button; parent; parent = trait_native_field<void*>(parent, 8)) {
                    bx += trait_native_field<float>(parent, 0x20);
                    by += trait_native_field<float>(parent, 0x24);
                }
                if (x >= bx && x <= bx + trait_native_field<int>(button, 0x28) &&
                    y >= by && y <= by + trait_native_field<int>(button, 0x2C)) {
                    auto* vtable = trait_native_field<std::uintptr_t*>(button, 0);
                    reinterpret_cast<void(*)(void*)>(vtable[0x78 / 8])(button);
                    break;
                }
            }
            return;
        }
    }
    reinterpret_cast<void(*)(void*)>(native_trait_input_click_hook.original)(layout);
}

void begin_native_trait_text_input(void* dialog) {
    auto& session = native_trait_session;
    if (session.frame().page == TraitPage::Name || session.frame().page == TraitPage::Description) {
        session.text_input_open = true;
        session.text_input_owner = dialog;
        session.draft->input_text(native_trait_module, session.frame().page == TraitPage::Description,
            [dialog](NativeTraitInputResult result, domain::TraitTextValidation validation) {
                auto& editor = native_trait_session;
                editor.text_input_open = false;
                editor.text_input_owner = nullptr;
                editor.text_input_layout = nullptr;
                if (result == NativeTraitInputResult::InvalidText || result == NativeTraitInputResult::UnrepresentableText) {
                    editor.message = result == NativeTraitInputResult::InvalidText ?
                        domain::trait_text_validation_text(validation) : "게임에서 입력할 수 없는 글자가 포함되어 있습니다.";
                    show_native_trait_page(TraitPage::Message);
                    return;
                }
                editor.frame().selection = result == NativeTraitInputResult::Accepted ? std::optional<std::size_t>{0} : std::nullopt;
                native_trait_dialog_closed(dialog, result == NativeTraitInputResult::Accepted ? 0x7FFFFFFD : 0x7FFFFFFE);
            });
    }
}

void native_trait_dialog_initialized(void* dialog, void* config) {
    reinterpret_cast<void(*)(void*, void*)>(native_trait_dialog_hook.original)(dialog, config);
    auto& session = native_trait_session;
    if (trait_native_field<const void*>(config, 0x18) != trait_list_columns.data()) return;
    session.frame().dialog = dialog;
    auto* dialog_vtable = trait_native_field<std::uintptr_t*>(dialog, 0);
    reinterpret_cast<void(*)(void*, int)>(dialog_vtable[0x80 / 8])(
        dialog, 5 + static_cast<int>(session.frames.size()));
    const bool list_page = session.frame().page == TraitPage::Catalog || session.frame().page == TraitPage::Effects;
    // Keep the game's backdrop and heading. Form controls replace the list,
    // tabs and scrollbar on the editor's own detail pages. Keep the native
    // octagonal confirm/back footer, including its layout and input controls.
    if (!list_page) for (std::size_t offset : {0x250u, 0x268u, 0x270u}) {
        void* part = trait_native_field<void*>(dialog, offset);
        if (part) {
            auto* vtable = trait_native_field<std::uintptr_t*>(part, 0);
            reinterpret_cast<void(*)(void*, bool)>(vtable[0x108 / 8])(part, false);
        }
    }
    std::function<void(void*)> footer_clicked = [dialog](void* button) {
        if (native_trait_session.frame().dialog != dialog) return;
        void* current_config = trait_native_field<void*>(dialog, 0x290);
        const int confirm_id = trait_native_field<int>(current_config, 0x98) != 0 ? 1 : 0;
        const bool confirmed = trait_native_field<int>(button, 0x88) == confirm_id;
        auto& current = native_trait_session;
        if (current.frame().page == TraitPage::Catalog && confirmed) {
            if (current.frame().selection) {
                current.row = *current.frame().selection;
                current.draft.emplace((*current.source.rows)[current.row]);
                show_native_trait_page(TraitPage::Detail);
            }
            return;
        }
        current.frame().selection = confirmed ? std::optional<std::size_t>{0} : std::nullopt;
        auto* vtable = trait_native_field<std::uintptr_t*>(dialog, 0);
        reinterpret_cast<void(*)(void*, int)>(vtable[0x2C8 / 8])(
            dialog, confirmed ? 0x7FFFFFFD : 0x7FFFFFFE);
    };
    reinterpret_cast<void(*)(void*, void*)>(native_trait_module + 0x1697A50)(
        trait_native_field<void*>(dialog, 0x258), &footer_clicked);
    if (list_page) {
        reinterpret_cast<void(*)(void*, bool)>(native_trait_module + 0x38550)(dialog, true);
        return;
    }
    const bool compact = session.frame().page == TraitPage::Name || session.frame().page == TraitPage::Parameters;
    if (compact) {
        // Scale the background subtree, including the generated texture children.
        // Resizing its container alone leaves those textures at their original size.
        void* resource_group = static_cast<std::byte*>(dialog) + 0x140;
        auto** panes = trait_native_field<void**>(resource_group, 8);
        const int count = trait_native_field<int>(resource_group, 0x10);
        void* background = panes[1];
        const auto xy = reinterpret_cast<std::uint64_t(*)(void*)>(native_trait_module + 0x1D16250)(background);
        using SetPaneProperty = void(*)(void*, int, double);
        reinterpret_cast<SetPaneProperty>(native_trait_module + 0xAAB2E0)(background, 6, 0.6);
        reinterpret_cast<SetPaneProperty>(native_trait_module + 0xAAB2E0)(background, 7, 0.5);
        reinterpret_cast<void(*)(void*, float, float)>(native_trait_module + 0x1D12640)(background,
            static_cast<float>(static_cast<std::int32_t>(xy)) * 0.6f + 384.0f,
            static_cast<float>(static_cast<std::int32_t>(xy >> 32)) * 0.5f + 270.0f);
        for (int index = 5; index < count; ++index)
            reinterpret_cast<void(*)(void*, bool)>(native_trait_module + 0x1D15BC0)(panes[index], false);
        void* footer = trait_native_field<void*>(dialog, 0x258);
        // This operation translates the existing footer; it does not set an
        // absolute position. Keep its native horizontal alignment.
        reinterpret_cast<void(*)(void*, int, int)>(native_trait_module + 0x1EE0760)(footer, 0, -300);
    }
    // A form does not select list rows; enable confirm independently of the
    // hidden list's selection count.
    reinterpret_cast<void(*)(void*, bool)>(native_trait_module + 0x38550)(dialog, true);
    // CLayout updates and destroys interactive children through its descriptor
    // group, not merely through the rendered scene/parent relationship.
    void* group = static_cast<std::byte*>(dialog) + 0x140;
    const int first_control = trait_native_field<int>(group, 0x10);
    const int form_count = static_cast<int>(session.frame().visible.size()) - 1;
    if (form_count > 0) {
        void* manager = reinterpret_cast<void*(*)()>(native_trait_module + 0x14ED0)();
        void* allocator = trait_native_field<void*>(manager, 0x28);
        auto* allocation_vtable = trait_native_field<std::uintptr_t*>(allocator, 0);
        const std::array<std::uint64_t, 2> allocation_tag{0x37, 0};
        for (std::size_t offset : {std::size_t{0}, std::size_t{8}}) {
            const std::size_t stride = offset == 0 ? 0x60 : sizeof(void*);
            const std::size_t old_size = static_cast<std::size_t>(first_control) * stride;
            const std::size_t new_size = static_cast<std::size_t>(first_control + form_count) * stride;
            auto* previous = trait_native_field<std::byte*>(group, offset);
            auto* extended = reinterpret_cast<std::byte*(*)(void*, std::size_t, const void*)>(
                allocation_vtable[0x28 / 8])(allocator, new_size, allocation_tag.data());
            std::copy_n(previous, old_size, extended);
            std::fill(extended + old_size, extended + new_size, std::byte{});
            trait_native_field<std::byte*>(group, offset) = extended;
            reinterpret_cast<void(*)(void*, void*)>(allocation_vtable[0x58 / 8])(allocator, previous);
        }
        trait_native_field<int>(group, 0x10) = first_control + form_count;
    }
    // Resource-based dialogs bypass the descriptor factory. Create native children
    // after the resource layout exists, using the same constructors as type 14/1.
    int y = session.frame().page == TraitPage::Detail ? 240 : 430;
    const int button_x = compact ? 600 : 280;
    const int text_x = compact ? 850 : 530;
    for (std::size_t index = 0; index < session.frame().visible.size(); ++index) {
        const auto* row = &session.frame().visible[index];
        const bool confirm = (session.frame().page == TraitPage::Detail && row->action == 8) ||
            (session.frame().page == TraitPage::Parameters && row->action == 2) ||
            session.frame().page == TraitPage::Name || session.frame().page == TraitPage::Description || session.frame().page == TraitPage::Message;
        if (confirm && session.frame().page != TraitPage::Message) continue;
        const std::size_t action = row->action;
        if (!confirm) {
            // Resource children need a pane in the parent's scene, just like the
            // descriptor factory's type-14 controls. Parenting the C++ object alone
            // registers input/lifetime but does not attach its rendered resource.
            using CreatePane = void*(*)(void*, int, int, int, int);
            void* pane = reinterpret_cast<CreatePane>(native_trait_module + 0x1ED8200)(
                dialog, button_x, y, 220, 48);
            void* button = reinterpret_cast<void*(*)(std::size_t)>(native_trait_module + 0xEE70)(0x1E0);
            reinterpret_cast<void(*)(void*)>(native_trait_module + 0x1694A10)(button);
            using InitializeButton = void(*)(void*, int, int, int, int, void*);
            reinterpret_cast<InitializeButton>(native_trait_module + 0x16945C0)(button,
                button_x, y, 220, 48, dialog);
            auto* button_vtable = trait_native_field<std::uintptr_t*>(button, 0);
            reinterpret_cast<void(*)(void*, const wchar_t*)>(button_vtable[0x298 / 8])(
                button, row->label.c_str());
            const int control_id = first_control + static_cast<int>(index);
            trait_native_field<int>(button, 0x88) = control_id;
            trait_native_field<int>(button, 0x8C) = 1;
            auto* descriptor = trait_native_field<std::byte*>(group, 0) + control_id * 0x60;
            trait_native_field<int>(descriptor, 0) = 14;
            trait_native_field<int>(descriptor, 4) = button_x;
            trait_native_field<int>(descriptor, 8) = y;
            trait_native_field<int>(descriptor, 0xC) = 220;
            trait_native_field<int>(descriptor, 0x10) = 48;
            trait_native_field<std::uint64_t>(descriptor, 0x18) = 0xFFFF;
            trait_native_field<const wchar_t*>(descriptor, 0x20) = row->label.c_str();
            trait_native_field<void**>(group, 8)[control_id] = pane;
            reinterpret_cast<void(*)(void*, int, void*)>(native_trait_module + 0x1D16E10)(
                group, control_id, button);
            std::function<void(void*)> clicked = [dialog, action](void*) {
                auto& current = native_trait_session;
                if (current.frame().dialog != dialog || current.text_input_open) return;
                if (current.frame().page == TraitPage::Detail) {
                    if (action < 2) {
                        show_native_trait_page(action == 0 ? TraitPage::Name : TraitPage::Description);
                    } else {
                        current.slot = action - 2;
                        current.effect = (*current.draft->row().effects)[current.slot];
                        show_native_trait_page(TraitPage::Effects);
                    }
                } else if (current.frame().page == TraitPage::Parameters) {
                    current.text_input_open = true;
                    current.text_input_owner = dialog;
                    current.draft->input_number(native_trait_module,
                        action == 0 ? current.effect.value1 : current.effect.value2,
                        [dialog, action](bool confirmed, std::uint16_t value) {
                            auto& editor = native_trait_session;
                            editor.text_input_open = false;
                            editor.text_input_owner = nullptr;
                            if (confirmed) (action == 0 ? editor.effect.value1 : editor.effect.value2) = value;
                            auto& text = editor.frame().visible[action].text;
                            text = std::to_wstring(action == 0 ? editor.effect.value1 : editor.effect.value2);
                            reinterpret_cast<void(*)(void*, const wchar_t*)>(native_trait_module + 0x1D154E0)(
                                editor.frame().text_nodes[action], text.c_str());
                            // The keypad leaves this dialog visible. Only resume input;
                            // showing it again registers its native callbacks twice.
                            auto* vtable = trait_native_field<std::uintptr_t*>(dialog, 0);
                            reinterpret_cast<void(*)(void*, bool)>(vtable[0x120 / 8])(dialog, true);
                        });
                }
            };
            // CButton emits 0x7FFFFFFE on activation; 0x44C is the tab-change event.
            reinterpret_cast<void(*)(void*, int, void*)>(native_trait_module + 0xEF40)(button, 0x7FFFFFFE, &clicked);
        }
        {
            // CreateText does not grow the resource's node array. The native
            // descriptor path first reserves a node via CreatePane (1ED8200).
            // Mirror that reservation before adding text to this loaded layout.
            void* resource = trait_native_field<void*>(dialog, 0x98);
            void* scene = trait_native_field<void*>(resource, 0x10);
            void* nodes = static_cast<std::byte*>(scene) + 0x18;
            auto* nodes_vtable = trait_native_field<std::uintptr_t*>(nodes, 0);
            const auto count = reinterpret_cast<std::size_t(*)(void*)>(nodes_vtable[0x18 / 8])(nodes);
            reinterpret_cast<void(*)(void*, std::size_t)>(nodes_vtable[0x60 / 8])(nodes, count + 1);
            using CreateText = void*(*)(void*, int, int, int, int, const wchar_t*);
            void* text = reinterpret_cast<CreateText>(native_trait_module + 0x1EDAB20)(dialog, text_x, y + 12, compact ? 450 : 1110,
                session.frame().page == TraitPage::Detail && row->action == 1 ? 100 : 60, row->text.c_str());
            // The descriptor's text flags encode font size in bits 25..31;
            // bit 0 selects top-left alignment. Palette 1 is the game's dark ink.
            reinterpret_cast<void(*)(void*, std::uint64_t)>(native_trait_module + 0x1D13550)(
                text, (std::uint64_t{24} << 25) | 1);
            reinterpret_cast<void(*)(void*, int)>(native_trait_module + 0x1D167B0)(text, 1);
            if (action < session.frame().text_nodes.size()) session.frame().text_nodes[action] = text;
        }
        y += session.frame().page == TraitPage::Detail && row->action == 1 ? 112 : 72;
    }
    if (session.frame().page == TraitPage::Name || session.frame().page == TraitPage::Description)
        begin_native_trait_text_input(dialog);

}

void native_trait_selected(void* dialog, void* control) {
    auto& session = native_trait_session;
    void* config = trait_native_field<void*>(dialog, 0x290);
    if (native_trait_active.load(std::memory_order_acquire) && config &&
        trait_native_field<const void*>(config, 0x18) == trait_list_columns.data()) {
        if (session.frame().dialog != dialog || session.text_input_open) return;
        void* list = trait_native_field<void*>(dialog, 0x250);
        void* core = trait_native_field<void*>(list, 0x140);
        auto* handle = trait_native_field<const std::uint32_t*>(core, 0x168);
        if (handle) {
            auto** heads = *reinterpret_cast<void***>(native_trait_module + 0x3B94AC0);
            void* node = heads[*handle];
            if (node) {
                const int index = trait_native_field<int>(node, 0);
                auto** data = trait_native_field<TraitListRow**>(core, 0x1D0);
                session.frame().selection = data[index]->action;
            }
        }
        if (session.frame().selection) {
            const std::size_t action = *session.frame().selection;
            if (session.frame().page == TraitPage::Catalog) {
                session.row = action;
                session.draft.emplace((*session.source.rows)[action]);
                show_native_trait_page(TraitPage::Detail);
            } else if (session.frame().page == TraitPage::Effects) {
                const auto previous = session.effect;
                session.effect.effect_id = static_cast<std::uint16_t>(action);
                if (action == 0) session.effect = {};
                show_native_trait_page(TraitPage::Parameters);
                session.frame().effect_before = previous;
            }
        }
        return;
    }
    reinterpret_cast<void(*)(void*, void*)>(native_trait_select_hook.original)(dialog, control);
}

void finish_native_trait_page(void* state) {
    auto& session = native_trait_session;
    auto& frame = session.frame();
    const bool accepted = trait_native_field<int>(state, 8) == 0x7FFFFFFD;
    if (frame.page == TraitPage::Parameters && !accepted) session.effect = frame.effect_before;
    if (frame.page == TraitPage::Effects) {
        if (accepted) session.draft->set_effect(session.slot, session.effect);
        else session.effect = frame.effect_before;
    }
    if (frame.page == TraitPage::Detail && accepted) {
        NativeTraitCommit commit{*session.source.rows, session.source.epoch, session.source.external};
        commit.rows[session.row] = session.draft->row();
        // Retain the accepted values in the catalog while the runtime applies them.
        session.source.rows = own::make_shared<domain::TraitTextEdits>(commit.rows);
        std::lock_guard lock(native_trait_mutex);
        native_trait_commit.emplace(std::move(commit));
    }
    reinterpret_cast<void(*)(void*)>(native_trait_module + 0x72DD0)(frame.native_rows.data());
    session.frames.pop_back();
    if (session.frames.empty()) {
        native_trait_active.store(false, std::memory_order_release);
        return;
    }
    auto& parent = session.frame();
    if (parent.page == TraitPage::Detail) {
        parent.visible[0].text = native_trait_wide(session.draft->name());
        parent.visible[1].text = native_trait_wide(session.draft->description());
        for (std::size_t index = 2; index + 1 < parent.visible.size(); ++index)
            parent.visible[index].text = native_trait_wide(session.draft->effect_description(index - 2, *session.source.effects));
        for (std::size_t index = 0; index + 1 < parent.visible.size(); ++index)
            reinterpret_cast<void(*)(void*, const wchar_t*)>(native_trait_module + 0x1D154E0)(
                parent.text_nodes[index], parent.visible[index].text.c_str());
    } else if (parent.page == TraitPage::Catalog && accepted) {
        const auto& row = (*session.source.rows)[session.row];
        parent.visible[session.row].label = native_trait_wide(row.name.empty() ? row.original_name : row.name);
        parent.visible[session.row].text = native_trait_wide(row.description.empty() ? row.original_description : row.description);
    }
    restore_native_trait_dialog(parent.dialog);
    if (parent.page == TraitPage::Name || parent.page == TraitPage::Description)
        begin_native_trait_text_input(parent.dialog);
}

void show_native_trait_page(TraitPage page) {
    auto& session = native_trait_session;
    if (!session.frames.empty()) {
        void* parent = session.frame().dialog;
        auto* vtable = trait_native_field<std::uintptr_t*>(parent, 0);
        reinterpret_cast<void(*)(void*, bool)>(vtable[0x120 / 8])(parent, false);
    }
    session.frames.push_back(std::make_unique<NativeTraitFrame>());
    session.frame().page = page;
    session.frame().effect_before = session.effect;
    if (page == TraitPage::Catalog) {
        for (std::size_t index = 0; index < session.source.rows->size(); ++index) {
            const auto& row = (*session.source.rows)[index];
            session.frame().visible.push_back({native_trait_wide(row.name.empty() ? row.original_name : row.name),
                native_trait_wide(row.description.empty() ? row.original_description : row.description), index});
        }
    } else if (page == TraitPage::Detail) {
        session.frame().visible.push_back({L"이름", native_trait_wide(session.draft->name()), 0});
        session.frame().visible.push_back({L"설명", native_trait_wide(session.draft->description()), 1});
        if (session.draft->row().effects) {
            for (std::size_t slot = 0; slot < 6; ++slot) {
                session.frame().visible.push_back({L"효과 종류 " + std::to_wstring(slot + 1),
                    native_trait_wide(session.draft->effect_description(slot, *session.source.effects)), slot + 2});
            }
        }
        session.frame().visible.push_back({L"결정", L"이 기재의 편집 내용을 게임에 적용합니다.", 8});
    } else if (page == TraitPage::Effects) {
        session.frame().visible.push_back({L"(없음)", L"이 효과 슬롯을 비웁니다.", 0});
        for (const auto& [id, definition] : *session.source.effects) {
            if (id != 0) session.frame().visible.push_back({std::to_wstring(id), native_trait_wide(definition.text), id});
        }
        std::sort(session.frame().visible.begin(), session.frame().visible.end(),
            [](const auto& left, const auto& right) { return left.action < right.action; });
    } else if (page == TraitPage::Parameters) {
        session.frame().visible.push_back({L"값", std::to_wstring(session.effect.value1), 0});
        session.frame().visible.push_back({L"P", std::to_wstring(session.effect.value2), 1});
        session.frame().visible.push_back({L"결정", L"효과 종류와 값을 편집 중인 기재에 반영합니다.", 2});
    } else if (page == TraitPage::Name || page == TraitPage::Description) {
        session.frame().visible.push_back({L"결정", L"", 0});
    } else session.frame().visible.push_back({L"확인", native_trait_wide(session.message), 0});

    // The count at config+0x10 limits selection; it does not enumerate rows.
    // Supply the engine's linked list at config+0. Its nodes own only row pointers.
    struct RowNode { TraitListRow* row; RowNode* next; };
    std::vector<RowNode> nodes(session.frame().visible.size());
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        nodes[index] = {&session.frame().visible[index], index + 1 < nodes.size() ? &nodes[index + 1] : nullptr};
    }
    session.frame().native_rows = {native_trait_module + 0x25869F8, 0};
    reinterpret_cast<void(*)(void*)>(native_trait_module + 0x10D10)(session.frame().native_rows.data());
    RowNode* first = nodes.empty() ? nullptr : nodes.data();
    RowNode* end = nullptr;
    reinterpret_cast<void(*)(void*, RowNode**, RowNode**)>(native_trait_module + 0x30340)(
        session.frame().native_rows.data(), &first, &end);

    // Use the engine constructor/destructor for every embedded callback and
    // string in this configuration. State creation deep-copies it before return.
    alignas(16) std::array<std::byte, 0x2580> config{};
    reinterpret_cast<void(*)(void*)>(native_trait_module + 0x1A870)(config.data());
    trait_native_field<void*>(config.data(), 0) = session.frame().native_rows.data();
    trait_native_field<const void*>(config.data(), 8) = trait_list_tabs.data();
    trait_native_field<int>(config.data(), 0x10) = static_cast<int>(session.frame().visible.size());
    trait_native_field<const void*>(config.data(), 0x18) = trait_list_columns.data();
    trait_native_field<int>(config.data(), 0x20) = 1; // Single selection; no officer assignment.
    trait_native_field<bool>(config.data(), 0x9C) = true;
    trait_native_field<const wchar_t*>(config.data(), 0x130) = L"기재 편집";
    trait_native_field<const wchar_t*>(config.data(), 0x138) = L"편집할 항목을 선택해 주십시오";
    std::function<void*(int)> get_row = [frame = &session.frame()](int index) -> void* {
        auto& rows = frame->visible;
        // The engine supplies this index, including terminal lookup probes.
        if (index < 0 || static_cast<std::size_t>(index) >= rows.size()) return nullptr;
        return &rows[static_cast<std::size_t>(index)];
    };
    reinterpret_cast<void(*)(void*, const void*)>(native_trait_module + 0x10830)(config.data() + 0x48, &get_row);
    std::function<void(void*)> completed = finish_native_trait_page;
    void* manager = reinterpret_cast<void*(*)()>(native_trait_module + 0xF0B0)();
    using Enqueue = void(*)(void*, const char*, void*, std::function<void(void*)>*);
    reinterpret_cast<Enqueue>(native_trait_module + 0x5D530)(manager,
        "CDataSelectState<PersonalityData>", config.data(), &completed);
    reinterpret_cast<void(*)(void*)>(native_trait_module + 0x179F0)(config.data());
}

} // namespace

bool install_native_trait_editor(std::uintptr_t module) {
    const auto hook = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x3C190),
        reinterpret_cast<void*>(&native_trait_selected));
    const auto dialog = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x392D0),
        reinterpret_cast<void*>(&native_trait_dialog_initialized));
    const auto close = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x294B0),
        reinterpret_cast<void*>(&native_trait_dialog_closed));
    const auto input = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x1EC60B0),
        reinterpret_cast<void*>(&native_trait_input_initialized));
    const auto input_event = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x1EC65E0),
        reinterpret_cast<void*>(&native_trait_input_event));
    const auto input_click = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x1ECA700),
        reinterpret_cast<void*>(&native_trait_input_clicked));
    const auto destroy = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x5C2B0),
        reinterpret_cast<void*>(&native_trait_state_destroyed));
    const auto state_close = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x11B10),
        reinterpret_cast<void*>(&native_trait_state_closed));
    if (!hook || !dialog || !close || !input || !input_event || !input_click || !destroy || !state_close) return false;
    native_trait_module = module;
    native_trait_select_hook = *hook;
    native_trait_dialog_hook = *dialog;
    native_trait_close_hook = *close;
    native_trait_input_hook = *input;
    native_trait_input_event_hook = *input_event;
    native_trait_input_click_hook = *input_click;
    native_trait_destroy_hook = *destroy;
    native_trait_state_close_hook = *state_close;
    if (platform::windows::enable_hook(native_trait_select_hook) != 0) return false;
    if (platform::windows::enable_hook(native_trait_dialog_hook) == 0) {
        if (platform::windows::enable_hook(native_trait_close_hook) == 0) {
            if (platform::windows::enable_hook(native_trait_input_hook) == 0) {
                if (platform::windows::enable_hook(native_trait_destroy_hook) == 0) {
                    if (platform::windows::enable_hook(native_trait_state_close_hook) == 0) {
                        if (platform::windows::enable_hook(native_trait_input_event_hook) == 0) {
                            if (platform::windows::enable_hook(native_trait_input_click_hook) == 0) return true;
                            platform::windows::disable_hook(native_trait_input_event_hook);
                        }
                        platform::windows::disable_hook(native_trait_state_close_hook);
                    }
                    platform::windows::disable_hook(native_trait_destroy_hook);
                }
                platform::windows::disable_hook(native_trait_input_hook);
            }
            platform::windows::disable_hook(native_trait_close_hook);
        }
        platform::windows::disable_hook(native_trait_dialog_hook);
    }
    platform::windows::disable_hook(native_trait_select_hook);
    return false;
}

void open_native_trait_editor() {
    if (native_trait_active.load(std::memory_order_acquire)) return;
    {
        std::lock_guard lock(native_trait_mutex);
        if (!native_trait_publication.ready || native_trait_commit) return;
        native_trait_session.source = native_trait_publication;
    }
    native_trait_active.store(true, std::memory_order_release);
    show_native_trait_page(TraitPage::Catalog);
}

void update_native_trait_editor(FeatureMenu& menu, services::UiBridge& bridge) {
    std::optional<NativeTraitCommit> commit;
    {
        std::lock_guard lock(native_trait_mutex);
        native_trait_publication = {menu.snapshot.traits.embedded_rows, menu.trait_effects, menu.epoch,
            menu.snapshot.traits.catalog == domain::TraitApplyStatus::ExternalRuntime,
            menu.ready && !menu.pending && !menu.trait_text_editor.save_pending &&
            !menu.trait_text_editor.save_rows && menu.snapshot.traits.embedded_rows &&
            !menu.snapshot.traits.embedded_rows->empty() && menu.trait_effects};
        if (native_trait_commit && (menu.pending || menu.trait_text_editor.save_pending ||
            menu.trait_text_editor.save_rows)) return;
        commit = std::move(native_trait_commit);
        native_trait_commit.reset();
        if (commit) native_trait_publication.ready = false;
    }
    if (!commit) return;
    if (commit->epoch != menu.epoch) {
        bridge.publish_notice("게임이 변경되어 기재 편집을 적용하지 않았습니다.");
        return;
    }
    auto rows = own::make_shared<domain::TraitTextEdits>(std::move(commit->rows));
    if (commit->external) {
        auto& editor = menu.trait_text_editor;
        editor.save_rows = std::move(rows);
        editor.save_embedded = true;
        editor.apply_external = true;
        editor.save_status.reset();
    } else {
        const domain::RequestId request{++menu.next_request};
        const auto accepted = bridge.commands.push({request, commit->epoch,
            domain::ApplyTraitTextEdits{true, std::move(rows)}});
        if (accepted) {
            menu.pending = request;
            menu.pending_notice = "기재 편집 적용을 요청했습니다.";
        } else {
            menu.error = accepted.error();
            bridge.publish_notice("기재 편집 적용 요청을 전달하지 못했습니다.");
        }
    }
}
