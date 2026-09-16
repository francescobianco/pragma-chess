find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Sql)

# Project files (.pch) are YAML. Prefer a system yaml-cpp, else fetch it.
find_package(yaml-cpp 0.8 QUIET)
if(NOT yaml-cpp_FOUND)
    include(FetchContent)
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
    set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.9.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(yaml-cpp)
endif()

qt_standard_project_setup()

set(APP_ID "io.github.francescobianco.PragmaChess")

qt_add_executable(pragma-chess
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/app/BoardState.cpp
    src/app/BoardState.h
    src/app/ClassicGames.cpp
    src/app/ClassicGames.h
    src/app/GameDatabase.h
    src/app/GameRecord.h
    src/app/Project.cpp
    src/app/Project.h
    src/app/GameSession.cpp
    src/app/GameSession.h
    src/app/SqliteGameDatabase.cpp
    src/app/SqliteGameDatabase.h
    src/app/UserFolders.cpp
    src/app/UserFolders.h
    src/platform/GtkDesktopStyle.cpp
    src/platform/GtkDesktopStyle.h
    src/models/GameListModel.cpp
    src/models/GameListModel.h
    src/models/MoveListModel.cpp
    src/models/MoveListModel.h
    src/widgets/BoardPanel.cpp
    src/widgets/BoardPanel.h
    src/widgets/BoardWidget.cpp
    src/widgets/BoardWidget.h
)

target_include_directories(pragma-chess PRIVATE src)
target_link_libraries(pragma-chess PRIVATE Qt6::Widgets Qt6::Sql yaml-cpp::yaml-cpp)
target_compile_definitions(pragma-chess PRIVATE
    APP_ID="${APP_ID}"
    APP_VERSION="${PROJECT_VERSION}"
    QT_NO_KEYWORDS
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pragma-chess PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(pragma-chess PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_GUI_IDENTIFIER "${APP_ID}"
    MACOSX_BUNDLE_BUNDLE_NAME "Pragma Chess"
)

include(GNUInstallDirs)
install(TARGETS pragma-chess
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(UNIX AND NOT APPLE)
    install(FILES data/${APP_ID}.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()
