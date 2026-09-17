find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Sql Network)
# Vector piece sets need Qt SVG; without it the board falls back to font glyphs.
find_package(Qt6 6.4 QUIET COMPONENTS Svg)
find_package(Qt6 6.4 QUIET COMPONENTS Test)
# Translations of the interface (translations/*.ts) need Qt Linguist's tools.
find_package(Qt6 6.4 QUIET COMPONENTS LinguistTools)

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

# Chess rules, engine driver and "Explain" logic: Qt Core only, shared by the
# desktop client, the command line tools and the tests.
qt_add_library(pragma-chess-core STATIC
    src/app/AdvantageProbe.cpp
    src/app/AdvantageProbe.h
    src/app/BoardState.cpp
    src/app/BoardState.h
    src/app/ChessPosition.cpp
    src/app/ChessPosition.h
    src/app/DatabaseOutline.cpp
    src/app/DatabaseOutline.h
    src/app/EngineEvaluation.cpp
    src/app/EngineEvaluation.h
    src/app/Explainer.cpp
    src/app/Explainer.h
    src/app/ExplanationSearch.cpp
    src/app/ExplanationSearch.h
    src/app/MoveExplanation.cpp
    src/app/MoveExplanation.h
    src/app/OpeningNames.cpp
    src/app/OpeningNames.h
    src/app/Pgn.cpp
    src/app/Pgn.h
    src/app/PolyglotBook.cpp
    src/app/PolyglotBook.h
    src/app/PolyglotRandom.cpp
    src/app/UciEngine.cpp
    src/app/UciEngine.h
    src/app/UiLanguage.cpp
    src/app/UiLanguage.h
    src/app/sources/ChessComFetch.cpp
    src/app/sources/ChessComFetch.h
    src/app/sources/GameSource.h
    src/app/sources/LichessFetch.cpp
    src/app/sources/LichessFetch.h
    src/app/sources/LichessSignIn.cpp
    src/app/sources/LichessSignIn.h
    src/app/sources/SourceCatalog.cpp
    src/app/sources/SourceCatalog.h
    src/app/sources/SourceCredentials.cpp
    src/app/sources/SourceCredentials.h
    src/app/sources/SourceFetch.cpp
    src/app/sources/SourceFetch.h
    src/app/sources/SourceSync.cpp
    src/app/sources/SourceSync.h
    src/app/sources/TorneiOnlineFetch.cpp
    src/app/sources/TorneiOnlineFetch.h
    src/app/sync/FolderSync.cpp
    src/app/sync/FolderSync.h
    src/app/sync/FtpStore.cpp
    src/app/sync/FtpStore.h
    src/app/sync/GitStore.cpp
    src/app/sync/GitStore.h
    src/app/sync/RemoteStore.h
    src/app/sync/SyncManifest.cpp
    src/app/sync/SyncManifest.h
    src/app/sync/SyncSettings.cpp
    src/app/sync/SyncSettings.h
    src/app/sync/WebDavStore.cpp
    src/app/sync/WebDavStore.h
)
target_include_directories(pragma-chess-core PUBLIC src)
target_link_libraries(pragma-chess-core PUBLIC Qt6::Core Qt6::Network)
target_compile_definitions(pragma-chess-core PUBLIC QT_NO_KEYWORDS APP_VERSION="${PROJECT_VERSION}")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(PRAGMA_WARNINGS -Wall -Wextra -Wpedantic)
endif()
target_compile_options(pragma-chess-core PRIVATE ${PRAGMA_WARNINGS})

qt_add_executable(pragma-chess
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/dialogs/ConnectSourceWizard.cpp
    src/dialogs/ConnectSourceWizard.h
    src/dialogs/GameInfoDialog.cpp
    src/dialogs/GameInfoDialog.h
    src/dialogs/ManageSourcesDialog.cpp
    src/dialogs/ManageSourcesDialog.h
    src/dialogs/SyncDialog.cpp
    src/dialogs/SyncDialog.h
    src/dialogs/SourceSettingsWidget.cpp
    src/dialogs/SourceSettingsWidget.h
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
    src/platform/SymbolicIcons.cpp
    src/platform/SymbolicIcons.h
    src/models/GameFilterProxyModel.cpp
    src/models/GameFilterProxyModel.h
    src/models/GameListModel.cpp
    src/models/GameListModel.h
    src/models/MoveListModel.cpp
    src/models/MoveListModel.h
    src/widgets/BoardPanel.cpp
    src/widgets/BoardPanel.h
    src/widgets/BookPanel.cpp
    src/widgets/BookPanel.h
    src/widgets/BoardWidget.cpp
    src/widgets/CapturedPiecesWidget.cpp
    src/widgets/CapturedPiecesWidget.h
    src/widgets/PieceRenderer.cpp
    src/widgets/PieceRenderer.h
    src/widgets/CentralArea.cpp
    src/widgets/DatabaseTreeWidget.cpp
    src/widgets/DatabaseTreeWidget.h
    src/widgets/CentralArea.h
    src/widgets/EnginePanel.cpp
    src/widgets/EnginePanel.h
    src/widgets/EvaluationBar.cpp
    src/widgets/EvaluationBar.h
    src/widgets/GameHeaderWidget.cpp
    src/widgets/GameHeaderWidget.h
    src/widgets/BoardWidget.h
)

target_include_directories(pragma-chess PRIVATE src)
target_link_libraries(pragma-chess PRIVATE pragma-chess-core Qt6::Widgets Qt6::Sql yaml-cpp::yaml-cpp)

# Application icon, generated from data/icons/pragma-chess.png by scripts/make-icons.py.
set(APP_ICON_SIZES 16 22 24 32 48 64 128 256)
set(APP_ICON_FILES)
foreach(size IN LISTS APP_ICON_SIZES)
    list(APPEND APP_ICON_FILES data/icons/hicolor/${size}x${size}/apps/${APP_ID}.png)
endforeach()
# Embedded for the window icon, available even when the app is not installed.
# Interface translations, loaded by UiLanguage from :/i18n.
file(GLOB PRAGMA_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/pragma-chess_*.ts)
if(TARGET Qt6::lrelease AND PRAGMA_TS_FILES)
    qt_add_translations(pragma-chess TS_FILES ${PRAGMA_TS_FILES} RESOURCE_PREFIX /i18n)
elseif(PRAGMA_TS_FILES)
    message(STATUS "Qt Linguist tools not found: the interface is built in English only")
endif()

# The opening book seeded into the Books folder on first launch.
qt_add_resources(pragma-chess books PREFIX /books BASE resources/books FILES resources/books/pragma-openings.bin)
# The named openings seeded as the Opening Names database on first launch.
qt_add_resources(pragma-chess openings PREFIX /openings BASE resources/openings
    FILES resources/openings/lichess-openings.tsv)
qt_add_resources(pragma-chess appicon PREFIX /icons BASE data/icons FILES ${APP_ICON_FILES})
if(WIN32)
    target_sources(pragma-chess PRIVATE data/pragma-chess.rc)
elseif(APPLE)
    set(APP_ICNS data/icons/pragma-chess.icns)
    set_source_files_properties(${APP_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(pragma-chess PRIVATE ${APP_ICNS})
endif()

if(TARGET Qt6::Svg)
    target_link_libraries(pragma-chess PRIVATE Qt6::Svg)
    target_compile_definitions(pragma-chess PRIVATE PRAGMA_HAS_SVG)
    file(GLOB PIECE_SVGS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} resources/pieces/companion/*.svg)
    qt_add_resources(pragma-chess pieces PREFIX / FILES ${PIECE_SVGS})
else()
    message(STATUS "Qt SVG not found: using font glyphs for chess pieces (install qt6-svg-dev)")
endif()
target_compile_definitions(pragma-chess PRIVATE
    APP_ID="${APP_ID}"
    QT_NO_KEYWORDS
)

target_compile_options(pragma-chess PRIVATE ${PRAGMA_WARNINGS})

set_target_properties(pragma-chess PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_GUI_IDENTIFIER "${APP_ID}"
    MACOSX_BUNDLE_BUNDLE_NAME "Pragma Chess"
    MACOSX_BUNDLE_ICON_FILE pragma-chess.icns
)

# Command line "Explain", for tuning it on real lines (see docs/explain-tuning.md).
qt_add_executable(pragma-explain tools/explain/main.cpp)
target_link_libraries(pragma-explain PRIVATE pragma-chess-core)
target_compile_options(pragma-explain PRIVATE ${PRAGMA_WARNINGS})
set_target_properties(pragma-explain PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)

# Builds and reads Polyglot opening books (see tools/book/main.cpp).
qt_add_executable(pragma-book tools/book/main.cpp)
target_link_libraries(pragma-book PRIVATE pragma-chess-core)
target_compile_options(pragma-book PRIVATE ${PRAGMA_WARNINGS})
set_target_properties(pragma-book PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)

# Tested without a display.
if(TARGET Qt6::Test)
    qt_add_executable(tst_chessrules tests/tst_chessrules.cpp
        src/app/SqliteGameDatabase.cpp
        src/app/SqliteGameDatabase.h)
    target_link_libraries(tst_chessrules PRIVATE pragma-chess-core Qt6::Sql Qt6::Test)
    target_compile_options(tst_chessrules PRIVATE ${PRAGMA_WARNINGS})
    set_target_properties(tst_chessrules PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)
    add_test(NAME chessrules COMMAND tst_chessrules)
else()
    message(STATUS "Qt Test not found: tests are not built")
endif()

include(GNUInstallDirs)
install(TARGETS pragma-chess
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(UNIX AND NOT APPLE)
    install(FILES data/${APP_ID}.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(DIRECTORY data/icons/hicolor
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)
endif()
