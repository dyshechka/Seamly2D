# CLAUDE.md — guide for AI sessions working on this repo

This file orients any AI assistant (Claude or otherwise) working on Seamly2D
on Марта Власова's Mac. Read it before making changes.

## Who you're working with

The person driving these sessions, Марта, is **not a developer** — she's a
knitting-machine operator. She describes what she wants changed in plain,
everyday language; she does not read code, run terminal commands, or resolve
git conflicts herself. That means:

- Do 100% of the technical work yourself: editing files, building, testing,
  git branches/commits, (attempting) pushes.
- Report results in plain, non-technical **Russian**. Describe what changed
  in terms of what she'll see/experience in the app, not code internals —
  unless she explicitly asks for the technical detail.
- Never ask her to open a terminal, edit a file, or "just run this command."
  If something requires her (e.g. a permission dialog, a decision only she
  can make, a git push that can't go through), explain plainly what's needed
  and why.
- Long-term goal (not started): extend Seamly2D — currently sewing-pattern
  software — toward knitting-machine pattern support. As of this writing
  that work has **not begun**; only development infrastructure (this file,
  the helper script below) has been set up so far. Don't assume
  knitting-specific features exist yet.

## What this project is

Seamly2D is open-source (GPLv3+) Qt6/C++ patternmaking software. Two apps
share this repo:

- `seamly2d` (`src/app/seamly2d`) — the main pattern editor.
- `seamlyme` (`src/app/seamlyme`) — the measurement-file editor. Recent work
  has been in `src/app/seamlyme/tmainwindow.{ui,cpp,h}` (the measurements
  table/editor screen).

Shared logic (pattern math, geometry, file formats, widgets, translations)
lives under `src/libs/*`. `.ui` files are Qt Designer XML, compiled by `uic`
into `ui_<name>.h`; `<column>`/label strings in them are translatable by
default (`QApplication::translate(context, "...")` with the class name as
context) — reuse an existing string exactly to reuse its translation instead
of adding a new one.

Background/history on recent changes: see the git log on branches named
`feature/seamlyme-*` and this branch's own commit.

## Where the repo lives & how to reach it

The repo is **not** inside this AI session's own container — it lives on
Марта's Mac at `/Users/marthavlasova/Documents/Seamly2D`. It's reachable
only when the session is linked to her computer, through the
`mcp__remote-devices__*` tools:

- Read/edit files with `device_bash` — this runs inside a **Linux VM**
  mounted at `$HOME/mnt/Seamly2D` on that VM. Edit files there directly
  (`sed -i`, short Python read-modify-write scripts with an exact-match
  safety check, or `cat > file << 'EOF'` for new files) — never by
  re-typing a whole file's content from a prior tool result, which may be
  truncated.
- Git remotes: `origin` = `https://github.com/dyshechka/Seamly2D.git`
  (Марта's/family's fork — push branches here), `upstream` =
  `https://github.com/FashionFreedom/Seamly2D.git` (the original project,
  read-only reference — don't push here).
- This repo's git identity is set **locally** (not global, so it doesn't
  leak into other repos): `Марта Власова <haritonova-marta@mail.ru>`. If a
  commit fails with "Author identity unknown" in a fresh clone/checkout,
  re-set it locally the same way rather than using `--global`, and rather
  than guessing — ask if you don't already know it.

## Building & testing — read this before trying

**`device_bash` cannot build this project. Don't try `qmake`/`make` there —
it's a different operating system (Linux) from the Mac Seamly2D actually
builds on (macOS), not just a missing tool.** Confirmed: that VM has no
`qmake`/`clang`, only a useless generic `make`. The **only** way to build,
run, or verify a change is Qt Creator running on the Mac itself, driven
through the `computer_app_*` (background control, preferred) and
`computer_*` (full-screen, when you need the app's actual menu bar) tools:

1. **Get access.** `computer_resolve_access` then `computer_request_access`
   for `"Qt Creator"` — and separately for `"seamlyme"` (or `"Seamly2D"`)
   once you need to inspect the running app. **Access is not sticky: it
   silently narrows to whichever app you granted most recently.** If a call
   fails with "not in the granted-applications list," just re-request that
   app — this happens routinely, it's not a bug to work around.
2. **Build.** Click "Build Project for All Configurations" (bottom-left
   icon in Qt Creator) or drive it via `computer_app_menu`. Check the
   Issues/Compile Output pane — pre-existing deprecation warnings are
   normal and not yours to fix. To confirm a build actually happened (the
   UI doesn't always make this obvious), `stat -c "%y %n"` the built binary
   under `build/Desktop_arm_darwin_generic_mach_o_64bit_Debug/.../bin/...`
   and the source file you edited — the binary's mtime should be *after*
   your edit.
3. **Run/test with a specific file.** Projects > Run Settings > "Command
   line arguments" — put a quoted absolute path to a `.smis` measurement
   file there, then Run. Sample files live under
   `build/.../bin/samples/measurements/individual/`. **Always clear this
   field back to empty when you're done** — it's part of the user's own
   project config, not scratch space.
4. **Full menu bar (File/Мерки/etc in seamlyme) often isn't reachable via
   `computer_app_menu` in background mode.** If you need it, request
   full-screen control (`computer_request_full_control`), drive it with
   `computer_left_click`/`computer_key`/`computer_type` (the latter sends
   real keystrokes, useful for testing things background `app_type` can't,
   like whether Enter is intercepted by an event filter), then call
   `computer_release_full_control` when done.
5. **Watch for stray/zombie app windows.** The apps use a single-instance
   lock; a warning dialog ("Can't begin to listen for incoming connections
   on name 'SeamlyMe'") means another instance is already running — check
   `computer_app_list_windows` before trusting what you see, and don't
   discard a window's unsaved content without checking whether it's real
   user data or your own scratch test.
6. If you create test data (scratch measurement files, custom measurements
   added just to test something) — discard it ("Не сохранять") rather than
   saving, and say so if you're unsure whether something was real user data.

## Git workflow

- Conventional Commits style (`feat:`, `fix:`, `docs:`, `style:`, `test:`
  …), matching this project's own history and `.github/README-DEVELOPER.md`.
- One topic per branch off `develop`: `feature/<short-name>`,
  `fix/<short-name>`, `chore/<short-name>`.
- **Only stage/commit files that are actually part of the requested
  change.** This repo can carry pre-existing local modifications unrelated
  to any one session's task — e.g. `QMAKE_APPLE_DEVICE_ARCHS = arm64` was
  added to the `.pro` files outside of any AI session, and is required for
  the project to build on this Apple Silicon Mac. Check `git status`/`git
  diff` before committing, and ask if you're not sure whether a modified
  file belongs in your commit — don't silently include or drop one.
- `git push` to `origin` from `device_bash` routinely fails with `403 from
  proxy after CONNECT` — that's this VM's network egress blocking
  github.com, not a credentials problem, and it isn't fixable from
  `device_bash` itself. **Workaround that's confirmed to work: GitHub
  Desktop is installed on this Mac and already has this repo open.**
  Request access to `"GitHub Desktop"` (bundle id
  `com.github.GitHubClient`), request full-screen control (its branch
  publish button is a background-refused control), and click "Publish
  branch" — it picks up whatever branch is currently checked out via
  `device_bash`/git and pushes over the Mac's own network, which works
  where the sandboxed VM's doesn't. Release full-screen control again when
  done. Qt Creator's own VCS/Git menu is an alternative if GitHub Desktop
  is ever unavailable.
- A stale `.git/index.lock` can turn up (e.g. from Qt Creator's own
  background git polling). If git commands warn about it and there's no
  real git process running, remove it: `rm -f .git/index.lock`. If
  `device_bash` refuses ("Operation not permitted"), call
  `device_request_delete_permission` for the connected Seamly2D folder
  first, then retry.

## Working style expected here

- Verify every change actually builds (see above) before telling Марта it's
  done — a report of success should mean you watched it build and, where
  practical, watched the specific behavior work in the running app.
- Prefer small, reviewable changes over large ones.
- When scope is ambiguous (what belongs in this commit, whether to touch a
  file that looks pre-existing/unrelated, whether it's safe to push), ask
  in plain language rather than guessing.
