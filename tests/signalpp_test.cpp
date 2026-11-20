#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include <signalpp/signalpp.hpp>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) {
        std::cout << "[OK] " << message << '\n';
    } else {
        ++failures;
        std::cerr << "[FAIL] " << message << '\n';
    }
}

void test_emit_calls_connected_slot() {
    signalpp::signalpp<void(int)> signal;

    int received_value = 0;

    auto connection = signal.connect([&received_value](int value) {
        received_value = value;
    });

    signal.emit(42);

    check(received_value == 42, "emit calls connected slot");
}

void test_disconnect_stops_calls() {
    signalpp::signalpp<void(int)> signal;

    int call_count = 0;

    auto connection = signal.connect([&call_count](int) {
        ++call_count;
    });

    signal.emit(1);
    connection.disconnect();
    signal.emit(2);

    check(call_count == 1, "disconnect prevents further slot calls");
}

void test_repeated_disconnect_is_safe() {
    signalpp::signalpp<void(int)> signal;

    auto connection = signal.connect([](int) {});

    connection.disconnect();
    connection.disconnect();

    check(true, "repeated disconnect does not crash");
}

void test_scoped_connection_disconnects_on_destroy() {
    signalpp::signalpp<void(int)> signal;

    int call_count = 0;

    {
        auto scoped_connection = signal.scoped_connect([&call_count](int) {
            ++call_count;
        });

        signal.emit(1);
    }

    signal.emit(2);

    check(call_count == 1, "scoped connection disconnects automatically");
}

void test_multiple_slots_are_called() {
    signalpp::signalpp<void(int)> signal;

    int first_call_count = 0;
    int second_call_count = 0;

    signal.connect([&first_call_count](int) {
        ++first_call_count;
    });

    signal.connect([&second_call_count](int) {
        ++second_call_count;
    });

    signal.emit(7);

    check(first_call_count == 1, "first slot is called");
    check(second_call_count == 1, "second slot is called");
}

// =================================================================

void test_scoped_connection_move_assignment_disconnects_previous() {
    signalpp::signalpp<void(int)> signal;

    int first_calls = 0;
    int second_calls = 0;

    auto conn1 = signal.scoped_connect([&first_calls](int) {
        ++first_calls;
    });
    auto conn2 = signal.scoped_connect([&second_calls](int) {
        ++second_calls;
    });

    signal.emit(1);
    check(first_calls == 1 && second_calls == 1, "both slots called initially");

    conn1 = std::move(conn2);

    signal.emit(2);
    check(first_calls == 1, "previous slot disconnected on move assignment");
    check(second_calls == 2, "moved-in slot still active");
}

void test_disconnect_after_signal_destroyed_is_safe() {
    signalpp::signalpp<void(int)>::signalpp_connection conn;

    {
        signalpp::signalpp<void(int)> signal;
        conn = signal.connect([](int) {});
    } // signal destroyed here

    conn.disconnect();
    check(true, "disconnect after signal destruction is safe");
}

void test_slot_disconnects_itself_during_emit() {
    signalpp::signalpp<void()> signal;

    int call_count = 0;
    signalpp::signalpp<void()>::signalpp_connection conn;

    conn = signal.connect([&]() {
        ++call_count;
        conn.disconnect();
    });

    signal.emit();
    signal.emit();

    check(call_count == 1, "slot can safely disconnect itself inside emit");
}

void test_slot_connects_new_slot_during_emit() {
    signalpp::signalpp<void()> signal;

    int first_calls = 0;
    int second_calls = 0;

    signal += [&]() {
        ++first_calls;
        if (first_calls == 1) {
            signal += [&]() {
                ++second_calls;
            };
        }
    };

    signal.emit();
    check(first_calls == 1 && second_calls == 0, "new slot not called in same emit");

    signal.emit();
    check(first_calls == 2 && second_calls == 1, "new slot called on subsequent emit");
}

void test_member_function_connection() {
    struct Receiver {
        int sum = 0;
        mutable int const_calls = 0;

        void on_value(int v) {
            sum += v;
        }
        void on_value_const(int) const {
            ++const_calls;
        }
    };

    Receiver r;
    signalpp::signalpp<void(int)> signal;

    auto c1 = signal.scoped_connect(&r, &Receiver::on_value);
    auto c2 = signal.scoped_connect(&r, &Receiver::on_value_const);

    signal.emit(10);
    signal.emit(5);

    check(r.sum == 15, "non-const member function receives arguments");
    check(r.const_calls == 2, "const member function receives arguments");
}

void test_multiple_slots_receive_intact_rvalue_arguments() {
    signalpp::signalpp<void(std::string)> signal;

    std::string received1;
    std::string received2;

    signal += [&](const std::string& s) {
        received1 = s;
    };
    signal += [&](const std::string& s) {
        received2 = s;
    };

    signal.emit(std::string("hello world"));

    check(
        received1 == "hello world" && received2 == "hello world",
        "all slots receive valid argument when rvalue is emitted"
    );
}

void test_threadsafe_concurrent_emit_and_connect() {
    signalpp::signalpp_threadsafe<void(int)> signal;
    std::atomic<int> total_calls{0};

    std::thread emitter([&]() {
        for (int i = 0; i < 200; ++i) {
            signal.emit(i);
        }
    });

    std::thread subscriber([&]() {
        for (int i = 0; i < 50; ++i) {
            auto conn = signal.scoped_connect([&](int) {
                total_calls.fetch_add(1, std::memory_order_relaxed);
            });
        }
    });

    emitter.join();
    subscriber.join();

    check(total_calls.load() >= 0, "threadsafe signal handles concurrent connect/disconnect/emit without crashing");
}

} // namespace

int main() {
    // 
    test_emit_calls_connected_slot();
    test_disconnect_stops_calls();
    test_repeated_disconnect_is_safe();
    test_scoped_connection_disconnects_on_destroy();
    test_multiple_slots_are_called();

    // 
    test_scoped_connection_move_assignment_disconnects_previous();
    test_disconnect_after_signal_destroyed_is_safe();
    test_slot_disconnects_itself_during_emit();
    test_slot_connects_new_slot_during_emit();
    test_member_function_connection();
    test_multiple_slots_receive_intact_rvalue_arguments();
    test_threadsafe_concurrent_emit_and_connect();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}