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
    void lock() {}
    void unlock() {}
    void lock_shared() {}
    void unlock_shared() {}
};

template <typename FuncSig, bool thread_safe = true>
class signalpp_impl {
private:
    using mutex_type = std::conditional_t<thread_safe, std::shared_mutex, empty_mutex>;
    using lock_type = std::unique_lock<mutex_type>;

public:
    using slot_type = std::function<FuncSig>;
    using slot_container = std::list<slot_type>;
    using slot_iterator = typename slot_container::iterator;

private:
    struct state_t {
        std::list<std::pair<std::uint64_t, slot_type>> slots;
        mutex_type mutex;
        std::uint64_t next_id = 0;
    };

    std::shared_ptr<state_t> state_ = std::make_shared<state_t>();

    class signalpp_connection {
    public:
        signalpp_connection() = default;
        signalpp_connection(std::function<void()> disconnect_func) : disconnect_func_(std::move(disconnect_func)) {}

        signalpp_connection(const signalpp_connection&) = delete;
        signalpp_connection& operator=(const signalpp_connection&) = delete;

        signalpp_connection(signalpp_connection&&) = default;
        signalpp_connection& operator=(signalpp_connection&&) = default;

        ~signalpp_connection() = default;

        void disconnect() {
            if (disconnect_func_) {
                disconnect_func_();
                disconnect_func_ = nullptr;
            }
        }

    private:
        std::function<void()> disconnect_func_;
    };

    class signalpp_scoped_connection : public signalpp_connection {
    public:
        signalpp_scoped_connection() = default;

        explicit signalpp_scoped_connection(std::function<void()> disconnect_func)
            : signalpp_connection(std::move(disconnect_func)) {}

        signalpp_scoped_connection(signalpp_scoped_connection&& other) noexcept
            : signalpp_connection(std::move(other)) {}

        signalpp_scoped_connection& operator=(signalpp_scoped_connection&& other) noexcept {
            signalpp_connection::operator=(std::move(other));
            return *this;
        }

        signalpp_scoped_connection(const signalpp_scoped_connection&) = delete;
        signalpp_scoped_connection& operator=(const signalpp_scoped_connection&) = delete;

        ~signalpp_scoped_connection() {
            disconnect();
        }
    };

    signalpp_connection connect(slot_type slot) {
        auto state = state_;

        std::uint64_t id;

        {
            std::unique_lock<mutex_type> lock{state->mutex};
            id = state->next_id++;
            state->slots.emplace_back(id, std::move(slot));
        }

        std::function<void()> disconnect_func = [state, it]() {
            std::unique_lock<mutex_type> lock{state->mutex};
            state->slots.remove_if([id](const auto& item) {
                return item.first == id;
            });
        };

        return signalpp_connection{std::move(disconnect_func)};
    }

    signalpp_scoped_connection scoped_connect(slot_type slot) {
        auto state = state_;

        std::uint64_t id;

        {
            std::unique_lock<mutex_type> lock{state->mutex};
            id = state->next_id++;
            state->slots.emplace_back(id, std::move(slot));
        }

        std::function<void()> disconnect_func = [state, it]() {
            std::unique_lock<mutex_type> lock{state->mutex};
            state->slots.remove_if([id](const auto& item) {
                return item.first == id;
            });
        };

        return signalpp_scoped_connection{std::move(disconnect_func)};
    }

    template <class C, typename R, typename... Args>
    signalpp_connection connect(C* object, R (C::*member_func)(Args...)) {
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

    // Subscribe slot without possible ability to disconnect it
    signalpp_impl& operator+=(slot_type slot) {
        connect(std::move(slot));
        return *this;
    }

    template <typename... Args>
    void emit(Args&&... args) {
        std::vector<slot_type> slots_snapshot;

        {
            std::unique_lock<mutex_type> lock{state_->mutex};

            slots_snapshot.reserve(state_->slots.size());
            for (const auto& [id, slot] : state_->slots) {
                slots_snapshot.push_back(slot);
            }
        }

        for (auto& slot : slots_snapshot) {
            slot(std::forward<Args>(args)...);
        }
    }

    template <typename... CallArgs>
    class iterate {
    public:
        iterate(slot_container& slots_, mutex_type& slots_mutex, CallArgs&&... args)
            : slots_(slots_), slots_mutex_(slots_mutex_), stored_args_(std::forward<CallArgs>(args)...) {}

        auto begin() {
            return signal_iterator<CallArgs...>(slots_.begin(), slots_mutex_, stored_args_);
        }
        auto end() {
            return signal_iterator<CallArgs...>(slots_.end(), slots_mutex_, stored_args_);
        }

    private:
        std::tuple<CallArgs&&...> stored_args_;
        slot_container& slots_;
        mutex_type& slots_mutex_;
    };

    template <typename... Args>
    iterate<Args...> emit_iterate(Args&&... args) {
        return iterate<Args...>(slots_, slots_mutex, std::forward<Args>(args)...);
    }
};

template <typename FuncSig>
using signalpp = signalpp_impl<FuncSig, false>;

template <typename FuncSig>
using signalpp_threadsafe = signalpp_impl<FuncSig, true>;

} // namespace signalpp
