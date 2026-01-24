cmake_minimum_required(VERSION 3.16)

project(obs-mal-scroll VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer libobs CONFIG package if available (Linux with libobs-dev)
find_package(libobs CONFIG QUIET)
if(NOT libobs_FOUND)
    # Fallback to our module finder
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
    find_package(libobs REQUIRED)
endif()
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_library(obs-mal-scroll MODULE
    src/plugin-main.cpp
    src/mal-source.cpp
    src/mal-fetcher.cpp
)

target_link_libraries(obs-mal-scroll
    OBS::libobs
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

target_include_directories(obs-mal-scroll PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Propagate libobs include directories explicitly (configs on some distros lack them)
set(_obs_inc "")
if(TARGET OBS::libobs)
    get_target_property(_obs_inc OBS::libobs INTERFACE_INCLUDE_DIRECTORIES)
    if(_obs_inc)
        target_include_directories(obs-mal-scroll PRIVATE ${_obs_inc})
    endif()
endif()

# Fallback for environments where libobs config omits include dirs
if(NOT _obs_inc)
    find_path(LIBOBS_FALLBACK_INCLUDE
        NAMES obs-module.h
        PATHS /usr/include/obs /usr/local/include/obs /usr/include /usr/local/include
    )
    if(LIBOBS_FALLBACK_INCLUDE)
        target_include_directories(obs-mal-scroll PRIVATE ${LIBOBS_FALLBACK_INCLUDE})
    endif()
endif()

# Install the plugin
option(INSTALL_FLATPAK "Install to Flatpak OBS location" OFF)

if(UNIX AND NOT APPLE)
    if(INSTALL_FLATPAK)
        set(PLUGIN_INSTALL_DIR "$ENV{HOME}/.var/app/com.obsproject.Studio/config/obs-studio/plugins/${PROJECT_NAME}")
    else()
        set(PLUGIN_INSTALL_DIR "$ENV{HOME}/.config/obs-studio/plugins/${PROJECT_NAME}")
    endif()
    install(TARGETS obs-mal-scroll
        LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}/bin/64bit"
    )
endif()

if(WIN32)
    install(TARGETS obs-mal-scroll
        LIBRARY DESTINATION "obs-plugins/64bit"
    )
endif()
