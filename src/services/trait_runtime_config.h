#pragma once

#include "../domain/error.h"
#include "../domain/trait_runtime.h"

#include <string_view>

namespace s8::services {

// Service-thread JSON decoding; produces immutable values with no game address.
domain::Result<domain::TraitRuntimeCatalog> read_trait_runtime_config(std::string_view source);

} // namespace s8::services
