#include <signalpp/signalpp.hpp>

#include <iostream>
#include <string>

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

} // namespace

int main() {
    test_emit_calls_connected_slot();
    test_disconnect_stops_calls();
    test_repeated_disconnect_is_safe();
    test_scoped_connection_disconnects_on_destroy();
    test_multiple_slots_are_called();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}