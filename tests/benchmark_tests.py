"""Tests of the measurement harness, not of chess-engine playing strength."""
import sys
from pathlib import Path
import unittest
import os
import importlib.util
import io
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'benchmarks'))
from stats import elo_difference, performance_summary, summarize_pairs


class StatisticsTests(unittest.TestCase):
    def test_elo_conversion_and_extremes(self):
        self.assertEqual(elo_difference(.5), 0)
        self.assertAlmostEqual(elo_difference(.75), 190.84850188786498)
        self.assertIsNone(elo_difference(0))
        self.assertIsNone(elo_difference(1))

    def test_color_pairs_and_unfinished_exclusion(self):
        result = summarize_pairs([[1.,0.],[.5,.5],[None,1.]], samples=100)
        self.assertEqual((result['wins'],result['draws'],result['losses']), (1,2,1))
        self.assertEqual(result['games'],4)
        self.assertEqual(result['excluded_pairs'],1)
        self.assertEqual(result['score'],.5)
        self.assertLess(result['score_ci95'][0],.5)
        self.assertGreater(result['score_ci95'][1],.5)

    def test_pair_bootstrap_is_seeded(self):
        pairs = [[1.,1.],[1.,.5],[0.,.5],[0.,0.]]
        self.assertEqual(summarize_pairs(pairs),summarize_pairs(pairs))

    def test_extreme_score_keeps_uncertainty(self):
        result = summarize_pairs([[1.,1.]]*100)
        self.assertIsNone(result['elo'])
        self.assertLess(result['score_ci95'][0],1)
        self.assertIsNone(result['elo_ci95'][1])

    def test_no_complete_games(self):
        self.assertIsNone(summarize_pairs([[None,0.]])['score'])

    def test_throughput_weights_actual_elapsed_time(self):
        result = performance_summary([dict(nodes=100,elapsed_ms=100,depth=3),
                                      dict(nodes=900,elapsed_ms=300,depth=5)])
        self.assertEqual(result['nps'],2500)
        self.assertEqual(result['mean_depth'],4)
        self.assertEqual(result['median_depth'],4)

    @unittest.skipUnless(os.name == 'nt', 'Windows delayed launcher')
    def test_delayed_launcher_refuses_incomplete_previous_results(self):
        import tempfile
        import json
        import subprocess
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            old = directory/'old'
            old.mkdir()
            (old/'metadata.json').write_text(json.dumps({'parameters':{'versions':[17],'adjusted':True}}))
            (old/'summary.json').write_text(json.dumps([{'version':17,'fixed':{'games':500}}]))
            state = directory/'state.json'
            command = ['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',
                       str(Path(__file__).resolve().parents[1]/'benchmarks/after-run.ps1'),
                       '-WaitProcessId','2147483647','-PreviousResults',str(old),
                       '-Book',str(directory/'book.bin'),'-NewResults',str(directory/'new'),
                       '-Binaries',str(directory),'-Python',sys.executable,'-StateFile',str(state)]
            result = subprocess.run(command,capture_output=True,text=True,timeout=15)
            self.assertNotEqual(result.returncode,0)
            saved = json.loads(state.read_text(encoding='utf-8-sig'))
            self.assertEqual(saved['status'],'failed')
            self.assertIn('did not finish v17',saved['detail'])
            self.assertFalse((directory/'new').exists())


@unittest.skipUnless(importlib.util.find_spec('chess'), 'Independent referee package is optional for statistics tests')
class RefereeTests(unittest.TestCase):
    def test_strength_only_run_prioritizes_latest_fixed_results(self):
        import run
        import tempfile
        import json
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            binaries = directory/'binaries'
            binaries.mkdir()
            for version in (16,17):
                (binaries/f'v{version}.exe').write_bytes(b'placeholder')
            (binaries/'build-info.json').write_text('{}')
            book = directory/'book.bin'
            book.write_bytes(bytes(16))
            labels = []
            class Stockfish:
                id = {'name':'test'}
                options = {'UCI_Elo':SimpleNamespace(min=1320,max=3190)}
                def __enter__(self): return self
                def __exit__(self,*args): pass
                def configure(self,options): pass
            def fake_match(version,opponent,fens,sf,elo,args,outputs,label):
                labels.append(label)
                result = summarize_pairs([[.5,.5]],samples=10)
                result.update(reference_elo=elo,calibrated_elo_estimate=elo,book_moves=0)
                return result
            out = directory/'results'
            argv = ['run.py','--mode','strength','--stockfish-only','--adjusted',
                    '--book',str(book),'--binaries',str(binaries),'--out',str(out),
                    '--pairs','1','--pilot-pairs','1','--pilot-rounds','1',
                    '--priority-versions','17','16','--versions','17','16']
            with patch('sys.argv',argv), patch('run.match',side_effect=fake_match), \
                 patch('run.chess.engine.SimpleEngine.popen_uci',return_value=Stockfish()):
                run.main()
            self.assertEqual(labels,['v17:sf-fixed','v16:sf-fixed','v17:sf-pilot-0',
                                     'v17:sf-adjusted','v16:sf-pilot-0','v16:sf-adjusted'])
            self.assertFalse((out/'performance.jsonl').exists())
            text = (out/'summary.md').read_text(encoding='utf-8')
            self.assertNotIn('Nodes/s',text)
            self.assertNotIn('Previous',text)
            metadata = json.loads((out/'metadata.json').read_text())
            self.assertEqual(metadata['status'],'completed')
            self.assertTrue(metadata['opening_book'])
            self.assertEqual(metadata['parameters']['increment_ms'],50)
            self.assertEqual(metadata['parameters']['versions'],[17,16])

    def test_stockfish_receives_equal_decimal_increments_for_both_colors(self):
        import run
        class FakeAdapter:
            def __init__(self, *args): pass
            def reset(self, *args): pass
            def close(self): pass
        args = SimpleNamespace(base_ms=3000,increment_ms=50,max_plies=1,match_mode='clock',binaries=None)
        for color in (run.chess.WHITE, run.chess.BLACK):
            board = run.chess.Board()
            if color == run.chess.BLACK:
                board.push_uci('e2e4')
            limits = []
            class Stockfish:
                def configure(self, options): pass
                def play(self, position, limit, **kwargs):
                    limits.append(limit)
                    return SimpleNamespace(move=next(iter(position.legal_moves)))
            white, black = ('Stockfish',17) if color else (17,'Stockfish')
            with self.subTest(color=color), patch('run.Adapter',FakeAdapter):
                outputs = {k:io.StringIO() for k in ('games','searches','pgn')}
                run.game(white,black,board.fen(),Stockfish(),2700,args,'increment-test',outputs)
                self.assertEqual(limits[0].white_inc, .05)
                self.assertEqual(limits[0].black_inc, .05)
                self.assertEqual(limits[0].white_clock, 3)
                self.assertEqual(limits[0].black_clock, 3)

    def test_adjusted_reference_is_visible_at_extreme_score(self):
        import run
        result = summarize_pairs([[1.,1.]])
        result.update(reference_elo=2850,calibrated_elo_estimate=None)
        self.assertIn('SF 2850',run.fmt_strength(result))
    def test_swapped_colors_are_scored_from_tested_version(self):
        import run
        args = SimpleNamespace(seed=42)
        with patch('run.game', side_effect=[(1.,'CHECKMATE'),(0.,'CHECKMATE')]):
            result = run.match(11,10,['unused fen'],None,None,args,{},'color-test')
        self.assertEqual(result['wins'],2)
        self.assertEqual(result['score'],1)

    def test_unfinished_pair_does_not_inflate_draws(self):
        import run
        with patch('run.game', side_effect=[(.5,'STALEMATE'),(None,'ply limit (unfinished)')]):
            result = run.match(11,10,['unused fen'],None,None,SimpleNamespace(seed=42),{},'unfinished-test')
        self.assertEqual(result['games'],0)
        self.assertEqual(result['excluded_pairs'],1)

    def test_referee_forfeit_and_ply_limit(self):
        import run
        class FakeAdapter:
            response = dict(move='a2a3',elapsed_ms=1,nodes=1,depth=1)
            def __init__(self,*args): pass
            def reset(self,*args): pass
            def close(self): pass
            def search(self,*args): return self.response
        args = SimpleNamespace(base_ms=100,increment_ms=0,max_plies=1,match_mode='clock',binaries=None)
        for response, expected, reason in [
            (dict(move='0000',elapsed_ms=1,nodes=1,depth=1),0.,'illegal move'),
            (dict(move='a2a3',elapsed_ms=101,nodes=1,depth=1),0.,'time forfeit'),
            (dict(move='a2a3',elapsed_ms=1,nodes=1,depth=1),None,'ply limit')]:
            with self.subTest(reason=reason), patch('run.Adapter',FakeAdapter):
                FakeAdapter.response = response
                outputs = {k:io.StringIO() for k in ('games','searches','pgn')}
                score, termination = run.game(11,10,run.chess.STARTING_FEN,None,None,args,'test',outputs)
                self.assertEqual(score,expected)
                self.assertTrue(termination.startswith(reason))
                self.assertIn('Termination',outputs['pgn'].getvalue())


@unittest.skipUnless(os.environ.get('CHESSBOT_BENCH_INTEGRATION'), 'Enable native adapter integration explicitly')
class AdapterIntegrationTests(unittest.TestCase):
    def test_each_version_telemetry_and_replayed_history(self):
        import chess
        from run import Adapter, ROOT
        for version in range(10,18):
            with self.subTest(version=version):
                client = Adapter(version, Path(os.environ.get('CHESSBOT_BENCH_BINARIES', ROOT/'build/benchmarks')))
                try:
                    client.reset()
                    board = chess.Board()
                    for text in ('e2e4','e7e5','g1f3'):
                        board.push_uci(text)
                    result = client.search(board,'fixed',100)
                    self.assertIn(chess.Move.from_uci(result['move']),board.legal_moves)
                    self.assertGreater(result['nodes'],0)
                    self.assertGreater(result['depth'],0)
                    self.assertGreater(result['elapsed_ms'],0)
                    self.assertGreater(result['elapsed_ms'],50)
                    client.reset()
                    terminal = chess.Board('7k/6Q1/5K2/8/8/8/8/8 b - - 0 1')
                    result = client.search(terminal,'fixed',100)
                    self.assertEqual(result['move'],'0000')
                    self.assertEqual(result['depth'],0)
                    self.assertEqual(result['nodes'],0)
                finally:
                    client.close()

    def test_book_loading_use_exit_reset_and_fixed_search(self):
        import chess
        import chess.polyglot
        import struct
        import tempfile
        from run import Adapter, ROOT
        binaries = Path(os.environ.get('CHESSBOT_BENCH_BINARIES', ROOT/'build/benchmarks'))
        with tempfile.TemporaryDirectory() as directory:
            book = Path(directory)/'test.bin'
            board = chess.Board()
            book.write_bytes(struct.pack('>QHHI', chess.polyglot.zobrist_hash(board), (12 << 6) | 28, 1, 0))
            for version in range(10,18):
                with self.subTest(version=version):
                    client = Adapter(version, binaries, book)
                    try:
                        client.reset(42)
                        first = client.search(board, 'clock', 1000, 50)
                        self.assertEqual(first['move'], 'e2e4')
                        self.assertTrue(first['book'])
                        self.assertEqual(first['nodes'], 0)
                        outside = board.copy()
                        outside.push_uci('e2e4')
                        self.assertFalse(client.search(outside, 'clock', 1000, 50)['book'])
                        self.assertFalse(client.search(board, 'clock', 1000, 50)['book'])
                        client.reset(42)
                        self.assertTrue(client.search(board, 'clock', 1000, 50)['book'])
                        fixed = client.search(board, 'fixed', 30)
                        self.assertFalse(fixed['book'])
                        self.assertGreater(fixed['nodes'], 0)
                    finally:
                        client.close()
            with self.assertRaises(RuntimeError):
                Adapter(17, binaries, Path(directory)/'missing.bin')


if __name__ == '__main__':
    unittest.main()
