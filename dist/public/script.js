const urlParams = new URLSearchParams(window.location.search);
const status = urlParams.get('status') || 'ALL';
const media = (urlParams.get('media') || 'manga').toLowerCase();
const speedParam = urlParams.get('speed');
const apiBase = urlParams.get('api') || window.location.origin;

const loadingEl = document.getElementById('loading');
const scrollWrapper = document.getElementById('scrollWrapper');
const container = document.querySelector('.container');

function applyThemeFromQuery() {
  const bg = urlParams.get('bg');
  const bgImage = urlParams.get('bgImage');
  const padding = urlParams.get('padding') || urlParams.get('pad');
  const radius = urlParams.get('radius');
  const borderColor = urlParams.get('borderColor');
  const borderWidth = urlParams.get('borderWidth');
  const shadow = urlParams.get('shadow');
  const gap = urlParams.get('gap');

  if (bg || bgImage) {
    const backgroundValue = bgImage ? `url(${bgImage}) center/cover no-repeat` + (bg ? `, ${bg}` : '') : bg;
    document.documentElement.style.setProperty('--bg', backgroundValue);
  }
  if (padding) document.documentElement.style.setProperty('--pad', padding);
  if (radius) document.documentElement.style.setProperty('--radius', radius);
  if (borderColor || borderWidth) {
    const width = borderWidth || '4px';
    const color = borderColor || '#ffffff40';
    document.documentElement.style.setProperty('--border', `${width} solid ${color}`);
  }
  if (shadow) document.documentElement.style.setProperty('--shadow', shadow);
  if (gap) document.documentElement.style.setProperty('--gap', gap);
}

async function fetchList() {
  const apiUrl = new URL('/api/list', apiBase);
  apiUrl.searchParams.set('status', status);
  apiUrl.searchParams.set('media', media);
  if (speedParam) apiUrl.searchParams.set('speed', speedParam);

  try {
    const response = await fetch(apiUrl);
    if (!response.ok) {
      const text = await response.text().catch(() => '');
      throw new Error(`API ${response.status} ${response.statusText} — ${text || 'no body returned'}`);
    }

    const data = await response.json();
    if (data.error) {
      throw new Error(data.error);
    }
    return data;
  } catch (error) {
    console.error('Error fetching list:', error);
    throw new Error(`${error.message} (url: ${apiUrl.toString()})`);
  }
}

function createItem(entry) {
  const item = document.createElement('div');
  item.className = 'manga-item';

  const cover = document.createElement('img');
  cover.className = 'manga-cover';
  cover.src = entry.coverImage;
  cover.alt = entry.title;
  cover.loading = 'lazy';

  const title = document.createElement('div');
  title.className = 'manga-title';
  title.textContent = entry.title;

  const statusBadge = document.createElement('div');
  statusBadge.className = `manga-status status-${entry.status}`;
  statusBadge.textContent = formatStatus(entry.status, entry.media);

  item.appendChild(cover);
  item.appendChild(title);
  item.appendChild(statusBadge);

  return item;
}

function formatStatus(status, mediaType) {
  const statusMap = {
    CURRENT: 'Reading',
    READING: 'Reading',
    COMPLETED: 'Completed',
    PAUSED: 'On Hold',
    ONHOLD: 'On Hold',
    DROPPED: 'Dropped',
    PLANNING: mediaType === 'anime' ? 'Plan to Watch' : 'Plan to Read',
    PLANTOREAD: 'Plan to Read',
    PLANTOWATCH: 'Plan to Watch',
    WATCHING: 'Watching',
    REPEATING: 'Rereading'
  };
  return statusMap[status] || status;
}

function showError(message) {
  loadingEl.style.display = 'none';
  const errorEl = document.createElement('div');
  errorEl.className = 'error';
  errorEl.innerHTML = `<strong>Error:</strong><br>${message}<br><br><small>Check config.json username, list privacy (public), and network.</small>`;
  document.querySelector('.container').appendChild(errorEl);
}

function calculateAnimationDuration(itemCount, baseSpeed) {
  const baseDuration = baseSpeed || 60;
  const baseItems = 20;
  const duration = Math.max(30, (itemCount / baseItems) * baseDuration);
  return duration;
}

function ensureSeamlessLoop(originalNodes) {
  const containerWidth = container.getBoundingClientRect().width;
  
  for (let i = 0; i < 2; i++) {
    originalNodes.forEach(node => {
      scrollWrapper.appendChild(node.cloneNode(true));
    });
  }

  const singleSetWidth = scrollWrapper.getBoundingClientRect().width / 3;
  scrollWrapper.style.setProperty('--scroll-distance', `${singleSetWidth}px`);
}

async function init() {
  try {
    applyThemeFromQuery();

    const data = await fetchList();
    if (!data.items || data.items.length === 0) {
      showError('No entries found. Please check your username in config.json');
      return;
    }

    const fragment = document.createDocumentFragment();
    const nodes = [];

    data.items.forEach(entry => {
      const item = createItem(entry);
      nodes.push(item);
      fragment.appendChild(item);
    });

    scrollWrapper.appendChild(fragment);
    ensureSeamlessLoop(nodes);

    const scrollSpeed = speedParam ? parseInt(speedParam, 10) : data.scrollSpeed;
    const duration = calculateAnimationDuration(data.items.length, scrollSpeed);
    scrollWrapper.style.animationDuration = `${duration}s`;
    scrollWrapper.style.animationDelay = `-${Math.min(duration * 0.1, 5)}s`;

    loadingEl.style.display = 'none';
  } catch (error) {
    console.error('Failed to initialize:', error);
    showError(error.message || 'Failed to load list. Check console for details.');
  }
}

init();

setInterval(() => {
  location.reload();
}, 5 * 60 * 1000);
