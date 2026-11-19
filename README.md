# signalpp

A lightweight, header-only C++20 signal/slot library.

## Usage

```cpp
#include <signalpp/signalpp.hpp>

signalpp::signalpp<void(int)> signal;

signal.connect([](int value) {
    std::cout << "Value: " << value << '\n';
});

signal.emit(42);
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