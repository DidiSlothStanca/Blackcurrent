cmake_minimum_required(VERSION 3.16)
project(BlackcurrantMusic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia)

set(SOURCES
    src/main.cpp
    src/i18n.cpp
    src/theme.cpp
    src/icons.cpp
    src/storage.cpp
    src/bridge.cpp
    src/player.cpp
    src/thumbnails.cpp
    src/widgets.cpp
    src/mainwindow.cpp
    src/views/homeview.cpp
    src/views/searchview.cpp
    src/views/libraryview.cpp
    src/views/detailview.cpp
    src/views/artistview.cpp
    src/views/lyricsview.cpp
    src/views/aboutview.cpp
    resources/resources.qrc
)

set(HEADERS
    src/track.h
    src/version.h
    src/i18n.h
    src/theme.h
    src/icons.h
    src/storage.h
    src/bridge.h
    src/player.h
    src/thumbnails.h
    src/widgets.h
    src/mainwindow.h
    src/flowlayout.h
    src/views/viewutil.h
    src/views/homeview.h
    src/views/searchview.h
    src/views/libraryview.h
    src/views/detailview.h
    src/views/artistview.h
    src/views/lyricsview.h
    src/views/aboutview.h
)

add_executable(blackcurrant ${SOURCES} ${HEADERS})

target_compile_definitions(blackcurrant PRIVATE
    BC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(blackcurrant PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(blackcurrant PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

# Salin jembatan Python ke folder build agar dapat ditemukan saat dijalankan.
add_custom_command(TARGET blackcurrant POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/bridge
            $<TARGET_FILE_DIR:blackcurrant>/bridge
    COMMENT "Menyalin jembatan Python ke folder build")

install(TARGETS blackcurrant RUNTIME DESTINATION bin)
install(DIRECTORY bridge DESTINATION share/blackcurrant)

# Ikon & entri menu aplikasi (Linux desktop)
install(FILES resources/blackcurrant.png
        DESTINATION share/icons/hicolor/512x512/apps)
install(FILES resources/blackcurrant.desktop
        DESTINATION share/applications)
