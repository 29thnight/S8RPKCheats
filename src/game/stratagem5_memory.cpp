#include "stratagem5_memory.h"
#include "../platform/windows/process_memory.h"

namespace s8::game {

bool read_stratagem_table(const Stratagem5Memory& memory, StratagemTableSnapshot& snapshot) {
    return platform::windows::read_memory(memory.payload_table, snapshot);
}

bool write_stratagem5(void* context, StratagemTableId table, const StratagemPayload& payload) {
    if (!context) {
        return false;
    }

    const Stratagem5Memory& memory = *static_cast<const Stratagem5Memory*>(context);

    if (memory.table != table) {
        return false;
    }
    return platform::windows::write_data_memory(
        memory.payload_table + 4 * STRATAGEM_RECORD_STRIDE, payload);
}

} // namespace s8::game
