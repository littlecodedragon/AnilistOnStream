cmake_minimum_required(VERSION 3.16)

project(obs-mal-scroll VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libobs REQUIRED)
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
        RUNTIME DESTINATION "obs-plugins/64bit"
    )
endif()
