# tidyfn

You download files from the internet, it's a bit of a horrorshow ...

... you need `tidyfn`, which prints the `mv` commands to make the filenames in the current working directory sane again

Just copy 'n' paste to rename

## Usage

```
tidyfn            # scan current directory
tidyfn -r         # also recurse into subdirectories
tidyfn -s         # stats mode: per-folder rename counts (also --stats)
```

In recursive mode (`-r`), subdirectories are also renamed (after their contents), with extra rules:

- Parentheses are preserved in directory names
- Dots are replaced with underscores (directories have no file extension)
- Symlinks are never followed

## Stats mode

Designed to be run from the top of a large drive (e.g. an NFS mount). For each
folder one level down, stats mode reports how many renames a recursive run
(`tidyfn -r`) would make inside it, plus up to 10 randomly sampled renames so
you can eyeball what would happen:

```
Movies: 3 renames
  Movies/Some Series (2024)/Ep 1 — Pilot.mkv -> Movies/Some Series (2024)/Ep_1_Pilot.mkv
  Movies/CLEAN_FILE.MP4 -> Movies/clean_file.mp4
  Movies/Some Series (2024) -> Movies/Some_Series_(2024)
Podcasts: 27 renames
  Podcasts/Episode 9 - Great Stuff!.mp3 -> Podcasts/Episode_9_Great_Stuff.mp3
  ...
clean_dir: 0 renames
```

Folders are listed alphabetically; the sample uses reservoir sampling, so even
enormous folders are summarised without holding every rename in memory. Stats
mode prints no `mv` commands and never modifies anything.

## Example output

```
mv "Andrej Karpathy — AGI is still a decade away [176425744].mp3" "Andrej_Karpathy_AGI_is_still_a_decade_away_176425744.mp3"
mv "Stanford CME295 Transformers & LLMs ｜ Autumn 2025 ｜ Lecture 1 - Transformer [Ub3GoFaUcds].mp4" "Stanford_CME295_Transformers_LLMs_Autumn_2025_Lecture_1_Transformer_Ub3GoFaUcds.mp4"
```

## How file names are sanitised

- Keeps letters, numbers, space, `.`, `-` and `_`
- Converts mostly-UPPERCASE names to lowercase (except `README.md`, `CLAUDE.md` and `AGENTS.md`,
  and camera files ending in `.JPG` or `.HEIC`, e.g. `IMG_0687.HEIC`)
- Replaces spaces with underscores
- Collapses repeated special characters
- Trims leading and trailing special characters
- Removes a separator before the final `.` (e.g. `name_.txt` -> `name.txt`)
- Replaces all dots except the last with underscores (preserves compound extensions like `.tar.gz` and `.min.css`/`.min.js`)

## What is skipped

Some entries are left untouched and never appear in the output:

- Dotfiles (names starting with `.`)
- Names containing Chinese, Japanese or Korean characters (e.g.
  `数学分析教程-常庚哲_史济怀-下册-2003.pdf`) — sanitising strips non-ASCII, which
  would destroy such names, so they are left alone entirely. Other non-ASCII
  (accents, dashes, fullwidth punctuation) is still sanitised as before
- Windows `Zone.Identifier` artifacts (NTFS alternate data streams that show up
  as e.g. `report.pdf:Zone.Identifier` on non-NTFS filesystems)
- Library/dependency directories — `node_modules`, `__pycache__`, `venv`,
  `virtualenv`, `env`, `site-packages`, `vendor` — are never renamed and never
  recursed into; their contents belong to tooling, not to you
- Anything that isn't a regular file or directory (symlinks, sockets, etc.)
- Names containing control characters — most notably the macOS custom-folder-icon
  file, which is literally named `Icon` followed by a trailing carriage return.
  Emitting an `mv` for it would produce a broken command, because the control byte
  sits inside the quoted source name and gets mangled when the output is copied and
  pasted back into a shell. These are skipped with a warning on stderr, so the
  warning never ends up in the copy-and-paste output.

## Files

```
tidyfn.c              main source (sanitisation logic + CLI)
tidyfn.h              header (function declarations)
tests.c               unit tests
test_recursive.sh     integration tests for recursive mode
makefile              build, test, install targets
```

## Build and test

```
make              # build
make run-tests    # unit tests + integration tests
make install      # copy to ~/bin
```
