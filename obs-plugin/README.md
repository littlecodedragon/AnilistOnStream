# OBS MAL Scroll Plugin

A native OBS Studio plugin that displays your MyAnimeList manga/anime collection as a scrolling gallery with titles and status badges.

## Features

- ✨ Native rendering with bitmap fonts (no browser source)
- 📚 Supports Manga, Anime, or Both
- 🎨 Color-coded status badges (Reading/Watching, Completed, On Hold, Dropped, Planning)
- 🔄 Auto-refresh from MyAnimeList
- ⚙️ Customizable scroll speed, item width, text colors, and spacing

## Installation

### Windows

1. Download the latest release
2. Extract `obs-mal-scroll.dll` to one of:
   - `%APPDATA%\obs-studio\plugins\obs-mal-scroll\bin\64bit\` (User install - Recommended)
   - `C:\Program Files\obs-studio\obs-plugins\64bit\` (System-wide install)
3. Restart OBS Studio

### Linux (Manual Build)

```bash
cd obs-plugin
mkdir build && cd build
cmake ..
make -j$(nproc)
make install  # Installs to ~/.config/obs-studio/plugins/obs-mal-scroll/
```

### Linux (Flatpak OBS)

```bash
cd obs-plugin
mkdir flatpak-build && cd flatpak-build
cmake -DCMAKE_INSTALL_PREFIX=$HOME/.var/app/com.obsproject.Studio/config/obs-studio ..
make -j$(nproc)
make install
```

## Usage in OBS

1. Add a new Source → **MyAnimeList Scroll**
2. Configure your MAL username
3. Select Media Type (Manga/Anime/Both)
4. Choose Status Filter (or "ALL" to show all statuses)
5. Adjust scroll speed, item width, text scale, and colors

## Status Colors

- **Green**: Reading/Watching
- **Blue**: Completed
- **Amber**: On Hold (Paused)
- **Red**: Dropped
- **Gray**: Planning

## Building from Source

### Requirements

- OBS Studio (with development headers)
- libcurl
- nlohmann-json (header-only, included)
- CMake 3.16+
- C++17 compiler

### Windows Build

1. Install OBS Studio
2. Install Visual Studio 2019+ with C++ tools
3. Install vcpkg and install dependencies:
   ```powershell
   vcpkg install curl:x64-windows
   ```
4. Build:
   ```powershell
   cd obs-plugin
   mkdir build && cd build
   cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake ..
   cmake --build . --config Release
   ```

## Architecture

- `mal-source.cpp/hpp`: Main OBS source with native rendering
- `mal-fetcher.cpp/hpp`: MAL web scraping (parses data-items JSON)
- `font5x7.hpp`: 5x7 bitmap font for text rendering
- Native graphics using libobs GS API
