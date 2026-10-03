export class GameClock {
  constructor(whiteMs, blackMs) { this.reset(whiteMs, blackMs); }
  reset(whiteMs, blackMs) { this.remaining = [whiteMs, blackMs]; this.active = null; this.anchor = 0; }
  values(now) {
    const values = [...this.remaining];
    if (this.active !== null) values[this.active] = Math.max(0, values[this.active] - Math.max(0, now - this.anchor));
    return values;
  }
  start(side, now) { this.pause(now); this.active = side; this.anchor = now; }
  pause(now) { this.remaining = this.values(now); this.active = null; this.anchor = now; }
  completeMove(side, increment, now) {
    const values = this.values(now);
    if (values[side] <= 0) return false;
    this.remaining = values;
    this.remaining[side] += increment;
    this.active = 1 - side; this.anchor = now;
    return true;
  }
}

export function clockText(ms) {
  const seconds = Math.max(0, Math.ceil(ms / 1000));
  if (ms < 10000) return `00:${(Math.ceil(Math.max(0, ms) / 100) / 10).toFixed(1).padStart(4, '0')}`;
  return `${String(Math.floor(seconds / 60)).padStart(2, '0')}:${String(seconds % 60).padStart(2, '0')}`;
}

export const INF = 300000;
export const MATE_THRESHOLD = INF - 64;
export function evaluationText(score) {
  if (Math.abs(score) >= MATE_THRESHOLD) {
    const moves = Math.ceil((INF - Math.abs(score)) / 2);
    return moves ? `${score < 0 ? '−' : '+'}M${moves}` : 'Mat';
  }
  return `${score >= 0 ? '+' : '−'}${(Math.abs(score) / 100).toFixed(2)}`;
}
export function evaluationFill(score) {
  if (Math.abs(score) >= MATE_THRESHOLD) return score > 0 ? 100 : 0;
  return Math.max(3, Math.min(97, 50 + 47 * Math.tanh(score / 500)));
}
