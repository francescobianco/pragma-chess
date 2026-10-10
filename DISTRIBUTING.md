# Distributing Pragma Chess

How Pragma Chess reaches the people it is for — club players, coaches,
chess programmers, Linux users — through channels that welcome free
software, one honest post at a time. This is the plan and its log: tick a
line when it is done, with the date and the link, so nobody posts twice.

Where we stand among the other free programs, and what to learn from the
way En Croissant grew, is in [POSITIONING.md](POSITIONING.md) (in Italian).

The messages are in [Messages](#messages); use them as they are or shorten
them, in the language of the place. Everything links to the site,
<https://yafb.net/pragma-chess/>, which speaks English and Italian and has
the downloads.

## Principles

- **One post per place, then listen.** Reply to every comment and open an
  issue for every problem reported, with a thank-you. Never repost the same
  thing in the same place.
- **Say what it is.** Version 0.3.0 (the next one will bring freechess.org
  and large databases), Windows/macOS/Linux, MIT; the Android app is in
  testing. No comparisons that talk down other programs; ChessBase,
  Scid and ChessX are mentioned only to place Pragma Chess among them.
- **Show, do not claim.** Lead with the Opera-game screenshot of Explain and
  a 30–60 s screen recording; the hook is Explain, then training with the
  tutor and the book, then playing online from the same board.
- **Follow the house rules.** Every community below has them (self-promotion
  days, flairs, "Show" prefixes, disclosure of being the author). Read
  before posting, say "I made this".
- **Keep the links stable.** Site, releases/latest, the repository. The
  download links of the README never change between versions.

## 1. Our own places (done or doing)

| Channel | What | Status |
|---|---|---|
| Web site | <https://yafb.net/pragma-chess/> — EN/IT, screenshots, download, supporters (generated from `site/`, see AGENTS.md) | ✅ live |
| GitHub repository | Description, topics, homepage pointing at the site, releases with notes, README with the site and the claim | ✅ 2026-10-04 (homepage → site, README) |
| GitHub social preview | `packaging/assets/social-preview.png` (1280×640: the hero screenshot with the logo, the name and the claim; `make-social-preview.py` draws it again): upload it in Settings ▸ Social preview | ✅ image 2026-10-10 · ☐ upload (web UI) |
| GitHub Discussions | Turned on, with GitHub's categories (Announcements, General, Ideas, Polls, Q&A, Show and tell); linked from the site's Support section, the README and the application (Help ▸ Questions and Ideas…); a welcome post in English and Italian, [#2](https://github.com/francescobianco/pragma-chess/discussions/2) — pin it from the web UI | ✅ on 2026-10-04 · ✅ links, welcome 2026-10-11 · ☐ pin |
| Guide on the site | The application's guide, a page per topic: <https://yafb.net/pragma-chess/en/guide/> (and `it/guide/`) | ✅ 2026-10-11 |
| Update check | Help ▸ Check for Updates… and once a day; each release's `version.json` (also attached to 0.3.0) counts the copies in use | ✅ 2026-10-11, in the next release |
| GitHub issues | Every issue answered, with a thank-you; the first from a stranger: [#1](https://github.com/francescobianco/pragma-chess/issues/1) (2026-10-06, in Russian: qrcodegen from the system's packages) — done in the code and answered (in Russian and English), closed 2026-10-10 | ✅ #1 |
| Releases | Each `vX.Y.Z` tag publishes installers and the CHANGELOG section as notes (CI) | ✅ since 0.2.0 |
| lichess.org blog | "Vi presento Pragma Chess…" (IT); the English counterpart is written, `packaging/announcements/lichess-blog-en.md` (title, intro, text, the picture to use), to paste in a new post; then a post per notable release | ✅ IT · ✅ EN written 2026-10-10 · ☐ EN posted |
| Screen recording | 40 s, recorded in the headless session: the Opera game move by move, E on 14.Rd1, Explain's arrows (`site/assets/pragma-chess-explain.webm`, the hero of the site). A longer tour with Training and Play Online is still to make | ✅ clip 2026-10-04 · ☐ tour |

## 2. Package channels

Where people already look for software. Each is a real distribution, not an
advert, and each needs a maintainer: us, until someone else steps in.

| Channel | How | Status |
|---|---|---|
| **Flathub** (Linux) | Manifest written: `packaging/flatpak/` (KDE 6.9 runtime, yaml-cpp module, Stockfish binary, metainfo with screenshots and releases, validated with `appstreamcli`). To do: build it once with `flatpak-builder` on a machine that has it, then submit to <https://github.com/flathub/flathub> (`new-pr` branch). The most visited Linux "store": GNOME Software, KDE Discover, Flathub.org | ✅ manifest · ☐ build & submit |
| **winget** (Windows) | `packaging/winget/make-winget.sh <version>` writes the manifests; 0.3.0 submitted as <https://github.com/microsoft/winget-pkgs/pull/446459> (CLA signed; on 2026-10-10 the checks pass and it waits for a community moderator's approval). Each release: run the script, PR "New version: FrancescoBianco.PragmaChess version X" (to automate in `release.yml`) | ✅ PR open 2026-10-04 · waiting for a moderator |
| **Homebrew cask** (macOS) | `Homebrew/homebrew-cask` accepts new casks only from projects with ~75 stars/forks/watchers: wait for that, then `brew create --cask` on the `.dmg` (the app is signed in CI; notarization needed) | ☐ when notable |
| **AUR** (Arch) | `packaging/aur/pragma-chess/` (PKGBUILD and .SRCINFO for 0.3.0: built from the tag against Arch's Qt, Arch's Stockfish as the bundled engine; how to publish in `packaging/aur/README.md`). En Croissant's three AUR packages were made by others: ours by us. Not tried on Arch yet (`makepkg -si`); publishing needs an AUR account with an SSH key | ✅ written 2026-10-11 · ☐ try and publish |
| **Chocolatey / Scoop** (Windows) | Scoop manifest in `extras`; Chocolatey package from the installer. Lower priority than winget | ☐ |
| **Debian / Fedora repositories** | Later: an OBS (openSUSE Build Service) project gives `.deb`/`.rpm` repositories for many distributions from the same spec | ☐ |
| **F-Droid** (Android) | When the app leaves testing: reproducible build from the tag, metadata in `fastlane/` | ☐ |

## 3. Directories and lists

| Channel | How | Status |
|---|---|---|
| **awesome-chess** lists on GitHub | <https://github.com/mbiesiad/awesome-chess>: PR [#59](https://github.com/mbiesiad/awesome-chess/pull/59) (Projects). <https://github.com/mersesarvari/awesome-chess>: issue #2 — the repository is gone (404 on 2026-10-10), nothing to follow. <https://github.com/atamano/awesome-chess> (Desktop GUIs, lists Scid/ChessX/En Croissant) requires ~50 stars and 3 months: submit then. <https://github.com/hkirat/awesome-chess> has no software section: skip | mbiesiad PR open (no review yet, 2026-10-10) · ☐ atamano at 50★ |
| **awesome-italia-opensource** | PR [#222](https://github.com/italia-opensource/awesome-italia-opensource/pull/222) (`awesome/opensource/data/pragma-chess.json`) | ✅ merged 2026-10-05 |
| **AlternativeTo** | Submitted 2026-10-04 (id `98fe179f-e51c-46b5-8a27-b02160f2b4e2`): description, MIT, Free, Windows/Mac/Linux, tags, icon and 4 screenshots, author Francesco Bianco (Italy); alternatives ChessBase, En Croissant, Scid vs. PC, scidCommunity, ChessX, ChessDB, Chess Assistant. Online: <https://alternativeto.net/software/pragma-chess/about/> | ✅ online 2026-10-05 |
| **OpenSourceAlternative.to** | Submitted to the free waitlist (6+ months; the 48 h review costs $29) as an alternative to ChessBase: <https://www.opensourcealternative.to/project/pragma-chess> says "under review" | ✅ 2026-10-04 · waiting |
| **Flathub / winget / Homebrew pages** | Come with section 2: they are the directories most people use | — |
| **FossHub, Softpedia, Uptodown** | Submit the Windows installer; they mirror and list. Softpedia reviews are read by Windows users | ☐ |
| **SourceForge mirror** | Optional: a project page mirroring the releases gives another download channel and listing | ☐ |
| **Chess programming wiki** | A short page under "GUIs" (<https://www.chessprogramming.org/GUI>), factual: written in the wiki's markup, `packaging/announcements/chessprogramming-wiki.md`; needs an account there | ✅ written 2026-10-10 · ☐ posted |
| **Wikipedia "List of chess software"** | Only when notable (coverage by third parties); do not add it ourselves before | — |

## 4. Communities

Posts are written by Francesco, as the author ("I made this…"), each adapted
to the place. Dates and links go here.

### Chess players

| Place | Notes | Status |
|---|---|---|
| **lichess.org forum** — General Chess Discussion / Lichess Feedback | Link the blog post; mention the lichess import and playing through the Board API | ☐ |
| **freechess.org** — the FICS community (its web page lists interfaces; `help interfaces` on the server, and the admins who keep that list) | Once a release plays there: ask to be listed among the interfaces, factual, with the download link | ☐ after the next release |
| **r/chess** | Allows software posts with "I made this"; post with the Explain screenshot, weekend | ☐ |
| **r/chessbeginners**, **r/TournamentChess** | Training with the tutor; the latter is where club players are | ☐ |
| **chess.com forums** — Chess Software | Factual post, the chess.com import is relevant there | ☐ |
| **TalkChess** (talkchess.com) — General Topics | The computer chess forum: GUI makers and engine authors read it, and En Croissant was launched there. The thread is written, `packaging/announcements/talkchess.md` (UCI, Polyglot, the database, Explain's method); post it after 0.4.0 | ✅ written 2026-10-11 · ☐ posted |
| **Chess Stack Exchange** | Not for announcements; answer questions where Pragma Chess is a legitimate answer | — |

### Free software and Linux

| Place | Notes | Status |
|---|---|---|
| **Hacker News** — "Show HN: Pragma Chess – open-source chess database that explains the engine" | Tuesday–Thursday, morning US time; be present for comments; link the site. Title and first comment in `packaging/announcements/show-hn.md`; after 0.4.0 | ✅ written 2026-10-11 · ☐ posted |
| **r/opensource**, **r/linux**, **r/linux_gaming**? (no: it is not a game) — r/opensource first | Self-promotion rules: one post, flair "Promotional" where asked | ☐ |
| **Lobste.rs** | Needs an invite; tag `show`, `c++` | ☐ |
| **Mastodon** (fosstodon.org or mastodon.social) | #chess #opensource #qt #linux, with screenshot and video; also the #ItalianMastodon chess folk | ☐ |
| **Qt community** — forum.qt.io Showcase, r/QtFramework | A Qt 6 Widgets application with a native GNOME/Wayland frame is of interest there | ☐ |
| **Phoronix / OMG! Ubuntu / It's FOSS / Linuxiac** | Tip by mail or form, with screenshots and the Flathub link once there is one | ☐ after Flathub |

### Italy

| Place | Notes | Status |
|---|---|---|
| **Circoli** | The supporters: ask the Circolo del Re to try it in a club evening, collect feedback in the WhatsApp group, their logo is on the site | ✅ started |
| **FSI** — federazione and regional committees, newsletters | A note to the Sicilian committee and to the FSI's "Scacchi e Scuola" people: free tool for clubs and schools | ☐ |
| **Scacchierando, SoloScacchi, Unoscacchista, Scacchi e Dintorni** | Blogs that cover software; offer a short article or an interview | ☐ |
| **Facebook groups** — "Scacchi Italia", regional groups, instructor groups; **Telegram** chess groups | Post with the Italian screenshot; the app is in Italian | ☐ |
| **Reddit r/scacchi** | Small but right | ☐ |
| **Italian Linux / open source** — r/ItalyInformatica, Linux Day (October!), LUGs, ILS | Linux Day 2026 is this month: a talk or a table in Castelvetrano/Trapani/Palermo | ☐ |
| **Schools** | Pragma Chess in Italian for school chess; via the clubs that teach in schools | ☐ |

### Video

| Place | Notes | Status |
|---|---|---|
| **YouTube** | A 3-minute tour (IT, EN subtitles) and the 45 s clip; linked from the site and the posts | ☐ |
| **Chess YouTubers / streamers** | Send the clip to a few who review tools; no pressure, one mail | ☐ |

## 5. Calendar

1. **Week 1** (from 2026-10-04): site live ✅, repository metadata ✅,
   README ✅, awesome-list PRs ✅, winget PR ✅, Flatpak manifest ✅,
   recording ✅, AlternativeTo ✅; still open: social preview, English blog
   post, Flathub build and submission.
2. **Weeks 2–3 (now, from 2026-10-11)**: issue #1 answered ✅; TalkChess and
   Show HN written ✅; the door open to contributors ✅ (CONTRIBUTING.md,
   issue forms, translations without code, #4 pinned); 0.4.0 announced in
   the Discussions ✅ ([#3](https://github.com/francescobianco/pragma-chess/discussions/3)).
   Italian blogs and groups, FSI note, Linux Day, Qt forum can go before
   the release: they speak of what 0.3.0 already does.
3. **End of October: release day of 0.4.0** (below).
4. **Each release**: release notes from the CHANGELOG, a line on Mastodon
   and the lichess blog, the package channels updated (automate winget and
   Flathub from `release.yml`).

### Release day (0.4.0, end of October)

What En Croissant did on 2024-02-10, the day that changed its course
(POSITIONING.md, "Seconda lettura"): the release and the lichess post the
same day, then two fixes in five days.

1. Tag `v0.4.0`; when CI has published, the Android APK (AGENTS.md,
   "Working agreements") and the package channels: winget, AUR, metainfo.
2. **The same day**: the English post on the lichess blog
   (`packaging/announcements/lichess-blog-en.md`, moved to 0.4.0), a line in
   the Discussions' announcement (#3), the short messages below moved to
   0.4.0.
3. **The days after**: answer every comment and issue the same day; a
   **0.4.1** as soon as the first real problems are fixed — the people who
   arrive with the post are the ones a quick fix keeps.
4. **A few days apart**: TalkChess, then Show HN (Tuesday–Thursday), then
   r/chess and Mastodon, each when the previous one has quietened down.

## 6. What to measure

GitHub stars and clones (Insights ▸ Traffic), release download counts
(`gh release view --json assets`), site visits (add a privacy-respecting
counter if wanted: Plausible/GoatCounter, or none), issues opened by new
people, clubs in Supporters. Write the numbers here once a month.

| Date | Stars | Forks | Visits, 14 days (unique) | Clones, 14 days (unique) | Downloads | Issues from others | Clubs |
|---|---|---|---|---|---|---|---|
| 2026-10-10 | 2 | 0 | 209 (23) | 2 360 (650, mostly bots and mirrors) | 0.3.0: 45 (Windows 25, Android 16, macOS 2, Linux 2) · 0.2.0: 31 | 1 | 1 |
| 2026-10-11 | 3 | 0 | 209 (23) | 2 360 (650) | 0.3.0: 46 (Windows 25, Android 16, macOS 2, Linux 2, `version.json` 4: its first reads) · 0.2.0: 31 · 0.1.0: 66 | 1 | 1 |

Referrers (14 days): github.com 33 (14), build.opensuse.org 7 (3), yafb.net 5
(3), Bing and Google 1 each. Nothing public about Pragma Chess on the openSUSE
Build Service (2026-10-11): someone may be packaging it in a `home:` project —
if they show up, thank them and point them to `packaging/linux/`.

## Messages

### One line

- EN: *Pragma Chess — a free, open source chess database for studying,
  training and playing: light, simple, with what really matters.
  Windows, macOS, Linux.*
- IT: *Pragma Chess — un database scacchistico libero e open source per
  studiare, allenarsi e giocare: leggero, semplice, con ciò che serve davvero.
  Windows, macOS, Linux.*

### Short (forum, Reddit, Mastodon)

**EN**

> I made Pragma Chess, a free and open source chess database (MIT) for
> Windows, macOS and Linux. It keeps your games in databases (PGN,
> lichess.org and chess.com import, ChessBase files), analyses them with
> Stockfish or any UCI engine, and has a feature I have not seen elsewhere:
> **Explain** — press E and it shows on the board *why* the last move is good
> or bad, with arrows for the material that falls or the reply that makes the
> difference. You can train against the engine, which plays from an opening
> book whose weights you tune, with a tutor that stops you on mistakes, and
> play on lichess.org or freechess.org from the same board (engine and book
> off while you play). Variations, annotations, Polyglot books, sync between
> computers, databases of millions of games.
>
> Site and downloads: https://yafb.net/pragma-chess/ — source:
> https://github.com/francescobianco/pragma-chess
>
> It is version 0.3.0: I would love to hear what breaks and what you miss.

**IT**

> Ho fatto Pragma Chess, un database scacchistico libero e open source (MIT)
> per Windows, macOS e Linux. Conserva le partite in database (importa PGN,
> partite di lichess.org e chess.com, file ChessBase), le analizza con
> Stockfish o qualunque motore UCI, e ha una funzione che non ho visto
> altrove: **Spiega** — premi E e ti mostra sulla scacchiera *perché*
> l'ultima mossa è buona o cattiva, con le frecce sul materiale che cade o
> sulla risposta che fa la differenza. Ti alleni contro il motore, che gioca
> dal libro di aperture con i pesi che regoli tu, con un tutor che ti ferma
> sugli errori, e giochi su lichess.org o freechess.org dalla stessa
> scacchiera (motore e libro spenti mentre giochi). Varianti, annotazioni,
> libri Polyglot, sincronizzazione tra computer, database di milioni di
> partite. Tutto in italiano.
>
> Sito e download: https://yafb.net/pragma-chess/ — codice:
> https://github.com/francescobianco/pragma-chess
>
> È la versione 0.3.0: mi interessa sapere cosa si rompe e cosa manca.

### Show HN title and first comment

Written in full in `packaging/announcements/show-hn.md`: the title, *Show
HN: Pragma Chess – open-source chess database that explains the engine*,
and the first comment — who I am, why (clubs need a free ChessBase-like tool
that does less and explains more), how Explain works (fed by the live
analysis, compares the searches before and after the move, replays the
engine's line to where the evaluation becomes concrete), the large databases,
and a request for feedback on Windows and macOS.

### Awesome-list line

`- [Pragma Chess](https://yafb.net/pragma-chess/) - Open source chess
database and analysis GUI (Qt) with Explain, training with a tutor, Polyglot
books and lichess.org/freechess.org play; Windows, macOS, Linux.`

### Package descriptions

- Short (80 chars): *Chess database for studying, training and playing, with Explain*
- Long: the AppStream description in `gui/qt/data/*.metainfo.xml`, kept in
  step with the site's hero text.

## Log

| Date | Channel | Link | Notes |
|---|---|---|---|
| 2026-10-02 | lichess.org blog (IT) | https://lichess.org/@/francescobianco/blog/vi-presento-pragma-chess-il-primo-database-di-scacchi-gratuito/rrzlQRZR | first announcement |
| 2026-10-04 | Web site | https://yafb.net/pragma-chess/ | EN/IT, GitHub Pages from `docs/` |
| 2026-10-04 | GitHub repository | https://github.com/francescobianco/pragma-chess | homepage → site, topics, README claim, Discussions on |
| 2026-10-04 | mbiesiad/awesome-chess | https://github.com/mbiesiad/awesome-chess/pull/59 | PR, Projects |
| 2026-10-04 | mersesarvari/awesome-chess | https://github.com/mersesarvari/awesome-chess/issues/2 | issue with the resource details |
| 2026-10-04 | awesome-italia-opensource | https://github.com/italia-opensource/awesome-italia-opensource/pull/222 | PR |
| 2026-10-04 | winget | https://github.com/microsoft/winget-pkgs/pull/446459 | New package 0.2.0 |
| 2026-10-04 | Flathub | packaging/flatpak/ | manifest ready, not yet built or submitted |
| 2026-10-05 | AlternativeTo | https://alternativeto.net/software/pragma-chess/about/ | online, listed as alternative to 7 apps |
| 2026-10-05 | Release | https://github.com/francescobianco/pragma-chess/releases/tag/v0.3.0 | 0.3.0, with the Android APK (beta); site links to releases/latest |
| 2026-10-05 | winget | https://github.com/microsoft/winget-pkgs/pull/446459 | CLA signed; PR moved to 0.3.0 (0.2.0 did not start on Windows) |
| 2026-10-04 | Clip | https://yafb.net/pragma-chess/assets/pragma-chess-explain.webm | 40 s, Explain on the Opera game; the site's hero |
| 2026-10-04 | OpenSourceAlternative.to | https://www.opensourcealternative.to/project/pragma-chess | waitlist, alternative to ChessBase |
| 2026-10-04 | AlternativeTo | https://alternativeto.net/ (in the review queue) | 7 alternatives suggested |
| 2026-10-05 | awesome-italia-opensource | https://github.com/italia-opensource/awesome-italia-opensource/pull/222 | merged |
| 2026-10-10 | Round of the plan | — | statuses checked: winget waits for a moderator, mbiesiad PR without review, mersesarvari repository gone; numbers in "What to measure" |
| 2026-10-10 | Web site | https://yafb.net/pragma-chess/ | Support links Discussions; Play online names freechess.org |
| 2026-10-10 | Issue #1 | https://github.com/francescobianco/pragma-chess/issues/1#issuecomment-6102469020 | qrcodegen from the system's packages: done, answered, closed |
| 2026-10-10 | Social preview, lichess blog EN, chessprogramming.org | packaging/assets/social-preview.png, packaging/announcements/ | ready to upload and paste |
| 2026-10-11 | Following En Croissant (POSITIONING.md) | https://github.com/francescobianco/pragma-chess/discussions/2 | guide on the site, update check with version.json, Discussions welcome, community links in README and Help |
| 2026-10-11 | Contributors (POSITIONING.md, steps 11–12) | https://github.com/francescobianco/pragma-chess/blob/main/CONTRIBUTING.md | CONTRIBUTING.md, issue forms (bug, Explain is wrong, translation), labels `explain` and `translation`; a language needs only its files |
| 2026-10-11 | Discussions: 0.4.0 announced | https://github.com/francescobianco/pragma-chess/discussions/3 | what is coming at the end of October, three ways to help (EN/IT) |
| 2026-10-11 | Issue #4, pinned | https://github.com/francescobianco/pragma-chess/issues/4 | "Translate Pragma Chess into your language": good first issue, help wanted |
| 2026-10-11 | Round of the plan | — | winget still waits for a moderator (checks green since 10-05), mbiesiad PR without review; 3 stars; build.opensuse.org among the referrers; TalkChess thread and Show HN written (`packaging/announcements/`) |
