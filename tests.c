#include <stdlib.h>
#include <string.h>

#include "tidyfn.h"

// Test infrastructure: each CHECK_STR_FN call evaluates the function once,
// checks against expected, prints on mismatch, frees the result, and
// sets `pass = false` on failure.

static int check_count = 0;
static int fail_count = 0;

#define CHECK(actual_value, expected_value, fmt)                                                                        \
  do {                                                                                                                 \
    check_count++;                                                                                                     \
    if ((expected_value) != (actual_value)) {                                                                          \
      fprintf(stderr, "[CHECK] %s:%d expected: " fmt "  actual: " fmt "\n", __FILE__, __LINE__, (expected_value),      \
              (actual_value));                                                                                         \
      pass = false;                                                                                                    \
    }                                                                                                                  \
  } while (0)

#define CHECK_STR_FN(fn_call, expected)                                                                                \
  do {                                                                                                                 \
    check_count++;                                                                                                     \
    char *_res = (fn_call);                                                                                            \
    if (strcmp((expected), _res) != 0) {                                                                               \
      fprintf(stderr, "[CHECK] %s:%d expected: '%s'  actual: '%s'\n", __FILE__, __LINE__, (expected), _res);           \
      pass = false;                                                                                                    \
    }                                                                                                                  \
    free(_res);                                                                                                        \
  } while (0)

bool test_is_ascii() {
  bool pass = true;
  char *not_ascii = (char *)calloc(2, sizeof(char));
  not_ascii[0] = (char)130;
  not_ascii[1] = '\0';

  CHECK(is_ascii("a"), true, "%d");
  CHECK(is_ascii("z"), true, "%d");
  CHECK(is_ascii("1"), true, "%d");
  CHECK(is_ascii("11"), true, "%d");
  CHECK(is_ascii(not_ascii), false, "%d");

  free(not_ascii);
  return pass;
}

bool test_proportion_block_caps() {
  bool pass = true;
  CHECK(proportion_block_caps("ARGHH"), 1.0f, "%f");
  CHECK(proportion_block_caps("arghh"), 0.0f, "%f");
  CHECK(proportion_block_caps("arGH"), 0.5f, "%f");
  return pass;
}

bool test_handle_last_dot() {
  bool pass = true;
  CHECK_STR_FN(handle_before_dot(".."), ".");
  CHECK_STR_FN(handle_before_dot("_."), ".");
  CHECK_STR_FN(handle_before_dot(" ."), ".");
  CHECK_STR_FN(handle_before_dot("x."), "x.");
  return pass;
}

bool test_remove_all_but_last_dot() {
  bool pass = true;
  CHECK_STR_FN(remove_all_but_last_dot("bob.tar.gz"), "bob.tar.gz");
  CHECK_STR_FN(remove_all_but_last_dot("bill.tar.bz"), "bill.tar.bz");
  CHECK_STR_FN(remove_all_but_last_dot("lots.of.dots.ext"), "lots_of_dots.ext");
  return pass;
}

bool test_sanitise_core() {
  bool pass = true;
  // 1. Keep only alphanumeric, ASCII letters, and KEY_NON_ALPHANUMERIC chars
  CHECK_STR_FN(sanitise_core("src.🔥"), "src");
  // 2. Convert to lowercase if mostly caps (except README.md)
  CHECK_STR_FN(sanitise_core("ALL_CAPS.md"), "all_caps.md");
  CHECK_STR_FN(sanitise_core("README.md"), "README.md");
  // 3. Replace spaces with underscores
  CHECK_STR_FN(sanitise_core("file name.ext"), "file_name.ext");
  // 4. Remove double special characters (but protect extension dot)
  CHECK_STR_FN(sanitise_core("file__name.ext"), "file_name.ext");
  CHECK_STR_FN(sanitise_core("item_.png"), "item_.png");
  // 5. Trim leading/trailing special characters
  CHECK_STR_FN(sanitise_core("__file_name.ext__"), "file_name.ext");
  // Consecutive specials collapsed, _-_ becomes _
  CHECK_STR_FN(sanitise_core("file_-_name.ext"), "file_name.ext");
  return pass;
}

bool test_sanitise() {
  bool pass = true;
  CHECK_STR_FN(sanitise("Hello World.txt"), "Hello_World.txt");
  CHECK_STR_FN(sanitise("ALL CAPS FILE.PDF"), "all_caps_file.pdf");
  CHECK_STR_FN(sanitise("lots.of.dots.ext"), "lots_of_dots.ext");
  CHECK_STR_FN(sanitise("clean.txt"), "clean.txt");
  // @ replaced with 'at', & replaced with 'and'
  CHECK_STR_FN(sanitise("spritesheet@2.png"), "spritesheetat2.png");
  CHECK_STR_FN(sanitise("Dice & Cards.txt"), "Dice_and_Cards.txt");
  CHECK_STR_FN(sanitise("keyboard-&-mouse.png"), "keyboard-and-mouse.png");
  // Extension preserved when special char precedes the dot
  CHECK_STR_FN(sanitise("item_.png"), "item.png");
  CHECK_STR_FN(sanitise("emote__.png"), "emote.png");
  CHECK_STR_FN(sanitise("name-.ext"), "name.ext");
  CHECK_STR_FN(sanitise("name .ext"), "name.ext");
  // tar.gz extension preserved
  CHECK_STR_FN(sanitise("archive.tar.gz"), "archive.tar.gz");
  CHECK_STR_FN(sanitise("my.archive.tar.gz"), "my_archive.tar.gz");
  return pass;
}

bool test_sanitise_dirname() {
  bool pass = true;
  CHECK_STR_FN(sanitise_dirname("Bad Dir Name"), "Bad_Dir_Name");
  CHECK_STR_FN(sanitise_dirname("LOUD DIR"), "loud_dir");
  CHECK_STR_FN(sanitise_dirname("some.dir.name"), "some_dir_name");
  CHECK_STR_FN(sanitise_dirname("Stanford (2025)"), "Stanford_(2025)");
  CHECK_STR_FN(sanitise_dirname("foo(bar)"), "foo(bar)");
  CHECK_STR_FN(sanitise_dirname("(leading)"), "(leading)");
  CHECK_STR_FN(sanitise_dirname("qlpq"), "qlpq");
  CHECK_STR_FN(sanitise_dirname("qrpq"), "qrpq");
  CHECK_STR_FN(sanitise_dirname("clean_dir"), "clean_dir");
  return pass;
}

bool test_escape_for_shell() {
  bool pass = true;
  CHECK_STR_FN(escape_for_shell("normal.txt"), "normal.txt");
  CHECK_STR_FN(escape_for_shell("price$100!.txt"), "price\\$100\\!.txt");
  CHECK_STR_FN(escape_for_shell("$start"), "\\$start");
  CHECK_STR_FN(escape_for_shell("end!"), "end\\!");
  CHECK_STR_FN(escape_for_shell("$!"), "\\$\\!");
  CHECK_STR_FN(escape_for_shell("$$!!"), "\\$\\$\\!\\!");
  CHECK_STR_FN(escape_for_shell("a$!b$c!d"), "a\\$\\!b\\$c\\!d");
  CHECK_STR_FN(escape_for_shell("say\"hi"), "say\\\"hi");
  CHECK_STR_FN(escape_for_shell("a\\b"), "a\\\\b");
  CHECK_STR_FN(escape_for_shell("x`y"), "x\\`y");
  CHECK_STR_FN(escape_for_shell("$!\"\\`"), "\\$\\!\\\"\\\\\\`");
  CHECK_STR_FN(escape_for_shell(""), "");
  return pass;
}

bool test_replace_substring() {
  bool pass = true;
  CHECK_STR_FN(replace_substring("hello_world", "_", "-"), "hello-world");
  CHECK_STR_FN(replace_substring("a_a_a", "_", "-"), "a-a-a");
  CHECK_STR_FN(replace_substring("xox", "x", "yz"), "yzoyz");
  CHECK_STR_FN(replace_substring("aaaa", "aa", "b"), "bb");
  CHECK_STR_FN(replace_substring("a_b_c", "_", ""), "abc");
  CHECK_STR_FN(replace_substring("abc", "z", "y"), "abc");
  CHECK_STR_FN(replace_substring("", "_", "-"), "");
  CHECK_STR_FN(replace_substring("abc", "abc", "x"), "x");
  CHECK_STR_FN(replace_substring("banana", "ana", "a"), "bana");
  CHECK_STR_FN(replace_substring("aaa", "a", "a"), "aaa");
  return pass;
}

bool test_nameset() {
  bool pass = true;
  NameSet ns;
  nameset_init(&ns);

  CHECK(nameset_contains(&ns, "a.txt"), false, "%d");
  nameset_add(&ns, "a.txt");
  CHECK(nameset_contains(&ns, "a.txt"), true, "%d");
  CHECK(nameset_contains(&ns, "b.txt"), false, "%d");

  // Grow past the initial capacity to exercise realloc
  char name[32];
  for (int i = 0; i < 100; i++) {
    snprintf(name, sizeof(name), "file_%d.txt", i);
    nameset_add(&ns, name);
  }
  CHECK(nameset_contains(&ns, "file_0.txt"), true, "%d");
  CHECK(nameset_contains(&ns, "file_99.txt"), true, "%d");
  CHECK(nameset_contains(&ns, "file_100.txt"), false, "%d");

  nameset_free(&ns);
  return pass;
}

bool test_resolve_collision() {
  bool pass = true;
  NameSet claimed;
  nameset_init(&claimed);
  nameset_add(&claimed, "file.txt");
  nameset_add(&claimed, "file_2.txt");
  nameset_add(&claimed, "makefile");
  nameset_add(&claimed, "archive.tar.gz");
  nameset_add(&claimed, "some.dir");

  // No collision: name returned unchanged
  CHECK_STR_FN(resolve_collision(&claimed, "other.txt", true), "other.txt");
  // File collision: suffix inserted before the extension
  CHECK_STR_FN(resolve_collision(&claimed, "file.txt", true), "file_3.txt");
  // File without extension: suffix appended
  CHECK_STR_FN(resolve_collision(&claimed, "makefile", true), "makefile_2");
  // Compound extension: suffix inserted before '.tar.gz', not the last dot
  CHECK_STR_FN(resolve_collision(&claimed, "archive.tar.gz", true), "archive_2.tar.gz");
  // Directory: suffix appended even when the name contains a dot
  CHECK_STR_FN(resolve_collision(&claimed, "some.dir", false), "some.dir_2");

  nameset_free(&claimed);
  return pass;
}

int main() {
  bool tests_all_pass =
      (test_is_ascii() && test_proportion_block_caps() && test_handle_last_dot() && test_remove_all_but_last_dot() &&
       test_sanitise_core() && test_sanitise() && test_sanitise_dirname() && test_escape_for_shell() &&
       test_replace_substring() && test_nameset() && test_resolve_collision());

  if (tests_all_pass) {
    printf("Passed all %d checks\n", check_count);
    return 0;
  }

  printf("Failed %d checks (of %d)\n", fail_count, check_count);
  return 1;
}
