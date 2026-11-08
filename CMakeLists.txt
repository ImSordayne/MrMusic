cmake_minimum_required(VERSION 3.21)

project(MrMusic
    VERSION 0.1.0
    DESCRIPTION "MrMusic - cross platform music player (NetEase Cloud Music + KuGou)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# NOTE: CMAKE_INCLUDE_CURRENT_DIR is intentionally OFF. Including the repo root
# would shadow the C++20 standard header <version> with our VERSION file on
# case-insensitive filesystems (Windows/macOS), where Qt's headers include
# <version>. All project headers are included as "api/...", "core/...", etc.,
# resolved through target_include_directories(.../src) below.

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network
    Multimedia Svg Concurrent)
find_package(Qt6 QUIET COMPONENTS DBus LinguistTools)

qt_standard_project_setup(REQUIRES 6.5)

# ---------------------------------------------------------------------------
# Core library: everything that is not QML glue.
# ---------------------------------------------------------------------------

set(MRMUSIC_CORE_SOURCES
    src/account/AccountStore.cpp
    src/account/AccountStore.h

    src/api/ApiTypes.cpp
    src/api/ApiTypes.h
    src/api/HttpClient.cpp
    src/api/HttpClient.h
    src/api/KugouClient.cpp
    src/api/KugouClient.h
    src/api/NeteaseClient.cpp
    src/api/NeteaseClient.h

    src/core/AppPaths.cpp
    src/core/AppPaths.h
    src/core/Crypto.cpp
    src/core/Crypto.h
    src/core/Fingerprint.cpp
    src/core/Fingerprint.h
    src/core/Inflate.cpp
    src/core/Inflate.h
    src/core/JsonStore.cpp
    src/core/JsonStore.h
    src/core/Logger.cpp
    src/core/Logger.h
    src/core/SaltStore.cpp
    src/core/SaltStore.h
    src/core/Settings.cpp
    src/core/Settings.h
    src/core/SingleInstance.cpp
    src/core/SingleInstance.h

    src/download/DownloadManager.cpp
    src/download/DownloadManager.h
    src/download/TagWriter.cpp
    src/download/TagWriter.h

    src/player/AudioEngine.cpp
    src/player/AudioEngine.h
    src/player/MediaControls.cpp
    src/player/MediaControls.h
    src/player/PlayerController.cpp
    src/player/PlayerController.h

    src/ui/AccountController.cpp
    src/ui/AccountController.h
    src/ui/AppController.cpp
    src/ui/BrowseController.cpp
    src/ui/BrowseController.h
    src/ui/CatalogModel.cpp
    src/ui/CatalogModel.h
    src/ui/AppController.h
    src/ui/LibraryController.cpp
    src/ui/LibraryController.h
    src/ui/QrCode.cpp
    src/ui/QrCode.h
    src/ui/TrackListModel.cpp
    src/ui/TrackListModel.h

    third_party/qrcodegen/qrcodegen.cpp
)

add_library(mrmusic_core STATIC ${MRMUSIC_CORE_SOURCES})

target_include_directories(mrmusic_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qrcodegen
)

target_link_libraries(mrmusic_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Svg
)

target_compile_definitions(mrmusic_core PUBLIC
    MRMUSIC_VERSION="${PROJECT_VERSION}"
)

option(MRMUSIC_NO_WINRT "Disable Windows SMTC (C++/WinRT) integration" OFF)

# MPRIS is a freedesktop/Linux D-Bus spec. Qt ships a Qt6::DBus target on
# Windows and macOS too, but using MprisMediaControls there is wrong (and on
# macOS it produced an undefined symbol for that .cpp-only skeleton class).
# Only enable the DBus/MPRIS backend on Linux; other platforms fall back to
# their native (or Null) implementation in MediaControls::create().
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET Qt6::DBus)
    target_link_libraries(mrmusic_core PUBLIC Qt6::DBus)
    target_compile_definitions(mrmusic_core PRIVATE MRMUSIC_HAVE_DBUS)
endif()

if(WIN32)
    target_compile_definitions(mrmusic_core PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

    # C++/WinRT projections from the Windows SDK (SystemMediaTransportControls).
    set(_sdk_roots
        "$ENV{WindowsSdkDir}"
        "C:/Program Files (x86)/Windows Kits/10"
        "D:/Windows Kits/10"
    )
    set(WINSDK_CPPWINRT "")
    foreach(_root IN LISTS _sdk_roots)
        if(_root AND EXISTS "${_root}/Include")
            file(GLOB _versions RELATIVE "${_root}/Include" "${_root}/Include/*")
            list(SORT _versions)
            list(REVERSE _versions)
            foreach(_version IN LISTS _versions)
                if(EXISTS "${_root}/Include/${_version}/cppwinrt/winrt/base.h")
                    set(WINSDK_CPPWINRT "${_root}/Include/${_version}/cppwinrt")
                    break()
                endif()
            endforeach()
        endif()
        if(WINSDK_CPPWINRT)
            break()
        endif()
    endforeach()

    if(WINSDK_CPPWINRT AND NOT MRMUSIC_NO_WINRT)
        message(STATUS "MrMusic: C++/WinRT from ${WINSDK_CPPWINRT}")
        target_include_directories(mrmusic_core PRIVATE "${WINSDK_CPPWINRT}")
        target_link_libraries(mrmusic_core PUBLIC windowsapp)
    else()
        message(WARNING "MrMusic: C++/WinRT headers not found, native media controls disabled")
        target_compile_definitions(mrmusic_core PRIVATE MRMUSIC_NO_WINRT)
    endif()
endif()

# ---------------------------------------------------------------------------
# Application (QML module)
# ---------------------------------------------------------------------------

set(MRMUSIC_QML_FILES
    qml/Main.qml
    qml/Theme.qml

    qml/components/AvatarBubble.qml
    qml/components/BottomNav.qml
    qml/components/DownloadsPanel.qml
    qml/components/EmptyState.qml
    qml/components/CatalogRow.qml
    qml/components/FilterChip.qml
    qml/components/FlatButton.qml
    qml/components/IconButton.qml
    qml/components/LoginDialog.qml
    qml/components/NavRail.qml
    qml/components/PlayerBar.qml
    qml/components/QueuePanel.qml
    qml/components/TintedIcon.qml
    qml/components/TopBar.qml
    qml/components/TrackListView.qml
    qml/components/TrackRow.qml

    qml/pages/FavoritesPage.qml
    qml/pages/HistoryPage.qml
    qml/pages/AlbumPage.qml
    qml/pages/ArtistPage.qml
    qml/pages/HomePage.qml
    qml/pages/SearchPage.qml
    qml/pages/SettingsPage.qml
)

qt_policy(SET QTP0002 NEW)
qt_policy(SET QTP0004 NEW)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(MrMusic WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/ui/IconProvider.cpp
    src/ui/IconProvider.h
)

qt_add_qml_module(MrMusic
    URI MrMusic
    VERSION 1.0
    QML_FILES ${MRMUSIC_QML_FILES}
    RESOURCES res/res.qrc
)

target_link_libraries(MrMusic PRIVATE
    mrmusic_core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
)

# qt_add_qml_module() unconditionally adds the target's source dir (the repo
# root) to this executable's INCLUDE_DIRECTORIES (QTBUG-93443). On
# case-insensitive filesystems (Windows/macOS) that would shadow the C++20
# standard header <version> with our VERSION file, making Qt headers fail with
# "expected unqualified-id" / C2059. Our includes are all "api/..."/"core/..."
# resolved through mrmusic_core's /src include dir, so the repo root is not
# needed here. Remove just that one entry.
get_target_property(_mr_includes MrMusic INCLUDE_DIRECTORIES)
if(_mr_includes)
    list(REMOVE_ITEM _mr_includes "${CMAKE_CURRENT_SOURCE_DIR}")
    set_property(TARGET MrMusic PROPERTY INCLUDE_DIRECTORIES "${_mr_includes}")
endif()

# Qt places an executable QML module's tooling output (qmldir/qmltypes/dir-map)
# in <build>/MrMusic/. On Linux the executable has no suffix and is also named
# "MrMusic", so the linker fails with
#   "cannot open output file MrMusic: Is a directory".
# Put the built executable in <build>/bin/ on Linux only; install(TARGETS)
# still installs it as bin/MrMusic, so every packaging script is unchanged.
# Windows (.exe) and macOS (.app) have distinct paths and are unaffected.
if(UNIX AND NOT APPLE AND NOT ANDROID)
    set_target_properties(MrMusic PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

if(ANDROID)
    set_target_properties(MrMusic PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "io.sordayne.github.mrmusic"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE "1"
        QT_ANDROID_MIN_SDK_VERSION 29
        QT_ANDROID_TARGET_SDK_VERSION 34
    )
endif()

set_target_properties(MrMusic PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "MrMusic"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.sordayne.github.mrmusic"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

# ---------------------------------------------------------------------------
# Translations: translations/mrmusic_en_US.ts is compiled to .qm by
# scripts/update-translations.ps1 and embedded through res/res.qrc.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Deployment / install
# ---------------------------------------------------------------------------

if(ANDROID)
    install(TARGETS MrMusic LIBRARY DESTINATION lib)
else()
    install(TARGETS MrMusic
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION bin
    )
endif()

qt_generate_deploy_qml_app_script(
    TARGET MrMusic
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# ---------------------------------------------------------------------------
# Tests / developer tools
# ---------------------------------------------------------------------------

enable_testing()

add_executable(mrmusic_selftest tests/selftest.cpp)
target_link_libraries(mrmusic_selftest PRIVATE mrmusic_core)
add_test(NAME mrmusic_selftest COMMAND mrmusic_selftest)

option(MRMUSIC_BUILD_PROBES "Build the live API probe tools" OFF)

if(MRMUSIC_BUILD_PROBES)
    qt_add_executable(mrmusic_catalogprobe tests/catalog_probe.cpp)
    target_link_libraries(mrmusic_catalogprobe PRIVATE mrmusic_core)
    add_executable(mrmusic_apiprobe tests/api_probe.cpp)
    target_link_libraries(mrmusic_apiprobe PRIVATE mrmusic_core)

    add_executable(mrmusic_kugouurl tests/kugou_url_probe.cpp)
    target_link_libraries(mrmusic_kugouurl PRIVATE mrmusic_core)
endif()
