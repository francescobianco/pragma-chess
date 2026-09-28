# CPack settings for the Linux packages, included by the top-level CMakeLists.
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
#   cmake --build build
#   cd build && cpack -G DEB     # or -G RPM
#
# Qt comes from the distribution: the package depends on its Qt libraries
# (found by dpkg-shlibdeps / rpm's autoreq) instead of shipping its own.
# packaging/linux/build-packages.sh does all of this.

set(CPACK_PACKAGE_NAME "pragma-chess")
set(CPACK_PACKAGE_VENDOR "Francesco Bianco")
set(CPACK_PACKAGE_CONTACT "Francesco Bianco <info.francescobianco@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Chess database with a native desktop client")
set(CPACK_PACKAGE_DESCRIPTION
    "Pragma Chess keeps chess games in databases, replays them on the board, \
analyzes them with any UCI engine and explains the evaluation of a move with \
arrows. Games can be imported from lichess.org and chess.com, and databases \
kept in sync across computers.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/francescobianco/pragma-chess")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_STRIP_FILES ON)
set(CPACK_PACKAGE_FILE_NAME "pragma-chess-${PROJECT_VERSION}-linux")

# Debian / Ubuntu
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Plugins are loaded at run time, so dpkg-shlibdeps cannot see them.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6sql6-sqlite")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "stockfish, git, qt6-wayland")

# Fedora / openSUSE
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_SUGGESTS "stockfish, git")
# Directories owned by the filesystem and the icon theme.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/share/applications
    /usr/share/metainfo
    /usr/share/icons
    /usr/share/icons/hicolor)
foreach(size 16 22 24 32 48 64 128 256)
    list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        /usr/share/icons/hicolor/${size}x${size}
        /usr/share/icons/hicolor/${size}x${size}/apps)
endforeach()

include(CPack)
