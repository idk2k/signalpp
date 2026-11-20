#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <functional>
#include <list>
#include <tuple>
#include <shared_mutex>

namespace signalpp {

struct empty_mutex {
    void lock() noexcept {}
    void unlock() noexcept {}
    void lock_shared() noexcept {}
    void unlock_shared() noexcept {}
};

template <typename FuncSig, bool thread_safe = true>
class signalpp_impl {
private:
    using mutex_type = std::conditional_t<thread_safe, std::shared_mutex, empty_mutex>;

public:
    using slot_type = std::function<FuncSig>;

private:
    struct state_t {
        std::list<std::pair<std::uint64_t, slot_type>> slots;
        mutable mutex_type mutex;
        std::uint64_t next_id = 0;

        void remove(std::uint64_t id) {
            std::unique_lock lock{mutex};
            slots.remove_if([id](const auto& item) {
                return id == item.first;
            });
        }
    };

    std::shared_ptr<state_t> state_ = std::make_shared<state_t>();

public:
    class signalpp_connection {
    public:
        signalpp_connection() noexcept = default;
        signalpp_connection(std::weak_ptr<state_t> state, std::uint64_t id) : state_(std::move(state)), id_(id) {}

        signalpp_connection(const signalpp_connection&) = delete;
        signalpp_connection& operator=(const signalpp_connection&) = delete;

        signalpp_connection(signalpp_connection&& other) noexcept
            : state_(std::move(other.state_)), id_(std::exchange(other.id_, 0)) {}

        signalpp_connection& operator=(signalpp_connection&& other) noexcept {
            if (this != &other) {
                state_ = std::move(other.state_);
                id_ = std::exchange(other.id_, 0);
            }
            return *this;
        }

        ~signalpp_connection() = default;

        void disconnect() {
            if (auto s = state_.lock()) {
                s->remove(id_);
                state_.reset();
            }
        }

        [[nodiscard]]
        bool connected() const noexcept {
            return !state_.expired();
        }

    private:
        std::weak_ptr<state_t> state_;
        std::uint64_t id_ = 0;
    };

    class signalpp_scoped_connection : public signalpp_connection {
    public:
        signalpp_scoped_connection() noexcept = default;

        signalpp_scoped_connection(std::function<void()> disconnect_func)
            : signalpp_connection(std::move(disconnect_func)) {}

        signalpp_scoped_connection(signalpp_connection&& conn) noexcept
            : signalpp_connection(std::move(conn)) {}

        signalpp_scoped_connection& operator=(signalpp_scoped_connection&& other) noexcept {
            if (this != &other) {
                signalpp_connection::disconnect();
                signalpp_connection::operator=(std::move(other));
            }
            return *this;
        }

        signalpp_scoped_connection(const signalpp_scoped_connection&) = delete;
        signalpp_scoped_connection& operator=(const signalpp_scoped_connection&) = delete;

        ~signalpp_scoped_connection() {
            signalpp_connection::disconnect();
        }
    };

    signalpp_connection connect(slot_type slot) {
        std::uint64_t id;
        {
            std::unique_lock<mutex_type> lock{state_->mutex};
            id = ++state_->next_id;
            state_->slots.emplace_back(id, std::move(slot));
        }

        return signalpp_connection{state_, id};
    }

    signalpp_scoped_connection scoped_connect(slot_type slot) {
        return signalpp_scoped_connection{connect(std::move(slot))};
    }

    template <class C, typename R, typename... Args>
    signalpp_connection connect(C* object, R (C::*member_func)(Args...)) {
        return connect([object, member_func](Args... args) -> R {
            return (object->*member_func)(std::forward<Args>(args)...);
        });
    }

    template <class C, typename R, typename... Args>
    signalpp_connection connect(const C* object, R (C::*member_func)(Args...) const) {
        return connect([object, member_func](Args... args) -> R {
            return (object->*member_func)(std::forward<Args>(args)...);
        });
    }

    template <class C, typename R, typename... Args>
    signalpp_scoped_connection scoped_connect(C* object, R (C::*member_func)(Args...)) {
        return scoped_connect([object, member_func](Args... args) -> R {
            return (object->*member_func)(std::forward<Args>(args)...);
        });
    }

    template <class C, typename R, typename... Args>
    signalpp_scoped_connection scoped_connect(const C* object, R (C::*member_func)(Args...) const) {
        return scoped_connect([object, member_func](Args... args) -> R {
            return (object->*member_func)(std::forward<Args>(args)...);
        });
    }

    signalpp_impl& operator+=(slot_type slot) {
        connect(std::move(slot));
        return *this;
    }

    template <typename... Args>
    void emit(Args... args) const {
        std::vector<slot_type> slots_snapshot;
        {
            std::shared_lock<mutex_type> lock{state_->mutex};
            slots_snapshot.reserve(state_->slots.size());
            for (const auto& [id, slot] : state_->slots) {
                slots_snapshot.push_back(slot);
            }
        }

        for (const auto& slot : slots_snapshot) {
            if (slot)
                slot(args...);
        }
    }

    template <typename... Args>
    void operator()(Args... args) const {
        emit(std::forward<Args>(args)...);
    }
};

template <typename FuncSig>
using signalpp = signalpp_impl<FuncSig, false>;

template <typename FuncSig>
using signalpp_threadsafe = signalpp_impl<FuncSig, true>;

} // namespace signalpp
