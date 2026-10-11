#!/usr/bin/env python3
"""Calibrates the estimated Elo of app/PlayStrength on rated games.

Reads lichess PGN files (the stress corpus, docs/tech/large-databases.md),
keeps the games the lichess server analysed — a [%eval] after every move —
and, for each player of each game, the same measures PlayStrength takes:
the loss of each move for the side that moved, evaluations bounded to
±1000 centipawns, moves in a decided position left out. Then fits the
player's rating against those measures and prints the fit, its error, and
the table by rating band. docs/tech/game-evaluation.md tells what it found.

    scripts/calibrate-play-strength.py ~/.cache/pragma-chess-stress/lichess_db_standard_rated_2013-0*.pgn
"""

import math
import re
import sys

import numpy as np

BOUND = 1000
MIN_MOVES = 8
EVAL = re.compile(r"\[%eval (#?-?[\d.]+)\]")
MOVE_NUMBER = re.compile(r"\d+\.(\.\.)?")


def centipawns(text):
    """A [%eval] as White's centipawns; a mate as a bounded large advantage."""
    if text.startswith("#"):
        mate = int(text[1:])
        return (10000 - 10 * abs(mate)) * (1 if mate > 0 else -1)
    return round(float(text) * 100)


def games(paths):
    """(white elo, black elo, [evaluation after each ply]) of analysed games."""
    for path in paths:
        with open(path, encoding="utf-8", errors="replace") as source:
            tags, moves = {}, []
            for line in source:
                if line.startswith("["):
                    if moves:
                        yield from finish(tags, " ".join(moves))
                        tags, moves = {}, []
                    key, _, value = line[1:].partition(" ")
                    tags[key] = value.strip().rstrip("]").strip('"')
                elif line.strip():
                    moves.append(line.strip())
            if moves:
                yield from finish(tags, " ".join(moves))


def finish(tags, text):
    if "%eval" not in text or not tags.get("WhiteElo", "?").isdigit() or not tags.get("BlackElo", "?").isdigit():
        return
    if "Classical" not in tags.get("Event", "") and "Rapid" not in tags.get("Event", ""):
        return  # Bullet and blitz are played differently: not the games Pragma Chess evaluates first.
    evaluations = [centipawns(found) for found in EVAL.findall(text)]
    # A game whose last move gave mate has no [%eval] after it.
    if text.rstrip().endswith("#") or "# {" in text:
        pass
    yield int(tags["WhiteElo"]), int(tags["BlackElo"]), evaluations


def measures(evaluations, side):
    """Loss of each judged move of `side` (0 White, 1 Black), as PlayStrength judges them."""
    sign = 1 if side == 0 else -1
    # The start, before the first move: about White's usual edge.
    evaluations = [20] + evaluations
    losses = []
    for ply in range(1 + side, len(evaluations), 2):
        was = max(-BOUND, min(BOUND, evaluations[ply - 1] * sign))
        now = max(-BOUND, min(BOUND, evaluations[ply] * sign))
        if (was >= BOUND and now >= BOUND) or (was <= -BOUND and now <= -BOUND):
            continue
        losses.append(max(0, was - now))
    return losses


def main(paths):
    rows = []
    for white, black, evaluations in games(paths):
        for side, elo in ((0, white), (1, black)):
            losses = measures(evaluations, side)
            if len(losses) < MIN_MOVES:
                continue
            n = len(losses)
            acpl = sum(losses) / n
            mistakes = sum(1 for loss in losses if loss >= 100) / n
            blunders = sum(1 for loss in losses if loss >= 300) / n
            rows.append((elo, acpl, mistakes, blunders, n))
    data = np.array(rows, dtype=float)
    print(f"{len(data)} players' games, rating {int(data[:, 0].min())}–{int(data[:, 0].max())}, "
          f"mean {data[:, 0].mean():.0f}, sd {data[:, 0].std():.0f}")
    elo, acpl, mistakes, blunders, moves = data.T

    def report(name, predicted):
        error = predicted - elo
        print(f"  {name}: mean absolute error {np.abs(error).mean():.0f}, rms {math.sqrt((error ** 2).mean()):.0f}, "
              f"correlation {np.corrcoef(predicted, elo)[0, 1]:.3f}")

    report("everyone the mean rating (baseline)", np.full_like(elo, elo.mean()))
    report("current curve 2850·e^(−0.01·ACPL)", np.clip(2850 * np.exp(-0.01 * acpl), 100, 2850))

    # Elo = a + b·ln(ACPL): the loss spreads over orders of magnitude.
    log_acpl = np.log(np.maximum(acpl, 1))
    b, a = np.polyfit(log_acpl, elo, 1)
    print(f"fit 1 (regression): Elo = {a:.0f} {b:+.0f}·ln(ACPL)")
    report("fit 1", a + b * log_acpl)

    # A regression draws every prediction towards the mean (one game says
    # little): weak play would never measure weak. The other way round: the
    # typical loss of each rating band, and the curve through those points,
    # so a player who loses what 1100s lose is called 1100.
    centres, medians = [], []
    for low in range(800, 2400, 100):
        band = (elo >= low) & (elo < low + 100)
        if band.sum() >= 30:
            centres.append(low + 50)
            medians.append(np.median(log_acpl[band]))
    b, a = np.polyfit(medians, centres, 1)
    print(f"fit 3 (by band, used): Elo = {a:.0f} {b:+.0f}·ln(ACPL)")
    report("fit 3", a + b * log_acpl)
    for acpl_value in (10, 20, 30, 50, 70, 100, 150, 200, 300):
        print(f"    ACPL {acpl_value:3d} → {a + b * math.log(acpl_value):5.0f}")

    # Elo = a + b·ln(ACPL) + c·mistakes + d·blunders (rates per move).
    features = np.column_stack([np.ones_like(elo), log_acpl, mistakes, blunders])
    coefficients, *_ = np.linalg.lstsq(features, elo, rcond=None)
    print("fit 2: Elo = {:.0f} {:+.0f}·ln(ACPL) {:+.0f}·mistakes {:+.0f}·blunders".format(*coefficients))
    report("fit 2", features @ coefficients)

    print("by rating band (fit 2's mean prediction, and the measures, median ACPL):")
    for low in range(800, 2400, 200):
        band = (elo >= low) & (elo < low + 200)
        if band.sum() < 20:
            continue
        print(f"  {low}–{low + 199}: {int(band.sum()):5d} players, predicted {np.mean((features @ coefficients)[band]):5.0f}, "
              f"ACPL {np.median(acpl[band]):5.1f}, mistakes {np.mean(mistakes[band]):.3f}, "
              f"blunders {np.mean(blunders[band]):.3f}")


if __name__ == "__main__":
    main(sys.argv[1:])
