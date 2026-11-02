#pragma once

#include "../domain/trait_text_edit.h"

#include <functional>

namespace s8::services { class UiBridge; }

namespace s8::overlay {

struct FeatureMenu;
// Called by Present for publication/command handoff; game callbacks never
// access FeatureMenu or ImGui objects.
void update_native_trait_editor(FeatureMenu& menu, services::UiBridge& bridge);
bool install_native_trait_editor(std::uintptr_t module);
void open_native_trait_editor();

enum class NativeTraitInputResult { Cancelled, Accepted, InvalidText, UnrepresentableText };

// Owned by the native editor screen, never by an officer-assignment dialog.
// Keep this object alive until every child input state's completion has run.
class NativeTraitDraft {
public:
    explicit NativeTraitDraft(domain::TraitTextEdit source);
    NativeTraitDraft(const NativeTraitDraft&) = delete;
    NativeTraitDraft& operator=(const NativeTraitDraft&) = delete;

    const domain::TraitTextEdit& row() const { return row_; }
    const std::string& name() const;
    const std::string& description() const;
    std::string effect_description(std::size_t slot,
        const domain::TraitEffectDefinitions& definitions) const;

    domain::TraitTextValidation set_name(std::string text);
    domain::TraitTextValidation set_description(std::string text);
    void set_effect(std::size_t slot, domain::TraitEffect effect);

    // SAN8RPK 0x69A67ED1 only. Caller validates the image once and invokes on
    // the game's UI thread. Only one child input may be outstanding per draft.
    // The callback can schedule the next native screen after the input closes.
    void input_text(std::uintptr_t module, bool description,
        std::function<void(NativeTraitInputResult, domain::TraitTextValidation)> completion);
    void input_number(std::uintptr_t module, std::uint16_t initial,
        std::function<void(bool, std::uint16_t)> completion);

private:
    domain::TraitTextEdit row_;
    std::array<char, 4096> input_{};
    std::string initial_input_;
    std::int32_t number_ = 0;
};

} // namespace s8::overlay
