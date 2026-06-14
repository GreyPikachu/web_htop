/**
 * @file common/unique_fd.hpp
 * @brief Exclusive ownership of a Linux file descriptor.
 */
#pragma once
#include <cerrno>
#include <unistd.h>
#include <utility>

namespace web_htop {
class UniqueFd {
  public:
    explicit UniqueFd(int fd = -1) noexcept : fd_(fd) {}
    ~UniqueFd() {
        Reset();
    }
    UniqueFd(UniqueFd const&) = delete;
    UniqueFd& operator=(UniqueFd const&) = delete;
    UniqueFd(UniqueFd&& other) noexcept : fd_(other.Release()) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other)
            Reset(other.Release());
        return *this;
    }
    [[nodiscard]] int Get() const noexcept {
        return fd_;
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return fd_ >= 0;
    }
    [[nodiscard]] int Release() noexcept {
        return std::exchange(fd_, -1);
    }
    void Reset(int fd = -1) noexcept {
        const int old = std::exchange(fd_, fd);
        if (old >= 0) {
            const int saved_errno = errno;
            // Linux releases the descriptor even when close reports EINTR.
            // Retrying could close an unrelated descriptor reused by another thread.
            (void)::close(old);
            errno = saved_errno;
        }
    }

  private:
    int fd_;
};
} // namespace web_htop
