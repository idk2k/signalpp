add_executable(signalpp_tests
    signalpp_test.cpp
)

target_link_libraries(signalpp_tests
    PRIVATE
        signalpp::signalpp
)

target_compile_options(signalpp_tests
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4
            /WX
        >
)

add_test(
    NAME signalpp_tests
    COMMAND signalpp_tests
)