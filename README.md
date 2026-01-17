# MyAnimeList On Stream

Display your MyAnimeList manga collection as a scrolling browser source for OBS.

## Setup

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
   - Height: 300 (adjust as needed)

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

## Available Status Options

- `READING` - Currently reading manga
- `COMPLETED` - Completed manga
- `PAUSED` - On hold manga
- `DROPPED` - Dropped manga
- `PLANNING` - Plan to read manga
- `ALL` - Show all manga (default)

## Multiple Browser Sources

You can create multiple browser sources with different statuses:
- `http://localhost:3000/manga?status=READING`
- `http://localhost:3000/manga?status=COMPLETED`
- `http://localhost:3000/manga?status=DROPPED`

Or use one source to show everything:
- `http://localhost:3000/manga?status=ALL`
