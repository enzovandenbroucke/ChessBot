import { GameClock, clockText, evaluationText, evaluationFill, INF } from './clock.js';
import { t, language, setLanguage, translatePage } from './i18n.js';

const START_FEN = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';
const $ = id => document.getElementById(id);
const typeNames = { p: 'pawn', n: 'knight', b: 'bishop', r: 'rook', q: 'queen', k: 'king' };
const promotionNames = { 5: 'queen', 4: 'rook', 3: 'bishop', 2: 'knight' };
const piecesFromFen = fen => {
  const pieces = Array(64).fill(null);
  fen.split(' ')[0].split('/').forEach((rank, index) => {
    let file = 0;
    for (const char of rank) {
      if (/\d/.test(char)) file += Number(char);
      else pieces[(7 - index) * 8 + file++] = char;
    }
  });
  return pieces;
};
const squareName = square => String.fromCharCode(97 + square % 8) + (1 + Math.floor(square / 8));
const pieceImage = piece => `pieces/${piece === piece.toUpperCase() ? 'white' : 'black'}-${typeNames[piece.toLowerCase()]}.svg`;

class EngineWorker {
  constructor() {
    this.worker = new Worker(new URL('./worker.js', import.meta.url));
    this.pending = new Map(); this.nextId = 0;
    this.worker.onmessage = ({ data }) => {
      const promise = this.pending.get(data.id);
      if (!promise) return;
      this.pending.delete(data.id);
      if (data.error) promise.reject(new Error(data.error)); else promise.resolve(data.result);
    };
    this.worker.onerror = () => this.rejectAll(new Error('loadError'));
    this.ready = this.request('init');
  }
  request(action, payload) {
    return new Promise((resolve, reject) => {
      const id = ++this.nextId;
      this.pending.set(id, { resolve, reject });
      this.worker.postMessage({ id, action, payload });
    });
  }
  rejectAll(error) { for (const promise of this.pending.values()) promise.reject(error); this.pending.clear(); }
  terminate() { this.rejectAll(new DOMException('Search cancelled', 'AbortError')); this.worker.terminate(); }
}

let stateWorker, searchWorker, state;
let mode = 'play', humanWhite = true, flipped = false, selected = null, busy = false;
let startFen = START_FEN, records = [], revision = 0, searchToken = 0, currentSearch = null;
let flag = null, resigned = null, config = { humanMs: 300000, botMs: 5000, increment: 100 };
let clock = new GameClock(config.humanMs, config.botMs), initialClocks = [...clock.remaining];
const evaluations = new Map();
let drag = null;

let noticeKey = '', badgeKey = 'loading';
function notice(message = '') { noticeKey = message; $('notice').textContent = t(message); }
function badge(key) { badgeKey = key; $('engine-badge').textContent = t(key); }
const sideName = side => t(side === 0 ? 'white' : 'black');
const scoreText = score => Math.abs(score) === INF ? t('mate') : evaluationText(score);
function cancelSearch() {
  searchToken++;
  if (currentSearch) {
    searchWorker.terminate();
    searchWorker = new EngineWorker();
    searchWorker.ready.catch(error => notice(error.message));
  }
  currentSearch = null;
  badge('ready');
}
function terminal() { return flag !== null || resigned !== null || !!state?.result; }
function canPlay() { return state && !busy && !terminal() && (mode === 'analysis' || state.whiteToMove === humanWhite); }
function runningClock(now = performance.now()) {
  if (mode !== 'play' || terminal() || clock.active === null) return;
  if (clock.values(now)[clock.active] <= 0) {
    flag = clock.active;
    clock.pause(now);
    cancelSearch();
    selected = null;
    if ($('promotion-dialog').open) $('promotion-dialog').close();
    render();
  }
}
function renderClocks() {
  const values = clock.values(performance.now());
  const topSide = flipped ? 0 : 1, bottomSide = 1 - topSide;
  for (const [position, side] of [['top', topSide], ['bottom', bottomSide]]) {
    const card = $(`${position}-player`);
    const human = (side === 0) === humanWhite;
    card.querySelector('.player-name').textContent = `${sideName(side)} · ${mode === 'analysis' ? t('analysis') : human ? t('you') : 'ChessBot v17a'}`;
    $(`${position}-clock`).textContent = clockText(values[side]);
    card.classList.toggle('active', clock.active === side && mode === 'play' && !terminal());
    card.classList.toggle('flagged', flag === side);
  }
}
function renderEvaluation() {
  if (!state) return;
  const analysis = evaluations.get(state.fen);
  let score = null, description = t('analysisPending');
  if (state.result === 1) {
    score = state.whiteToMove ? -INF : INF;
    description = t('mateWin', { side: sideName(score > 0 ? 0 : 1) });
  } else if ([2, 3, 4].includes(state.result)) {
    score = 0; description = t('draw');
  } else if (analysis && analysis.depth > 0) {
    score = analysis.score;
    description = Math.abs(score) >= INF - 64
      ? t('mateAnnounced', { side: sideName(score > 0 ? 0 : 1) })
      : t('searchScore');
  } else if (analysis) description = t('forcedMove');
  $('eval-score').textContent = score === null ? '—' : scoreText(score);
  $('bar-score').textContent = score === null ? '…' : scoreText(score);
  $('eval-white').parentElement.classList.toggle('flipped', flipped);
  $('eval-white').style.height = `${score === null ? 50 : evaluationFill(score)}%`;
  $('eval-description').textContent = description;
  $('static-score').textContent = t('staticScore', { score: scoreText(state.staticScore) });
  $('search-stats').textContent = analysis
    ? t('stats', { depth: analysis.depth, nodes: analysis.nodes.toLocaleString(language === 'fr' ? 'fr-FR' : 'en-GB'), elapsed: analysis.elapsed })
    : t('noSearch');
  $('pv').textContent = analysis?.pv.join(' ') || '';
}
function renderBoard() {
  if (!state) return;
  const pieces = piecesFromFen(state.fen);
  const destinations = new Set(selected === null ? [] : state.legal.filter(move => move.from === selected).map(move => move.to));
  const last = records[state.cursor - 1];
  const buttons = [];
  for (let row = 0; row < 8; row++) for (let file = 0; file < 8; file++) {
    const square = flipped ? row * 8 + 7 - file : (7 - row) * 8 + file;
    const piece = pieces[square];
    const button = document.createElement('button');
    button.type = 'button'; button.dataset.square = square;
    button.className = 'square';
    if ((row + file) % 2) button.classList.add('dark');
    if (square === selected) button.classList.add('selected');
    if (last && (last.from === square || last.to === square)) button.classList.add('last');
    if (destinations.has(square)) { button.classList.add('destination'); if (piece) button.classList.add('capture'); }
    button.setAttribute('aria-label', `${squareName(square)}, ${piece ? `${t(typeNames[piece.toLowerCase()])} ${t(piece === piece.toUpperCase() ? 'whitePiece' : 'blackPiece')}` : t('empty')}`);
    if (piece) {
      const image = document.createElement('img');
      image.className = 'piece'; image.src = pieceImage(piece); image.alt = ''; image.draggable = false;
      button.append(image);
    }
    if (file === 0) {
      const label = document.createElement('span'); label.className = 'coordinate rank-label';
      label.textContent = 1 + Math.floor(square / 8); button.append(label);
    }
    if (row === 7) {
      const label = document.createElement('span'); label.className = 'coordinate file-label';
      label.textContent = String.fromCharCode(97 + square % 8); button.append(label);
    }
    buttons.push(button);
  }
  $('board').replaceChildren(...buttons);
}
function render() {
  if (!state) return;
  renderBoard(); renderClocks(); renderEvaluation();
  $('mode-play').classList.toggle('selected', mode === 'play');
  $('mode-analysis').classList.toggle('selected', mode === 'analysis');
  $('undo').disabled = busy || state.cursor === 0;
  $('redo').disabled = busy || state.cursor >= state.historyCount;
  $('new-game').disabled = busy;
  $('resign').disabled = busy || mode !== 'play' || terminal();
  $('load-fen').disabled = busy; $('copy-fen').disabled = busy;
  $('fen').value = state.fen;
  const results = ['', 'checkmate', 'stalemate', 'fiftyMoves', 'repetition', 'historyLimit'];
  $('turn-status').textContent = resigned !== null
    ? t('resigned', { side: sideName(resigned), winner: sideName(1 - resigned) })
    : flag !== null ? t('timeout', { side: sideName(flag) })
    : state.result ? t(results[state.result])
    : t('turn', { side: sideName(state.whiteToMove ? 0 : 1) }) + (state.check ? t('check') : '') + (mode === 'analysis' ? t('paused') : '');
}
function enterAnalysis() {
  cancelSearch(); clock.pause(performance.now()); mode = 'analysis'; selected = null;
  render();
}
async function runSearch(playMove = false) {
  if (!state || terminal()) { renderEvaluation(); return; }
  const token = ++searchToken, positionRevision = revision, rootFen = state.fen;
  currentSearch = playMove ? 'play' : 'analysis';
  badge(playMove ? 'thinking' : 'analyzing');
  try {
    await searchWorker.ready;
    if (token !== searchToken) return;
    runningClock();
    if (terminal()) return;
    const side = state.whiteToMove ? 0 : 1;
    const analysis = await searchWorker.request('search', {
      startFen, moves: records.slice(0, state.cursor).map(({ from, to, promotion }) => ({ from, to, promotion })),
      timeLeft: playMove ? clock.values(performance.now())[side] : 10000,
      increment: playMove ? config.increment : 0,
    });
    if (token !== searchToken || positionRevision !== revision || state.fen !== rootFen) return;
    runningClock();
    if (flag !== null) return;
    evaluations.set(rootFen, analysis);
    if (evaluations.size > 100) evaluations.delete(evaluations.keys().next().value);
    currentSearch = null; badge('ready');
    renderEvaluation();
    if (playMove && analysis.move) await commitMove(analysis.move, true);
  } catch (error) {
    if (token !== searchToken || error.name === 'AbortError') return;
    currentSearch = null; badge('unavailable');
    notice(error.message);
    if (playMove) { clock.pause(performance.now()); mode = 'analysis'; render(); }
  }
}
function updateSearch() {
  if (terminal()) { renderEvaluation(); return; }
  const botTurn = mode === 'play' && state.whiteToMove !== humanWhite;
  runSearch(botTurn);
}
async function commitMove(move, engineMove = false) {
  if (busy || terminal() || (!engineMove && !canPlay())) return;
  const legal = state.legal.find(candidate => candidate.from === move.from && candidate.to === move.to &&
    (!candidate.promotion || candidate.promotion === move.promotion));
  if (!legal) return;
  runningClock(); if (flag !== null) return;
  cancelSearch(); selected = null; busy = true;
  const side = state.whiteToMove ? 0 : 1, oldCursor = state.cursor;
  const wasPlaying = mode === 'play';
  clock.pause(performance.now());
  try {
    const next = await stateWorker.request('move', legal);
    if (wasPlaying) clock.completeMove(side, engineMove ? config.increment : 0, performance.now());
    state = next; revision++;
    records = records.slice(0, oldCursor);
    records.push({ ...legal, clocks: clock.values(performance.now()) });
    if (terminal()) clock.pause(performance.now());
    notice();
  } catch (error) {
    notice(error.message);
    if (wasPlaying) clock.start(side, performance.now());
  } finally { busy = false; render(); }
  updateSearch();
}
function choosePromotion(white) {
  const dialog = $('promotion-dialog');
  $('promotion-options').replaceChildren();
  return new Promise(resolve => {
    let choice = null;
    for (const type of [5, 4, 3, 2]) {
      const button = document.createElement('button'); button.type = 'button';
      const label = t(promotionNames[type]);
      button.setAttribute('aria-label', t('promote', { piece: label }));
      const image = document.createElement('img'); image.alt = label;
      image.src = `pieces/${white ? 'white' : 'black'}-${promotionNames[type]}.svg`;
      button.append(image); button.onclick = () => { choice = type; dialog.close(); };
      $('promotion-options').append(button);
    }
    dialog.addEventListener('close', () => resolve(choice), { once: true });
    dialog.showModal();
  });
}
async function attemptMove(from, to) {
  const choices = state?.legal.filter(move => move.from === from && move.to === to) || [];
  if (!choices.length || !canPlay()) return;
  const positionRevision = revision;
  let move = choices[0];
  if (choices.length > 1 && choices.some(candidate => candidate.promotion)) {
    const promotion = await choosePromotion(state.whiteToMove);
    if (!promotion || positionRevision !== revision || terminal()) return;
    move = choices.find(candidate => candidate.promotion === promotion);
  }
  await commitMove(move);
}
function selectSquare(square) {
  if (!canPlay()) return;
  const piece = piecesFromFen(state.fen)[square];
  const friendly = piece && (piece === piece.toUpperCase()) === state.whiteToMove;
  if (friendly) { selected = square; renderBoard(); }
  else if (selected !== null) attemptMove(selected, square);
}
$('board').addEventListener('pointerdown', event => {
  const button = event.target.closest('[data-square]');
  if (!button || !canPlay() || event.button !== 0) return;
  const square = Number(button.dataset.square), piece = piecesFromFen(state.fen)[square];
  const friendly = piece && (piece === piece.toUpperCase()) === state.whiteToMove;
  if (friendly) {
    drag = { from: square, x: event.clientX, y: event.clientY, width: button.getBoundingClientRect().width, moved: false };
    $('drag-piece').src = pieceImage(piece);
    selectSquare(square);
  } else selectSquare(square);
  event.preventDefault();
});
window.addEventListener('pointermove', event => {
  if (!drag) return;
  if (Math.hypot(event.clientX - drag.x, event.clientY - drag.y) > 5) drag.moved = true;
  if (!drag.moved) return;
  $('drag-piece').hidden = false;
  $('drag-piece').style.cssText = `left:${event.clientX}px;top:${event.clientY}px;width:${drag.width * .91}px;height:${drag.width * .91}px`;
  $('board').classList.add('dragging');
});
window.addEventListener('pointerup', event => {
  if (!drag) return;
  const from = drag.from, moved = drag.moved;
  drag = null; $('drag-piece').hidden = true; $('board').classList.remove('dragging');
  const button = document.elementFromPoint(event.clientX, event.clientY)?.closest('[data-square]');
  if (button && (moved || Number(button.dataset.square) !== from)) attemptMove(from, Number(button.dataset.square));
});
window.addEventListener('pointercancel', () => { drag = null; $('drag-piece').hidden = true; $('board').classList.remove('dragging'); });
$('board').addEventListener('click', event => {
  if (event.detail === 0) {
    const button = event.target.closest('[data-square]');
    if (button) selectSquare(Number(button.dataset.square));
  }
});
$('cancel-promotion').onclick = () => $('promotion-dialog').close();
$('resign').onclick = () => {
  if (!state || busy || mode !== 'play' || terminal()) return;
  runningClock(); if (terminal()) return;
  resigned = humanWhite ? 0 : 1;
  clock.pause(performance.now()); cancelSearch(); selected = null;
  if ($('promotion-dialog').open) $('promotion-dialog').close();
  notice(); render();
};
$('flip').onclick = () => { flipped = !flipped; selected = null; render(); };
$('mode-analysis').onclick = () => { if (state && !busy) { enterAnalysis(); updateSearch(); } };
$('mode-play').onclick = () => {
  if (!state || busy || terminal()) return;
  cancelSearch(); mode = 'play'; clock.start(state.whiteToMove ? 0 : 1, performance.now());
  render(); updateSearch();
};

async function newGame() {
  if (busy || !state) return;
  const humanMinutes = Number($('human-time').value), botSeconds = Number($('bot-time').value), increment = Number($('bot-increment').value);
  if (!Number.isFinite(humanMinutes) || humanMinutes < .1 || humanMinutes > 60 ||
      !Number.isFinite(botSeconds) || botSeconds < 1 || botSeconds > 300 ||
      !Number.isFinite(increment) || increment < 0 || increment > 5000) {
    notice('invalidTimes'); return;
  }
  enterAnalysis(); busy = true;
  if ($('promotion-dialog').open) $('promotion-dialog').close();
  try {
    await searchWorker.ready;
    state = await stateWorker.request('load', { fen: START_FEN });
    config = { humanMs: humanMinutes * 60000, botMs: botSeconds * 1000, increment };
    humanWhite = document.querySelector('input[name="human-color"]:checked').value === 'white'; flipped = !humanWhite;
    startFen = START_FEN; records = []; revision++; flag = null; resigned = null;
    clock.reset(humanWhite ? config.humanMs : config.botMs, humanWhite ? config.botMs : config.humanMs);
    initialClocks = [...clock.remaining]; mode = 'play'; clock.start(0, performance.now());
    notice();
  } catch (error) { notice(error.message); }
  finally { busy = false; render(); }
  updateSearch();
}
$('new-game').onclick = newGame;
for (const action of ['undo', 'redo']) $(action).onclick = async () => {
  if (busy || !state) return;
  enterAnalysis(); busy = true;
  try {
    state = await stateWorker.request(action); revision++; flag = null; resigned = null;
    clock.remaining = [...(state.cursor ? records[state.cursor - 1].clocks : initialClocks)];
    clock.active = null; notice();
  } catch (error) { notice(error.message); }
  finally { busy = false; render(); }
  updateSearch();
};
window.addEventListener('keydown', event => {
  if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey ||
      event.target.closest('input, textarea, select, [contenteditable]:not([contenteditable="false"])') ||
      $('promotion-dialog').open) return;
  const action = event.key === 'ArrowLeft' ? 'undo' : event.key === 'ArrowRight' ? 'redo' : null;
  if (!action || !state || busy || $(action).disabled) return;
  event.preventDefault();
  $(action).click();
});
function syncPresets() {
  const values = [$('human-time'), $('bot-time'), $('bot-increment')].map(input => Number(input.value));
  for (const button of document.querySelectorAll('[data-preset]')) {
    const selected = button.dataset.preset.split(',').every((value, index) => Number(value) === values[index]);
    button.setAttribute('aria-pressed', String(selected));
  }
}
for (const button of document.querySelectorAll('[data-preset]')) button.onclick = () => {
  const values = button.dataset.preset.split(',');
  ['human-time', 'bot-time', 'bot-increment'].forEach((id, index) => { $(id).value = values[index]; });
  syncPresets();
};
for (const id of ['human-time', 'bot-time', 'bot-increment']) $(id).addEventListener('input', syncPresets);
syncPresets();
$('load-fen').onclick = async () => {
  if (busy || !state) return;
  const fen = $('fen').value.trim();
  enterAnalysis(); busy = true;
  try {
    state = await stateWorker.request('load', { fen });
    startFen = state.fen; records = []; revision++; flag = null; resigned = null;
    initialClocks = [...clock.remaining];
    notice('loaded');
  } catch (error) { notice(error.message); }
  finally { busy = false; render(); }
  updateSearch();
};
$('copy-fen').onclick = async () => {
  try { await navigator.clipboard.writeText(state.fen); notice('copied'); }
  catch { $('fen').focus(); $('fen').select(); notice('copyFallback'); }
};

setInterval(() => { runningClock(); renderClocks(); }, 50);
window.addEventListener('beforeunload', () => { stateWorker?.terminate(); searchWorker?.terminate(); });
async function boot() {
  try {
    if (location.protocol === 'file:') throw new Error('fileError');
    stateWorker = new EngineWorker(); searchWorker = new EngineWorker();
    [state] = await Promise.all([stateWorker.ready, searchWorker.ready]);
    badge('ready');
    const queryFen = new URLSearchParams(location.search).get('fen');
    if (queryFen) {
      state = await stateWorker.request('load', { fen: queryFen });
      startFen = state.fen; mode = 'analysis';
    } else clock.start(0, performance.now());
    render(); updateSearch();
  } catch (error) { notice(error.message); badge('failed'); }
}
for (const button of document.querySelectorAll('[data-language]')) button.onclick = () => {
  const fenDraft = $('fen').value;
  setLanguage(button.dataset.language); translatePage(); badge(badgeKey); notice(noticeKey); render();
  $('fen').value = fenDraft;
};
translatePage(); badge(badgeKey);
boot();
