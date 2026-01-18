# MyAnimeList On Stream

Display your MyAnimeList manga or anime collection as a scrolling browser source for OBS or StreamElements.

## Quick Start (Recommended)

**Download pre-built binaries from [Releases](../../releases) - no Node.js installation needed!**

1. **Make sure your MyAnimeList manga list is PUBLIC**
   - Go to https://myanimelist.net/editprofile.php?go=listpreferences
   - Under "Manga List" → "Default Privacy" select "Public"

2. **Download the latest release** for your OS:
   - Windows: `myanimelist-on-stream.exe`
   - Linux: `myanimelist-on-stream`

3. **Create a `config.json` file** in the same folder as the binary:
```json
{
  "malUsername": "YOUR_MAL_USERNAME",
  "port": 3000,
  "scrollSpeed": 60,
  "debug": false
}
```

Replace `YOUR_MAL_USERNAME` with your MyAnimeList username.

4. **Run the binary:**
   - Windows: Double-click `myanimelist-on-stream.exe` (runs in system tray)
   - Linux: `chmod +x myanimelist-on-stream && ./myanimelist-on-stream` (runs in system tray)
   - Right-click the tray icon to open dashboard or quit
   - To run without tray (console mode): set environment variable `NO_TRAY=1`

5. **Add browser source in OBS:**
   - URL: `http://localhost:3000/manga?status=READING&speed=60`
   - Width: 1920
   - Height: 600 (adjust as needed)

### Auto-start on boot (optional but recommended!)

**Windows:**
1. Press `Win+R`, type `shell:startup`, press Enter
2. Create a shortcut to `myanimelist-on-stream.exe` and `config.json` in the Startup folder
3. The server will start automatically when Windows boots

**Linux (systemd):**
1. Create `/etc/systemd/system/myanimelist-on-stream.service`:
```ini
[Unit]
Description=MyAnimeList On Stream
After=network.target

[Service]
Type=simple
User=YOUR_USERNAME
WorkingDirectory=/path/to/folder/with/binary/and/config
ExecStart=/path/to/folder/myanimelist-on-stream
Restart=on-failure

[Install]
WantedBy=multi-user.target
```
2. Enable and start: `sudo systemctl enable myanimelist-on-stream && sudo systemctl start myanimelist-on-stream`

---

## Setup from Source (for developers)

1. **Make sure your MyAnimeList manga list is PUBLIC**
   - Go to https://myanimelist.net/editprofile.php?go=listpreferences
   - Under "Manga List" → "Default Privacy" select "Public"

2. **Install dependencies:**
```bash
npm install
```

3. **Create a `config.json` file** in the root directory:
```json
{
  "malUsername": "YOUR_MAL_USERNAME",
   "port": 3000,
   "scrollSpeed": 60,
   "debug": false
}
```

Replace:
- `YOUR_MAL_USERNAME` with your MyAnimeList username
- `scrollSpeed`: Duration in seconds for one full scroll cycle (default: 60)
- `debug`: Set to true to enable verbose logging on the server

4. **Start the server:**
```bash
npm start
```

5. **Add browser source in OBS:**
   - URL: `http://localhost:3000/manga?status=READING&speed=60`
   - Width: 1920
   - Height: 600 (adjust as needed)

6. **Optional: build single-file binaries locally:**
```bash
npm install --include=dev
npm run build:win   # outputs dist/myanimelist-on-stream.exe + public/ + config.example.json
npm run build:linux # outputs dist/myanimelist-on-stream + public/ + config.example.json
```
After building, copy your `config.json` to `dist/` and run the binary from that folder.

## Customization

### Scroll Speed

Control how fast the manga scrolls:

1. **Default speed** - Set in `config.json`:
   ```json
   "scrollSpeed": 60
   ```
   (60 = one full cycle in 60 seconds)

2. **Override per browser source** - Add `speed` parameter to URL:
   - Slow: `http://localhost:3000/manga?status=READING&speed=120`
   - Fast: `http://localhost:3000/manga?status=READING&speed=30`
   - Very fast: `http://localhost:3000/manga?status=READING&speed=15`

### Sorting

Sort items by adding `sort` parameter:
- `sort=title` - Alphabetical by title
- `sort=status` - Group by status (reading/watching first, then completed, etc.)
- `sort=progress` - Sort by progress (highest first)
- `sort=random` - Randomize order
- Default: as returned by MyAnimeList

Example: `http://localhost:3000/manga?status=ALL&sort=title`

## Available Status Options

- `READING` - Currently reading manga
- `COMPLETED` - Completed manga
- `PAUSED` - On hold manga
- `DROPPED` - Dropped manga
- `PLANNING` - Plan to read manga
- `ALL` - Show all manga (default)

For anime use the same endpoint with `media=anime` and statuses: `WATCHING`, `COMPLETED`, `PAUSED`, `DROPPED`, `PLANNING`, `ALL`.

## Multiple Browser Sources

You can create multiple browser sources with different statuses:
- `http://localhost:3000/manga?status=READING`
- `http://localhost:3000/manga?status=COMPLETED`
- `http://localhost:3000/manga?status=DROPPED`

Or use one source to show everything:
- `http://localhost:3000/manga?status=ALL`

Anime examples:
- `http://localhost:3000/anime?status=WATCHING`
- `http://localhost:3000/anime?status=ALL`

**Mix manga and anime together:**
- `http://localhost:3000/manga?status=ALL&mixed=true` - All anime + manga
- `http://localhost:3000/manga?status=COMPLETED&mixed=true` - Completed anime + manga
- `http://localhost:3000/anime?status=READING&mixed=true` - Reading manga + watching anime (use READING for manga status, it auto-converts)

Note: `/anime` route auto-detects anime (no need for `&media=anime`). Use `&mixed=true` on either route to combine both.

### Background / border theming

You can style the widget via query params (works in OBS and StreamElements):

- `bg`: CSS color or gradient. Example: `bg=linear-gradient(90deg,%2308162f,%23132f54)`
- `bgImage`: URL to an image for a banner. Example: `bgImage=https://example.com/banner.png`
- `padding` (or `pad`): e.g. `padding=20px`
- `radius`: e.g. `radius=16px`
- `borderColor`, `borderWidth`: e.g. `borderColor=%23ffffff80&borderWidth=4px`
- `shadow`: e.g. `shadow=0 10px 40px rgba(0,0,0,0.45)`
- `gap`: spacing between items, e.g. `gap=40px`

Example themed URL:
```
http://localhost:3000/manga?status=READING&bg=linear-gradient(90deg,%2308162f,%23132f54)&padding=16px&radius=18px&borderColor=%23ffffff33&borderWidth=3px&gap=40px
```
