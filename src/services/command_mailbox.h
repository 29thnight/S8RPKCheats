#pragma once

#include "../domain/commands.h"
#include "../domain/error.h"

#include <array>
#include <cstddef>
#include <mutex>
#include <optional>

namespace s8::services {

struct CompletionToken {
    std::size_t index;
    std::uint64_t generation;
};

struct AcceptedCommand {
    domain::Command command;
    CompletionToken completion;
};

struct CommandCompletion {
    domain::RequestId request;
    domain::GameEpoch epoch;
    domain::Result<void> result;
};

class CommandMailbox {
public:
    static constexpr std::size_t CAPACITY = 256;

    domain::Result<void> push(domain::Command command);
    std::optional<AcceptedCommand> pop();
    bool complete(CompletionToken token, domain::Result<void> result);
    std::optional<CommandCompletion> take_completion();
    void close();

private:
    enum class SlotState { Free, Queued, Running, Ready };
    struct Slot {
        SlotState state = SlotState::Free;
        std::uint64_t generation = 0;
        std::optional<domain::Command> command;
        domain::Result<void> result;
    };

    std::mutex mutex;
    std::array<Slot, CAPACITY> slots{};
    std::array<std::size_t, CAPACITY> queue{};
    std::size_t head = 0;
    std::size_t count = 0;
    bool closed = false;
};

} // namespace s8::services
