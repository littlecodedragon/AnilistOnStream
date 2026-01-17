
const urlParams = new URLSearchParams(window.location.search);
const status = urlParams.get('status') || 'ALL';
const speedParam = urlParams.get('speed');


const loadingEl = document.getElementById('loading');
const scrollWrapper = document.getElementById('scrollWrapper');


async function fetchMangaList() {
  try {
    const response = await fetch(`/api/manga?status=${status}`);
    
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    
    const data = await response.json();
    
    if (data.error) {
      throw new Error(data.error);
    }
    
    return data;
  } catch (error) {
    console.error('Error fetching manga:', error);
    throw error;
  }
}


function createMangaItem(manga) {
  const item = document.createElement('div');
  item.className = 'manga-item';
  
  const cover = document.createElement('img');
  cover.className = 'manga-cover';
  cover.src = manga.coverImage;
  cover.alt = manga.title;
  cover.loading = 'lazy';
  
  const title = document.createElement('div');
  title.className = 'manga-title';
  title.textContent = manga.title;
  
  const statusBadge = document.createElement('div');
  statusBadge.className = `manga-status status-${manga.status}`;
  statusBadge.textContent = formatStatus(manga.status);
  
  item.appendChild(cover);
  item.appendChild(title);
  item.appendChild(statusBadge);
  
  return item;
}


function formatStatus(status) {
  const statusMap = {
    'CURRENT': 'Reading',
    'READING': 'Reading',
    'COMPLETED': 'Completed',
    'PAUSED': 'On Hold',
    'ONHOLD': 'On Hold',
    'DROPPED': 'Dropped',
    'PLANNING': 'Plan to Read',
    'PLANTOREAD': 'Plan to Read',
    'REPEATING': 'Rereading'
  };
  return statusMap[status] || status;
}


function showError(message) {
  loadingEl.style.display = 'none';
  const errorEl = document.createElement('div');
  errorEl.className = 'error';
  errorEl.innerHTML = `<strong>Error:</strong><br>${message}`;
  document.querySelector('.container').appendChild(errorEl);
}


function calculateAnimationDuration(itemCount, baseSpeed) {
  const baseDuration = baseSpeed || 60;
  const baseItems = 20;
  const duration = Math.max(30, (itemCount / baseItems) * baseDuration);
  return duration;
}


async function init() {
  try {
    const data = await fetchMangaList();
    
    if (!data.manga || data.manga.length === 0) {
      showError('No manga found. Please check your username in config.json');
      return;
    }
    
    
    const fragment = document.createDocumentFragment();
    
    
    const mangaList = [...data.manga, ...data.manga];
    
    mangaList.forEach(manga => {
      const item = createMangaItem(manga);
      fragment.appendChild(item);
    });
    
    scrollWrapper.appendChild(fragment);
    
    // Use speed from API response or URL parameter
    const scrollSpeed = speedParam ? parseInt(speedParam) : data.scrollSpeed;
    const duration = calculateAnimationDuration(data.manga.length, scrollSpeed);
    scrollWrapper.style.animationDuration = `${duration}s`;
    
    
    loadingEl.style.display = 'none';
    
  } catch (error) {
    console.error('Failed to initialize:', error);
    showError('Failed to load manga list. Check console for details.');
  }
}


init();


setInterval(() => {
  location.reload();
}, 5 * 60 * 1000);
