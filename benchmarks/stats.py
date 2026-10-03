"""Paired-game strength summaries; no engine or chess dependency."""
import math
import random


def elo_difference(score):
    """No finite estimate exists at 0% or 100%."""
    return 400 * math.log10(score / (1 - score)) if 0 < score < 1 else None


def summarize_pairs(pairs, seed=20261002, samples=2000):
    complete = [p for p in pairs if len(p) == 2 and all(x is not None for x in p)]
    values = [sum(p) / 2 for p in complete]
    games = [x for p in complete for x in p]
    result = dict(pairs=len(values), games=len(games), excluded_pairs=len(pairs)-len(values),
                  wins=games.count(1.0), draws=games.count(0.5), losses=games.count(0.0),
                  score=None, elo=None, score_ci95=None, elo_ci95=None)
    if not values:
        return result
    score = sum(values) / len(values)
    rng = random.Random(seed)
    if len(set(values)) == 1:
        # A degenerate bootstrap would misleadingly claim zero uncertainty.
        radius = math.sqrt(math.log(40) / (2 * len(values)))
        lo, hi = max(0, score-radius), min(1, score+radius)
        method = 'Hoeffding fallback on independent color-pair scores'
    else:
        means = sorted(sum(rng.choices(values, k=len(values))) / len(values)
                       for _ in range(samples))
        lo, hi = means[int(samples*.025)], means[min(samples-1, int(samples*.975))]
        method = 'percentile bootstrap resampling complete color pairs'
    result.update(score=score, elo=elo_difference(score), score_ci95=[lo, hi],
                  elo_ci95=[elo_difference(lo), elo_difference(hi)], ci_method=method)
    return result


def performance_summary(records):
    if not records:
        return {}
    import statistics
    seconds = sum(r['elapsed_ms'] for r in records) / 1000
    return dict(searches=len(records), nps=sum(r['nodes'] for r in records)/seconds,
                mean_depth=statistics.mean(r['depth'] for r in records),
                median_depth=statistics.median(r['depth'] for r in records),
                mean_depth_per_second=statistics.mean(r['depth']*1000/r['elapsed_ms'] for r in records),
                mean_elapsed_ms=statistics.mean(r['elapsed_ms'] for r in records))
