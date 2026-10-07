# Training sets

`endgames.tsv` and `tactics.tsv` are puzzles from the lichess.org puzzle
database (https://database.lichess.org/#puzzles), released under CC0
(public domain). `make-training.py` picks them — for each theme and each
band of rating, the most played among the well liked and well rated — and
writes the two files; Pragma Chess seeds them as the databases Endgame
Training (with the theoretical endgames of `app/TrainingSets`) and Tactics
Training.

    curl -O https://database.lichess.org/lichess_db_puzzle.csv.zst
    ./make-training.py lichess_db_puzzle.csv.zst
