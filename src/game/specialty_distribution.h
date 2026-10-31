#pragma once

#include "../domain/named_specialties.h"
#include <cstdint>
#include <random>

namespace s8::game {

// Definition loading is performed outside the game context.
int assign_specialties(std::uintptr_t module_base, const domain::NamedSpecialties& named, std::mt19937& random);

} // namespace s8::game
