"""Serial native performance and paired strength measurements for v10-v17."""
import argparse
from collections import Counter
import contextlib
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import queue
import random
import subprocess
import threading
import time

import chess
import chess.engine
import chess.pgn

from stats import performance_summary, summarize_pairs

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_fingerprints():
    files = {}
    for directory, folders, names in os.walk(ROOT):
        folders[:] = [d for d in folders if d not in ('build', '.git', '__pycache__')]
        for name in names:
            path = Path(directory)/name
            if path.suffix in ('.c','.h','.py','.ps1') and name not in ('test.c','Compare.c','v18at.h'):
                files[str(path.relative_to(ROOT))] = digest(path)
    return files


def positions(path):
    result = []
    seen = set()
    for number, raw in enumerate(Path(path).read_text(encoding='utf-8-sig').splitlines(), 1):
        fen = raw.strip()
        if not fen or fen.startswith('#'):
            continue
        board = chess.Board(fen)
        if not board.is_valid() or board.is_game_over(claim_draw=True):
            raise ValueError(f'{path}:{number}: invalid or terminal starting position')
        key = ' '.join(board.fen().split()[:4])
        if key not in seen:
            result.append(board.fen())
            seen.add(key)
    if not result:
        raise ValueError(f'Empty position dataset: {path}')
    return result


class Adapter:
    def __init__(self, version, binaries, book=None):
        self.name = f'v{version}'
        path = binaries / f'{self.name}.exe'
        self.process = subprocess.Popen([str(path)], cwd=ROOT, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        text=True, bufsize=1,
                                        env=dict(os.environ,
                                                 CHESSBOT_BOOK=str(book or ROOT/'build/no-benchmark-book.bin'),
                                                 CHESSBOT_BENCH_BOOK='1' if book else '0'))
        self.responses = queue.Queue()
        self.diagnostics = []
        def read_stdout():
            for line in self.process.stdout:
                if line.startswith('{'):
                    self.responses.put(line)
                else:
                    self.diagnostics.append(line.strip())
            self.responses.put(None)
        def read_stderr():
            for line in self.process.stderr:
                self.diagnostics.append(line.strip())
        threading.Thread(target=read_stdout, daemon=True).start()
        threading.Thread(target=read_stderr, daemon=True).start()
        if book:
            try:
                info = self.command('info')
                if not info.get('book_enabled') or info.get('book_entries', 0) <= 0:
                    raise RuntimeError(f'{self.name}: requested book is not enabled')
            except Exception:
                self.close()
                raise

    def command(self, text, timeout=10):
        try:
            self.process.stdin.write(text + '\n')
            self.process.stdin.flush()
            line = self.responses.get(timeout=timeout)
        except (queue.Empty, BrokenPipeError, OSError) as error:
            raise RuntimeError(f'{self.name}: timeout or terminated adapter') from error
        if line is None:
            raise RuntimeError(f'{self.name}: exited ({self.process.poll()}): {self.diagnostics[-5:]}')
        reply = json.loads(line)
        if reply.get('error') or reply.get('ok') is False:
            raise RuntimeError(f'{self.name}: {reply}')
        return reply

    def reset(self, seed=None):
        self.command('reset' if seed is None else f'reset {seed}')

    def position(self, board):
        initial = board.root().fen(en_passant='fen')
        moves = ' '.join(m.uci() for m in board.move_stack)
        self.command(f'position {initial}|{moves}')

    def search(self, board, mode, budget, increment=0):
        self.position(board)
        return self.command(f'search {mode} {max(1, int(budget))} {increment}',
                            timeout=budget/1000 + 10)

    def close(self):
        if self.process.poll() is None:
            try:
                self.process.stdin.write('quit\n'); self.process.stdin.flush()
                self.process.wait(timeout=2)
            except (OSError, subprocess.TimeoutExpired):
                self.process.kill(); self.process.wait()
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            # Closing a buffered stdin can flush into an already terminated Windows pipe.
            with contextlib.suppress(OSError):
                stream.close()


def game(a, b, fen, sf, sf_elo, args, game_id, outputs):
    """Independent referee. Search wall time is charged; setup/IPC is excluded."""
    board = chess.Board(fen)
    clocks = {chess.WHITE: float(args.base_ms), chess.BLACK: float(args.base_ms)}
    engines = {chess.WHITE: a, chess.BLACK: b}
    node = pgn = chess.pgn.Game()
    pgn.setup(board)
    pgn.headers.update(White=str(a), Black=str(b), Event='ChessBot version metrics',
                       Date=datetime.date.today().strftime('%Y.%m.%d'),
                       TimeControl=f'{args.base_ms/1000}+{args.increment_ms/1000}',
                       StockfishElo=str(sf_elo) if sf_elo else '-')
    winner = None
    reason = 'ply limit (unfinished)'
    finished = False
    book_moves = Counter()
    book = getattr(args, 'book', None)
    seed_text = f"{getattr(args, 'seed', 1)}:{game_id.rsplit(':', 1)[0]}"
    book_seed = int.from_bytes(hashlib.sha256(seed_text.encode()).digest()[:4], 'little')
    pgn.headers['OpeningBook'] = Path(book).name if book else 'none'
    pgn.headers['BookSeed'] = str(book_seed)
    with contextlib.ExitStack() as stack:
        clients = {}
        for engine in (a, b):
            if engine != 'Stockfish' and engine not in clients:
                client = Adapter(engine, args.binaries, book)
                stack.callback(client.close)
                client.reset(book_seed if book else None)
                clients[engine] = client
        if sf_elo is not None:
            sf.configure({'UCI_Elo': sf_elo})
        token = object()  # python-chess sends ucinewgame for this game.
        for ply in range(args.max_plies):
            outcome = board.outcome(claim_draw=True)
            if outcome:
                winner, reason, finished = outcome.winner, outcome.termination.name, True
                break
            color = board.turn
            engine = engines[color]
            if engine == 'Stockfish':
                limit = (chess.engine.Limit(time=args.move_ms/1000) if args.match_mode == 'fixed'
                         else chess.engine.Limit(white_clock=clocks[chess.WHITE]/1000,
                                                 black_clock=clocks[chess.BLACK]/1000,
                                                 white_inc=args.increment_ms/1000,
                                                 black_inc=args.increment_ms/1000))
                start = time.perf_counter()
                # A Stockfish protocol failure aborts the run, not a fabricated game result.
                move = sf.play(board, limit, game=token).move
                elapsed = (time.perf_counter()-start)*1000
            else:
                budget = args.move_ms if args.match_mode == 'fixed' else clocks[color]
                try:
                    reply = clients[engine].search(board, args.match_mode, budget, args.increment_ms)
                    elapsed = reply['elapsed_ms']
                    move = chess.Move.from_uci(reply['move'])
                    if reply.get('book'):
                        book_moves[str(engine)] += 1
                except (RuntimeError, ValueError) as error:
                    winner, reason, finished = not color, f'adapter failure: {error}', True
                    break
                outputs['searches'].write(json.dumps(dict(game=game_id, ply=ply, version=engine,
                                                          budget_ms=budget, **reply))+'\n')
            if args.match_mode == 'clock':
                clocks[color] -= elapsed
                if clocks[color] <= 0:
                    winner, reason, finished = not color, f'time forfeit: {engine}', True
                    break
                clocks[color] += args.increment_ms
            if move not in board.legal_moves:
                winner, reason, finished = not color, f'illegal move: {engine} {move}', True
                break
            board.push(move)
            node = node.add_variation(move)
        else:
            outcome = board.outcome(claim_draw=True)
            if outcome:
                winner, reason, finished = outcome.winner, outcome.termination.name, True
        result = '*' if not finished else '1/2-1/2' if winner is None else '1-0' if winner else '0-1'
        pgn.headers['Result'] = result
        pgn.headers['Termination'] = reason
        outputs['pgn'].write(str(pgn)+'\n\n'); outputs['pgn'].flush()
        record = dict(id=game_id, white=a, black=b, fen=fen, sf_elo=sf_elo,
                      result=result, reason=reason, plies=len(board.move_stack),
                      book_moves=dict(book_moves), book_seed=book_seed if book else None)
        for engine, count in book_moves.items():
            if 'book_counts' in outputs:
                outputs['book_counts'][engine] += count
        outputs['games'].write(json.dumps(record)+'\n'); outputs['games'].flush()
        score_white = None if not finished else 0.5 if winner is None else float(winner)
        return score_white, reason


def match(version, opponent, fens, sf, elo, args, outputs, label):
    pairs = []
    terminations = Counter()
    book_before = outputs.get('book_counts', {}).get(str(version), 0)
    for index, fen in enumerate(fens):
        pair = []
        for swap in (False, True):
            white, black = (opponent, version) if swap else (version, opponent)
            result, reason = game(white, black, fen, sf, elo, args, f'{label}:{index}:{int(swap)}', outputs)
            terminations[reason.split(':',1)[0]] += 1
            pair.append(None if result is None else 1-result if swap else result)
        pairs.append(pair)
        print(f'{label}: {index+1}/{len(fens)} color pairs', flush=True)
    summary = summarize_pairs(pairs, args.seed)
    summary['terminations'] = dict(terminations)
    summary['reference_elo'] = elo
    summary['book_moves'] = outputs.get('book_counts', {}).get(str(version), 0) - book_before
    summary['calibrated_elo_estimate'] = elo+summary['elo'] if elo and summary['elo'] is not None else None
    return summary


def fmt_strength(s):
    if not s or s['score'] is None:
        return '—'
    elo = 'unbounded' if s['elo'] is None else f"{s['elo']:+.0f}"
    lo, hi = s['elo_ci95']
    interval = f"{'−∞' if lo is None else f'{lo:+.0f}'}…{'+∞' if hi is None else f'{hi:+.0f}'}"
    estimate = s['calibrated_elo_estimate']
    reference = f"; SF {s['reference_elo']}" if s['reference_elo'] is not None else ''
    if estimate is not None:
        reference += f"; estimate {estimate:.0f}"
    return f"{s['wins']}/{s['draws']}/{s['losses']}; {s['score']:.1%}; Δ {elo} [{interval}]{reference}"


def report(out, rows):
    has_performance = any('performance' in row for row in rows)
    has_previous = any('previous' in row for row in rows)
    text = ['# Version metrics', '', 'W/D/L; points%; ΔElo [approximate 95% interval].',
            'SF-calibrated estimates are specific to this protocol, not tournament ratings.', '',
            '| Version | ' + ('Nodes/s | Mean depth | Median depth | Depth/s* | ' if has_performance else '') +
            'SF fixed | ' + ('Previous | ' if has_previous else '') + 'SF adjusted | Book moves (fixed / adjusted) |',
            '| --- | ' + ('---: | ---: | ---: | ---: | ' if has_performance else '') +
            '--- | ' + ('--- | ' if has_previous else '') + '--- | ---: |']
    flat = []
    for row in rows:
        p = row.get('performance', {})
        perf_cells = (f"{p['nps']:,.0f} | {p['mean_depth']:.2f} | {p['median_depth']:.1f} | "
                      f"{p['mean_depth_per_second']:.1f}" if p else '— | — | — | —')
        book_cells = ' / '.join(str(row[name]['book_moves']) if name in row else '—'
                                for name in ('fixed', 'adjusted'))
        text.append(f"| v{row['version']} | " + (f'{perf_cells} | ' if has_performance else '') +
                    f"{fmt_strength(row.get('fixed'))} | " +
                    (f"{fmt_strength(row.get('previous'))} | " if has_previous else '') +
                    f"{fmt_strength(row.get('adjusted'))} | {book_cells} |")
        entry = dict(version=row['version'], **p)
        for name in ('fixed', 'previous', 'adjusted'):
            for key, value in row.get(name, {}).items():
                entry[f'{name}_{key}'] = json.dumps(value) if isinstance(value, (list,dict)) else value
        flat.append(entry)
    if has_performance:
        text += ['', '*Depth/s is nonlinear and affected by pruning; compare depth at the same budget.']
    text += ['', 'Unfinished color pairs are excluded from strength summaries.',
             'Adjusted-SF tuning games are excluded from the final rating sample. See metadata.json and raw records.', '']
    (out/'summary.md').write_text('\n'.join(text), encoding='utf-8')
    (out/'summary.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
    fields = list(dict.fromkeys(key for row in flat for key in row))
    with (out/'summary.csv').open('w', newline='', encoding='utf-8') as handle:
        writer = csv.DictWriter(handle, fields); writer.writeheader(); writer.writerows(flat)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--versions', nargs='+', type=int, choices=range(10,18), default=list(range(10,18)))
    parser.add_argument('--mode', choices=['performance', 'strength', 'all'], default='performance')
    parser.add_argument('--binaries', type=Path, default=ROOT/'build/benchmarks')
    parser.add_argument('--out', type=Path)
    parser.add_argument('--performance-fens', type=Path, default=ROOT/'benchmarks/positions.txt')
    parser.add_argument('--openings', type=Path, default=ROOT/'test_pos_dataset_opening.txt')
    parser.add_argument('--positions', type=int, default=8)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--budget-ms', type=int, default=1000)
    parser.add_argument('--pairs', type=int, default=250)
    parser.add_argument('--base-ms', type=int, default=3000)
    parser.add_argument('--increment-ms', type=int, default=50)
    parser.add_argument('--match-mode', choices=['clock','fixed'], default='clock')
    parser.add_argument('--move-ms', type=int, default=100)
    parser.add_argument('--max-plies', type=int, default=512)
    parser.add_argument('--stockfish', type=Path, default=ROOT/'stockfish.exe')
    parser.add_argument('--sf-elo', type=int, default=2700)
    parser.add_argument('--sf-hash-mb', type=int, default=32)
    parser.add_argument('--adjusted', action='store_true')
    parser.add_argument('--stockfish-only', action='store_true', help='Skip comparisons against predecessor versions')
    parser.add_argument('--priority-versions', nargs='+', type=int, default=[],
                        help='Publish fixed-SF results for these versions before their adjusted-SF matches')
    parser.add_argument('--book', type=Path, help='External Polyglot book for clock matches (disabled by default); performance stays book-free')
    parser.add_argument('--pilot-pairs', type=int, default=10)
    parser.add_argument('--pilot-rounds', type=int, default=3)
    parser.add_argument('--seed', type=int, default=20261002)
    args = parser.parse_args()
    for key in ('positions','repeats','budget_ms','pairs','base_ms','move_ms','max_plies',
                'sf_hash_mb','pilot_pairs','pilot_rounds'):
        if getattr(args,key) <= 0:
            parser.error(f'{key} must be positive')
    if args.increment_ms < 0 or args.max_plies > 1024 or args.budget_ms > 60000:
        parser.error('Invalid increment, max_plies (<=1024) or benchmark budget (<=60000 ms)')
    args.versions = list(dict.fromkeys(args.versions))
    args.priority_versions = list(dict.fromkeys(args.priority_versions))
    if any(v not in args.versions for v in args.priority_versions):
        parser.error('Priority versions must be included in --versions')
    if args.book:
        args.book = args.book.resolve()
        if not args.book.is_file() or args.book.stat().st_size == 0 or args.book.stat().st_size % 16:
            parser.error('--book must be a nonempty Polyglot file with 16-byte entries')
        if args.match_mode != 'clock':
            parser.error('Opening-book matches use clock mode; fixed mode is reserved for book-free searches')
    args.binaries = args.binaries.resolve()
    out = args.out or ROOT/'build/metrics'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    out.mkdir(parents=True, exist_ok=False)
    rows = [dict(version=v) for v in args.versions]
    metadata = dict(created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    platform=platform.platform(), processor=platform.processor(), python=platform.python_version(),
                    chess=chess.__version__, parameters={k:str(v) if isinstance(v,Path) else v for k,v in vars(args).items()},
                    build_flags='-std=gnu11 -O3 -Wall -Wextra; no -march=native',
                    engine_hash_bytes=0x200000*16, serial=True, opening_book=bool(args.book), status='running',
                    source_sha256=source_fingerprints())
    needed = set(args.versions)
    if args.book:
        metadata['book'] = dict(path=str(args.book), sha256=digest(args.book),
                                entries=args.book.stat().st_size//16, performance_enabled=False,
                                seed_policy='SHA256(run seed and color-pair ID), first 32 bits; same seed for both colors')
    if args.mode != 'performance' and not args.stockfish_only:
        needed.update(v-1 for v in args.versions if v>10)
    metadata['binary_sha256'] = {f'v{v}':digest(args.binaries/f'v{v}.exe') for v in sorted(needed)}
    metadata['compiler'] = json.loads((args.binaries/'build-info.json').read_text(encoding='utf-8-sig'))
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2), encoding='utf-8')
    with contextlib.ExitStack() as stack:
        outputs = {key:stack.enter_context((out/name).open('w',encoding='utf-8')) for key,name in
                   [('games','games.jsonl'),('searches','game-searches.jsonl'),('pgn','games.pgn')]}
        outputs['book_counts'] = Counter()
        if args.mode != 'strength':
            fens = positions(args.performance_fens)[:args.positions]
            metadata['performance_dataset_sha256'] = digest(args.performance_fens)
            raw = stack.enter_context((out/'performance.jsonl').open('w', encoding='utf-8'))
            for row in rows:
                records = []
                client = Adapter(row['version'], args.binaries)
                try:
                    # Warm up code/data without using a measured TT state.
                    client.search(chess.Board(fens[0]), 'fixed', min(100,args.budget_ms))
                    for repeat in range(args.repeats):
                        for index, fen in enumerate(fens):
                            client.reset()
                            board = chess.Board(fen)
                            record = client.search(board,'fixed',args.budget_ms)
                            if chess.Move.from_uci(record['move']) not in board.legal_moves:
                                raise RuntimeError(f'Illegal benchmark move: v{row["version"]}, {fen}')
                            if record['nodes'] < 0 or record['elapsed_ms'] <= 0:
                                raise RuntimeError('Invalid measurement counter/time')
                            record.update(version=row['version'],position=index,fen=fen,repeat=repeat,budget_ms=args.budget_ms)
                            records.append(record); raw.write(json.dumps(record)+'\n'); raw.flush()
                    row['performance'] = performance_summary(records)
                    print(f"v{row['version']}: {row['performance']['nps']:,.0f} nodes/s", flush=True)
                finally:
                    client.close()
                report(out, rows)
        if args.mode != 'performance':
            fens = positions(args.openings)
            random.Random(args.seed).shuffle(fens)
            tuning_count = args.pilot_pairs * args.pilot_rounds if args.adjusted else 0
            if len(fens) < args.pairs + tuning_count:
                raise ValueError('Not enough unique openings for disjoint tuning and measurement sets')
            final_fens, pilot_fens = fens[tuning_count:tuning_count+args.pairs], fens[:tuning_count]
            sf = stack.enter_context(chess.engine.SimpleEngine.popen_uci(str(args.stockfish.resolve()), timeout=10))
            option = sf.options['UCI_Elo']
            if not option.min <= args.sf_elo <= option.max:
                raise ValueError(f'Stockfish Elo must be in [{option.min}, {option.max}]')
            sf.configure({'Threads':1,'Hash':args.sf_hash_mb,'UCI_LimitStrength':True})
            metadata.update(stockfish_id=sf.id, stockfish_sha256=digest(args.stockfish),
                            stockfish_elo_range=[option.min,option.max], openings_sha256=digest(args.openings),
                            final_fens=final_fens, pilot_fens=pilot_fens)
            (out/'metadata.json').write_text(json.dumps(metadata,indent=2), encoding='utf-8')
            def fixed_matches(row):
                v = row['version']
                row['fixed'] = match(v,'Stockfish',final_fens,sf,args.sf_elo,args,outputs,f'v{v}:sf-fixed')
                report(out, rows)
                if v>10 and not args.stockfish_only:
                    row['previous'] = match(v,v-1,final_fens,None,None,args,outputs,f'v{v}:previous')
                    report(out, rows)

            def adjusted_matches(row):
                v = row['version']
                if args.adjusted:
                    elo = args.sf_elo
                    row['pilot'] = []
                    for round_index in range(args.pilot_rounds):
                        batch = pilot_fens[round_index*args.pilot_pairs:(round_index+1)*args.pilot_pairs]
                        pilot = match(v,'Stockfish',batch,sf,elo,args,outputs,f'v{v}:sf-pilot-{round_index}')
                        row['pilot'].append(pilot)
                        if pilot['score'] is None:
                            raise RuntimeError('No completed pilot pairs')
                        adjustment = 150 if pilot['elo'] is None and pilot['score']>0.5 else -150 if pilot['elo'] is None else round(pilot['elo'])
                        elo = max(option.min,min(option.max,elo+max(-150,min(150,adjustment))))
                    row['adjusted'] = match(v,'Stockfish',final_fens,sf,elo,args,outputs,f'v{v}:sf-adjusted')
                report(out,rows)
            priority = [next(row for row in rows if row['version'] == v) for v in args.priority_versions]
            for row in priority:
                fixed_matches(row)
            for row in priority:
                adjusted_matches(row)
            for row in rows:
                if row not in priority:
                    fixed_matches(row)
                    adjusted_matches(row)
        metadata.update(status='completed', completed_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
        (out/'metadata.json').write_text(json.dumps(metadata,indent=2), encoding='utf-8')
    report(out,rows)
    print(f'Results: {out}', flush=True)


if __name__ == '__main__':
    main()
