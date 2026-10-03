const messages = {
  fr: {
    title: 'ChessBot — jouer et analyser', description: 'Jouez contre ChessBot, un moteur d’échecs écrit en C, directement dans votre navigateur.',
    boardClocks: 'Échiquier et pendules', evaluation: 'Évaluation en faveur des Blancs', board: 'Échiquier', mode: 'Mode',
    loading: 'Chargement du moteur…', ready: 'Moteur prêt', thinking: 'Le moteur réfléchit…', analyzing: 'Analyse de la position…', unavailable: 'Analyse indisponible', failed: 'Chargement impossible', preparing: 'Préparation de la partie…',
    play: 'Jouer', analyze: 'Analyser', nextGame: 'Votre prochaine partie', color: 'Votre couleur', white: 'Blancs', black: 'Noirs',
    humanTime: 'Votre temps (minutes)', botTime: 'Moteur (secondes)', increment: 'Incrément du moteur (ms / coup)', newGame: 'Nouvelle partie', resign: 'Abandonner',
    standardTimes: 'Cadences standards', presetHint: 'Vous : minutes · moteur : secondes + incrément.',
    settingsHint: 'Les réglages s’appliquent à la prochaine partie. Votre pendule n’a pas d’incrément.',
    positionEvaluation: 'Évaluation de la position', analysisPending: 'Analyse en cours…', undo: '← Annuler', redo: 'Rétablir →',
    historyHint: '← / → : annuler / rétablir, hors des champs de saisie. Les pendules passent en pause en mode analyse.', searchDetails: 'Détails de recherche', noSearch: 'Aucune recherche terminée pour cette position.',
    fen: 'Position FEN', load: 'Charger', copy: 'Copier', fenHint: 'Charger une position ouvre le mode analyse.',
    footer: 'Moteur C exécuté dans votre navigateur · aucune partie envoyée à un serveur.', credits: 'Crédits des pièces',
    promotion: 'Choisir la promotion', cancel: 'Annuler', promote: 'Promouvoir en {piece}', flip: 'Retourner ↕', flipLabel: 'Retourner l’échiquier',
    you: 'Vous', analysis: 'Analyse', whitePiece: 'blanc', blackPiece: 'noir', empty: 'vide',
    pawn: 'pion', knight: 'cavalier', bishop: 'fou', rook: 'tour', queen: 'dame', king: 'roi',
    checkmate: 'Échec et mat', stalemate: 'Pat — partie nulle', fiftyMoves: 'Nulle — règle des 50 coups', repetition: 'Nulle — répétition selon le moteur', historyLimit: 'Limite de l’historique atteinte',
    timeout: '{side} : temps écoulé', resigned: '{side} abandonnent · {winner} gagnent', turn: '{side} au trait', check: ' · échec', paused: ' · pendules en pause',
    mateWin: 'Échec et mat. {side} gagnent.', draw: 'Partie nulle selon les règles du moteur.', mateAnnounced: 'Mat annoncé par le moteur pour les {side}.',
    searchScore: 'Score de recherche, du point de vue des Blancs.', forcedMove: 'Coup forcé : score de recherche indisponible.',
    staticScore: 'Score statique : {score}. Il ne reflète pas forcément la vraie évaluation de la position.', stats: 'Profondeur {depth} · {nodes} nœuds · {elapsed} ms',
    loaded: 'Position chargée. Les pendules sont en pause.', copied: 'FEN copiée.', copyFallback: 'Copiez la FEN sélectionnée avec Ctrl+C.',
    invalidTimes: 'Vérifiez les temps : 0,1–60 min, 1–300 s et 0–5000 ms d’incrément.',
    loadError: 'Impossible de charger le moteur WebAssembly.', fileError: 'Ouvrez cette interface avec le serveur local web.ps1.',
    invalidFen: 'FEN invalide ou non prise en charge.', invalidMove: 'Ce coup ne peut pas être joué.', invalidSearch: 'Position de recherche invalide.', invalidHistory: 'Historique de recherche invalide.',
    initError: 'Échec de l’initialisation du moteur.', allocationError: 'Mémoire insuffisante pour la recherche.', mate: 'Mat',
  },
  en: {
    title: 'ChessBot — play and analyze', description: 'Play against ChessBot, a chess engine written in C, directly in your browser.',
    boardClocks: 'Chessboard and clocks', evaluation: 'Evaluation in White’s favor', board: 'Chessboard', mode: 'Mode',
    loading: 'Loading engine…', ready: 'Engine ready', thinking: 'Engine is thinking…', analyzing: 'Analyzing position…', unavailable: 'Analysis unavailable', failed: 'Loading failed', preparing: 'Preparing game…',
    play: 'Play', analyze: 'Analyze', nextGame: 'Your next game', color: 'Your color', white: 'White', black: 'Black',
    humanTime: 'Your time (minutes)', botTime: 'Engine (seconds)', increment: 'Engine increment (ms / move)', newGame: 'New game', resign: 'Resign',
    standardTimes: 'Standard time controls', presetHint: 'You: minutes · engine: seconds + increment.',
    settingsHint: 'Settings apply to the next game. Your clock has no increment.',
    positionEvaluation: 'Position evaluation', analysisPending: 'Analyzing…', undo: '← Undo', redo: 'Redo →',
    historyHint: '← / →: undo / redo outside input fields. Clocks pause in analysis mode.', searchDetails: 'Search details', noSearch: 'No completed search for this position.',
    fen: 'FEN position', load: 'Load', copy: 'Copy', fenHint: 'Loading a position opens analysis mode.',
    footer: 'C engine running in your browser · no games sent to a server.', credits: 'Piece artwork credits',
    promotion: 'Choose promotion', cancel: 'Cancel', promote: 'Promote to {piece}', flip: 'Flip ↕', flipLabel: 'Flip the chessboard',
    you: 'You', analysis: 'Analysis', whitePiece: 'white', blackPiece: 'black', empty: 'empty',
    pawn: 'pawn', knight: 'knight', bishop: 'bishop', rook: 'rook', queen: 'queen', king: 'king',
    checkmate: 'Checkmate', stalemate: 'Stalemate — draw', fiftyMoves: 'Draw — fifty-move rule', repetition: 'Draw — engine repetition rule', historyLimit: 'History limit reached',
    timeout: '{side}: time expired', resigned: '{side} resigns · {winner} wins', turn: '{side} to move', check: ' · check', paused: ' · clocks paused',
    mateWin: 'Checkmate. {side} wins.', draw: 'Draw under the engine’s rules.', mateAnnounced: 'Engine announces mate for {side}.',
    searchScore: 'Search score, from White’s perspective.', forcedMove: 'Forced move: search score unavailable.',
    staticScore: 'Static score: {score}. It does not necessarily reflect the true evaluation of the position.', stats: 'Depth {depth} · {nodes} nodes · {elapsed} ms',
    loaded: 'Position loaded. Clocks are paused.', copied: 'FEN copied.', copyFallback: 'Copy the selected FEN with Ctrl+C.',
    invalidTimes: 'Check the times: 0.1–60 min, 1–300 s and 0–5000 ms increment.',
    loadError: 'Could not load the WebAssembly engine.', fileError: 'Open this interface using the web.ps1 local server.',
    invalidFen: 'Invalid or unsupported FEN.', invalidMove: 'This move cannot be played.', invalidSearch: 'Invalid search position.', invalidHistory: 'Invalid search history.',
    initError: 'Engine initialization failed.', allocationError: 'Insufficient memory for search.', mate: 'Mate',
  },
};

export let language = 'fr';
try { if (localStorage.getItem('chessbot-language') === 'en') language = 'en'; } catch {}
export function t(key, values = {}) {
  return (messages[language][key] ?? key).replace(/\{(\w+)\}/g, (_, name) => values[name] ?? `{${name}}`);
}
export function setLanguage(value) {
  language = value === 'en' ? 'en' : 'fr';
  try { localStorage.setItem('chessbot-language', language); } catch {}
}
export function translatePage() {
  document.documentElement.lang = language;
  document.title = t('title');
  document.querySelector('meta[name="description"]').content = t('description');
  for (const element of document.querySelectorAll('[data-i18n]')) element.textContent = t(element.dataset.i18n);
  for (const element of document.querySelectorAll('[data-i18n-label]')) element.setAttribute('aria-label', t(element.dataset.i18nLabel));
  for (const button of document.querySelectorAll('[data-language]')) button.setAttribute('aria-pressed', String(button.dataset.language === language));
}
