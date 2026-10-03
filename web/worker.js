/* Each worker owns a separate Wasm instance; no shared memory or pthreads. */
importScripts('engine.js');
let engine;
const ready = createChessBot({ print: () => {}, printErr: text => console.error(text) })
  .then(module => {
    engine = module;
    if (!engine._cb_init()) throw new Error('initError');
  });

self.onmessage = async ({ data }) => {
  const { id, action, payload = {} } = data;
  try {
    await ready;
    const call = (name, types = [], values = []) => engine.ccall(name, 'number', types, values);
    let result;
    if (action === 'init') result = JSON.parse(engine.UTF8ToString(engine._cb_state()));
    else if (action === 'load') {
      if (!call('cb_load', ['string'], [payload.fen])) throw new Error('invalidFen');
      result = JSON.parse(engine.UTF8ToString(engine._cb_state()));
    } else if (action === 'move') {
      if (!call('cb_move', ['number', 'number', 'number'], [payload.from, payload.to, payload.promotion || 5]))
        throw new Error('invalidMove');
      result = JSON.parse(engine.UTF8ToString(engine._cb_state()));
    } else if (action === 'undo' || action === 'redo') {
      call(action === 'undo' ? 'cb_undo' : 'cb_redo');
      result = JSON.parse(engine.UTF8ToString(engine._cb_state()));
    } else if (action === 'search') {
      if (!call('cb_load', ['string'], [payload.startFen])) throw new Error('invalidSearch');
      for (const move of payload.moves) {
        if (!call('cb_move', ['number', 'number', 'number'], [move.from, move.to, move.promotion || 5]))
          throw new Error('invalidHistory');
      }
      result = JSON.parse(engine.UTF8ToString(engine.ccall('cb_search', 'number',
        ['number', 'number'], [Math.max(1, Math.floor(payload.timeLeft)), Math.floor(payload.increment)])));
      if (result.error) throw new Error(result.error === 'Search allocation failed' ? 'allocationError' : result.error);
    } else throw new Error('Unknown worker action');
    self.postMessage({ id, result });
  } catch (error) { self.postMessage({ id, error: error.message }); }
};
