'use strict';

const assetRoot = 'wiitogether-assets/';
const games = [
  { title: 'Wii Sports Resort', genre: 'Sports', players: '1–4 players', cover: 'RZTE01.png' },
  { title: 'Super Mario Galaxy', genre: 'Adventure', players: '1–2 players', cover: 'RMGE01.png' },
  { title: 'Mario Kart Wii', genre: 'Racing', players: '1–4 players', cover: 'RMCE01.png' },
  { title: 'Super Smash Bros. Brawl', genre: 'Fighting', players: '1–4 players', cover: 'RSBE01.png' },
  { title: 'Wii Party', genre: 'Party', players: '1–4 players', cover: 'SUPE01.png' }
];
const playerColors = [ ['#27a9e3', '39,169,227'], ['#ec6370', '236,99,112'], ['#70b65b', '112,182,91'], ['#e3ad33', '227,173,51'] ];
const players = [false, false, false, false];
let selected = 2;
let soundEnabled = false;
let motionEnabled = !window.matchMedia('(prefers-reduced-motion: reduce)').matches;
document.body.classList.toggle('motion-disabled', !motionEnabled);
let audioContext;
let toastTimer;
const byId = id => document.getElementById(id);

function tone(click = false) {
  if (!soundEnabled) return;
  try {
    audioContext ||= new (window.AudioContext || window.webkitAudioContext)();
    const oscillator = audioContext.createOscillator();
    const gain = audioContext.createGain();
    oscillator.type = 'sine';
    oscillator.frequency.setValueAtTime(click ? 880 : 660, audioContext.currentTime);
    oscillator.frequency.exponentialRampToValueAtTime(click ? 1320 : 880, audioContext.currentTime + .07);
    gain.gain.setValueAtTime(.045, audioContext.currentTime);
    gain.gain.exponentialRampToValueAtTime(.001, audioContext.currentTime + .12);
    oscillator.connect(gain).connect(audioContext.destination);
    oscillator.start(); oscillator.stop(audioContext.currentTime + .13);
  } catch (_) { /* Browsers may defer audio until the next user gesture. */ }
}

function notify(message) {
  clearTimeout(toastTimer);
  byId('toast').textContent = message;
  byId('toast').classList.add('visible');
  toastTimer = setTimeout(() => byId('toast').classList.remove('visible'), 2700);
}

function renderGame() {
  document.querySelectorAll('.game-cover').forEach((cover, index) => {
    let offset = (index - selected + games.length) % games.length;
    if (offset > 2) offset -= games.length;
    cover.style.setProperty('--offset', offset);
    cover.style.setProperty('--distance', Math.abs(offset));
    cover.style.setProperty('--scale', offset === 0 ? 1 : (Math.abs(offset) === 1 ? .82 : .67));
    cover.style.setProperty('--angle', `${offset === 0 ? 0 : (offset < 0 ? 25 : -25)}deg`);
    cover.classList.toggle('selected', offset === 0);
    cover.setAttribute('aria-label', `${games[index].title}${offset === 0 ? ', selected' : ''}`);
    cover.setAttribute('aria-pressed', String(offset === 0));
  });
  document.querySelectorAll('.pagination button').forEach((dot, i) => dot.setAttribute('aria-current', String(i === selected)));
}

function move(delta) { selected = (selected + delta + games.length) % games.length; renderGame(); tone(true); }

function togglePlayer(index) {
  players[index] = !players[index];
  const count = players.filter(Boolean).length;
  document.querySelectorAll('.player').forEach((card, i) => {
    card.classList.toggle('active', players[i]);
    card.querySelector('.player-subtitle').textContent = players[i] ? 'Ready to play' : 'Connect a phone';
    card.querySelector('.simulate').textContent = players[i] ? 'Preview disconnect' : 'Preview connection';
  });
  byId('lobby-status').replaceChildren(document.createElement('i'),
    document.createTextNode(count ? `${count} ${count === 1 ? 'player' : 'players'} ready to play` : 'Connect a phone to play together'));
  document.querySelector('.lobby-bar').classList.toggle('connected', count > 0);
  notify(`Player ${index + 1} ${players[index] ? 'connected' : 'disconnected'} · Preview`);
  tone(true);
}

games.forEach((game, index) => {
  const cover = document.createElement('button');
  cover.className = 'game-cover';
  cover.style.setProperty('--float-delay', `${index * -.7}s`);
  const image = document.createElement('img'); image.src = assetRoot + game.cover; image.alt = game.title + ' cover'; image.draggable = false;
  const shadow = document.createElement('span'); shadow.className = 'ground-shadow';
  cover.append(shadow, image);
  cover.addEventListener('click', () => { if (selected === index) startGame(); else { selected = index; renderGame(); tone(true); } });
  byId('carousel').append(cover);
  const dot = document.createElement('button'); dot.setAttribute('aria-label', `Select ${game.title}`);
  dot.addEventListener('click', () => { selected = index; renderGame(); tone(true); });
  byId('pagination').append(dot);
});

players.forEach((_, index) => {
  const card = document.createElement('div'); card.className = 'player';
  card.style.setProperty('--player', playerColors[index][0]); card.style.setProperty('--rgb', playerColors[index][1]);
  card.innerHTML = `<button class="player-card" aria-label="Connect player ${index + 1}" aria-expanded="false"><svg aria-hidden="true"><use href="#remote"/></svg><span class="player-copy"><span class="player-title">Player ${index + 1}</span><span class="player-subtitle">Connect a phone</span></span><span class="player-number">${index + 1}</span></button><section class="join-panel" aria-label="Player ${index + 1} connection"><h3>Let's play together.</h3><p>Scan with your phone to join.</p><img src="${assetRoot}join-preview.svg" alt="Demonstration QR code, no live PartyPad session"><p>Same Wi-Fi. No extra controllers.</p><button class="wii-button simulate">Preview connection</button><small>Demo QR · live connection in Dolphin</small></section>`;
  card.querySelector('.player-card').addEventListener('click', event => {
    const open = !card.classList.contains('open');
    document.querySelectorAll('.player').forEach(p => { p.classList.remove('open'); p.querySelector('.player-card').setAttribute('aria-expanded', 'false'); });
    card.classList.toggle('open', open); event.currentTarget.setAttribute('aria-expanded', String(open)); tone(true);
  });
  card.querySelector('.simulate').addEventListener('click', () => togglePlayer(index));
  byId('players').append(card);
});

const dialog = byId('system-dialog');
function closePlayerPanels() {
  document.querySelectorAll('.player').forEach(player => {
    player.classList.remove('open');
    player.querySelector('.player-card').setAttribute('aria-expanded', 'false');
  });
}
function showDialog(title, content) {
  closePlayerPanels();
  byId('dialog-title').textContent = title;
  byId('dialog-content').innerHTML = content;
  dialog.showModal(); tone(true);
}
document.querySelector('.close-dialog').addEventListener('click', () => dialog.close());
dialog.addEventListener('click', event => { if (event.target === dialog) { const r = dialog.getBoundingClientRect(); if (event.clientX < r.left || event.clientX > r.right || event.clientY < r.top || event.clientY > r.bottom) dialog.close(); } });

byId('settings').addEventListener('click', () => {
  showDialog('WiiTogether Settings', `<label class="setting-row">Player 1 pointer<input id="pointer-toggle" type="checkbox" ${document.body.classList.contains('pointer-mode') ? 'checked' : ''}></label><label class="setting-row">Menu sounds<input id="sound-toggle" type="checkbox" ${soundEnabled ? 'checked' : ''}></label><label class="setting-row">Menu animations<input id="motion-toggle" type="checkbox" ${motionEnabled ? 'checked' : ''}></label><p class="setting-title">PREVIEW PLAYER CONNECTIONS</p>${players.map((on, i) => `<label class="setting-row">Player ${i + 1}<input type="checkbox" data-player="${i}" ${on ? 'checked' : ''}></label>`).join('')}<p class="dialog-description">Use ← / → to browse games and Enter to select. Player connections here are simulated for the design preview.</p>`);
  byId('pointer-toggle').addEventListener('change', event => document.body.classList.toggle('pointer-mode', event.target.checked));
  byId('sound-toggle').addEventListener('change', event => { soundEnabled = event.target.checked; tone(true); });
  byId('motion-toggle').addEventListener('change', event => {
    motionEnabled = event.target.checked;
    document.body.classList.toggle('motion-enabled', motionEnabled);
    document.body.classList.toggle('motion-disabled', !motionEnabled);
  });
  dialog.querySelectorAll('[data-player]').forEach(toggle => toggle.addEventListener('change', () => togglePlayer(Number(toggle.dataset.player))));
});
byId('quit').addEventListener('click', () => {
  showDialog('Finished playing?', '<p class="dialog-description">Would you like to close WiiTogether?</p><div class="dialog-actions"><button class="wii-button" id="cancel-quit">Cancel</button><button class="wii-button" id="confirm-quit">Quit</button></div>');
  byId('cancel-quit').onclick = () => dialog.close();
  byId('confirm-quit').onclick = () => { dialog.close(); window.close(); notify('Preview complete. You can close this tab.'); };
});
function startGame() {
  showDialog(games[selected].title, '<p class="dialog-description">Your game is selected. This frontend preview does not launch Dolphin or connect phones yet.</p><div class="dialog-actions"><button class="wii-button" id="back-to-games">Back to games</button></div>');
  byId('back-to-games').onclick = () => dialog.close();
}
byId('start').addEventListener('click', startGame);
byId('previous').addEventListener('click', () => move(-1));
byId('next').addEventListener('click', () => move(1));
document.addEventListener('keydown', event => {
  if (dialog.open || /INPUT|TEXTAREA/.test(event.target.tagName)) return;
  if (event.key === 'ArrowLeft') { event.preventDefault(); move(-1); }
  if (event.key === 'ArrowRight') { event.preventDefault(); move(1); }
  if (event.key === 'Enter' && !event.target.closest('button')) startGame();
  if (event.key === 'Escape') closePlayerPanels();
});
const pointer = byId('pointer');
document.body.classList.add('pointer-mode');
document.addEventListener('pointermove', event => {
  if (event.pointerType === 'touch') return;
  pointer.classList.add('moving');
  pointer.style.transform = `translate3d(${event.clientX - 19}px,${event.clientY - 3}px,0)`;
});
document.addEventListener('pointerout', event => { if (!event.relatedTarget) pointer.classList.remove('moving'); });
document.addEventListener('pointerover', event => { if (event.target.closest('button')) tone(); });
function updateClock() { byId('clock').textContent = new Intl.DateTimeFormat(undefined, { hour: 'numeric', minute: '2-digit' }).format(new Date()); }
updateClock(); setInterval(updateClock, 1000 * 30);
renderGame();
