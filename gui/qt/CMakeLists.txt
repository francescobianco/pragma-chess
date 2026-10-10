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

# Phone Link (docs/phone-link.md): WebRTC and the WebSocket client for Nostr
# relays from libdatachannel, BIP-340 keys from libsecp256k1, QR codes from
# nayuki's qrcodegen. System packages are preferred; otherwise they are
# fetched and linked statically. Without them the client builds without it.
option(PRAGMA_PHONE_LINK "Let the Android app copy the databases (Phone Link)" ON)
set(PRAGMA_HAS_PHONE_LINK OFF)
if(PRAGMA_PHONE_LINK)
    # FetchContent_Declare's SYSTEM (3.25) and EXCLUDE_FROM_ALL (3.28): an
    # older CMake fetches nothing and the client is built without the network.
    if(CMAKE_VERSION VERSION_LESS 3.28)
        message(WARNING "CMake ${CMAKE_VERSION} is too old to fetch libdatachannel and libsecp256k1 "
                        "(3.28 or later is needed): unless they are installed, Phone Link and the lobby are left out")
    endif()
    include(FetchContent)
    find_package(OpenSSL QUIET)
    find_package(LibDataChannel CONFIG QUIET)
    if(TARGET LibDataChannel::LibDataChannel)
        set(PRAGMA_DATACHANNEL LibDataChannel::LibDataChannel)
    elseif(OpenSSL_FOUND)
        set(BUILD_SHARED_LIBS OFF)
        set(NO_MEDIA ON CACHE BOOL "" FORCE)
        set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
        set(NO_TESTS ON CACHE BOOL "" FORCE)
        set(NO_WEBSOCKET OFF CACHE BOOL "" FORCE)
        set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
        set(USE_MBEDTLS OFF CACHE BOOL "" FORCE)
        set(USE_NICE OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(libdatachannel
            GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
            GIT_TAG v0.24.6
            GIT_SHALLOW TRUE
            GIT_SUBMODULES deps/plog deps/usrsctp deps/libjuice
            EXCLUDE_FROM_ALL
            SYSTEM)
        FetchContent_MakeAvailable(libdatachannel)
        set(PRAGMA_DATACHANNEL LibDataChannel::LibDataChannelStatic)
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SECP256K1 QUIET IMPORTED_TARGET libsecp256k1)
    endif()
    if(SECP256K1_FOUND)
        find_path(SECP256K1_SCHNORRSIG_H secp256k1_schnorrsig.h HINTS ${SECP256K1_INCLUDE_DIRS})
        find_path(SECP256K1_ECDH_H secp256k1_ecdh.h HINTS ${SECP256K1_INCLUDE_DIRS})
    endif()
    if(SECP256K1_FOUND AND SECP256K1_SCHNORRSIG_H AND SECP256K1_ECDH_H)
        set(PRAGMA_SECP256K1 PkgConfig::SECP256K1)
    else()
        set(BUILD_SHARED_LIBS OFF)
        set(SECP256K1_ENABLE_MODULE_ECDH ON CACHE BOOL "" FORCE)
        set(SECP256K1_ENABLE_MODULE_EXTRAKEYS ON CACHE BOOL "" FORCE)
        set(SECP256K1_ENABLE_MODULE_SCHNORRSIG ON CACHE BOOL "" FORCE)
        set(SECP256K1_ENABLE_MODULE_MUSIG OFF CACHE BOOL "" FORCE)
        set(SECP256K1_ENABLE_MODULE_ELLSWIFT OFF CACHE BOOL "" FORCE)
        set(SECP256K1_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
        set(SECP256K1_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(SECP256K1_BUILD_EXHAUSTIVE_TESTS OFF CACHE BOOL "" FORCE)
        set(SECP256K1_BUILD_CTIME_TESTS OFF CACHE BOOL "" FORCE)
        set(SECP256K1_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(secp256k1
            GIT_REPOSITORY https://github.com/bitcoin-core/secp256k1.git
            GIT_TAG v0.7.1
            GIT_SHALLOW TRUE
            EXCLUDE_FROM_ALL
            SYSTEM)
        FetchContent_MakeAvailable(secp256k1)
        set(PRAGMA_SECP256K1 secp256k1)
    endif()

    FetchContent_Declare(qrcodegen
        GIT_REPOSITORY https://github.com/nayuki/QR-Code-generator.git
        GIT_TAG v1.8.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(qrcodegen)

    if(PRAGMA_DATACHANNEL AND TARGET ${PRAGMA_SECP256K1})
        set(PRAGMA_HAS_PHONE_LINK ON)
    else()
        message(STATUS "libdatachannel or libsecp256k1 not available: building without Phone Link")
    endif()
endif()

# Third-party code fetched and built here (yaml-cpp, libdatachannel and what
# it brings, secp256k1): its warnings are for its authors, and MSVC prints a
# dozen of them in every Windows build. Our own code keeps all its warnings.
foreach(dependency yaml-cpp datachannel-static usrsctp juice-static srtp2 secp256k1)
    if(TARGET ${dependency})
        get_target_property(imported ${dependency} IMPORTED)
        get_target_property(aliased ${dependency} ALIASED_TARGET)
        if(NOT imported AND NOT aliased)
            target_compile_options(${dependency} PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
        endif()
    endif()
endforeach()

qt_standard_project_setup()

set(APP_ID "io.github.francescobianco.PragmaChess")

# Chess rules, engine driver and "Explain" logic: Qt Core only, shared by the
# desktop client, the command line tools and the tests.
qt_add_library(pragma-chess-core STATIC
    src/app/api/LocalHttpServer.cpp
    src/app/api/LocalHttpServer.h
    src/app/AdvantageProbe.cpp
    src/app/AdvantageProbe.h
    src/app/BookWeights.cpp
    src/app/BookWeights.h
    src/app/GraphicsSettings.cpp
    src/app/GraphicsSettings.h
    src/app/BoardState.cpp
    src/app/BoardState.h
    src/app/ChessPosition.cpp
    src/app/ChessPosition.h
    src/app/DatabaseOutline.cpp
    src/app/TrainingSets.cpp
    src/app/DatabaseDedupe.cpp
    src/app/DatabaseDedupe.h
    src/app/DatabaseProperties.cpp
    src/app/DatabaseOutline.h
    src/app/TrainingSets.h
    src/app/EngineCatalog.cpp
    src/app/EngineCatalog.h
    src/app/EngineDetector.cpp
    src/app/EngineDetector.h
    src/app/EngineEvaluation.cpp
    src/app/EngineEvaluation.h
    src/app/EnginePower.cpp
    src/app/EnginePower.h
    src/app/ExplainTicks.cpp
    src/app/ExplainTicks.h
    src/app/Explainer.cpp
    src/app/Explainer.h
    src/app/ExplanationSearch.cpp
    src/app/ExplanationSearch.h
    src/app/GameIdentity.cpp
    src/app/GameIdentity.h
    src/app/GameState.cpp
    src/app/GameState.h
    src/app/LineInsight.cpp
    src/app/LineInsight.h
    src/app/MoveExplanation.cpp
    src/app/MoveExplanation.h
    src/app/MoveComment.cpp
    src/app/MoveComment.h
    src/app/MoveAnnotation.cpp
    src/app/MoveAnnotation.h
    src/app/OpeningNames.cpp
    src/app/OpeningNames.h
    src/app/Pgn.cpp
    src/app/Pgn.h
    src/app/chessbase/Cba2Decoder.cpp
    src/app/chessbase/Cba2Decoder.h
    src/app/chessbase/Cbg2Decoder.cpp
    src/app/chessbase/Cbg2Decoder.h
    src/app/chessbase/CbgDecoder.cpp
    src/app/chessbase/CbgDecoder.h
    src/app/chessbase/ChessBase2Database.cpp
    src/app/chessbase/ChessBase2Database.h
    src/app/chessbase/ChessBaseDatabase.cpp
    src/app/chessbase/ChessBaseDatabase.h
    src/app/sources/ChessBaseFetch.cpp
    src/app/sources/ChessBaseFetch.h
    src/app/GameSession.cpp
    src/app/GameSession.h
    src/app/GameVariations.cpp
    src/app/GameVariations.h
    src/app/HelpGuide.cpp
    src/app/HelpGuide.h
    src/app/TrainingTutor.cpp
    src/app/TrainingTutor.h
    src/app/PlayerRole.cpp
    src/app/PlayerRole.h
    src/app/PolyglotBook.cpp
    src/app/Chapters.cpp
    src/app/LocalizedText.cpp
    src/app/LocalizedText.h
    src/app/Chapters.h
    src/app/PositionSetup.cpp
    src/app/PositionSetup.h
    src/app/PolyglotBook.h
    src/app/PolyglotRandom.cpp
    src/app/PositionIndex.cpp
    src/app/PositionIndex.h
    src/app/PositionIndexBuilder.cpp
    src/app/PositionIndexBuilder.h
    src/app/Reconcile.cpp
    src/app/Reconcile.h
    src/app/ShippedOpeningNames.cpp
    src/app/ShippedOpeningNames.h
    src/app/UciEngine.cpp
    src/app/UciEngine.h
    src/app/TimeControl.cpp
    src/app/TimeControl.h
    src/app/UiLanguage.cpp
    src/app/UiLanguage.h
    src/app/online/LichessBoardClient.cpp
    src/app/online/LichessBoardClient.h
    src/app/online/OnlineAccount.cpp
    src/app/online/OnlineAccount.h
    src/app/lobby/Lobby.cpp
    src/app/lobby/Lobby.h
    src/app/lobby/LobbyLedger.cpp
    src/app/lobby/LobbyLedger.h
    src/app/lobby/LobbyPlans.cpp
    src/app/lobby/LobbyPlans.h
    src/app/lobby/LobbyService.h
    src/app/lobby/RoomName.cpp
    src/app/lobby/RoomName.h
    src/app/online/OnlineGame.cpp
    src/app/online/OnlineGame.h
    src/app/sources/ChessComFetch.cpp
    src/app/sources/ChessComFetch.h
    src/app/sources/GameSource.h
    src/app/sources/LichessStudy.cpp
    src/app/sources/LichessStudy.h
    src/app/sources/LichessStudyFetch.cpp
    src/app/sources/LichessStudyFetch.h
    src/app/sources/LichessFetch.cpp
    src/app/sources/LichessFetch.h
    src/app/sources/LichessSignIn.cpp
    src/app/sources/LichessSignIn.h
    src/app/sources/PgnFile.cpp
    src/app/sources/PgnFile.h
    src/app/sources/PgnFileFetch.cpp
    src/app/sources/PgnFileFetch.h
    src/app/sources/PgnFilePlan.cpp
    src/app/sources/PgnFilePlan.h
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
    src/app/smart/SmartChess.cpp
    src/app/smart/SmartChess.h
    src/app/smart/SmartInterpreter.cpp
    src/app/smart/SmartInterpreter.h
    src/app/smart/SmartPrograms.cpp
    src/app/smart/SmartPrograms.h
    src/app/smart/SmartScript.cpp
    src/app/smart/SmartScript.h
    src/app/smart/SmartValue.cpp
    src/app/smart/SmartValue.h
    src/app/sync/FolderSync.cpp
    src/app/sync/FolderSync.h
    src/app/sync/FtpStore.cpp
    src/app/sync/FtpStore.h
    src/app/sync/GitStore.cpp
    src/app/sync/GitStore.h
    src/app/sync/RemoteStore.h
    src/app/sync/SyncManifest.cpp
    src/app/sync/SyncManifest.h
    src/app/sync/SyncPipeline.cpp
    src/app/sync/SyncPipeline.h
    src/app/sync/SyncTasks.cpp
    src/app/sync/SyncTasks.h
    src/app/sync/SyncSettings.cpp
    src/app/sync/SyncSettings.h
    src/app/sync/WebDavStore.cpp
    src/app/sync/WebDavStore.h
)
target_include_directories(pragma-chess-core PUBLIC src)
target_link_libraries(pragma-chess-core PUBLIC Qt6::Core Qt6::Network)
# The SMART programs (smart/README.md), built in; PRAGMA_SMART_DIR reads them from a folder instead.
qt_add_resources(pragma-chess-core smart PREFIX /smart BASE ${CMAKE_CURRENT_SOURCE_DIR}/../../smart
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../smart/TUTOR.smart ${CMAKE_CURRENT_SOURCE_DIR}/../../smart/EXPLAIN.smart
          ${CMAKE_CURRENT_SOURCE_DIR}/../../smart/INSIGHT.smart)
target_compile_definitions(pragma-chess-core PUBLIC QT_NO_KEYWORDS APP_VERSION="${PROJECT_VERSION}")

# The engine shipped with the app (EngineCatalog): its version is pinned in
# packaging/stockfish.env, and scripts/build-stockfish.sh builds and stages it in
# engines/ next to the executable (`make stockfish` for a development build).
file(STRINGS ${CMAKE_SOURCE_DIR}/packaging/stockfish.env STOCKFISH_VERSION_LINE REGEX "^STOCKFISH_VERSION=")
string(REGEX REPLACE "^STOCKFISH_VERSION=" "" STOCKFISH_VERSION "${STOCKFISH_VERSION_LINE}")
target_compile_definitions(pragma-chess-core PRIVATE PRAGMA_BUNDLED_ENGINE_NAME="Stockfish ${STOCKFISH_VERSION}")
set(PRAGMA_ENGINES_STAGE ${CMAKE_CURRENT_BINARY_DIR}/engines CACHE PATH
    "Folder where scripts/build-stockfish.sh staged the bundled engine")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(PRAGMA_WARNINGS -Wall -Wextra -Wpedantic)
endif()
target_compile_options(pragma-chess-core PRIVATE ${PRAGMA_WARNINGS})

# Phone Link (docs/phone-link.md): Nostr signaling, WebRTC and the file
# protocol; linked by the desktop client, pragma-phone and the tests.
if(PRAGMA_HAS_PHONE_LINK)
    qt_add_library(pragma-phone-link STATIC
        src/app/phone/Nip44.cpp
        src/app/phone/Nip44.h
        src/app/phone/NostrEvent.cpp
        src/app/phone/NostrEvent.h
        src/app/phone/NostrKey.cpp
        src/app/phone/NostrKey.h
        src/app/phone/NostrRelayPool.cpp
        src/app/phone/NostrRelayPool.h
        src/app/phone/PairingLink.cpp
        src/app/phone/PairingLink.h
        src/app/phone/PhoneFiles.cpp
        src/app/phone/PhoneFiles.h
        src/app/phone/PhoneGameStore.h
        src/app/phone/PhoneGames.cpp
        src/app/phone/PhoneGames.h
        src/app/phone/PhoneLink.cpp
        src/app/phone/PhoneLink.h
        src/app/phone/PhoneLinkClient.cpp
        src/app/phone/PhoneLinkClient.h
        src/app/phone/PhoneLinkSession.cpp
        src/app/phone/PhoneLinkSession.h
        src/app/phone/WebRtcPeer.cpp
        src/app/phone/WebRtcPeer.h
        src/app/lobby/net/LobbyIdentity.cpp
        src/app/lobby/net/LobbyIdentity.h
        src/app/lobby/net/LobbyNetwork.cpp
        src/app/lobby/net/LobbyNetwork.h
        src/app/lobby/net/LobbyNode.cpp
        src/app/lobby/net/LobbyNode.h
    )
    target_link_libraries(pragma-phone-link PUBLIC pragma-chess-core PRIVATE ${PRAGMA_DATACHANNEL} ${PRAGMA_SECP256K1})
    target_compile_definitions(pragma-phone-link PUBLIC PRAGMA_HAS_PHONE_LINK)
    target_compile_options(pragma-phone-link PRIVATE ${PRAGMA_WARNINGS})

    add_library(qrcodegen STATIC ${qrcodegen_SOURCE_DIR}/cpp/qrcodegen.cpp ${qrcodegen_SOURCE_DIR}/cpp/qrcodegen.hpp)
    target_include_directories(qrcodegen SYSTEM PUBLIC ${qrcodegen_SOURCE_DIR}/cpp)
endif()

qt_add_executable(pragma-chess
    src/main.cpp
    src/DesktopApi.cpp
    src/DesktopApi.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/dialogs/ConnectSourceWizard.cpp
    src/dialogs/ConnectSourceWizard.h
    src/dialogs/SourceKindDelegate.cpp
    src/dialogs/SourceKindDelegate.h
    src/dialogs/GameInfoDialog.cpp
    src/dialogs/GameInfoDialog.h
    src/dialogs/ManageChaptersDialog.cpp
    src/dialogs/ManageChaptersDialog.h
    src/dialogs/ManageEnginesDialog.cpp
    src/dialogs/ManageEnginesDialog.h
    src/dialogs/ManageSourcesDialog.cpp
    src/dialogs/ManageSourcesDialog.h
    src/dialogs/GraphicsSettingsDialog.cpp
    src/dialogs/GraphicsSettingsDialog.h
    src/dialogs/FolderSettingsDialog.cpp
    src/dialogs/FolderSettingsDialog.h
    src/dialogs/DatabaseSettingsDialog.cpp
    src/dialogs/DatabaseSettingsDialog.h
    src/dialogs/AboutDialog.cpp
    src/dialogs/AboutDialog.h
    src/dialogs/HelpDialog.cpp
    src/dialogs/HelpDialog.h
    src/dialogs/NewGameChoiceDialog.cpp
    src/dialogs/NewGameChoiceDialog.h
    src/dialogs/NewTrainingDialog.cpp
    src/dialogs/NewTrainingDialog.h
    src/dialogs/PersonalSettingsDialog.cpp
    src/dialogs/PersonalSettingsDialog.h
    src/dialogs/DrawersDialog.cpp
    src/dialogs/DrawersDialog.h
    src/dialogs/LobbyDialog.cpp
    src/dialogs/LobbyDialog.h
    src/dialogs/PlayOnlineDialog.cpp
    src/dialogs/PlayOnlineDialog.h
    src/dialogs/ProjectSettingsDialog.cpp
    src/dialogs/ProjectSettingsDialog.h
    src/dialogs/PositionSetupDialog.cpp
    src/dialogs/PositionSetupDialog.h
    src/dialogs/SyncDialog.cpp
    src/dialogs/SyncDialog.h
    src/dialogs/ManageSyncFilesDialog.cpp
    src/dialogs/ManageSyncFilesDialog.h
    src/dialogs/SourceSettingsWidget.cpp
    src/dialogs/SourceSettingsWidget.h
    src/dialogs/WelcomeDialog.cpp
    src/dialogs/WelcomeDialog.h
    src/app/ClassicGames.cpp
    src/app/ClassicGames.h
    src/app/GameDatabase.h
    src/app/GameRecord.h
    src/app/Drawers.cpp
    src/app/Drawers.h
    src/app/PersonalSettings.cpp
    src/app/PersonalSettings.h
    src/app/Project.cpp
    src/app/Project.h
    src/app/WorkspaceLayout.h
    src/app/DatabaseMerge.cpp
    src/app/DatabaseMerge.h
    src/app/DatabaseMigrations.cpp
    src/app/DatabaseMigrations.h
    src/app/SqliteGameDatabase.cpp
    src/app/SqliteGameDatabase.h
    src/app/UserFolders.cpp
    src/app/UserFolders.h
    src/platform/WindowChrome.cpp
    src/platform/WindowChrome.h
    src/platform/Appearance.cpp
    src/platform/Appearance.h
    src/platform/FolderIcon.cpp
    src/platform/FolderIcon.h
    src/platform/MoveSound.cpp
    src/platform/MoveSound.h
    src/platform/DialogFrame.cpp
    src/platform/DialogFrame.h
    src/platform/GtkDesktopStyle.cpp
    src/platform/GtkDesktopStyle.h
    src/platform/MacDesktopStyle.cpp
    src/platform/MacDesktopStyle.h
    src/platform/SymbolicIcons.cpp
    src/platform/SymbolicIcons.h
    src/models/GameFilterProxyModel.cpp
    src/models/GameFilterProxyModel.h
    src/models/GameListModel.cpp
    src/models/GameListModel.h
    src/widgets/BoardPanel.cpp
    src/widgets/BoardPanel.h
    src/widgets/BookPanel.cpp
    src/widgets/BookPanel.h
    src/widgets/BookFont.cpp
    src/widgets/BookFont.h
    src/widgets/BoardWidget.cpp
    src/widgets/BoardSideColumn.cpp
    src/widgets/BoardSideColumn.h
    src/widgets/CapturedPiecesWidget.cpp
    src/widgets/CapturedPiecesWidget.h
    src/widgets/PaddedHeaderView.cpp
    src/widgets/PaddedHeaderView.h
    src/widgets/PaddedItemDelegate.cpp
    src/widgets/PaddedItemDelegate.h
    src/widgets/PaddedStatusBar.cpp
    src/widgets/PaddedStatusBar.h
    src/widgets/BoardTheme.cpp
    src/widgets/BoardTheme.h
    src/widgets/PieceRenderer.cpp
    src/widgets/PositionEditorWidget.cpp
    src/widgets/PositionEditorWidget.h
    src/widgets/PieceRenderer.h
    src/widgets/CentralArea.cpp
    src/widgets/DatabaseTreeWidget.cpp
    src/widgets/DatabaseTreeWidget.h
    src/widgets/CentralArea.h
    src/widgets/EnginePanel.cpp
    src/widgets/EnginePanel.h
    src/widgets/EvaluationBar.cpp
    src/widgets/EvaluationBar.h
    src/widgets/FigurineFont.cpp
    src/widgets/FigurineFont.h
    src/widgets/GameHeaderWidget.cpp
    src/widgets/GameHeaderWidget.h
    src/widgets/GlyphMenuAction.cpp
    src/widgets/GlyphMenuAction.h
    src/widgets/MoveTreeView.cpp
    src/widgets/MoveTreeView.h
    src/widgets/BoardWidget.h
)

target_include_directories(pragma-chess PRIVATE src)
if(PRAGMA_HAS_PHONE_LINK)
    target_sources(pragma-chess PRIVATE
        src/app/phone/DatabaseFolderStore.cpp
        src/app/phone/DatabaseFolderStore.h
        src/dialogs/ConnectMobileDialog.cpp
        src/dialogs/ConnectMobileDialog.h)
    target_link_libraries(pragma-chess PRIVATE pragma-phone-link qrcodegen)
endif()
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
    # A warning, not a status line: the build works and nothing else says why
    # the interface ignores the language chosen in Options.
    message(WARNING "Qt Linguist tools not found (qt6-tools-dev, qt6-l10n-tools): "
        "the interface is built in English only")
endif()
# Qt's own texts (dialog buttons, colour and file dialogs) in the same
# languages, embedded as well: what each platform deploys differs (Windows
# ships qt_<code>.qm, macOS nothing, Linux a package nobody has to install).
get_target_property(PRAGMA_QTPATHS Qt6::qtpaths LOCATION)
execute_process(COMMAND ${PRAGMA_QTPATHS} --query QT_INSTALL_TRANSLATIONS
    OUTPUT_VARIABLE PRAGMA_QT_TRANSLATIONS OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
set(PRAGMA_QT_QM_FILES)
foreach(ts IN LISTS PRAGMA_TS_FILES)
    string(REGEX REPLACE ".*pragma-chess_(.+)\\.ts$" "\\1" code ${ts})
    set(qm "${PRAGMA_QT_TRANSLATIONS}/qtbase_${code}.qm")
    if(EXISTS "${qm}")
        set_source_files_properties("${qm}" PROPERTIES QT_RESOURCE_ALIAS qtbase_${code}.qm)
        list(APPEND PRAGMA_QT_QM_FILES "${qm}")
    else()
        message(STATUS "qtbase_${code}.qm not found: Qt's own texts come from the system, if it has them")
    endif()
endforeach()
if(PRAGMA_QT_QM_FILES)
    qt_add_resources(pragma-chess qt-translations PREFIX /i18n FILES ${PRAGMA_QT_QM_FILES})
endif()

# The opening book seeded into the Books folder on first launch.
qt_add_resources(pragma-chess books PREFIX /books BASE resources/books FILES resources/books/pragma-openings.bin)
# The opening names databases (English, Italian) seeded into Books/Opening Names.
qt_add_resources(pragma-chess openings PREFIX /openings BASE resources/openings
    FILES resources/openings/opening-names.pdb resources/openings/lichess-openings.tsv
          resources/openings/opening-names-it.pdb resources/openings/lichess-openings-it.tsv)
# The guide (Help ▸ Pragma Chess Guide), one Markdown file per language.
qt_add_resources(pragma-chess classics PREFIX /classics BASE resources/classics
    FILES resources/classics/classic-games.pgn)
qt_add_resources(pragma-chess training PREFIX /training BASE resources/training
    FILES resources/training/endgames.tsv resources/training/tactics.tsv)
# The projects we distribute, seeded into the Projects folder (MainWindow::seedDistributedProjects).
file(GLOB DISTRIBUTED_PROJECTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/projects/*.pch)
qt_add_resources(pragma-chess projects PREFIX /projects BASE resources/projects
    FILES ${DISTRIBUTED_PROJECTS} resources/projects/former-versions.txt)
qt_add_resources(pragma-chess help PREFIX /help BASE resources/help
    FILES resources/help/guide_en.md resources/help/guide_it.md)
# Logos of the clubs that support Pragma Chess, shown in About.
qt_add_resources(pragma-chess credits PREFIX /credits BASE resources/credits
    FILES resources/credits/circolo-del-re.png)
# SkakNew figurines for the move list (scripts/make-figurine-font.py).
qt_add_resources(pragma-chess fonts PREFIX /fonts BASE resources/fonts
    FILES resources/fonts/pragma-figurine.otf resources/fonts/crimson-pro.ttf)
qt_add_resources(pragma-chess welcome PREFIX /welcome BASE resources/welcome
    FILES resources/welcome/chess-study.png)
qt_add_resources(pragma-chess appicon PREFIX /icons BASE data/icons FILES ${APP_ICON_FILES})
# The sound of a move (a CC0 wooden knock, resources/sounds/README.md), played by the system's own player.
qt_add_resources(pragma-chess sounds PREFIX / FILES resources/sounds/move.wav)
if(WIN32)
    target_sources(pragma-chess PRIVATE data/pragma-chess.rc)
    target_link_libraries(pragma-chess PRIVATE winmm shell32)
elseif(APPLE)
    target_link_libraries(pragma-chess PRIVATE "-framework AudioToolbox" "-framework CoreFoundation" objc)
    set(APP_ICNS data/icons/pragma-chess.icns)
    set_source_files_properties(${APP_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(pragma-chess PRIVATE ${APP_ICNS})
endif()

if(TARGET Qt6::Svg)
    target_link_libraries(pragma-chess PRIVATE Qt6::Svg)
    target_compile_definitions(pragma-chess PRIVATE PRAGMA_HAS_SVG)
    file(GLOB PIECE_SVGS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} resources/pieces/*/*.svg)
    qt_add_resources(pragma-chess pieces PREFIX / FILES ${PIECE_SVGS})
else()
    message(STATUS "Qt SVG not found: using font glyphs for chess pieces (install qt6-svg-dev)")
endif()
target_compile_definitions(pragma-chess PRIVATE
    APP_ID="${APP_ID}"
    QT_NO_KEYWORDS
)

target_compile_options(pragma-chess PRIVATE ${PRAGMA_WARNINGS})
# A crash prints its backtrace (main.cpp): with the app's own function names in Debug builds.
target_link_options(pragma-chess PRIVATE $<$<AND:$<PLATFORM_ID:Linux>,$<CONFIG:Debug>>:-rdynamic>)

set_target_properties(pragma-chess PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_GUI_IDENTIFIER "${APP_ID}"
    MACOSX_BUNDLE_BUNDLE_NAME "Pragma Chess"
    MACOSX_BUNDLE_ICON_FILE pragma-chess.icns
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright © Francesco Bianco and contributors"
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

if(PRAGMA_HAS_PHONE_LINK)
    # Plays the phone's side of Phone Link, or the computer's with --serve.
    qt_add_executable(pragma-phone tools/phone/main.cpp
        src/app/phone/DatabaseFolderStore.cpp
        src/app/phone/DatabaseFolderStore.h
        src/app/DatabaseMigrations.cpp
        src/app/DatabaseMigrations.h
        src/app/SqliteGameDatabase.cpp
        src/app/SqliteGameDatabase.h)
    target_link_libraries(pragma-phone PRIVATE pragma-phone-link ${PRAGMA_DATACHANNEL} Qt6::Sql)
    target_compile_options(pragma-phone PRIVATE ${PRAGMA_WARNINGS})
    set_target_properties(pragma-phone PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)
endif()

# Tested without a display.
if(TARGET Qt6::Test)
    qt_add_executable(tst_chessrules tests/tst_chessrules.cpp
        src/app/DatabaseMerge.cpp
        src/app/DatabaseMerge.h
        src/app/DatabaseMigrations.cpp
        src/app/DatabaseMigrations.h
        src/app/SqliteGameDatabase.cpp
        src/app/SqliteGameDatabase.h
        src/app/UserFolders.cpp
        src/app/UserFolders.h
        src/app/Drawers.cpp
        src/app/PersonalSettings.cpp
        src/app/PersonalSettings.h
        src/app/Project.cpp
        src/app/Project.h)
    target_link_libraries(tst_chessrules PRIVATE pragma-chess-core Qt6::Sql Qt6::Test yaml-cpp::yaml-cpp)
    target_compile_definitions(tst_chessrules PRIVATE
        PRAGMA_SMART_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../smart/tests"
        PRAGMA_HELP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/help"
        PRAGMA_CLASSICS_PGN="${CMAKE_CURRENT_SOURCE_DIR}/resources/classics/classic-games.pgn"
        PRAGMA_PROJECTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/projects")
    target_compile_options(tst_chessrules PRIVATE ${PRAGMA_WARNINGS})
    set_target_properties(tst_chessrules PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)
    add_test(NAME chessrules COMMAND tst_chessrules)
    if(PRAGMA_HAS_PHONE_LINK)
        qt_add_executable(tst_phonelink tests/tst_phonelink.cpp
            src/app/phone/DatabaseFolderStore.cpp
            src/app/phone/DatabaseFolderStore.h
            src/app/DatabaseMigrations.cpp
            src/app/DatabaseMigrations.h
            src/app/SqliteGameDatabase.cpp
            src/app/SqliteGameDatabase.h)
        target_link_libraries(tst_phonelink PRIVATE pragma-phone-link ${PRAGMA_DATACHANNEL} Qt6::Sql Qt6::Test)
        target_compile_definitions(tst_phonelink PRIVATE
            PHONE_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
        target_compile_options(tst_phonelink PRIVATE ${PRAGMA_WARNINGS})
        set_target_properties(tst_phonelink PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)
        add_test(NAME phonelink COMMAND tst_phonelink)
    endif()
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
    install(FILES data/${APP_ID}.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    # The bundled engine, when staged. A literal lib/, not CMAKE_INSTALL_LIBDIR
    # (lib64, lib/<multiarch>): EngineCatalog::bundledEngineDirs looks there.
    install(PROGRAMS ${PRAGMA_ENGINES_STAGE}/stockfish
        DESTINATION lib/pragma-chess/engines OPTIONAL)
    install(FILES ${PRAGMA_ENGINES_STAGE}/Copying.txt ${PRAGMA_ENGINES_STAGE}/AUTHORS
        ${PRAGMA_ENGINES_STAGE}/README.txt
        DESTINATION lib/pragma-chess/engines OPTIONAL)
endif()
