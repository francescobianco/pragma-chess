#!/usr/bin/env python3
"""Builds the training sets Pragma Chess ships from the lichess puzzle
database (https://database.lichess.org/#puzzles, CC0):

    curl -O https://database.lichess.org/lichess_db_puzzle.csv.zst
    ./make-training.py lichess_db_puzzle.csv.zst

writes endgames.tsv and tactics.tsv here: for each theme and each band of
rating, the most played puzzles among the well liked and well rated ones,
so that every theme has easy and hard ones. Columns: id, FEN (before the
opponent's move), moves (UCI: the opponent's move, then the solution),
rating, themes, game URL.
"""
import csv, io, subprocess, sys

ENDGAME_THEMES = ["pawnEndgame", "rookEndgame", "bishopEndgame", "knightEndgame",
                  "queenEndgame", "queenRookEndgame"]
TACTIC_THEMES = ["pin", "skewer", "fork", "discoveredAttack", "doubleCheck", "deflection",
                 "attraction", "xRayAttack", "interference", "intermezzo", "clearance",
                 "capturingDefender", "hangingPiece", "trappedPiece", "sacrifice",
                 "backRankMate", "smotheredMate", "quietMove", "zugzwang", "promotion"]
BANDS = [(0, 1200), (1200, 1500), (1500, 1800), (1800, 2100), (2100, 2400), (2400, 4000)]
ENDGAMES_PER_BAND = 60
TACTICS_PER_BAND = 15


def main(path):
    stream = subprocess.Popen(["zstdcat", path], stdout=subprocess.PIPE)
    reader = csv.DictReader(io.TextIOWrapper(stream.stdout, encoding="utf-8"))
    # theme -> band -> [(plays, row)]
    candidates = {theme: {band: [] for band in BANDS} for theme in ENDGAME_THEMES + TACTIC_THEMES}
    for row in reader:
        if int(row["Popularity"]) < 85 or int(row["NbPlays"]) < 1000 or int(row["RatingDeviation"]) > 80:
            continue
        rating = int(row["Rating"])
        themes = row["Themes"].split()
        band = next(b for b in BANDS if b[0] <= rating < b[1])
        for theme in themes:
            if theme in candidates:
                bucket = candidates[theme][band]
                bucket.append((int(row["NbPlays"]), row))
                if len(bucket) > 400:  # keep memory small: the most played
                    bucket.sort(key=lambda entry: -entry[0])
                    del bucket[200:]
    stream.wait()

    def pick(themes, per_band):
        chosen, seen = [], set()
        for theme in themes:
            for band in BANDS:
                bucket = sorted(candidates[theme][band], key=lambda entry: (-entry[0], entry[1]["PuzzleId"]))
                taken = 0
                for _, row in bucket:
                    if taken >= per_band:
                        break
                    if row["PuzzleId"] in seen:
                        continue
                    seen.add(row["PuzzleId"])
                    chosen.append(row)
                    taken += 1
        return chosen

    for name, themes, per_band in (("endgames.tsv", ENDGAME_THEMES, ENDGAMES_PER_BAND),
                                   ("tactics.tsv", TACTIC_THEMES, TACTICS_PER_BAND)):
        rows = pick(themes, per_band)
        with open(name, "w", encoding="utf-8") as out:
            out.write("# From the lichess puzzle database (database.lichess.org, CC0): make-training.py\n")
            for row in rows:
                out.write("\t".join([row["PuzzleId"], row["FEN"], row["Moves"], row["Rating"],
                                     row["Themes"], row["GameUrl"]]) + "\n")
        print(name, len(rows))


if __name__ == "__main__":
    main(sys.argv[1])
