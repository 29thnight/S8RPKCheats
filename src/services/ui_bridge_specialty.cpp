#include "ui_bridge.h"

namespace s8::services {

void UiBridge::publish_specialty_definitions(own::shared_owner<const domain::SpecialtyDefinitions> definitions) {
    {
        const std::lock_guard<std::mutex> lock(mutex);

        current_specialty_definitions.swap(definitions);
    }
}

own::shared_owner<const domain::SpecialtyDefinitions> UiBridge::specialty_definitions_view() {
    const std::lock_guard<std::mutex> lock(mutex);

    return current_specialty_definitions;
}

} // namespace s8::services
