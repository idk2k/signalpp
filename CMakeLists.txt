cmake_minimum_required(VERSION 3.14)
project(signalpp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(signalpp INTERFACE)

target_include_directories(signalpp
    INTERFACE
    $<BUILD_INTERFACE>:${CMAKE_CURRENT_SOURCE_DIR/include}
    $<INSTALL_INTERFACE>:incldue
)

# install is not needed here, but acturlly its cool
# install(
#     DIRECTORY include/ 
#     DESTINATION include/${PROJECT_NAME}
# )