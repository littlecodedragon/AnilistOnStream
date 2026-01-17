const express = require('express');
const fetch = require('node-fetch');
const cheerio = require('cheerio');
const fs = require('fs');
const path = require('path');
const app = express();

let config = { malUsername: 'YOUR_USERNAME', port: 3000, scrollSpeed: 60, debug: false };
try {
  const configFile = fs.readFileSync(path.join(__dirname, 'config.json'), 'utf8');
  config = JSON.parse(configFile);
  if (!config.port) config.port = 3000;
  if (!config.scrollSpeed) config.scrollSpeed = 60;
  if (config.debug === undefined) config.debug = false;
} catch (error) {
  console.warn('Warning: config.json not found. Using example config. Please create config.json from config.example.json');
}
const PORT = config.port || 3000;
app.use(express.static('public'));
const statusMap = {
  'READING': '1',
  'COMPLETED': '2',
  'PAUSED': '3',
  'DROPPED': '4',
  'PLANNING': '6',
  'ALL': '7'
};

const statusNames = {
  '1': 'READING',
  '2': 'COMPLETED',
  '3': 'PAUSED',
  '4': 'DROPPED',
  '6': 'PLANNING'
};
async function scrapeMangaList(username, status) {
  const malStatus = statusMap[status] || '7';
  const url = `https://myanimelist.net/mangalist/${username}?status=${malStatus}`;
  
  try {
    const response = await fetch(url, {
      headers: {
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
      }
    });

    if (!response.ok) {
      throw new Error(`Failed to fetch manga list: ${response.status}`);
    }

    const html = await response.text();
    const $ = cheerio.load(html);

    let dataScript = $('table.list-table').attr('data-items');
    if (!dataScript) {
      dataScript = $('table').attr('data-items');
    }
    if (!dataScript) {
      throw new Error('Could not find manga data on the page. Make sure the username is correct, the list is public, and MAL layout has not changed.');
    }

    const mangaData = JSON.parse(dataScript);
    const mangaList = [];

    mangaData.forEach(item => {
      let imageUrl = item.manga_image_path;
      if (imageUrl) {
        imageUrl = imageUrl.replace('/r/96x136', '').replace('/r/50x70', '');
        if (imageUrl.startsWith('//')) {
          imageUrl = 'https:' + imageUrl;
        } else if (!imageUrl.startsWith('http')) {
          imageUrl = 'https://cdn.myanimelist.net' + imageUrl;
        }
      }

      const entry = {
        id: item.manga_id,
        title: item.manga_title,
        coverImage: imageUrl || 'https://cdn.myanimelist.net/images/qm_50.gif',
        status: statusNames[item.status] || status || 'UNKNOWN',
        progress: item.num_read_chapters || 0
      };
      mangaList.push(entry);
    });

    return mangaList;
  } catch (error) {
    console.error('Scraping error:', error);
    throw error;
  }
}
app.get('/api/manga', async (req, res) => {
  const status = req.query.status || 'ALL';
  const speed = req.query.speed ? parseInt(req.query.speed) : config.scrollSpeed;

  if (!config.malUsername || config.malUsername === 'YOUR_USERNAME') {
    return res.status(400).json({ 
      error: 'MyAnimeList username not configured. Please set malUsername in config.json' 
    });
  }
  
  try {
    let mangaList = [];

    if (status === 'ALL') {
      const statuses = ['READING', 'COMPLETED', 'PAUSED', 'DROPPED', 'PLANNING'];
      const results = await Promise.allSettled(statuses.map(s => scrapeMangaList(config.malUsername, s)));

      const seen = new Set();
      results.forEach((res, idx) => {
        if (res.status === 'fulfilled') {
          if (config.debug) {
            console.log(`Fetched ${res.value.length} items for ${statuses[idx]}`);
          }
          res.value.forEach(item => {
            if (!seen.has(item.id)) {
              seen.add(item.id);
              mangaList.push(item);
            }
          });
        } else {
          console.error(`Error fetching ${statuses[idx]}:`, res.reason?.message || res.reason);
        }
      });
    } else {
      mangaList = await scrapeMangaList(config.malUsername, status);
    }

    res.json({ manga: mangaList, username: config.malUsername, scrollSpeed: speed });
  } catch (error) {
    console.error('Error fetching manga list:', error);
    res.status(500).json({ error: 'Failed to fetch manga list: ' + error.message });
  }
});
app.get('/manga', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.get('/', (req, res) => {
  res.send(`
    <html>
      <head><title>MyAnimeList On Stream</title></head>
      <body style="font-family: Arial; padding: 20px;">
        <h1>MyAnimeList On Stream</h1>
        <p>Server is running! Add a browser source in OBS with one of these URLs:</p>
        <ul>
          <li><a href="/manga?status=READING">Currently Reading</a> - <code>http://localhost:${PORT}/manga?status=READING</code></li>
          <li><a href="/manga?status=COMPLETED">Completed</a> - <code>http://localhost:${PORT}/manga?status=COMPLETED</code></li>
          <li><a href="/manga?status=PAUSED">On Hold</a> - <code>http://localhost:${PORT}/manga?status=PAUSED</code></li>
          <li><a href="/manga?status=DROPPED">Dropped</a> - <code>http://localhost:${PORT}/manga?status=DROPPED</code></li>
          <li><a href="/manga?status=PLANNING">Plan to Read</a> - <code>http://localhost:${PORT}/manga?status=PLANNING</code></li>
          <li><a href="/manga?status=ALL">All Manga</a> - <code>http://localhost:${PORT}/manga?status=ALL</code></li>
        </ul>
        <p>Username: <strong>${config.malUsername}</strong></p>
        <p style="color: #666; margin-top: 30px;">Configure your username in config.json</p>
        <p style="color: #999; font-size: 12px;"><strong>Note:</strong> Your MyAnimeList manga list must be set to public for scraping to work.</p>
      </body>
    </html>
  `);
});

app.listen(PORT, () => {
  console.log(`🚀 MyAnimeList On Stream server running on http://localhost:${PORT}`);
  console.log(`📚 Username: ${config.malUsername}`);
  console.log(`\nAdd browser source in OBS: http://localhost:${PORT}/manga?status=READING`);
  console.log(`\n⚠️  Make sure your manga list is set to PUBLIC on MyAnimeList!`);
  if (config.debug) {
    console.log('Debug logging is ENABLED');
  }
});
