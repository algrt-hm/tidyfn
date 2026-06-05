# tidyfn

You download files from the internet, it's a bit of a horrorshow ...

... you need `tidyfn`, which prints the `mv` commands to make the filenames in the current working directory sane again

Just copy 'n' paste to rename

## Usage

```
tidyfn        # scan current directory
tidyfn -r     # also recurse into subdirectories
```

In recursive mode (`-r`), subdirectories are also renamed (after their contents), with extra rules:

- Parentheses are preserved in directory names
- Dots are replaced with underscores (directories have no file extension)
- Symlinks are never followed

## Example output

```
mv "Andrej Karpathy — AGI is still a decade away [176425744].mp3" "Andrej_Karpathy_AGI_is_still_a_decade_away_176425744.mp3"
mv "Stanford CME295 Transformers & LLMs ｜ Autumn 2025 ｜ Lecture 1 - Transformer [Ub3GoFaUcds].mp4" "Stanford_CME295_Transformers_LLMs_Autumn_2025_Lecture_1_Transformer_Ub3GoFaUcds.mp4"
```

## How file names are sanitised

- Keeps letters, numbers, space, `.`, `-` and `_`
- Converts mostly-UPPERCASE names to lowercase (except `README.md`)
- Replaces spaces with underscores
- Collapses repeated special characters
- Trims leading and trailing special characters
- Removes a separator before the final `.` (e.g. `name_.txt` -> `name.txt`)
- Replaces all dots except the last with underscores (preserves `.tar.gz` etc.)

## What is skipped

Some entries are left untouched and never appear in the output:

- Dotfiles (names starting with `.`)
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
