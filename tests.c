#include <stdlib.h>
#include <string.h>

#include "tidyfn.h"

// Simple expectation helper: compares two values and prints both if unequal.
// Usage: e.g. CHECK(expected_value, actual_value, "%d")

#define CHECK(actual_value, expected_value, fmt)                                                                       \
  do {                                                                                                                 \
    if ((expected_value) != (actual_value)) {                                                                          \
      fprintf(stderr, "[CHECK] %s:%d expected: " fmt "  actual: " fmt "\n", __FILE__, __LINE__, (expected_value),      \
              (actual_value));                                                                                         \
    }                                                                                                                  \
  } while (0);

// Note: For strings, compare with strcmp and print with "%s" explicitly.
#define CHECK_STR(actual_value, expected_value)                                                                        \
  do {                                                                                                                 \
    if (strcmp(expected_value, actual_value)) {                                                                        \
      fprintf(stderr, "[CHECK] %s:%d expected: '%s' actual: '%s' \n", __FILE__, __LINE__, (expected_value),            \
              (actual_value));                                                                                         \
    }                                                                                                                  \
  } while (0);

bool test_is_ascii() {
  char *not_ascii = (char *)calloc(2, sizeof(char));

  not_ascii[0] = (char)130;
  not_ascii[1] = '\0';

  CHECK(is_ascii("a"), true, "%d");
  CHECK(is_ascii("z"), true, "%d");
  CHECK(is_ascii("1"), true, "%d");
  CHECK(is_ascii("11"), true, "%d");
  CHECK(is_ascii("11"), true, "%d");
  CHECK(is_ascii(not_ascii), false, "%d");

  return ((is_ascii("a") == true) && (is_ascii("z") == true) && (is_ascii("1") == true) && (is_ascii("11") == true) &&
          (is_ascii(not_ascii) == false));
}

bool test_proportion_block_caps() {
  CHECK(proportion_block_caps("ARGHH"), 1.0f, "%f");
  CHECK(proportion_block_caps("arghh"), 0.0f, "%f");
  CHECK(proportion_block_caps("arGH"), 0.5f, "%f");

  return ((proportion_block_caps("ARGHH") == 1.0f) && (proportion_block_caps("arghh") == 0.0f) &&
          (proportion_block_caps("arGH") == 0.5f));
}

bool test_handle_last_dot() {
  CHECK_STR(handle_before_dot(".."), ".");
  CHECK_STR(handle_before_dot("_."), ".");
  CHECK_STR(handle_before_dot(" ."), ".");
  CHECK_STR(handle_before_dot("_."), ".");
  CHECK_STR(handle_before_dot("x."), "x.");

  return (!strcmp(handle_before_dot(".."), ".") && !strcmp(handle_before_dot("_."), ".") &&
          !strcmp(handle_before_dot(" ."), ".") && !strcmp(handle_before_dot("_."), ".") &&
          !strcmp(handle_before_dot("x."), "x."));
}

// remove_all_but_last_dot
bool test_remove_all_but_last_dot() {
  CHECK_STR(remove_all_but_last_dot("bob.tar.gz"), "bob.tar.gz");
  CHECK_STR(remove_all_but_last_dot("bill.tar.bz"), "bill.tar.bz");
  CHECK_STR(remove_all_but_last_dot("lots.of.dots.ext"), "lots_of_dots.ext");

  return (!strcmp(remove_all_but_last_dot("bob.tar.gz"), "bob.tar.gz") &&
          !strcmp(remove_all_but_last_dot("bill.tar.bz"), "bill.tar.bz") &&
          !strcmp(remove_all_but_last_dot("lots.of.dots.ext"), "lots_of_dots.ext"));
}

// sanitise_core
bool test_sanitise_core() {
  // Might consider below case:
  // CHECK_STR(sanitise_core("__file_name__.ext"), "file_name.ext");

  // 1. Keep only alphanumeric, ASCII letters, and KEY_NON_ALPHANUMERIC chars
  CHECK_STR(sanitise_core("src.🔥"), "src");
  // 2. Convert to lowercase if mostly caps (except README.md)
  CHECK_STR(sanitise_core("ALL_CAPS.md"), "all_caps.md");
  CHECK_STR(sanitise_core("README.md"), "README.md");
  // 3. Replace spaces with underscores
  CHECK_STR(sanitise_core("file name.ext"), "file_name.ext");
  CHECK_STR(sanitise_core("README.md"), "README.md");
  // 4. Remove double special characters
  CHECK_STR(sanitise_core("file__name.ext"), "file_name.ext");
  // 5. Trim leading/trailing special characters
  CHECK_STR(sanitise_core("__file_name.ext__"), "file_name.ext");
  // 6. Get rid of "_-_" -> "-"
  CHECK_STR(sanitise_core("file_-_name.ext"), "file_name.ext");

  return (
      !strcmp(sanitise_core("src.🔥"), "src") && !strcmp(sanitise_core("ALL_CAPS.md"), "all_caps.md") &&
      !strcmp(sanitise_core("README.md"), "README.md") && !strcmp(sanitise_core("file name.ext"), "file_name.ext") &&
      !strcmp(sanitise_core("README.md"), "README.md") && !strcmp(sanitise_core("file__name.ext"), "file_name.ext") &&
      !strcmp(sanitise_core("__file_name.ext__"), "file_name.ext") &&
      !strcmp(sanitise_core("file_-_name.ext"), "file_name.ext"));
}

// sanitise
bool test_sanitise() { return true; }

// escape_for_shell
bool test_escape_for_shell() {
  CHECK_STR(escape_for_shell("normal.txt"), "normal.txt");
  CHECK_STR(escape_for_shell("price$100!.txt"), "price\\$100\\!.txt");
  CHECK_STR(escape_for_shell("$start"), "\\$start");
  CHECK_STR(escape_for_shell("end!"), "end\\!");
  CHECK_STR(escape_for_shell("$!"), "\\$\\!");
  CHECK_STR(escape_for_shell("$$!!"), "\\$\\$\\!\\!");
  CHECK_STR(escape_for_shell("a$!b$c!d"), "a\\$\\!b\\$c\\!d");
  // double quote, backslash, backtick
  CHECK_STR(escape_for_shell("say\"hi"), "say\\\"hi");
  CHECK_STR(escape_for_shell("a\\b"), "a\\\\b");
  CHECK_STR(escape_for_shell("x`y"), "x\\`y");
  CHECK_STR(escape_for_shell("$!\"\\`"), "\\$\\!\\\"\\\\\\`");
  CHECK_STR(escape_for_shell(""), "");

  return (!strcmp(escape_for_shell("normal.txt"), "normal.txt") &&
          !strcmp(escape_for_shell("price$100!.txt"), "price\\$100\\!.txt") &&
          !strcmp(escape_for_shell("$start"), "\\$start") && !strcmp(escape_for_shell("end!"), "end\\!") &&
          !strcmp(escape_for_shell("$!"), "\\$\\!") && !strcmp(escape_for_shell("$$!!"), "\\$\\$\\!\\!") &&
          !strcmp(escape_for_shell("a$!b$c!d"), "a\\$\\!b\\$c\\!d") &&
          !strcmp(escape_for_shell("say\"hi"), "say\\\"hi") && !strcmp(escape_for_shell("a\\b"), "a\\\\b") &&
          !strcmp(escape_for_shell("x`y"), "x\\`y") && !strcmp(escape_for_shell("$!\"\\`"), "\\$\\!\\\"\\\\\\`") &&
          !strcmp(escape_for_shell(""), ""));
}

// replace_substring
bool test_replace_substring() {
  // basic replacement
  CHECK_STR(replace_substring("hello_world", "_", "-"), "hello-world");
  // multiple occurrences
  CHECK_STR(replace_substring("a_a_a", "_", "-"), "a-a-a");
  // replacement longer than old
  CHECK_STR(replace_substring("xox", "x", "yz"), "yzoyz");
  // replacement shorter than old (non-overlapping matches)
  CHECK_STR(replace_substring("aaaa", "aa", "b"), "bb");
  // delete occurrences when new_sub is empty
  CHECK_STR(replace_substring("a_b_c", "_", ""), "abc");
  // old not found -> unchanged
  CHECK_STR(replace_substring("abc", "z", "y"), "abc");
  // empty input string
  CHECK_STR(replace_substring("", "_", "-"), "");
  // whole string match
  CHECK_STR(replace_substring("abc", "abc", "x"), "x");
  // overlapping pattern behavior (left-to-right, non-overlapping)
  CHECK_STR(replace_substring("banana", "ana", "a"), "bana");
  // identity replacement (no change)
  CHECK_STR(replace_substring("aaa", "a", "a"), "aaa");

  return (
      !strcmp(replace_substring("hello_world", "_", "-"), "hello-world") &&
      !strcmp(replace_substring("a_a_a", "_", "-"), "a-a-a") && !strcmp(replace_substring("xox", "x", "yz"), "yzoyz") &&
      !strcmp(replace_substring("aaaa", "aa", "b"), "bb") && !strcmp(replace_substring("a_b_c", "_", ""), "abc") &&
      !strcmp(replace_substring("abc", "z", "y"), "abc") && !strcmp(replace_substring("", "_", "-"), "") &&
      !strcmp(replace_substring("abc", "abc", "x"), "x") && !strcmp(replace_substring("banana", "ana", "a"), "bana") &&
      !strcmp(replace_substring("aaa", "a", "a"), "aaa"));
}

int main() {
  bool tests_all_pass =
      (test_is_ascii() && test_proportion_block_caps() && test_handle_last_dot() && test_remove_all_but_last_dot() &&
       test_sanitise_core() && test_sanitise() && test_escape_for_shell() && test_replace_substring());

  if (tests_all_pass) {
    puts("Passed tests");
    return 0;
  }

  puts("Failed tests");
  return 1;
}