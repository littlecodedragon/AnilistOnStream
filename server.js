if (typeof File === 'undefined' && typeof Blob !== 'undefined') {
  globalThis.File = class File extends Blob {
    constructor(parts, name, options = {}) {
      super(parts, options);
      this.name = name;
      this.lastModified = options.lastModified || Date.now();
    }
  };
}

const express = require('express');
const fetch = require('node-fetch');
const cheerio = require('cheerio');
const fs = require('fs');
const path = require('path');
const cors = require('cors');

const runtimeRoot = process.pkg ? path.dirname(process.execPath) : __dirname;
const userCwd = process.cwd();
const app = express();

let tray = null;
const useTray = process.env.NO_TRAY !== '1' && (process.platform === 'win32' || process.platform === 'darwin' || process.platform === 'linux');

if (useTray) {
  try {
    const SysTray = require('systray2').default;

    const restoreCwd = () => {
      try {
        process.chdir(userCwd);
      } catch (err) {
        console.warn('⚠️  Could not restore working directory:', err.message);
      }
    };

    try {
      if (process.cwd() !== runtimeRoot) {
        process.chdir(runtimeRoot);
      }
    } catch (err) {
      console.warn('⚠️  Could not change working directory for tray init:', err.message);
    }

    // Ensure tray binaries exist on real filesystem (pkg snapshot cannot be executed directly)
    try {
      const systrayRoot = path.dirname(require.resolve('systray2/package.json'));
      const traybinSource = path.join(systrayRoot, 'traybin');
      const traybinTarget = path.join(runtimeRoot, 'traybin');
      if (!fs.existsSync(traybinTarget)) {
        fs.mkdirSync(traybinTarget, { recursive: true });
      }
      const hasBins = fs.readdirSync(traybinTarget).some(name => name.startsWith('tray_'));
      if (!hasBins && fs.existsSync(traybinSource)) {
        fs.cpSync(traybinSource, traybinTarget, { recursive: true });
        console.log('Copied systray2 traybin into runtime directory');
      }
      const platformBin = process.platform === 'win32'
        ? 'tray_windows_release.exe'
        : process.platform === 'darwin'
          ? 'tray_darwin_release'
          : 'tray_linux_release';
      const platformBinPath = path.join(traybinTarget, platformBin);
      console.log('Tray binaries expected at:', traybinTarget, 'platform binary:', platformBinPath);
      if (!fs.existsSync(platformBinPath)) {
        console.warn('⚠️  Tray platform binary missing:', platformBinPath);
      }
      // Ensure the traybin folder is on PATH for any internal lookups
      process.env.PATH = `${traybinTarget}${path.delimiter}${process.env.PATH || ''}`;
    } catch (copyErr) {
      console.warn('⚠️  Could not prepare tray binaries:', copyErr.message);
    }

    const iconPng = path.join(runtimeRoot, 'icon.png');
    const iconIco = path.join(runtimeRoot, 'icon.ico');
    const iconPath = process.platform === 'win32' && fs.existsSync(iconIco)
      ? iconIco
      : (fs.existsSync(iconPng) ? iconPng : '');

    const itemOpen = {
      title: 'Open in browser',
      tooltip: 'Open the dashboard',
      enabled: true,
      click: () => {
        try {
          const openUrl = `http://localhost:${PORT || 3000}`;
          const { exec } = require('child_process');
          const cmd = process.platform === 'win32' ? `start ${openUrl}`
            : process.platform === 'darwin' ? `open ${openUrl}`
            : `xdg-open ${openUrl}`;
          exec(cmd);
        } catch (e) {
          console.warn('Tray open handler error:', e.message);
        }
      }
    };

    const itemQuit = {
      title: 'Quit',
      tooltip: 'Exit application',
      enabled: true,
      click: () => {
        try {
          console.log('Quitting via tray...');
          process.exit(0);
        } catch (e) {
          console.warn('Tray quit handler error:', e.message);
        }
      }
    };

    tray = new SysTray({
      menu: {
        icon: iconPath,
        isTemplateIcon: process.platform === 'darwin',
        title: 'MAL On Stream',
        tooltip: 'MyAnimeList On Stream',
        items: [itemOpen, itemQuit]
      },
      debug: false,
      copyDir: false
    });

    if (tray && typeof tray.onClick === 'function' && typeof tray.ready === 'function') {
      tray.onClick(action => {
        try {
          if (action.item && typeof action.item.click === 'function') {
            action.item.click();
          }
        } catch (e) {
          console.warn('Tray click handler error:', e.message);
        }
      });
      tray.ready().then(() => {
        console.log('✅ System tray enabled');
      }).catch(err => {
        console.warn('⚠️  System tray failed to start:', err.message);
      });

      if (tray._process && typeof tray._process.on === 'function') {
        tray._process.on('error', err => {
          console.warn('⚠️  Tray process error:', err.message);
        });
        tray._process.on('exit', (code, signal) => {
          if (code !== 0) {
            console.warn(`⚠️  Tray process exited with code ${code}${signal ? ` signal ${signal}` : ''}`);
          }
        });
      }
    } else {
      console.warn('⚠️  System tray did not initialize properly. Continuing without tray.');
    }

    restoreCwd();
  } catch (error) {
    console.warn('⚠️  System tray unavailable:', error.message);
  }
}

let config = { malUsername: 'YOUR_USERNAME', port: 3000, scrollSpeed: 60, debug: false };
try {
  const configPathCwd = path.join(process.cwd(), 'config.json');
  const configPathRoot = path.join(runtimeRoot, 'config.json');
  const configPath = fs.existsSync(configPathCwd) ? configPathCwd : configPathRoot;
  const configFile = fs.readFileSync(configPath, 'utf8');
  config = JSON.parse(configFile);
  if (!config.port) config.port = 3000;
  if (!config.scrollSpeed) config.scrollSpeed = 60;
  if (config.debug === undefined) config.debug = false;
  console.log(`Loaded config from ${configPath}`);
} catch (error) {
  console.warn('Warning: config.json not found. Using example config. Please create config.json from config.example.json');
}
const PORT = config.port || 3000;
app.use(cors());
const staticDir = fs.existsSync(path.join(process.cwd(), 'public'))
  ? path.join(process.cwd(), 'public')
  : path.join(runtimeRoot, 'public');
app.use(express.static(staticDir));

const mediaConfigs = {
  manga: {
    statusMap: { READING: '1', COMPLETED: '2', PAUSED: '3', DROPPED: '4', PLANNING: '6', ALL: '7' },
    statusNames: { '1': 'READING', '2': 'COMPLETED', '3': 'PAUSED', '4': 'DROPPED', '6': 'PLANNING' },
    url: (username, statusCode) => `https://myanimelist.net/mangalist/${username}?status=${statusCode}`,
    idKey: 'manga_id',
    titleKey: 'manga_title',
    imageKey: 'manga_image_path',
    progressKey: 'num_read_chapters',
    defaultStatuses: ['READING', 'COMPLETED', 'PAUSED', 'DROPPED', 'PLANNING']
  },
  anime: {
    statusMap: { WATCHING: '1', COMPLETED: '2', PAUSED: '3', DROPPED: '4', PLANNING: '6', ALL: '7' },
    statusNames: { '1': 'WATCHING', '2': 'COMPLETED', '3': 'PAUSED', '4': 'DROPPED', '6': 'PLANNING' },
    url: (username, statusCode) => `https://myanimelist.net/animelist/${username}?status=${statusCode}`,
    idKey: 'anime_id',
    titleKey: 'anime_title',
    imageKey: 'anime_image_path',
    progressKey: 'num_watched_episodes',
    defaultStatuses: ['WATCHING', 'COMPLETED', 'PAUSED', 'DROPPED', 'PLANNING']
  }
};

function normalizeImage(imagePath) {
  if (!imagePath) return 'https://cdn.myanimelist.net/images/qm_50.gif';
  let imageUrl = imagePath.replace('/r/96x136', '').replace('/r/50x70', '');
  if (imageUrl.startsWith('//')) return 'https:' + imageUrl;
  if (!imageUrl.startsWith('http')) return 'https://cdn.myanimelist.net' + imageUrl;
  return imageUrl;
}

async function scrapeList(username, status, media = 'manga') {
  const mediaConfig = mediaConfigs[media] || mediaConfigs.manga;
  const statusCode = mediaConfig.statusMap[status] || '7';
  const url = mediaConfig.url(username, statusCode);

  try {
    const response = await fetch(url, {
      headers: {
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
      }
    });

    if (!response.ok) {
      throw new Error(`Failed to fetch list: ${response.status}`);
    }

    const html = await response.text();
    const $ = cheerio.load(html);

    let dataScript = $('table.list-table').attr('data-items');
    if (!dataScript) {
      dataScript = $('table').attr('data-items');
    }
    if (!dataScript) {
      throw new Error('Could not find list data on the page. Make sure the username is correct, the list is public, and MAL layout has not changed.');
    }

    const parsed = JSON.parse(dataScript);
    const list = [];

    parsed.forEach(item => {
      const imageUrl = normalizeImage(item[mediaConfig.imageKey]);
      const entry = {
        id: item[mediaConfig.idKey],
        title: item[mediaConfig.titleKey],
        coverImage: imageUrl,
        status: mediaConfig.statusNames[item.status] || status || 'UNKNOWN',
        progress: item[mediaConfig.progressKey] || 0,
        media
      };
      list.push(entry);
    });

    return list;
  } catch (error) {
    console.error('Scraping error:', error);
    throw error;
  }
}

async function handleListRequest(req, res, mediaOverride) {
  const status = req.query.status || 'ALL';
  const speed = req.query.speed ? parseInt(req.query.speed, 10) : config.scrollSpeed;
  const mixed = req.query.mixed === 'true' || req.query.both === 'true';
  const media = mediaOverride || (req.query.media || 'manga').toLowerCase();
  const mediaTypes = mixed ? ['manga', 'anime'] : [media];

  if (!config.malUsername || config.malUsername === 'YOUR_USERNAME') {
    return res.status(400).json({
      error: 'MyAnimeList username not configured. Please set malUsername in config.json'
    });
  }

  try {
    let items = [];
    const seen = new Set();

    for (const mediaType of mediaTypes) {
      const mediaConfig = mediaConfigs[mediaType] || mediaConfigs.manga;
      
      if (status === 'ALL') {
        const statuses = mediaConfig.defaultStatuses;
        const results = await Promise.allSettled(statuses.map(s => scrapeList(config.malUsername, s, mediaType)));

        results.forEach((result, idx) => {
          if (result.status === 'fulfilled') {
            if (config.debug) {
              console.log(`Fetched ${result.value.length} ${mediaType} items for ${statuses[idx]}`);
            }
            result.value.forEach(entry => {
              const uniqueKey = `${entry.media}-${entry.id}`;
              if (!seen.has(uniqueKey)) {
                seen.add(uniqueKey);
                items.push(entry);
              }
            });
          } else {
            console.error(`Error fetching ${statuses[idx]} (${mediaType}):`, result.reason?.message || result.reason);
          }
        });
      } else {
        const list = await scrapeList(config.malUsername, status, mediaType);
        list.forEach(entry => {
          const uniqueKey = `${entry.media}-${entry.id}`;
          if (!seen.has(uniqueKey)) {
            seen.add(uniqueKey);
            items.push(entry);
          }
        });
      }
    }

    const sortBy = req.query.sort || 'default';
    if (sortBy === 'title') {
      items.sort((a, b) => a.title.localeCompare(b.title));
    } else if (sortBy === 'status') {
      const statusOrder = { READING: 1, WATCHING: 1, COMPLETED: 2, PAUSED: 3, DROPPED: 4, PLANNING: 5 };
      items.sort((a, b) => (statusOrder[a.status] || 9) - (statusOrder[b.status] || 9));
    } else if (sortBy === 'progress') {
      items.sort((a, b) => (b.progress || 0) - (a.progress || 0));
    } else if (sortBy === 'random') {
      items.sort(() => Math.random() - 0.5);
    }

    res.json({ items, username: config.malUsername, scrollSpeed: speed, media: mixed ? 'mixed' : media });
  } catch (error) {
    console.error('Error fetching list:', error);
    res.status(500).json({ error: 'Failed to fetch list: ' + error.message });
  }
}

app.get('/api/list', (req, res) => handleListRequest(req, res));
app.get('/api/manga', (req, res) => handleListRequest(req, res, 'manga'));
app.get('/api/anime', (req, res) => handleListRequest(req, res, 'anime'));

app.get('/manga', (req, res) => {
  res.sendFile(path.join(staticDir, 'index.html'));
});

app.get('/anime', (req, res) => {
  res.sendFile(path.join(staticDir, 'index.html'));
});

app.get('/', (req, res) => {
  res.send(`
    <html>
      <head><title>MyAnimeList On Stream</title></head>
      <body style="font-family: Arial; padding: 20px;">
        <h1>MyAnimeList On Stream</h1>
        <p>Server is running! Add a browser source in OBS with one of these URLs:</p>
        <ul>
          <li><a href="/manga?status=READING">Currently Reading (Manga)</a> - <code>http://localhost:${PORT}/manga?status=READING</code></li>
          <li><a href="/manga?status=COMPLETED">Completed (Manga)</a> - <code>http://localhost:${PORT}/manga?status=COMPLETED</code></li>
          <li><a href="/manga?status=ALL">All Manga</a> - <code>http://localhost:${PORT}/manga?status=ALL</code></li>
          <li><a href="/anime?status=WATCHING&media=anime">Watching (Anime)</a> - <code>http://localhost:${PORT}/anime?status=WATCHING&media=anime</code></li>
          <li><a href="/anime?status=ALL&media=anime">All Anime</a> - <code>http://localhost:${PORT}/anime?status=ALL&media=anime</code></li>
        </ul>
        <p>Username: <strong>${config.malUsername}</strong></p>
        <p style="color: #666; margin-top: 30px;">Configure your username in config.json</p>
        <p style="color: #999; font-size: 12px;"><strong>Note:</strong> Your MyAnimeList list must be set to public for scraping to work.</p>
      </body>
    </html>
  `);
});

app.listen(PORT, () => {
  console.log(`🚀 MyAnimeList On Stream server running on http://localhost:${PORT}`);
  console.log(`📚 Username: ${config.malUsername}`);
  console.log(`\nAdd browser source in OBS: http://localhost:${PORT}/manga?status=READING`);
  console.log(`\n⚠️  Make sure your list is set to PUBLIC on MyAnimeList!`);
  if (config.debug) {
    console.log('Debug logging is ENABLED');
  }
});
