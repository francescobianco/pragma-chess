#pragma once

/// The sound of a piece set down on the board, played when a move is made:
/// the user's, the engine's in training, the opponent's online.
///
/// Played by each system's own means — PlaySound on Windows, AudioToolbox on
/// macOS, pw-play/paplay/aplay elsewhere — so the packages need no
/// multimedia library. Where none of them is there, nothing is heard.
namespace MoveSound {

/// Plays the sound without waiting for it; a new move may cut the last one short.
void play();

} // namespace MoveSound
