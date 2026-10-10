cmake_minimum_required(VERSION 3.21)

# Release builds pass the version of their tag (see packaging/).
set(PRAGMA_VERSION 0.3.0 CACHE STRING "Version of Pragma Chess being built")

project(PragmaChess
    VERSION ${PRAGMA_VERSION}
    DESCRIPTION "Open source chess database with a native desktop client"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

# The desktop client, its core library (gui/qt/src/app: rules, databases,
# indexes, Explain) and its command line tools all live in gui/qt.
add_subdirectory(gui/qt)

# .deb and .rpm packages (cpack -G DEB|RPM, see packaging/linux).
include(packaging/linux/Packaging.cmake)
