<!-- Show HN (DISTRIBUTING.md, "Hacker News"; POSITIONING.md, step 9: En
Croissant never had one). Submit after the 0.4.0 release, Tuesday–Thursday,
morning US Eastern time, with the site as the URL; post the first comment at
once and stay for a few hours to answer. Needs a Hacker News account. -->

Title: Show HN: Pragma Chess – open-source chess database that explains the engine

URL: https://yafb.net/pragma-chess/

---

First comment:

Hi HN, I'm Francesco. Pragma Chess is a chess database and analysis program
I've been building: C++20 and Qt 6 Widgets, MIT licensed, for Windows, macOS
and Linux, with an Android app in testing.

Why: the club players I know either pay for ChessBase or juggle a handful of
websites. I wanted a free desktop program that keeps all your games in one
place, does less than ChessBase, and explains more.

The part I'm proudest of is Explain. Engines give a number and a line; they
don't say why. Press E on a move and Pragma Chess draws the reason on the
board — the material that falls, the reply that makes the difference, a short
forced mate played out. It doesn't start a second engine: every line of the
live analysis is fed to a small program that compares the search after the
move with the one before, replays the principal variation to where the
evaluation becomes concrete (material won once exchanges and checks are over,
or a mate), and only draws an arrow once it has held for 3 of the last 4
depths. That judgement is written in a tiny BASIC dialect instead of C++, so
the desktop app, the command line tool and the phone run the same files, and a
wrong explanation becomes a recorded test that every client replays.

The other hard problem is the database itself: on 2.3 million lichess games
the window is ready in under 6 s and about 0.5 GB of memory, with search by
position (Polyglot keys, an index built on every core and kept on disk).

Also: any UCI engine (Stockfish bundled, capped to a share of the CPU so your
laptop stays cool), Polyglot books you can edit, training with a tutor, play
on lichess.org and freechess.org, folder sync over FTP/WebDAV/Git, English and
Italian.

Source: https://github.com/francescobianco/pragma-chess

It's young (0.4.0). I'd especially like to hear from Windows and macOS users,
and about any position where Explain says something wrong.
