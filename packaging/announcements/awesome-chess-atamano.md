<!-- Pull request to https://github.com/atamano/awesome-chess (DISTRIBUTING.md,
"awesome-chess lists"). Its contributing.md asks ~50 stars and 3 months, and
defers anything under 30 days old: the repository was created on 2026-09-16,
so not before 2026-10-17. Below the thresholds it takes a "Hidden Gem" only
when the PR discloses the affiliation, names the closest entries and the
practical difference, and shows quality evidence — this text does that.
Add the line at the bottom of "Desktop GUIs" (not alphabetized), commit
"Add Pragma Chess to Desktop GUIs". -->

Line (at the bottom of Desktop GUIs):

```markdown
- [Pragma Chess](https://yafb.net/pragma-chess/) - Open-source database and analysis GUI whose engine explains its evaluation on the board.
```

Title: Add Pragma Chess to Desktop GUIs

---

**Disclosure:** I am the author of Pragma Chess.

It is below the star threshold, so I submit it as a **Hidden Gem**:

- **Distinct problem.** Every GUI in the list shows the engine's evaluation
  and best line; none says *why*. Pragma Chess's Explain draws the reason on
  the board — the material that falls once exchanges are over, the reply that
  makes the difference, a short forced mate played out — judged from the
  engine's live analysis before and after the move.
- **Closest entries and the difference.** *En Croissant* and *ChessX* are
  open-source databases with engine analysis; *Lucas Chess R* trains with
  tutors. Pragma Chess is a database like the first two (PGN, lichess and
  chess.com imports, ChessBase `.cbh`/`.2cbh` files, search by position on
  millions of games) and trains with a tutor like the third, but its centre is
  the explanation of the evaluation, which none of them has.
- **Usable today, documented.** Releases for Windows, macOS and Linux:
  https://github.com/francescobianco/pragma-chess/releases — a guide per
  topic, in English and Italian: https://yafb.net/pragma-chess/en/guide/
- **Quality evidence.** MIT licensed; a unit-test suite (perft counts, PGN,
  the explanation logic replayed from recorded engine output); a public
  method for the explanation:
  https://github.com/francescobianco/pragma-chess/blob/main/docs/tech/explain-tuning.md
  and measurements on databases of up to 2.3 million games:
  https://github.com/francescobianco/pragma-chess/blob/main/docs/tech/large-databases.md
- Listed on AlternativeTo: https://alternativeto.net/software/pragma-chess/about/
