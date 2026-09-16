cmake_minimum_required(VERSION 3.21)

project(PragmaChess
    VERSION 0.1.0
    DESCRIPTION "Open source chess database with a native desktop client"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

# The chess database engine (core/, Rust) is built separately with Cargo.
# The desktop client lives in gui/qt and talks to it through an interface
# layer (gui/qt/src/app) so it can be developed on its own.
add_subdirectory(gui/qt)
