#pragma once

#include <atomic>
#include <memory>

namespace LAStudio {

class InferenceCancellationToken final
{
public:
    InferenceCancellationToken()
        : m_flag(std::make_shared<std::atomic_bool>(false)) {}

    explicit InferenceCancellationToken(std::shared_ptr<std::atomic_bool> flag)
        : m_flag(std::move(flag)) {}

    void cancel() const { if (m_flag) m_flag->store(true, std::memory_order_relaxed); }
    bool isCancelled() const { return m_flag && m_flag->load(std::memory_order_relaxed); }
    std::shared_ptr<std::atomic_bool> sharedFlag() const { return m_flag; }

private:
    std::shared_ptr<std::atomic_bool> m_flag;
};

} // namespace LAStudio
