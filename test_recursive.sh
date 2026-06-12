#!/bin/sh
# Integration tests for tidyfn recursive mode (-r).
# Exercises: traversal, symlink safety, destination escaping,
# directory-name sanitisation, rename ordering, and status messages.

set -e

TIDYFN="$(cd "$(dirname "$0")" && pwd)/tidyfn"
PASS=0
FAIL=0
TMPDIR_BASE=$(mktemp -d)

cleanup() { rm -rf "$TMPDIR_BASE"; }
trap cleanup EXIT

fail() {
  FAIL=$((FAIL + 1))
  printf "FAIL: %s\n" "$1"
  if [ -n "$2" ]; then printf "  expected: %s\n" "$2"; fi
  if [ -n "$3" ]; then printf "  actual:   %s\n" "$3"; fi
}

pass() {
  PASS=$((PASS + 1))
}

# Helper: run tidyfn in a temp directory and capture stdout+stderr
run_in() {
  dir="$1"; shift
  (cd "$dir" && "$TIDYFN" "$@" 2>&1)
}

# --- Test 1: symlink loop is not followed ---
t="$TMPDIR_BASE/t1"
mkdir -p "$t/sub"
touch "$t/sub/file.txt"
ln -s . "$t/self"
out=$(run_in "$t" -r)
if echo "$out" | grep -q "self/"; then
  fail "symlink followed" "no self/ prefix" "$out"
else
  pass
fi

# --- Test 2: destination path is shell-escaped ---
t="$TMPDIR_BASE/t2"
mkdir -p "$t/bad\$dir"
touch "$t/bad\$dir/file name.txt"
out=$(run_in "$t" -r)
# Both old and new paths must escape $
old_ok=$(echo "$out" | grep 'file name.txt' | grep -c 'bad\\\$dir.*bad\\\$dir')
if [ "$old_ok" -ge 1 ]; then
  pass
else
  fail "destination path not escaped" "both sides escape \$" "$out"
fi

# --- Test 3: no placeholder collision (dir named qlpq) ---
t="$TMPDIR_BASE/t3"
mkdir -p "$t/qlpq"
touch "$t/qlpq/test.txt"
out=$(run_in "$t" -r)
if echo "$out" | grep -q '('; then
  fail "placeholder collision with qlpq" "no parens in output" "$out"
else
  pass
fi

# --- Test 4: parens preserved in directory names ---
t="$TMPDIR_BASE/t4"
mkdir -p "$t/Stanford (2025)"
touch "$t/Stanford (2025)/test.txt"
out=$(run_in "$t" -r)
if echo "$out" | grep -q 'Stanford_(2025)'; then
  pass
else
  fail "parens not preserved in dir name" "Stanford_(2025)" "$out"
fi

# --- Test 5: dots replaced with underscores in dir names ---
t="$TMPDIR_BASE/t5"
mkdir -p "$t/some.dir.name"
touch "$t/some.dir.name/test.txt"
out=$(run_in "$t" -r)
if echo "$out" | grep -q 'some_dir_name'; then
  pass
else
  fail "dots not replaced in dir name" "some_dir_name" "$out"
fi

# --- Test 6: directory rename printed after file renames ---
t="$TMPDIR_BASE/t6"
mkdir -p "$t/Bad Dir"
touch "$t/Bad Dir/Messy File.txt"
out=$(run_in "$t" -r)
file_line=$(echo "$out" | grep -n 'Messy' | head -1 | cut -d: -f1)
dir_line=$(echo "$out" | grep -n '"Bad Dir"' | grep -v Messy | head -1 | cut -d: -f1)
if [ -n "$file_line" ] && [ -n "$dir_line" ] && [ "$file_line" -lt "$dir_line" ]; then
  pass
else
  fail "directory rename not after file rename" "file line < dir line" "$out"
fi

# --- Test 7: no spurious "no regular files" when only dirs are renamed ---
t="$TMPDIR_BASE/t7"
mkdir -p "$t/Bad Dir"
out=$(run_in "$t" -r)
if echo "$out" | grep -q "no regular files"; then
  fail "spurious 'no regular files' with dir renames" "no such message" "$out"
else
  pass
fi

# --- Test 8: "no regular files" shown (with hint) when truly empty ---
t="$TMPDIR_BASE/t8"
mkdir -p "$t/clean_dir"
out=$(run_in "$t")
if echo "$out" | grep -q "no regular files" && echo "$out" | grep -q "\-r"; then
  pass
else
  fail "missing 'no regular files' message or -r hint" "message with -r hint" "$out"
fi

# --- Test 9: non-recursive mode ignores subdirectory contents ---
t="$TMPDIR_BASE/t9"
mkdir -p "$t/sub"
touch "$t/sub/Bad File.txt"
out=$(run_in "$t")
if echo "$out" | grep -q "Bad File"; then
  fail "non-recursive mode processed subdirectory file" "no output" "$out"
else
  pass
fi

# --- Test 10: deeply nested recursion works ---
t="$TMPDIR_BASE/t10"
mkdir -p "$t/a/b/c"
touch "$t/a/b/c/Deep File.txt"
out=$(run_in "$t" -r)
if echo "$out" | grep -q 'a/b/c/Deep_File.txt'; then
  pass
else
  fail "deep recursion path wrong" "a/b/c/Deep_File.txt" "$out"
fi

# --- Test 11: collision detection — two files sanitise to the same name ---
t="$TMPDIR_BASE/t11"
mkdir -p "$t"
touch "$t/Hello World.txt"
touch "$t/Hello_World.txt"
out=$(run_in "$t" -r)
# Hello_World.txt is already clean, so Hello World.txt must get a suffix
if echo "$out" | grep -q 'Hello_World_2.txt'; then
  pass
else
  fail "collision not resolved" "Hello_World_2.txt" "$out"
fi

# --- Test 12: collision detection — file would collide with existing dir ---
t="$TMPDIR_BASE/t12"
mkdir -p "$t/foo_bar"
touch "$t/foo_bar/clean.txt"
touch "$t/FOO BAR"
out=$(run_in "$t" -r)
if echo "$out" | grep -q 'foo_bar_2'; then
  pass
else
  fail "file-dir collision not resolved" "foo_bar_2" "$out"
fi

# --- Test 13: extension preserved when special char precedes dot ---
t="$TMPDIR_BASE/t13"
mkdir -p "$t"
touch "$t/item_.png"
touch "$t/emote__.png"
out=$(run_in "$t")
if echo "$out" | grep -q '"item.png"' && echo "$out" | grep -q '"emote.png"'; then
  pass
else
  fail "extension not preserved" "item.png and emote.png" "$out"
fi

# --- Test 14: case-only renames use two-step via temp name ---
t="$TMPDIR_BASE/t14"
mkdir -p "$t"
touch "$t/LICENSE.md"
out=$(run_in "$t")
if echo "$out" | grep -q 'tidyfn_tmp'; then
  pass
else
  fail "case-only rename missing two-step" "tidyfn_tmp intermediate" "$out"
fi

# --- Test 15: filenames with control characters are skipped ---
# Regression test for the macOS custom-folder-icon file, which is literally named
# "Icon" followed by a carriage return ("Icon\r"). Such a name must never produce an
# mv command: the embedded control byte sits inside the quoted source name and gets
# mangled when the output is copy-pasted back into a shell (the \r is read as a line
# break), so the rename targets a file that does not exist. The skip warning must go
# to stderr only, never to stdout — stdout is the payload the user pipes to pbcopy.
t="$TMPDIR_BASE/t15"
mkdir -p "$t"
printf 'x' > "$t/$(printf 'Icon\r')"  # "Icon" + carriage return, as Finder creates it
touch "$t/Bad File.txt"               # a normal file that should still be renamed
stdout=$(cd "$t" && "$TIDYFN" 2>/dev/null)      # the copy-paste payload
stderr=$(cd "$t" && "$TIDYFN" 2>&1 1>/dev/null) # the warnings
if echo "$stdout" | grep -q 'Icon'; then
  fail "control-char filename emitted an mv command" "no Icon line on stdout" "$stdout"
elif ! echo "$stdout" | grep -q 'Bad_File.txt'; then
  fail "normal file not renamed alongside skipped icon" "Bad_File.txt on stdout" "$stdout"
elif ! echo "$stderr" | grep -q 'control character'; then
  fail "no skip warning on stderr" "control character warning" "$stderr"
else
  pass
fi

# --- Test 16: library/dependency directories are excluded ---
# node_modules etc. must be neither recursed into nor renamed. __pycache__ is the
# interesting rename case: without the exclusion its leading/trailing underscores
# would be trimmed, producing a rename to "pycache".
t="$TMPDIR_BASE/t16"
mkdir -p "$t/node_modules/some-pkg" "$t/__pycache__" "$t/venv/lib" "$t/Messy Dir"
touch "$t/node_modules/some-pkg/Messy File.txt"
touch "$t/__pycache__/Mod Name.pyc"
touch "$t/venv/lib/Messy Lib.py"
touch "$t/Messy Dir/Keep Me.txt"
out=$(run_in "$t" -r)
if echo "$out" | grep -q 'node_modules\|pycache\|venv'; then
  fail "library dir processed" "no node_modules/__pycache__/venv in output" "$out"
elif ! echo "$out" | grep -q 'Messy Dir/Keep Me.txt'; then
  fail "normal dir not processed alongside excluded dirs" "Messy Dir/Keep Me.txt rename" "$out"
else
  pass
fi

# --- Summary ---
total=$((PASS + FAIL))
printf "\n%d/%d integration tests passed\n" "$PASS" "$total"
if [ "$FAIL" -gt 0 ]; then
  exit 1
fi
