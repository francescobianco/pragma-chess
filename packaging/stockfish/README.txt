Stockfish in Pragma Chess
=========================

Version: Stockfish @VERSION@, modified (small network only)

Pragma Chess includes Stockfish, a free and open source UCI chess engine, as
its default engine. It is a separate program: Pragma Chess starts it and talks
to it through the UCI protocol.

Stockfish is developed by the Stockfish developers (see AUTHORS) and licensed
under the GNU General Public License version 3 (see Copying.txt). Pragma Chess
itself is licensed under the MIT License.

This is NOT the official release build. It is built by Pragma Chess from the
source of https://github.com/official-stockfish/Stockfish/tree/@TAG@
with one change, so that it is about 4 MB instead of 100: it embeds and uses
only the small evaluation network (@NET@) instead of the big one. It is
weaker than the official Stockfish @VERSION@, and still far stronger than any
human player. The change is the file PRAGMA-CHESS.patch in the source.

Source code: the complete corresponding source of this build (Stockfish's
source with the change applied and the network it embeds) is attached to
every Pragma Chess release as stockfish-@TAG@-pragma-source.tar.gz
(https://github.com/francescobianco/pragma-chess/releases). The script that
builds it is scripts/build-stockfish.sh in Pragma Chess's own repository.
