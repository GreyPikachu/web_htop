/** @file server/transport/output_queue.hpp
 *  @brief Latest-wins queue. A partially sent frame is never replaced.
 */
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace web_htop::server
{
class OutputQueue
{
  public:
    using Buffer = std::shared_ptr<std::string const>;

    // Return true when an unsent snapshot was superseded.
    bool Push(Buffer buffer)
    {
        if (!current_)
        {
            current_ = std::move(buffer);
            offset_ = 0;
            return false;
        }
        if (offset_ == 0)
        {
            current_ = std::move(buffer);
            return true;
        }
        bool dropped = static_cast<bool>(pending_);
        pending_ = std::move(buffer);
        return dropped;
    }

    [[nodiscard]] std::string_view Front() const
    {
        return current_ ? std::string_view(*current_).substr(offset_) : std::string_view{};
    }

    void Consume(std::size_t size)
    {
        if (!current_ || size > Front().size())
        {
            throw std::logic_error("output queue over-consumed");
        }
        offset_ += size;

        if (offset_ == current_->size())
        {
            current_ = std::move(pending_);
            offset_ = 0;
        }
    }

    [[nodiscard]] bool Empty() const noexcept
    {
        return !current_;
    }

    [[nodiscard]] std::size_t Bytes() const noexcept
    {
        return (current_ ? current_->size() - offset_ : 0) + (pending_ ? pending_->size() : 0);
    }

  private:
    Buffer current_, pending_;
    std::size_t offset_{};
};
} // namespace web_htop::server
