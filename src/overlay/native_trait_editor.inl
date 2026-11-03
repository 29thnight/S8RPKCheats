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

enum class TraitPage { Catalog, Detail, Effects, Voices, Parameters, Name, Description, Message, Jewels, JewelTree };
struct NativeTraitFrame {
    std::vector<TraitListRow> visible;
    std::array<std::uintptr_t, 2> native_rows{};
    std::optional<std::size_t> selection;
    std::array<void*, 10> text_nodes{};
    void* dialog = nullptr;
    TraitPage page = TraitPage::Catalog;
    domain::TraitEffect effect_before{};
    alignas(16) std::array<std::byte, 0x50> jewel_state{};
    std::array<std::byte, 0x138> jewel_context{};
    std::array<std::byte, 0x68> jewel_owner{};
    std::array<std::byte, 0x18> jewel_root{};
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

struct NativeJewelEdit {
    domain::GameEpoch epoch{};
    domain::SecondaryJewelMask mask{};
    std::optional<std::array<std::uint8_t, 34>> bitmap;
    std::vector<domain::SetSecondaryJewels> commands;
    bool ready = false;
    void* owner = nullptr; // Retained only while its child selection dialog is open.
    void* tree = nullptr; // Only this separately created tree has cheat behavior.
    void* editor_parent = nullptr;
    bool tree_shown = false;
    bool categories_bound = false;
    void* pending_button_window = nullptr;
    void* draft_window = nullptr;
    void* draft_state = nullptr;
    void* row = nullptr;
    bool draft_active = false;
    domain::SecondaryJewelMask initial_mask{};
    std::array<std::uint8_t, 34> initial_bitmap{};
} native_jewel_edit;
domain::SecondaryJewelMask native_jewel_draft;
platform::windows::Hook native_jewel_update_hook{}, native_jewel_click_hook{}, native_jewel_footer_hook{};
platform::windows::Hook native_jewel_back_hook{}, native_jewel_user_hook{};
platform::windows::Hook native_jewel_dialog_update_hook{}, native_jewel_visual_hook{};
platform::windows::Hook native_jewel_user_dirty_hook{}, native_jewel_user_exit_hook{};

void update_native_jewel_layout(void* layout, void* event);
void native_jewel_clicked(void* callback, void** argument);
void native_jewel_footer_clicked(void* callback, void** argument);
void native_jewel_back_clicked(void* callback, void** argument);
void update_native_jewel_user(void* window, void* event);

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

struct TraitVoiceChoice { std::uint16_t value; const wchar_t* label; };
// Lowest native trait ID for each shared voice value; labels do not follow renames.
constexpr std::array<TraitVoiceChoice, 16> trait_voice_choices{{
    {0, L"0 — 없음"}, {3, L"3 — 일신시담"}, {7, L"7 — 병귀신속"},
    {12, L"12 — 노당익장"}, {13, L"13 — 복룡"}, {14, L"14 — 낭고"},
    {19, L"19 — 만인적"}, {22, L"22 — 산도강습"}, {24, L"24 — 의협"},
    {60, L"60 — 초세지걸"}, {61, L"61 — 금강불괴"}, {99, L"99 — 대덕"},
    {117, L"117 — 금마초"}, {118, L"118 — 소패왕"},
    {139, L"139 — 수화폐월"}, {140, L"140 — 괄목"}
}};

std::wstring native_trait_voice_label(std::uint16_t value) {
    for (const auto& choice : trait_voice_choices)
        if (choice.value == value) return choice.label;
    return std::to_wstring(value);
}

void begin_native_trait_detail(std::size_t row) {
    auto& session = native_trait_session;
    session.row = row;
    session.draft.emplace((*session.source.rows)[row]);
    if (!session.draft->row().voice) {
        void* root = *reinterpret_cast<void**>(native_trait_module + 0x2E98BC8);
        void* record = trait_native_field<void*>(root,
            0x57A4D0 + session.draft->row().trait_id * sizeof(void*));
        session.draft->set_voice(trait_native_field<std::uint16_t>(record, 0x30));
    }
    show_native_trait_page(TraitPage::Detail);
}


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
        if (frame->page == TraitPage::Jewels && result == 0x7FFFFFFD) {
            // The native multi-select list owns its checked-index chain.
            native_jewel_draft = {};
            void* list = trait_native_field<void*>(dialog, 0x250);
            void* core = trait_native_field<void*>(list, 0x140);
            auto* handle = trait_native_field<const std::uint32_t*>(core, 0x168);
            auto** heads = *reinterpret_cast<void***>(native_trait_module + 0x3B94AC0);
            void* node = handle ? heads[*handle] : nullptr;
            while (node) {
                const int index = trait_native_field<int>(node, 0);
                const auto id = frame->visible[static_cast<std::size_t>(index)].action;
                native_jewel_draft.words[id >> 6] |= std::uint64_t{1} << (id & 63);
                node = trait_native_field<void*>(node, 8);
            }
        }
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
        dialog, session.frame().page == TraitPage::Jewels ? 8 : 5 + static_cast<int>(session.frames.size()));
    if (session.frame().page == TraitPage::JewelTree) {
        // The selection state supplies modal lifetime; the game's own tree supplies its UI.
        void* panes = trait_native_field<void*>(dialog, 0x148);
        void* background = trait_native_field<void*>(panes, sizeof(void*));
        reinterpret_cast<void(*)(void*, bool)>(native_trait_module + 0x1D15BC0)(background, false);
        for (std::size_t offset : {0x250u, 0x258u, 0x268u, 0x270u}) {
            void* child = trait_native_field<void*>(dialog, offset);
            if (child) {
                auto* vtable = trait_native_field<std::uintptr_t*>(child, 0);
                reinterpret_cast<void(*)(void*, bool)>(vtable[0x108 / 8])(child, false);
            }
        }
        auto& state = session.frame().jewel_state;
        trait_native_field<std::uintptr_t>(state.data(), 0) = native_trait_module + 0x26988C8;
        trait_native_field<int>(state.data(), 0x2C) = 1; // Full unlock tree, including common/status categories.
        // Init resolves its UI parent through context->owner->root->layout.
        // Keep that indirection alive in the frame and point it at this modal,
        // not the suspended city's CityUILayoutRoot.
        auto& frame = session.frame();
        trait_native_field<void*>(state.data(), 0x18) = frame.jewel_context.data();
        trait_native_field<void*>(frame.jewel_context.data(), 0x130) = frame.jewel_owner.data();
        trait_native_field<void*>(frame.jewel_owner.data(), 0x60) = frame.jewel_root.data();
        trait_native_field<void*>(frame.jewel_root.data(), 0x10) = dialog;
        // Init creates the normal native objects, but its game-state Update (which
        // charges courage and changes the selected category) is never called.
        reinterpret_cast<void(*)(void*)>(native_trait_module + 0x183B180)(state.data());
        native_jewel_edit.tree = trait_native_field<void*>(state.data(), 0x20);
        // CityStateGemUnlock::Init registers the tree but leaves it hidden;
        // normally the city state machine performs Show afterwards. Our modal
        // owns that step. The temporary COfsLayout belongs to the city scene,
        // not this modal: unregister and destroy it before it can receive events.
        void* offset_layout = trait_native_field<void*>(state.data(), 0x30);
        trait_native_field<void*>(state.data(), 0x30) = nullptr;
        alignas(16) std::array<std::byte, 0x38> offset_state{};
        trait_native_field<void*>(offset_state.data(), 0x30) = offset_layout;
        reinterpret_cast<void(*)(void*)>(native_trait_module + 0x183B0E0)(offset_state.data());
        // Init already registered the tree and its children on the inherited
        // input layer. Changing it afterwards leaves callbacks on the old layer.
        native_jewel_edit.tree_shown = false;
        native_jewel_edit.categories_bound = false;
        return;
    }
    const bool list_page = session.frame().page == TraitPage::Catalog || session.frame().page == TraitPage::Effects || session.frame().page == TraitPage::Voices ||
        session.frame().page == TraitPage::Jewels;
    if (session.frame().page == TraitPage::Jewels) {
        void* list = trait_native_field<void*>(dialog, 0x250);
        void* core = trait_native_field<void*>(list, 0x140);
        const auto* handle = trait_native_field<const std::uint32_t*>(core, 0x168);
        for (std::size_t index = 0; index < session.frame().visible.size(); ++index) {
            const auto id = static_cast<std::uint16_t>(session.frame().visible[index].action);
            if (!domain::secondary_jewel_selected(native_jewel_draft, id)) continue;
            // Use the selected-index pool's own allocator and list handle.
            void* node = reinterpret_cast<void*(*)(void*, std::uint32_t, bool)>(native_trait_module + 0x11050)(
                reinterpret_cast<void*>(native_trait_module + 0x3B94AB0), *handle, true);
            *static_cast<int*>(node) = static_cast<int>(index);
        }
    }
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
                begin_native_trait_detail(*current.frame().selection);
            }
            return;
        }
        if (current.frame().page == TraitPage::Voices) {
            if (confirmed && !current.frame().selection) return;
            if (!confirmed) current.frame().selection.reset();
        } else current.frame().selection = confirmed ? std::optional<std::size_t>{0} : std::nullopt;
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
                    } else if (action == 9) {
                        show_native_trait_page(TraitPage::Voices);
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
        y += session.frame().page == TraitPage::Detail ? (row->action == 1 ? 104 : 64) : 72;
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
        if (session.frame().page == TraitPage::Jewels) {
            reinterpret_cast<void(*)(void*, void*)>(native_trait_select_hook.original)(dialog, control);
            // Allow deciding with no checked rows, which clears forced application.
            reinterpret_cast<void(*)(void*, bool)>(native_trait_module + 0x38550)(dialog, true);
            return;
        }
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
                begin_native_trait_detail(action);
            } else if (session.frame().page == TraitPage::Voices) {
                auto* vtable = trait_native_field<std::uintptr_t*>(dialog, 0);
                reinterpret_cast<void(*)(void*, int)>(vtable[0x2C8 / 8])(dialog, 0x7FFFFFFD);
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
    if (frame.page == TraitPage::JewelTree) {
        // Native tree construction pushes its caption; Exit only destroys layouts.
        reinterpret_cast<void(*)(void*, int)>(native_trait_module + 0x16913E0)(
            *reinterpret_cast<void**>(native_trait_module + 0x350A4C0), 0);
        reinterpret_cast<void(*)(void*)>(native_trait_module + 0x183B0E0)(frame.jewel_state.data());
        native_jewel_edit.tree = nullptr;
        void* parent = native_jewel_edit.editor_parent;
        native_jewel_edit.editor_parent = nullptr;
        // The selection modal hid the enclosing EditInGameUIRootLayout, not
        // just its protagonist window. Restore its retained animation without
        // pushing another entry onto the game's modal stack.
        void* root = trait_native_field<void*>(parent, 8);
        void* animation = trait_native_field<void*>(root, 0x1C8);
        const bool managed = trait_native_field<bool>(animation, 0x1CC);
        trait_native_field<bool>(animation, 0x1CC) = false;
        const auto caption_flags = trait_native_field<std::uint32_t>(animation, 0x1C0);
        trait_native_field<std::uint32_t>(animation, 0x1C0) = caption_flags & ~0x5u;
        reinterpret_cast<void(*)(void*, int, int)>(native_trait_module + 0x1697F90)(
            animation, trait_native_field<int>(animation, 0x1C4), 0);
        trait_native_field<std::uint32_t>(animation, 0x1C0) = caption_flags;
        trait_native_field<bool>(animation, 0x1CC) = managed;
        auto* root_vtable = trait_native_field<std::uintptr_t*>(root, 0);
        reinterpret_cast<void(*)(void*, bool)>(root_vtable[0x108 / 8])(root, true);
        reinterpret_cast<void(*)(void*, bool)>(root_vtable[0x120 / 8])(root, true);
        auto* vtable = trait_native_field<std::uintptr_t*>(parent, 0);
        reinterpret_cast<void(*)(void*, bool)>(vtable[0x120 / 8])(parent, true);
    }
    if (frame.page == TraitPage::Jewels) {
        if (accepted) {
            std::lock_guard lock(native_trait_mutex);
            native_jewel_edit.mask = native_jewel_draft;
        }
        void* owner = native_jewel_edit.owner;
        native_jewel_edit.owner = nullptr;
        auto* vtable = trait_native_field<std::uintptr_t*>(owner, 0);
        reinterpret_cast<void(*)(void*, bool)>(vtable[0x120 / 8])(owner, true);
        // The tree's display controller is intentionally outside the wrapper's
        // modal stack. Restore it explicitly when its child list is popped.
        native_jewel_edit.tree_shown = false;
    }
    if (frame.page == TraitPage::Parameters && !accepted) session.effect = frame.effect_before;
    if (frame.page == TraitPage::Effects) {
        if (accepted) session.draft->set_effect(session.slot, session.effect);
        else session.effect = frame.effect_before;
    }
    if (frame.page == TraitPage::Voices && accepted && frame.selection)
        session.draft->set_voice(static_cast<std::uint16_t>(*frame.selection));
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
        for (auto& row : parent.visible) {
            if (row.action >= 2 && row.action < 8)
                row.text = native_trait_wide(session.draft->effect_description(row.action - 2, *session.source.effects));
            else if (row.action == 9) row.text = native_trait_voice_label(*session.draft->row().voice);
            if (row.action != 8)
                reinterpret_cast<void(*)(void*, const wchar_t*)>(native_trait_module + 0x1D154E0)(
                    parent.text_nodes[row.action], row.text.c_str());
        }
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
    if (page == TraitPage::Jewels) {
        std::lock_guard lock(native_trait_mutex);
        for (const auto& jewel : domain::SECONDARY_JEWEL_DEFINITIONS) {
            const bool open = ((*native_jewel_edit.bitmap)[jewel.raw_id >> 3] & (1u << (jewel.raw_id & 7))) != 0;
            if (open) session.frame().visible.push_back({native_trait_wide(jewel.name),
                L"보조계통 강제적용 · ID " + std::to_wstring(jewel.id), jewel.id});
        }
    } else if (page == TraitPage::Catalog) {
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
        session.frame().visible.push_back({L"발동 음성", native_trait_voice_label(*session.draft->row().voice), 9});
        session.frame().visible.push_back({L"결정", L"이 기재의 편집 내용을 게임에 적용합니다.", 8});
    } else if (page == TraitPage::Voices) {
        for (const auto& choice : trait_voice_choices)
            session.frame().visible.push_back({choice.label,
                choice.value == 0 ? L"캐릭터 음성을 재생하지 않습니다." : L"발동 캐릭터의 음성으로 재생합니다.", choice.value});
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
    trait_native_field<int>(config.data(), 0x20) = page == TraitPage::Jewels ? 2 : 1;
    trait_native_field<bool>(config.data(), 0x9C) = true;
    trait_native_field<const wchar_t*>(config.data(), 0x130) = page == TraitPage::Jewels ? L"보주 강제적용" :
        page == TraitPage::JewelTree ? L"보주 편집" : page == TraitPage::Voices ? L"발동 음성 선택" : L"기재 편집";
    trait_native_field<const wchar_t*>(config.data(), 0x138) = page == TraitPage::Jewels ?
        L"강제적용할 개방 보주를 선택해 주십시오" : L"편집할 항목을 선택해 주십시오";
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

// Only the editor's native gem controls read the transaction draft for paint.
void native_jewel_visual(void* gem, int animation) {
    void* layout = native_jewel_edit.tree;
    if (layout) {
        void* trees = trait_native_field<void*>(layout, 0x288);
        void* current = trait_native_field<void*>(trees,
            0x198 + sizeof(void*) * trait_native_field<std::uint8_t>(trees, 0x1F8));
        if (trait_native_field<void*>(gem, 8) == current) {
            const auto id = trait_native_field<std::uint16_t>(gem, 0x1D8);
            const auto definition = std::find_if(domain::SECONDARY_JEWEL_DEFINITIONS.begin(),
                domain::SECONDARY_JEWEL_DEFINITIONS.end(), [id](const auto& value) { return value.id == id; });
            if (definition != domain::SECONDARY_JEWEL_DEFINITIONS.end()) {
                bool open;
                {
                    std::lock_guard lock(native_trait_mutex);
                    const auto raw = definition->raw_id;
                    open = ((*native_jewel_edit.bitmap)[raw >> 3] & (1u << (raw & 7))) != 0;
                }
                // These are the native renderer's seven button-state mappings.
                static constexpr int closed[] = {0, 1, 1, 1, 3, 5, 5};
                static constexpr int opened[] = {6, 7, 7, 7, 6, 7, 8};
                const int state = trait_native_field<int>(gem, 0x1A0);
                animation = open ? opened[state] : closed[state];
            }
        }
    }
    reinterpret_cast<void(*)(void*, int)>(native_jewel_visual_hook.original)(gem, animation);
}

void update_native_jewel_layout(void* layout, void* event) {
    if (layout != native_jewel_edit.tree) {
        reinterpret_cast<void(*)(void*, void*)>(native_jewel_update_hook.original)(layout, event);
        return;
    }
    if (native_jewel_edit.owner == layout) return;
    if (!native_jewel_edit.tree_shown) {
        void* dialog = native_trait_session.frame().dialog;
        void* controller = trait_native_field<void*>(dialog, 0x238);
        if (trait_native_field<int>(controller, 0x1C8) != 1) return;
        // The wrapper's full-screen input blocker otherwise intercepts every
        // tree click. Wait until its Show has completed before removing it.
        void* blocker = trait_native_field<void*>(dialog, 0x248);
        auto* blocker_vtable = trait_native_field<std::uintptr_t*>(blocker, 0);
        reinterpret_cast<void(*)(void*, bool)>(blocker_vtable[0x108 / 8])(blocker, false);
        void* animation = trait_native_field<void*>(layout, 0x1E0);
        // Show returns without doing anything while a footer is animating.
        // Leave tree_shown false so the wrapper retries once hiding finishes.
        if (reinterpret_cast<bool(*)(void*)>(native_trait_module + 0x1698930)(animation) ||
            reinterpret_cast<bool(*)(void*)>(native_trait_module + 0x16989B0)(animation)) return;
        trait_native_field<bool>(animation, 0x1CC) = false;
        auto* vtable = trait_native_field<std::uintptr_t*>(layout, 0);
        reinterpret_cast<void(*)(void*, bool)>(vtable[0x108 / 8])(layout, true);
        // Show reattaches the shared footer only if its owner is active already.
        // The retained CDataSelect owns the shared caption stack. Showing this
        // unmanaged tree must not push another caption on every child return:
        // destroying the tree does not pop those entries, leaking the title.
        const auto animation_flags = trait_native_field<std::uint32_t>(animation, 0x1C0);
        trait_native_field<std::uint32_t>(animation, 0x1C0) = animation_flags & ~0x5u;
        reinterpret_cast<void(*)(void*, int, int)>(native_trait_module + 0x1697F90)(animation, 0x589, 0);
        trait_native_field<std::uint32_t>(animation, 0x1C0) = animation_flags;
        auto* dialog_vtable = trait_native_field<std::uintptr_t*>(dialog, 0);
        reinterpret_cast<void(*)(void*, bool)>(dialog_vtable[0x120 / 8])(dialog, true);
        // Hiding the paper's texture pane alone leaves its two native BG
        // controls hit-testable above the tree on the wrapper's input layer.
        void* backgrounds = trait_native_field<void*>(dialog, 0x180);
        void* background_handle = trait_native_field<void*>(backgrounds, 8);
        const auto background_index = trait_native_field<std::uint32_t>(background_handle, 0);
        auto** background_heads = *reinterpret_cast<void***>(native_trait_module + 0x34C8630);
        for (void* node = background_heads[background_index]; node; node = trait_native_field<void*>(node, 8)) {
            void* entry = trait_native_field<void*>(node, 0);
            const int id = trait_native_field<int>(entry, 0);
            if (id != 2 && id != 3) continue;
            void* background = trait_native_field<void*>(entry, 8);
            auto* background_vtable = trait_native_field<std::uintptr_t*>(background, 0);
            reinterpret_cast<void(*)(void*, bool)>(background_vtable[0x108 / 8])(background, false);
            reinterpret_cast<void(*)(void*, bool)>(background_vtable[0x120 / 8])(background, false);
        }
        reinterpret_cast<void(*)(void*, bool)>(vtable[0x238 / 8])(layout, true);
        native_jewel_edit.tree_shown = true;
    }
    reinterpret_cast<void(*)(void*, void*)>(native_jewel_update_hook.original)(layout, event);
    // The normal city state consumes category selection. This editor does not
    // run that state (it spends courage), so switch only the native tree UI.
    void* tree = trait_native_field<void*>(layout, 0x288);
    void* category_list = trait_native_field<void*>(trait_native_field<void*>(layout, 0x298), 0x1A0);
    const auto category = trait_native_field<std::uint8_t>(category_list, 0x26C);
    if (category != trait_native_field<std::uint8_t>(tree, 0x1F8)) {
        reinterpret_cast<void(*)(void*, std::uint8_t)>(native_trait_module + 0x1349220)(tree, category);
        reinterpret_cast<void(*)(void*)>(native_trait_module + 0x15ECD90)(layout);
        native_jewel_edit.categories_bound = false;
    }
    if (trait_native_field<int>(layout, 0x2C4) == 1)
        trait_native_field<int>(layout, 0x2C4) = 0;
    if (trait_native_field<int>(layout, 0x2C4) == 2) {
        trait_native_field<int>(layout, 0x2C4) = 0;
        void* dialog = native_trait_session.frame().dialog;
        auto* vtable = trait_native_field<std::uintptr_t*>(dialog, 0);
        reinterpret_cast<void(*)(void*, int)>(vtable[0x2C8 / 8])(dialog, 0x7FFFFFFE);
        return;
    }
    bool ready;
    std::optional<std::array<std::uint8_t, 34>> bitmap;
    {
        std::lock_guard lock(native_trait_mutex);
        ready = native_jewel_edit.ready && native_jewel_edit.commands.empty();
        bitmap = native_jewel_edit.bitmap;
    }
    if (!ready) return;
    // FooterBarButtonSetLayout owns the original protagonist-info button (ID 0).
    void* footer = trait_native_field<void*>(layout, 0x1E8);
    void* bar = trait_native_field<void*>(footer, 0x1A0);
    void* bar_layout = trait_native_field<void*>(bar, 0x1A0);
    void* button = trait_native_field<void*>(bar_layout, 0x1D0);
    reinterpret_cast<void(*)(void*, int, const wchar_t*, int)>(native_trait_module + 0x1D17190)(
        static_cast<std::byte*>(button) + 0x140, 23, L"강제적용", 0);
    static void* previous_layout = nullptr;
    static std::optional<std::array<std::uint8_t, 34>> previous_bitmap;
    if (previous_layout != layout || previous_bitmap != bitmap) {
        previous_layout = layout;
        previous_bitmap = bitmap;
        // Refresh the retained tree; the visual hook reads the draft for its gems.
        reinterpret_cast<void(*)(void*)>(native_trait_module + 0x15ECD90)(layout);
        native_jewel_edit.categories_bound = false;
    }
    if (!native_jewel_edit.categories_bound) {
        void* handle = trait_native_field<void*>(category_list, 0x68);
        const auto index = trait_native_field<std::uint32_t>(handle, 0);
        auto** heads = *reinterpret_cast<void***>(native_trait_module + 0x34C8630);
        for (void* node = heads[index]; node; node = trait_native_field<void*>(node, 8)) {
            void* button = trait_native_field<void*>(node, 0);
            if (trait_native_field<std::uintptr_t>(button, 0) != native_trait_module + 0x259FF20) continue;
            const auto category = trait_native_field<std::uint8_t>(button, 0x1C8);
            std::function<void(void*)> clicked = [layout, category_list, category](void*) {
                if (native_jewel_edit.owner == layout) return;
                trait_native_field<std::uint8_t>(category_list, 0x26C) = category;
                void* tree = trait_native_field<void*>(layout, 0x288);
                reinterpret_cast<void(*)(void*, std::uint8_t)>(native_trait_module + 0x1349220)(tree, category);
                reinterpret_cast<void(*)(void*)>(native_trait_module + 0x15ECD90)(layout);
                native_jewel_edit.categories_bound = false;
            };
            // RegisterCallback keeps an occupied event unchanged. Release the
            // game's category-selection callback before installing the editor's.
            void* event_handle = trait_native_field<void*>(button, 0x80);
            if (event_handle) {
                const auto event_index = trait_native_field<std::uint32_t>(event_handle, 0);
                for (void* event_node = heads[event_index]; event_node;
                    event_node = trait_native_field<void*>(event_node, 8)) {
                    void* entry = trait_native_field<void*>(event_node, 0);
                    if (trait_native_field<int>(entry, 0) != 0x7FFFFFFE) continue;
                    *reinterpret_cast<std::function<void(void*)>*>(static_cast<std::byte*>(entry) + 8) = nullptr;
                    break;
                }
            }
            reinterpret_cast<void(*)(void*, int, void*)>(native_trait_module + 0xEF40)(button, 0x7FFFFFFE, &clicked);
        }
        native_jewel_edit.categories_bound = true;
    }
    bool category_open = false;
    void* game_data = *reinterpret_cast<void**>(native_trait_module + 0x2E98BC8);
    for (const auto& definition : domain::SECONDARY_JEWEL_DEFINITIONS) {
        void* data = trait_native_field<void*>(game_data, 0x588EF0 + definition.id * sizeof(void*));
        if (trait_native_field<std::uint8_t>(data, 0xC) == category &&
            ((*bitmap)[definition.raw_id >> 3] & (1u << (definition.raw_id & 7)))) {
            category_open = true;
            break;
        }
    }
    reinterpret_cast<void(*)(void*, int, bool)>(native_trait_module + 0x1693230)(footer, 1, category_open);
    // Native unlock UI disables opened/ineligible gems. The editor must allow
    // both directions independently of ordinary unlock prerequisites.
    void* current_tree = trait_native_field<void*>(tree,
        0x198 + sizeof(void*) * trait_native_field<std::uint8_t>(tree, 0x1F8));
    void* children = trait_native_field<void*>(current_tree, 0x68);
    const auto child_index = trait_native_field<std::uint32_t>(children, 0);
    auto** child_heads = *reinterpret_cast<void***>(native_trait_module + 0x34C8630);
    for (void* node = child_heads[child_index]; node; node = trait_native_field<void*>(node, 8)) {
        void* gem = trait_native_field<void*>(node, 0);
        auto* gem_vtable = trait_native_field<std::uintptr_t*>(gem, 0);
        if (reinterpret_cast<std::uintptr_t>(gem_vtable) != native_trait_module + 0x25A0A28) continue;
        reinterpret_cast<void(*)(void*, bool)>(gem_vtable[0x110 / 8])(gem, true);
        const auto id = trait_native_field<std::uint16_t>(gem, 0x1D8);
        const auto definition = std::find_if(domain::SECONDARY_JEWEL_DEFINITIONS.begin(),
            domain::SECONDARY_JEWEL_DEFINITIONS.end(), [id](const auto& value) { return value.id == id; });
        if (definition != domain::SECONDARY_JEWEL_DEFINITIONS.end()) {
            const auto raw_id = definition->raw_id;
            const bool open = ((*bitmap)[raw_id >> 3] & (1u << (raw_id & 7))) != 0;
            void* marker = trait_native_field<void*>(gem, 0x1E0);
            if (marker)
                reinterpret_cast<void(*)(void*, int)>(native_trait_module + 0x134A770)(marker, open ? 2 : 0);
        }
    }
}

void update_native_jewel_dialog(void* dialog, void* event) {
    reinterpret_cast<void(*)(void*, void*)>(native_jewel_dialog_update_hook.original)(dialog, event);
    // Wrapper Show hides its children after initialization. Bootstrap from the
    // visible wrapper, since the hidden tree cannot receive its own Update yet.
    if (native_jewel_edit.tree && !native_jewel_edit.tree_shown &&
        native_trait_session.frame().dialog == dialog &&
        native_trait_session.frame().page == TraitPage::JewelTree &&
        trait_native_field<int>(dialog, 0x230) == -1)
        update_native_jewel_layout(native_jewel_edit.tree, event);
}

void native_jewel_clicked(void* callback, void** argument) {
    void* layout = trait_native_field<void*>(callback, 8);
    if (layout != native_jewel_edit.tree) {
        reinterpret_cast<void(*)(void*, void**)>(native_jewel_click_hook.original)(callback, argument);
        return;
    }
    if (!*argument || native_jewel_edit.owner == layout) return;
    const auto id = trait_native_field<std::uint16_t>(*argument, 0x1D8);
    const auto definition = std::find_if(domain::SECONDARY_JEWEL_DEFINITIONS.begin(),
        domain::SECONDARY_JEWEL_DEFINITIONS.end(), [id](const auto& jewel) { return jewel.id == id; });
    if (definition == domain::SECONDARY_JEWEL_DEFINITIONS.end()) {
        reinterpret_cast<void(*)(void*, void**)>(native_jewel_click_hook.original)(callback, argument);
        return;
    }
    std::lock_guard lock(native_trait_mutex);
    auto& jewels = native_jewel_edit;
    if (!jewels.ready) return;
    const auto raw_id = definition->raw_id;
    auto& byte = (*jewels.bitmap)[raw_id >> 3];
    const auto bit = static_cast<std::uint8_t>(1u << (raw_id & 7));
    const bool open = (byte & bit) == 0;
    byte ^= bit;
    if (!open) jewels.mask.words[definition->id >> 6] &= ~(std::uint64_t{1} << (definition->id & 63));
}

void native_jewel_footer_clicked(void* callback, void** argument) {
    if (trait_native_field<void*>(callback, 8) == native_jewel_edit.tree && *argument &&
        trait_native_field<int>(*argument, 0x88) == 1) {
        // The editor's Reset clears the displayed category without the normal
        // state's courage refund/spending or category-change workflow.
        if (native_trait_session.frame().page != TraitPage::JewelTree) return;
        std::lock_guard lock(native_trait_mutex);
        auto& jewels = native_jewel_edit;
        if (!jewels.ready || !jewels.commands.empty()) return;
        void* tree = trait_native_field<void*>(jewels.tree, 0x288);
        const auto category = trait_native_field<std::uint8_t>(tree, 0x1F8);
        void* data = *reinterpret_cast<void**>(native_trait_module + 0x2E98BC8);
        for (const auto& jewel : domain::SECONDARY_JEWEL_DEFINITIONS) {
            void* gem = trait_native_field<void*>(data, 0x588EF0 + jewel.id * sizeof(void*));
            auto& byte = (*jewels.bitmap)[jewel.raw_id >> 3];
            const auto bit = static_cast<std::uint8_t>(1u << (jewel.raw_id & 7));
            if (trait_native_field<std::uint8_t>(gem, 0xC) != category || !(byte & bit)) continue;
            byte &= static_cast<std::uint8_t>(~bit);
            jewels.mask.words[jewel.id >> 6] &= ~(std::uint64_t{1} << (jewel.id & 63));
        }
        return;
    }
    if (trait_native_field<void*>(callback, 8) != native_jewel_edit.tree ||
        !*argument || trait_native_field<int>(*argument, 0x88) != 0) {
        reinterpret_cast<void(*)(void*, void**)>(native_jewel_footer_hook.original)(callback, argument);
        return;
    }
    if (native_trait_session.frame().page != TraitPage::JewelTree) return;
    {
        std::lock_guard lock(native_trait_mutex);
        if (!native_jewel_edit.ready || !native_jewel_edit.commands.empty()) return;
        native_jewel_draft = native_jewel_edit.mask;
    }
    void* layout = trait_native_field<void*>(callback, 8);
    native_jewel_edit.owner = layout;
    auto* vtable = trait_native_field<std::uintptr_t*>(layout, 0);
    reinterpret_cast<void(*)(void*, bool)>(vtable[0x120 / 8])(layout, false);
    // This is a retained parent, not a closing city state. Hide its layout
    // without running the footer's city-exit animation; the child owns that
    // modal transition and the same footer resources must survive its pop.
    reinterpret_cast<void(*)(void*, bool)>(vtable[0x108 / 8])(layout, false);
    native_trait_active.store(true, std::memory_order_release);
    show_native_trait_page(TraitPage::Jewels);
}

void native_jewel_back_clicked(void* callback, void** argument) {
    if (trait_native_field<void*>(callback, 8) != native_jewel_edit.tree) {
        reinterpret_cast<void(*)(void*, void**)>(native_jewel_back_hook.original)(callback, argument);
        return;
    }
    if (native_trait_session.frame().page != TraitPage::JewelTree) return;
    void* dialog = native_trait_session.frame().dialog;
    auto* vtable = trait_native_field<std::uintptr_t*>(dialog, 0);
    reinterpret_cast<void(*)(void*, int)>(vtable[0x2C8 / 8])(dialog, 0x7FFFFFFE);
}

void finish_native_jewel_draft(void* window, bool accepted) {
    std::lock_guard lock(native_trait_mutex);
    auto& jewels = native_jewel_edit;
    if (!jewels.draft_active || jewels.draft_window != window) return;
    if (accepted) {
        const auto previous_count = jewels.commands.size();
        for (const auto& definition : domain::SECONDARY_JEWEL_DEFINITIONS) {
            const auto raw_id = definition.raw_id;
            const auto bit = static_cast<std::uint8_t>(1u << (raw_id & 7));
            const bool open = ((*jewels.bitmap)[raw_id >> 3] & bit) != 0;
            if (open != ((jewels.initial_bitmap[raw_id >> 3] & bit) != 0))
                jewels.commands.push_back({jewels.mask, raw_id, open});
        }
        if (jewels.commands.size() == previous_count && jewels.mask.words != jewels.initial_mask.words)
            jewels.commands.push_back({jewels.mask});
    } else {
        jewels.mask = jewels.initial_mask;
        jewels.bitmap = jewels.initial_bitmap;
    }
    jewels.draft_active = false;
}

bool native_jewel_user_dirty(void* window) {
    const bool changed = reinterpret_cast<bool(*)(void*)>(native_jewel_user_dirty_hook.original)(window);
    std::lock_guard lock(native_trait_mutex);
    const auto& jewels = native_jewel_edit;
    return changed || (jewels.draft_active && jewels.draft_window == window &&
        (jewels.mask.words != jewels.initial_mask.words || *jewels.bitmap != jewels.initial_bitmap));
}

void native_jewel_user_exit(void* state) {
    // Native Exit runs after the confirmation and the retained windows close.
    // Those window pointers have already been cleared from the native state.
    const bool editing = state == native_jewel_edit.draft_state;
    void* window = native_jewel_edit.draft_window;
    bool accepted = false;
    if (editing) {
        void* context = *trait_native_field<void**>(state, 0x18);
        accepted = trait_native_field<bool>(context, 0x80);
    }
    reinterpret_cast<void(*)(void*)>(native_jewel_user_exit_hook.original)(state);
    if (editing) finish_native_jewel_draft(window, accepted);
}

void update_native_jewel_user(void* window, void* event) {
    if (window == native_jewel_edit.editor_parent) return;
    reinterpret_cast<void(*)(void*, void*)>(native_jewel_user_hook.original)(window, event);
    if (window == native_jewel_edit.draft_window && native_jewel_edit.row) {
        void* row = native_jewel_edit.row;
        auto* vtable = trait_native_field<std::uintptr_t*>(row, 0);
        reinterpret_cast<void(*)(void*, bool)>(vtable[0x108 / 8])(row, true);
        reinterpret_cast<void(*)(void*, bool)>(vtable[0x120 / 8])(row, true);
    }
    if (window != native_jewel_edit.pending_button_window) return;
    // The protagonist list has ten native row slots, with four used here.
    // Reuse slot five so label, button, spacing and EnvSettingCursor all share
    // the same hierarchy and resource as the existing protagonist fields.
    void* list = trait_native_field<void*>(trait_native_field<void*>(window, 0x238), 0x1A0);
    void* handle = trait_native_field<void*>(list, 0x68);
    if (!handle) return;
    const auto index = trait_native_field<std::uint32_t>(handle, 0);
    auto** heads = *reinterpret_cast<void***>(native_trait_module + 0x34C8630);
    void* row = nullptr;
    int slot = 0;
    for (void* node = heads[index]; node; node = trait_native_field<void*>(node, 8)) {
        void* child = trait_native_field<void*>(node, 0);
        if (trait_native_field<std::uintptr_t>(child, 0) != native_trait_module + 0x258E050) continue;
        if (slot++ == 4) { row = child; break; }
    }
    if (!row) return;
    native_jewel_edit.pending_button_window = nullptr;
    native_jewel_edit.row = row;
    void* button = trait_native_field<void*>(row, 0x1B8);
    // The spare row has the game's value-edit callback but no backing value.
    // Keep its hover cursor and let only our button handle activation.
    void* row_events = trait_native_field<void*>(row, 0x80);
    if (row_events) {
        const auto event_index = trait_native_field<std::uint32_t>(row_events, 0);
        for (void* node = heads[event_index]; node; node = trait_native_field<void*>(node, 8)) {
            void* entry = trait_native_field<void*>(node, 0);
            if (trait_native_field<int>(entry, 0) == 0x7FFFFFFE)
                *reinterpret_cast<std::function<void(void*)>*>(static_cast<std::byte*>(entry) + 8) = nullptr;
        }
    }
    void* row_handle = trait_native_field<void*>(row, 0x68);
    const auto row_index = trait_native_field<std::uint32_t>(row_handle, 0);
    for (void* node = heads[row_index]; node; node = trait_native_field<void*>(node, 8)) {
        void* child = trait_native_field<void*>(node, 0);
        if (child == button || child == trait_native_field<void*>(row, 0x1A0)) continue;
        auto* vtable = trait_native_field<std::uintptr_t*>(child, 0);
        reinterpret_cast<void(*)(void*, bool)>(vtable[0x108 / 8])(child, false);
    }
    auto** panes = trait_native_field<void**>(row, 0x148);
    const auto pane_count = trait_native_field<int>(row, 0x150);
    for (int id = 0; id < pane_count; ++id)
        reinterpret_cast<void(*)(void*, bool)>(native_trait_module + 0x1D15BC0)(panes[id], id == 0 || id == 14 || id == 9);
    reinterpret_cast<void(*)(void*, int, const wchar_t*, int)>(native_trait_module + 0x1D17190)(
        static_cast<std::byte*>(row) + 0x140, 14, L"보주", 0);
    auto* row_vtable = trait_native_field<std::uintptr_t*>(row, 0);
    reinterpret_cast<void(*)(void*, bool)>(row_vtable[0x108 / 8])(row, true);
    reinterpret_cast<void(*)(void*, bool)>(row_vtable[0x120 / 8])(row, true);
    auto* button_vtable = trait_native_field<std::uintptr_t*>(button, 0);
    reinterpret_cast<void(*)(void*, bool)>(button_vtable[0x108 / 8])(button, true);
    reinterpret_cast<void(*)(void*, bool)>(button_vtable[0x110 / 8])(button, true);
    reinterpret_cast<void(*)(void*, const wchar_t*)>(button_vtable[0x298 / 8])(button, L"보주편집");
    std::function<void(void*)> clicked = [window](void*) {
        if (native_trait_active.load(std::memory_order_acquire)) return;
        {
            std::lock_guard lock(native_trait_mutex);
            if (!native_jewel_edit.ready) return;
            if (!native_jewel_edit.draft_active) {
                native_jewel_edit.initial_mask = native_jewel_edit.mask;
                native_jewel_edit.initial_bitmap = *native_jewel_edit.bitmap;
                native_jewel_edit.draft_window = window;
                native_jewel_edit.draft_state = trait_native_field<void*>(
                    trait_native_field<void*>(window, 0x288), 8);
                native_jewel_edit.draft_active = true;
            }
        }
        native_jewel_edit.editor_parent = window;
        auto* vtable = trait_native_field<std::uintptr_t*>(window, 0);
        reinterpret_cast<void(*)(void*, bool)>(vtable[0x120 / 8])(window, false);
        native_trait_active.store(true, std::memory_order_release);
        show_native_trait_page(TraitPage::JewelTree);
    };
    void* event_handle = trait_native_field<void*>(button, 0x80);
    if (event_handle) {
        const auto event_index = trait_native_field<std::uint32_t>(event_handle, 0);
        for (void* node = heads[event_index]; node; node = trait_native_field<void*>(node, 8)) {
            void* entry = trait_native_field<void*>(node, 0);
            if (trait_native_field<int>(entry, 0) != 0x7FFFFFFE) continue;
            *reinterpret_cast<std::function<void(void*)>*>(static_cast<std::byte*>(entry) + 8) = nullptr;
            break;
        }
    }
    reinterpret_cast<void(*)(void*, int, void*)>(native_trait_module + 0xEF40)(button, 0x7FFFFFFE, &clicked);
}

} // namespace

void bind_native_jewel_button(void* window) {
    // The descriptor factory runs before EditInGameUIUserWindow creates its
    // controls. Bind only after that window reaches its own Update callback.
    native_jewel_edit.pending_button_window = window;
    native_jewel_edit.row = nullptr;
    std::lock_guard lock(native_trait_mutex);
    if (native_jewel_edit.draft_active) {
        native_jewel_edit.mask = native_jewel_edit.initial_mask;
        native_jewel_edit.bitmap = native_jewel_edit.initial_bitmap;
    }
    native_jewel_edit.draft_active = false;
    native_jewel_edit.draft_window = window;
}

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
    const auto jewel_update = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x15EB4A0),
        reinterpret_cast<void*>(&update_native_jewel_layout));
    const auto jewel_click = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x16F860),
        reinterpret_cast<void*>(&native_jewel_clicked));
    const auto jewel_footer = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x16F660),
        reinterpret_cast<void*>(&native_jewel_footer_clicked));
    const auto jewel_back = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x16F6D0),
        reinterpret_cast<void*>(&native_jewel_back_clicked));
    const auto jewel_user = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x12D4870),
        reinterpret_cast<void*>(&update_native_jewel_user));
    const auto jewel_dialog_update = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x28B10),
        reinterpret_cast<void*>(&update_native_jewel_dialog));
    const auto jewel_user_dirty = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x12D47B0),
        reinterpret_cast<void*>(&native_jewel_user_dirty));
    const auto jewel_visual = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x1349BB0),
        reinterpret_cast<void*>(&native_jewel_visual));
    const auto jewel_user_exit = platform::windows::create_hook(reinterpret_cast<void*>(module + 0x12E56F0),
        reinterpret_cast<void*>(&native_jewel_user_exit));
    if (!jewel_update || !jewel_click || !jewel_footer || !jewel_back || !jewel_user || !jewel_dialog_update ||
        !jewel_user_dirty || !jewel_user_exit || !jewel_visual) return false;
    native_jewel_visual_hook = *jewel_visual;
    native_jewel_user_dirty_hook = *jewel_user_dirty;
    native_jewel_user_exit_hook = *jewel_user_exit;
    native_jewel_dialog_update_hook = *jewel_dialog_update;
    native_jewel_update_hook = *jewel_update;
    native_jewel_click_hook = *jewel_click;
    native_jewel_footer_hook = *jewel_footer;
    native_jewel_back_hook = *jewel_back;
    native_jewel_user_hook = *jewel_user;
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
                            if (platform::windows::enable_hook(native_trait_input_click_hook) == 0) {
                                if (platform::windows::enable_hook(native_jewel_update_hook) == 0) {
                                    if (platform::windows::enable_hook(native_jewel_click_hook) == 0) {
                                        if (platform::windows::enable_hook(native_jewel_footer_hook) == 0) {
                                            if (platform::windows::enable_hook(native_jewel_back_hook) == 0) {
                                                if (platform::windows::enable_hook(native_jewel_user_hook) == 0) {
                                                    if (platform::windows::enable_hook(native_jewel_dialog_update_hook) == 0) {
                                                        if (platform::windows::enable_hook(native_jewel_user_dirty_hook) == 0) {
                                                            if (platform::windows::enable_hook(native_jewel_user_exit_hook) == 0) {
                                                                if (platform::windows::enable_hook(native_jewel_visual_hook) == 0) return true;
                                                                platform::windows::disable_hook(native_jewel_user_exit_hook);
                                                            }
                                                            platform::windows::disable_hook(native_jewel_user_dirty_hook);
                                                        }
                                                        platform::windows::disable_hook(native_jewel_dialog_update_hook);
                                                    }
                                                    platform::windows::disable_hook(native_jewel_user_hook);
                                                }
                                                platform::windows::disable_hook(native_jewel_back_hook);
                                            }
                                            platform::windows::disable_hook(native_jewel_footer_hook);
                                        }
                                        platform::windows::disable_hook(native_jewel_click_hook);
                                    }
                                    platform::windows::disable_hook(native_jewel_update_hook);
                                }
                                platform::windows::disable_hook(native_trait_input_click_hook);
                            }
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
    {
        std::lock_guard lock(native_trait_mutex);
        auto& jewels = native_jewel_edit;
        const bool same_game = jewels.epoch == menu.epoch;
        if (!same_game) {
            jewels.commands.clear();
            jewels.draft_active = false;
        }
        jewels.epoch = menu.epoch;
        jewels.ready = menu.ready && !menu.pending && menu.snapshot.jewel_bitmap.has_value();
        if (!jewels.draft_active && jewels.commands.empty() && !menu.pending) {
            jewels.mask = menu.snapshot.secondary_jewels.value_or(domain::SecondaryJewelMask{});
            jewels.bitmap = menu.snapshot.jewel_bitmap;
        }
        if (!menu.pending && jewels.ready && !jewels.commands.empty()) {
            const domain::RequestId request{++menu.next_request};
            const auto accepted = bridge.commands.push({request, menu.epoch, jewels.commands.front()});
            if (accepted) {
                menu.pending = request;
                menu.pending_notice = "보주 편집 적용을 요청했습니다.";
                jewels.commands.erase(jewels.commands.begin());
                jewels.ready = false;
            } else {
                menu.error = accepted.error();
                jewels.commands.clear();
            }
        }
    }
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
