#ifndef SIGNALPP_HPP
#define SIGNALPP_HPP

#include <functional>
#include <list>
#include <tuple>
//#include <mutex>
#include <shared_mutex>

namespace signalpp {
	// Fake mutex for use with thread-safe option disabled
	struct empty_mutex {
		void lock() {}
		void unlock() {}

		void lock_shared() {}
		void unlock_shared() {}
	};

	// General implementation
	template<typename FuncSig, bool thread_safe = true, bool emit_thread_safe = false>
	class signalpp_impl {
		using mutex_type = std::conditional_t<thread_safe, std::shared_mutex, empty_mutex>;
		using emit_locker = std::conditional_t<thread_safe, std::unique_lock<mutex_type>, std::shared_lock<mutex_type>>;
	public:
		using slot_type = std::function<FuncSig>;
		using slot_container = std::list<slot_type>;
		using slot_iterator = typename slot_container::iterator;


		class signalpp_connection {
		public:
			void disconnect() {
				erase_func();
				erase_func() = []() {};
			}
			signalpp_connection(std::function<void()> func) : erase_func(func) {};
			signalpp_connection(signalpp_connection&) = delete;
			signalpp_connection(signalpp_connection&&) = delete;

			signalpp_connection& operator=(signalpp_connection&) = delete;
			signalpp_connection& operator=(signalpp_connection&&) = delete;
			~signalpp_connection() = delete;
		private:
			std::function<void()> erase_func;
		};

		class signalpp_scoped_connection : public signalpp_connection {
		public:
			~signalpp_scoped_connection() { signalpp_connection::disconnect(); }
			signalpp_scoped_connection(std::function<void()> func) : signalpp_connection(func) {};
			signalpp_scoped_connection(signalpp_scoped_connection&) = delete;
			signalpp_scoped_connection(signalpp_scoped_connection&& Right) = default;

			signalpp_scoped_connection& operator=(signalpp_scoped_connection&) = delete;
			signalpp_scoped_connection& operator=(signalpp_scoped_connection&&) = default;
		};

		signalpp_connection connect(slot_type slot) {
			std::unique_lock<mutex_type> lock{ slots_mutex };

			auto erase_iter = slots_.emplace(slots_.end(), std::move(slot));
			
			std::function<void()> lambda = [&slots_mutes = this->slots_mutex, &slots_ = this->slots_, erase_iter]() -> void {
				std::unique_lock<mutex_type> lock{ slots_mutex };
				slots_.erase(erase_iter);
			};
			return signalpp_connection{ std::move(lambda) };
		}

		signalpp_scoped_connection scoped_connect(slot_type slot) {
			std::unique_lock<mutex_type> lock{ slots_mutex };

			auto erase_iter = slots_.emplace(slots_.end(), std::move(slot));

			std::function<void()> lambda = [&slots_mutes = this->slots_mutex, &slots_ = this->slots_, erase_iter]() -> void {
				std::unique_lock<mutex_type> lock{ slots_mutex };
				slots_.erase(erase_iter);
			};
			return signalpp_scoped_connection{ std::move(lambda) };
		}

		template<class C, typename R, typename... Args>
		signalpp_connection connect(C* class_ptr, R(C::* member_func)(Args ...)) {
			return connect([class_ptr, member_func](Args&& ... args) -> R { return (class_ptr->*member_func)(std::forward<Args>(args)...); });
		}

		template<class C, typename R, typename... Args>
		signalpp_scoped_connection scoped_connect(C* class_ptr, R(C::* member_func)(Args ...)) {
			return scoped_connect([class_ptr, member_func](Args&& ... args) -> R { return (class_ptr->*member_func)(std::forward<Args>(args)...); });
		}

		signalpp_impl& operator+=(slot_type slot) {
			connect(slot);
			return *this;
		}

		template<typename ... Args>
		void emit(Args&& ... args) {
			emit_locker lock{ slots_mutex };
			for (auto& f : slots_) {
				f(std::forward<Args>(args)...);
			}
		}

		// Emit iteration

		// template<typename... CallArgs>
		// class signal_iterator {
		// public:
		// 	signal_iterator(slot_iterator iter, mutex_type& slots_mutex_, std::tuple<CallArgs&&...>& args) :
		// 		iter(iter), slots_mutex_(slots_mutex_), stored_args_(args) {}
		// private:
		// 	slot_iterator iter_;
		// 	mutex_type& slots_mutex_;
		// 	std::tuple<CallArgs&&...> stored_args_;
		// };

		template<typename... CallArgs>
		class iterate {
		public:
			iterate(slot_container& slots_, mutex_type& slots_mutex, CallArgs&&... args) :
				slots_(slots_), slots_mutex_(slots_mutex_), stored_args_(std::forward<CallArgs>(args)...) {}

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

		template<typename... Args>
		iterate<Args...> emit_iterate(Args&& ... args) {
			return iterate<Args...>(slots_, slots_mutex, std::forward<Args>(args)...);
		}


	private:
		slot_container slots_;
		mutex_type slots_mutex;
	};

	// Public interfaces
	template<typename FuncSig>
	using signalpp = signalpp_impl<FuncSig, false, false>;
	template<typename FuncSig>
	using signalpp_threadsafe = signalpp_impl<FuncSig, true, false>;
	template<typename FuncSig>
	using signalpp_threadsafe_emit = signalpp_impl<FuncSig, true, true>;
}


#endif