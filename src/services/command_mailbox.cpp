#include "command_mailbox.h"

#include <utility>

namespace s8::services {

domain::Result<void> CommandMailbox::push(domain::Command command) {
    std::lock_guard lock(mutex);
    if (closed)
        return std::unexpected(domain::Error{domain::ErrorCode::Cancelled});
    if (count == CAPACITY)
        return std::unexpected(domain::Error{domain::ErrorCode::QueueFull});
    std::size_t index = 0;
    while (index < CAPACITY && slots[index].state != SlotState::Free)
        ++index;
    if (index == CAPACITY)
        return std::unexpected(domain::Error{domain::ErrorCode::QueueFull});
    auto& slot = slots[index];
    ++slot.generation;
    slot.command = std::move(command);
    slot.state = SlotState::Queued;
    queue[(head + count) % CAPACITY] = index;
    ++count;
    return {};
}

std::optional<AcceptedCommand> CommandMailbox::pop() {
    std::lock_guard lock(mutex);
    if (count == 0)
        return std::nullopt;
    const auto index = queue[head];
    auto& slot = slots[index];
    slot.state = SlotState::Running;
    head = (head + 1) % CAPACITY;
    --count;
    return AcceptedCommand{*slot.command, CompletionToken{index, slot.generation}};
}

bool CommandMailbox::complete(CompletionToken token, domain::Result<void> result) {
    std::lock_guard lock(mutex);
    if (token.index >= CAPACITY)
        return false;
    auto& slot = slots[token.index];
    if (slot.state != SlotState::Running || slot.generation != token.generation)
        return false;
    slot.result = std::move(result);
    slot.state = SlotState::Ready;
    return true;
}

std::optional<CommandCompletion> CommandMailbox::take_completion() {
    std::lock_guard lock(mutex);
    for (auto& slot : slots) {
        if (slot.state != SlotState::Ready)
            continue;
        CommandCompletion completion{slot.command->request, slot.command->epoch, std::move(slot.result)};
        slot.command.reset();
        slot.state = SlotState::Free;
        return completion;
    }
    return std::nullopt;
}

void CommandMailbox::close() {
    std::lock_guard lock(mutex);
    closed = true;
}

} // namespace s8::services
