# signalpp

A lightweight, header-only C++20 signal/slot library.

## Usage

### 1. Basic Usage
Create a signal with the desired function signature, connect lambdas or functions, and emit events:

```cpp
#include <iostream>
#include <string>
#include <signalpp/signalpp.hpp>

int main() {
    signalpp::signalpp<void(const std::string&, int)> on_message;

    // Fire-and-forget subscription
    on_message += [](const std::string& user, int code) {
        std::cout << "[" << code << "] " << user << " connected\n";
    };

    // Manual connection management
    auto conn = on_message.connect([](const std::string& user, int) {
        std::cout << "Logging event for: " << user << "\n";
    });

    on_message.emit("Alice", 200);

    // Unsubscribe manually
    conn.disconnect();
    on_message.emit("Bob", 200); // Only the first slot is called
}
```

### 2. Automatic Unsubscription (RAII)
Use `scoped_connect` when subscribing inside classes or temporary scopes so the slot is automatically disconnected upon destruction:

```cpp
#include <iostream>
#include <signalpp/signalpp.hpp>

class Player {
public:
    signalpp::signalpp<void(int)> on_health_changed;

    void take_damage(int amount) {
        hp_ -= amount;
        on_health_changed.emit(hp_);
    }

private:
    int hp_ = 100;
};

class HealthBarUI {
public:
    explicit HealthBarUI(Player& player) {
        // Automatically disconnects when HealthBarUI is destroyed
        conn_ = player.on_health_changed.scoped_connect(this, &HealthBarUI::render);
    }

    void render(int current_hp) {
        std::cout << "UI Health Bar: " << current_hp << " HP\n";
    }

private:
    signalpp::signalpp<void(int)>::signalpp_scoped_connection conn_;
};

int main() {
    Player player;

    {
        HealthBarUI ui(player);
        player.take_damage(25); // Prints: UI Health Bar: 75 HP
    } // `ui` is destroyed here and safely unsubscribed

    player.take_damage(10); // Safe: no dangling pointer call
}
```

### 3. Thread-Safe Signals
Use `signalpp::signalpp_threadsafe` when connecting, disconnecting, or emitting signals across multiple threads:

```cpp
#include <thread>
#include <signalpp/signalpp.hpp>

int main() {
    signalpp::signalpp_threadsafe<void(int)> on_tick;

    auto conn = on_tick.scoped_connect([](int tick) {
        // Safely invoked even if other threads connect/disconnect concurrently
    });

    std::thread worker([&on_tick]() {
        for (int i = 0; i < 10; ++i) {
            on_tick.emit(i);
        }
    });

    worker.join();
}
```


## Build and test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

For Visual Studio, use `-C Debug` with `ctest`.

## Use in another project

```cmake
add_subdirectory(path/to/signalpp)

target_link_libraries(my_app PRIVATE signalpp::signalpp)
```

## API Reference

### Signal Types

| Type | Description |
| :--- | :--- |
| `signalpp::signalpp<Sig>` | Single-threaded signal with zero mutex overhead (`empty_mutex`). |
| `signalpp::signalpp_threadsafe<Sig>` | Thread-safe signal protected by `std::shared_mutex`. |

### Signal Methods

| Method | Description |
| :--- | :--- |
| `connect(slot)` | Connects a callable and returns a `signalpp_connection` for manual unsubscription. |
| `connect(obj, &Class::method)` | Connects a member function of an object and returns a `signalpp_connection`. |
| `scoped_connect(slot)` | Connects a callable and returns an RAII `signalpp_scoped_connection`. |
| `scoped_connect(obj, &Class::method)` | Connects a member function and returns an RAII `signalpp_scoped_connection`. |
| `operator+=(slot)` | Connects a callable without returning a connection handle (fire-and-forget). |
| `emit(args...)` | Invokes all currently connected slots with the provided arguments. |

### Connection Handles

| Type / Method | Description |
| :--- | :--- |
| `signalpp_connection` | Move-only handle for a slot subscription. Does **not** disconnect on destruction. |
| `signalpp_scoped_connection` | Move-only RAII handle. Automatically calls `disconnect()` when destroyed or overwritten. |
| `conn.disconnect()` | Unsubscribes the slot. Safe to call multiple times or after the signal is destroyed. |
