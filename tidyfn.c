#include <ctype.h>   // ctype.h for character type functions (isupper, isdigit, tolower, isascii)
#include <dirent.h>  // dirent.h for reading directory
#include <limits.h>  // PATH_MAX
#include <stdbool.h> // stdbool.h for the bool type
#include <stdio.h>   // stdio.h for input/output (printf)
#include <stdlib.h>  // stdlib.h for memory allocation (malloc, free)
#include <string.h>  // string.h for string manipulation (strlen, strcpy, strchr, etc.)
#include <strings.h> // strings.h for strcasecmp
#include <sys/stat.h>
#include <time.h>   // time, to seed the stats-mode sampler
#include <unistd.h> // getcwd (POSIX)

#include "tidyfn.h"

// Global constant equivalent to Python's KEY_NON_ALPHANUMERIC
const char *KEY_NON_ALPHANUMERIC = " .-_";

// Names that are conventionally UPPERCASE and must never be lowercased
static const char *CAPS_EXCEPTIONS[] = {"README.md", "CLAUDE.md", "AGENTS.md"};

static bool is_caps_exception(const char *s) {
  for (size_t i = 0; i < sizeof(CAPS_EXCEPTIONS) / sizeof(CAPS_EXCEPTIONS[0]); i++) {
    if (strcmp(s, CAPS_EXCEPTIONS[i]) == 0)
      return true;
  }
  return false;
}

// Uppercase camera-file extensions: names like IMG_1234.JPG, IMG_0687.HEIC,
// IMG_O0631.AAE or TALGE0042.MOV are canonical camera-generated identifiers,
// so the mostly-caps lowercase rule must leave them alone
static const char *CAPS_EXT_EXCEPTIONS[] = {".JPG", ".HEIC", ".AAE", ".MOV"};

static bool has_caps_ext_exception(const char *s) {
  size_t len = strlen(s);
  for (size_t i = 0; i < sizeof(CAPS_EXT_EXCEPTIONS) / sizeof(CAPS_EXT_EXCEPTIONS[0]); i++) {
    size_t ext_len = strlen(CAPS_EXT_EXCEPTIONS[i]);
    if (len >= ext_len && strcmp(s + len - ext_len, CAPS_EXT_EXCEPTIONS[i]) == 0)
      return true;
  }
  return false;
}

// Excel owner/lock files ('~$Report.xlsx') are transient files Office creates
// alongside an open workbook. Stripping the '~$' prefix would make the name
// collide with the real workbook, so they are left untouched.
static bool is_excel_lockfile(const char *s) {
  if (s[0] != '~')
    return false;
  const char *last_dot = strrchr(s, '.');
  if (!last_dot)
    return false;
  for (const char *p = last_dot + 1; p[0] && p[1] && p[2]; p++) {
    if (tolower((unsigned char)p[0]) == 'x' && tolower((unsigned char)p[1]) == 'l' &&
        tolower((unsigned char)p[2]) == 's')
      return true;
  }
  return false;
}

// Markers that start a compound extension (e.g. archive.tar.gz, bootstrap.min.css)
// whose inner dot must be preserved
static const char *COMPOUND_EXT_MARKERS[] = {".tar.", ".min."};

/**
 * @brief Find the start of a compound extension like '.tar.gz' or '.min.css'.
 * @return Pointer into s at the marker, or NULL if none present.
 */
static const char *compound_extension(const char *s) {
  for (size_t i = 0; i < sizeof(COMPOUND_EXT_MARKERS) / sizeof(COMPOUND_EXT_MARKERS[0]); i++) {
    const char *p = strstr(s, COMPOUND_EXT_MARKERS[i]);
    if (p)
      return p;
  }
  return NULL;
}

/**
 * @brief Check if the string contains only ASCII characters.
 */
bool is_ascii(const char *s) {
  while (*s) {
    if ((unsigned char)*s >= 128) {
      return false;
    }
    s++;
  }
  return true;
}

/**
 * @brief Calculate the proportion of uppercase letters in a string.
 */
float proportion_block_caps(const char *s) {
  size_t len = strlen(s);
  if (len == 0) {
    return 0.0f;
  }
  int upper_count = 0;
  for (size_t i = 0; i < len; i++) {
    if (isupper((unsigned char)s[i])) {
      upper_count++;
    }
  }
  return (float)upper_count / len;
}

/**
 * @brief Handle special characters before the last dot. If a char from
 * KEY_NON_ALPHANUMERIC is before the last dot, it's removed.
 * @return A new dynamically allocated string. Caller must free.
 */
char *handle_before_dot(const char *s) {
  char *last_dot = strrchr(s, '.');
  if (last_dot == NULL || last_dot == s) {
    return strdup(s); // No dot or dot is the first character
  }

  char char_before_dot = *(last_dot - 1);
  if (strchr(KEY_NON_ALPHANUMERIC, char_before_dot) != NULL) {
    size_t len = strlen(s);
    char *result = malloc(len); // len is enough because one char is removed
    if (!result)
      return NULL;

    size_t idx_before_dot = (last_dot - 1) - s;
    // Copy part before the character
    strncpy(result, s, idx_before_dot);
    // Copy part from the dot onwards
    strcpy(result + idx_before_dot, last_dot);

    return result;
  }

  return strdup(s);
}

/**
 * @brief Replaces all '.' characters with '_' except for the last one.
 * @return A new dynamically allocated string. Caller must free.
 */
char *remove_all_but_last_dot(const char *s) {
  char *result = strdup(s);
  if (!result)
    return NULL;

  char *last_dot = strrchr(result, '.');
  if (last_dot == NULL) {
    return result; // No dots, return copy of original
  }

  // If we see a compound extension ('.tar.', '.min.') in the string we only
  // replace up to the penultimate dot, not the final one
  bool compound_in_str = (compound_extension(s) != NULL);

  if (compound_in_str) {
    char *penultimate_dot = NULL;

    for (char *p = result; p < last_dot; p++) {
      if (*p == '.') {
        penultimate_dot = p;
      }
    }

    if (penultimate_dot != NULL) {
      last_dot = penultimate_dot;
    }
  }

  // Iterate up to the last dot and replace any other dots
  for (char *p = result; p < last_dot; p++) {
    if (*p == '.') {
      *p = '_';
    }
  }
  return result;
}

/**
 * @brief Core sanitisation function.
 * @return A new dynamically allocated string. Caller must free.
 */
char *sanitise_core(const char *s) {
  size_t len = strlen(s);
  char *temp = malloc(len + 1);
  if (!temp)
    return NULL;

  // 1. Keep only alphanumeric, ASCII letters, and KEY_NON_ALPHANUMERIC chars
  int k = 0;
  for (size_t i = 0; i < len; i++) {
    char c = s[i];
    if (isdigit(c) || ((unsigned char)c < 128 && isalpha((unsigned char)c)) || strchr(KEY_NON_ALPHANUMERIC, c)) {
      temp[k++] = c;
    }
  }
  temp[k] = '\0';

  // 2. Convert to lowercase if mostly caps (except README.md, CLAUDE.md, AGENTS.md
  //    and camera files ending in .JPG/.HEIC/.AAE/.MOV)
  if (!is_caps_exception(temp) && !has_caps_ext_exception(temp) && proportion_block_caps(temp) > 0.5) {
    for (int i = 0; temp[i]; i++) {
      temp[i] = tolower((unsigned char)temp[i]);
    }
  }

  // 3. Replace spaces with underscores
  for (int i = 0; temp[i]; i++) {
    if (temp[i] == ' ') {
      temp[i] = '_';
    }
  }

  // 4. Remove double special characters, but protect extension dot(s)
  //    Find where the extension starts so we never collapse those dots.
  const char *ext_start = compound_extension(temp);
  if (ext_start == NULL) {
    ext_start = strrchr(temp, '.');
  }

  char *result = malloc(strlen(temp) + 1);
  if (!result) {
    free(temp);
    return NULL;
  }
  int j = 0;
  if (strlen(temp) > 0) {
    result[j++] = temp[0];
    for (size_t i = 1; i < strlen(temp); i++) {
      bool is_special_current = strchr(KEY_NON_ALPHANUMERIC, temp[i]);
      bool is_special_prev = strchr(KEY_NON_ALPHANUMERIC, result[j - 1]);
      if (is_special_current && is_special_prev) {
        // Never collapse a dot that is part of the file extension
        if (temp[i] != '.' || ext_start == NULL || &temp[i] < ext_start) {
          continue; // collapse
        }
      }
      result[j++] = temp[i];
    }
  }
  result[j] = '\0';
  free(temp); // free intermediate buffer

  // 5. Trim leading/trailing special characters
  char *start = result;
  while (*start && strchr(KEY_NON_ALPHANUMERIC, *start)) {
    start++;
  }

  char *end = result + strlen(result) - 1;
  while (end >= start && strchr(KEY_NON_ALPHANUMERIC, *end)) {
    *end = '\0';
    end--;
  }

  // Move the trimmed string to the beginning of the buffer if needed
  if (start != result) {
    memmove(result, start, strlen(start) + 1);
  }

  return result;
}

/**
 * @brief Decode one UTF-8 sequence starting at s and store the codepoint in *cp.
 * @return Number of bytes consumed (1-4); an invalid lead byte is consumed
 *         alone with *cp set to 0.
 */
static size_t utf8_decode(const unsigned char *s, unsigned int *cp) {
  if (s[0] < 0x80) {
    *cp = s[0];
    return 1;
  }
  if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
    *cp = ((unsigned int)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    return 2;
  }
  if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
    *cp = ((unsigned int)(s[0] & 0x0F) << 12) | ((unsigned int)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    return 3;
  }
  if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
    *cp = ((unsigned int)(s[0] & 0x07) << 18) | ((unsigned int)(s[1] & 0x3F) << 12) |
          ((unsigned int)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    return 4;
  }
  *cp = 0;
  return 1;
}

/**
 * @brief Check whether the string contains any Chinese, Japanese or Korean
 *        letters (Han ideographs, kana, hangul). CJK punctuation and
 *        fullwidth symbols do not count: stripping those is harmless, but
 *        stripping CJK letters destroys the name's meaning.
 */
bool contains_cjk(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    unsigned int cp;
    p += utf8_decode(p, &cp);
    if ((cp >= 0x1100 && cp <= 0x11FF) ||  // Hangul Jamo
        (cp >= 0x3040 && cp <= 0x30FF) ||  // Hiragana + Katakana
        (cp >= 0x3400 && cp <= 0x4DBF) ||  // CJK Extension A
        (cp >= 0x4E00 && cp <= 0x9FFF) ||  // CJK Unified Ideographs
        (cp >= 0xAC00 && cp <= 0xD7AF) ||  // Hangul Syllables
        (cp >= 0xF900 && cp <= 0xFAFF) ||  // CJK Compatibility Ideographs
        (cp >= 0x20000 && cp <= 0x2FA1F))  // CJK Extensions B+, Supplement
      return true;
  }
  return false;
}

/**
 * @brief Full sanitisation pipeline.
 * @return A new dynamically allocated string. Caller must free.
 */
char *sanitise(const char *s) {
  // Names written in CJK scripts would lose their meaning entirely if
  // non-ASCII were stripped (e.g. a Chinese book title reduced to just its
  // year), so they are left untouched.
  if (contains_cjk(s))
    return strdup(s);

  // Excel lock files keep their '~$' prefix: sanitising it away would
  // collide with the workbook the lock file belongs to.
  if (is_excel_lockfile(s))
    return strdup(s);

  // Pre-process: replace @ with 'at' and & with 'and' to preserve meaning
  char *s0a = replace_substring(s, "@", "at");
  char *s0b = replace_substring(s0a, "&", "and");
  free(s0a);

  char *s1 = sanitise_core(s0b);
  free(s0b);
  char *s2 = handle_before_dot(s1);
  char *s3 = remove_all_but_last_dot(s2);

  free(s1);
  free(s2);
  return s3;
}

/**
 * @brief Escapes characters that are special to the shell inside double quotes.
 *
 * '!' is only special during interactive history expansion, and '\!' inside
 * double quotes keeps its backslash when run from a script — so escape it
 * only when escape_bang is set (i.e. output is a terminal, destined for
 * copy-paste into an interactive shell), not when writing to a pipe or file.
 *
 * @return A new dynamically allocated string. Caller must free.
 */
char *escape_for_shell(const char *s, bool escape_bang) {
  size_t len = strlen(s);
  size_t escaped_len = len;

  // First pass: count how many characters need escaping
  for (size_t i = 0; i < len; i++) {
    if ((s[i] == '!' && escape_bang) || s[i] == '$' || s[i] == '"' || s[i] == '\\' || s[i] == '`') {
      escaped_len++;
    }
  }

  char *result = malloc(escaped_len + 1);
  if (!result)
    return NULL;

  // Second pass: build the new string
  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    if ((s[i] == '!' && escape_bang) || s[i] == '$' || s[i] == '"' || s[i] == '\\' || s[i] == '`') {
      result[j++] = '\\';
    }
    result[j++] = s[i];
  }
  result[j] = '\0';

  return result;
}

/**
 * @brief Replaces all occurrences of a substring with another.
 * @return A new dynamically allocated string. Caller must free.
 */
char *replace_substring(const char *str, const char *old, const char *new_sub) {
  char *result;
  int i, cnt = 0;
  int new_len = strlen(new_sub);
  int old_len = strlen(old);

  // Count the number of occurrences of old substring
  for (i = 0; str[i] != '\0'; i++) {
    if (strstr(&str[i], old) == &str[i]) {
      cnt++;
      i += old_len - 1;
    }
  }

  // Allocate memory for the new string
  result = (char *)malloc(i + cnt * (new_len - old_len) + 1);
  if (!result)
    return NULL;

  i = 0;
  char *p = (char *)str;
  while (*p) {
    // If substring matches, copy new_sub
    if (strstr(p, old) == p) {
      strcpy(&result[i], new_sub);
      i += new_len;
      p += old_len;
    } else {
      result[i++] = *p++;
    }
  }

  result[i] = '\0';
  return result;
}

/**
 * @brief Sanitise a directory name: run sanitise(), replace dots with
 * underscores, and re-insert any parentheses from the original name.
 * @return A new dynamically allocated string. Caller must free.
 */
char *sanitise_dirname(const char *original) {
  // CJK names are left untouched, same as for files
  if (contains_cjk(original))
    return strdup(original);

  // Strip parens from the original before sanitising
  size_t len = strlen(original);
  char *no_parens = malloc(len + 1);
  if (!no_parens)
    return strdup(original);
  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    if (original[i] != '(' && original[i] != ')')
      no_parens[j++] = original[i];
  }
  no_parens[j] = '\0';

  char *sanitised = sanitise(no_parens);
  free(no_parens);

  // Directories don't have file extensions, so replace all dots with underscores
  for (char *p = sanitised; *p; p++) {
    if (*p == '.')
      *p = '_';
  }

  // Re-insert parens at their original relative positions.
  // sanitise only removes/transforms characters, never reorders, so we can
  // walk both strings in order: emit parens from the original, and advance
  // through the sanitised output for everything else.
  size_t san_len = strlen(sanitised);
  size_t paren_count = 0;
  for (size_t i = 0; i < len; i++) {
    if (original[i] == '(' || original[i] == ')')
      paren_count++;
  }
  char *result = malloc(san_len + paren_count + 1);
  if (!result) {
    free(sanitised);
    return strdup(original);
  }

  size_t si = 0; // index into sanitised
  size_t ri = 0; // index into result
  for (size_t oi = 0; oi < len; oi++) {
    if (original[oi] == '(' || original[oi] == ')') {
      result[ri++] = original[oi];
    } else if (si < san_len) {
      result[ri++] = sanitised[si++];
    }
  }
  // Emit any remaining sanitised chars
  while (si < san_len) {
    result[ri++] = sanitised[si++];
  }
  result[ri] = '\0';

  free(sanitised);
  return result;
}

// --- Collision detection helpers ---

#define NAMESET_INIT_CAP 64

void nameset_init(NameSet *ns) {
  ns->entries = malloc(NAMESET_INIT_CAP * sizeof(char *));
  if (!ns->entries) {
    fprintf(stderr, "Out of memory\n");
    exit(1);
  }
  ns->count = 0;
  ns->capacity = NAMESET_INIT_CAP;
}

void nameset_free(NameSet *ns) {
  for (size_t i = 0; i < ns->count; i++)
    free(ns->entries[i]);
  free(ns->entries);
}

bool nameset_contains(const NameSet *ns, const char *name) {
  for (size_t i = 0; i < ns->count; i++) {
    if (strcmp(ns->entries[i], name) == 0)
      return true;
  }
  return false;
}

void nameset_add(NameSet *ns, const char *name) {
  if (ns->count >= ns->capacity) {
    ns->capacity *= 2;
    char **tmp = realloc(ns->entries, ns->capacity * sizeof(char *));
    if (!tmp) {
      fprintf(stderr, "Out of memory\n");
      exit(1);
    }
    ns->entries = tmp;
  }
  ns->entries[ns->count++] = strdup(name);
}

/**
 * @brief If name is already in claimed, append _2, _3, ... until unique.
 *        For files, the suffix is inserted before the extension
 *        (before '.tar.' for compound extensions like .tar.gz).
 * @return A new dynamically allocated string. Caller must free.
 */
char *resolve_collision(NameSet *claimed, const char *name, bool is_file) {
  if (!nameset_contains(claimed, name))
    return strdup(name);

  char buf[PATH_MAX];
  for (int n = 2;; n++) {
    if (is_file) {
      const char *dot = compound_extension(name);
      if (!dot)
        dot = strrchr(name, '.');
      if (dot) {
        snprintf(buf, sizeof(buf), "%.*s_%d%s", (int)(dot - name), name, n, dot);
      } else {
        snprintf(buf, sizeof(buf), "%s_%d", name, n);
      }
    } else {
      snprintf(buf, sizeof(buf), "%s_%d", name, n);
    }
    if (!nameset_contains(claimed, buf))
      return strdup(buf);
  }
}

#ifndef TESTING

/**
 * @brief Print usage information.
 */
static void print_usage(FILE *stream, const char *prog_name) {
  const char *usage =
      "Usage: %s [-r | -s]\n"
      "\n"
      "Scans the current directory for regular files and prints safe rename commands. It does not modify "
      "files itself; it only prints shell 'mv' commands that you can review and run.\n"
      "\n"
      "Options:\n"
      "  -r            Recurse into subdirectories\n"
      "  -s, --stats   Stats mode: for each folder one level down, print how many renames\n"
      "                a recursive run (-r) would make inside it, plus up to 10 randomly\n"
      "                sampled renames. Useful from the top of a large drive.\n"
      "\n"
      "Output format:\n"
      "  mv <original file name> <sanitised file name>\n"
      "  Special shell characters '$', '\"', '\\' and '`' in the original file name are escaped.\n"
      "  '!' is escaped only when output goes to a terminal (for interactive copy-paste);\n"
      "  when piped or redirected to a script, '!' is left alone.\n"
      "\n"
      "How file names are sanitised:\n"
      "  - Replaces '@' with 'at' and '&' with 'and'\n"
      "  - Keeps letters, numbers, space, '.', '-' and '_'\n"
      "  - Converts mostly-UPPERCASE names to lowercase (except 'README.md', 'CLAUDE.md', 'AGENTS.md'\n"
      "    and camera files ending in '.JPG', '.HEIC', '.AAE' or '.MOV')\n"
      "  - Leaves Excel lock files (starting with '~' with 'xls' in the extension) untouched\n"
      "  - Replaces spaces with underscores\n"
      "  - Collapses repeated special characters (space/dot/dash/underscore)\n"
      "  - Trims leading and trailing special characters\n"
      "  - Removes a separator before the final '.' (e.g., 'name_.txt' -> 'name.txt')\n"
      "  - Replaces all dots except the last one with underscores (exception for compound\n"
      "    extensions like filename.tar.gz and bootstrap.min.css)\n"
      "  - Detects collisions: if two files would get the same name, appends _2, _3, etc.\n"
      "\n"
      "Library/dependency directories (node_modules, __pycache__, venv, virtualenv, env,\n"
      "site-packages, vendor) and hidden entries are skipped entirely:\n"
      "never renamed and never recursed into.\n"
      "\n"
      "Also left untouched: names containing Chinese/Japanese/Korean characters (stripping\n"
      "them would destroy the name) and Windows 'Zone.Identifier' artifact files.\n"
      "\n";

  fprintf(stream, usage, prog_name);
}

// --- Library/dependency directory exclusion ---

// Directory names that hold tool-managed library code rather than user files.
// Renaming anything inside these would break the tooling that owns them, so
// they are skipped entirely: not renamed, not recursed into. Hidden ones
// (.git, .venv, .tox, ...) are already covered by the dot-prefix skip.
static const char *EXCLUDED_DIRS[] = {
    "node_modules", "__pycache__", "venv", "virtualenv", "env", "site-packages", "vendor",
};

static bool is_excluded_dir(const char *name) {
  for (size_t i = 0; i < sizeof(EXCLUDED_DIRS) / sizeof(EXCLUDED_DIRS[0]); i++) {
    if (strcmp(name, EXCLUDED_DIRS[i]) == 0)
      return true;
  }
  return false;
}

/**
 * @brief Emit a mv command. For case-only renames (e.g. LICENSE.md -> license.md),
 *        uses a two-step rename via a temp name to avoid prompts on case-insensitive
 *        filesystems (macOS APFS).
 */
static void emit_mv(const char *old_prefixed, const char *new_prefixed, const char *old_name, const char *new_name) {
  // '!' only needs escaping for interactive copy-paste; when output is piped
  // or redirected to a script, '\!' would survive as a literal backslash.
  static int out_is_tty = -1;
  if (out_is_tty < 0)
    out_is_tty = isatty(fileno(stdout));

  char *old_escaped = escape_for_shell(old_prefixed, out_is_tty);
  char *new_escaped = escape_for_shell(new_prefixed, out_is_tty);

  if (strcasecmp(old_name, new_name) == 0) {
    char tmp_path[PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tidyfn_tmp", old_prefixed);
    char *tmp_escaped = escape_for_shell(tmp_path, out_is_tty);
    printf("mv \"%s\" \"%s\" && mv \"%s\" \"%s\"\n", old_escaped, tmp_escaped, tmp_escaped, new_escaped);
    free(tmp_escaped);
  } else {
    printf("mv \"%s\" \"%s\"\n", old_escaped, new_escaped);
  }

  free(old_escaped);
  free(new_escaped);
}

// --- Stats mode rename collection ---

#define STATS_SAMPLE_MAX 10

// Accumulates renames for one top-level folder in stats mode: a total count
// plus a uniform random sample of the renames (reservoir sampling), so huge
// folders can be summarised without holding every rename in memory.
typedef struct {
  int count;                       // total renames recorded so far
  char *samples[STATS_SAMPLE_MAX]; // reservoir of "old -> new" lines
  int sample_count;
} RenameStats;

static void stats_record(RenameStats *stats, const char *old_prefixed, const char *new_prefixed) {
  char line[2 * PATH_MAX + 8];
  snprintf(line, sizeof(line), "%s -> %s", old_prefixed, new_prefixed);

  if (stats->sample_count < STATS_SAMPLE_MAX) {
    stats->samples[stats->sample_count++] = strdup(line);
  } else {
    // Reservoir sampling: the i-th rename (0-based) replaces a random slot
    // with probability STATS_SAMPLE_MAX / (i + 1)
    int j = rand() % (stats->count + 1);
    if (j < STATS_SAMPLE_MAX) {
      free(stats->samples[j]);
      stats->samples[j] = strdup(line);
    }
  }
  stats->count++;
}

static void stats_free(RenameStats *stats) {
  for (int i = 0; i < stats->sample_count; i++)
    free(stats->samples[i]);
}

// --- Directory entry collection ---

typedef struct {
  char *name;
  bool is_dir;
} DirEntry;

/**
 * @brief Scan a directory for files to rename.
 * If recursive is true, descend into subdirectories.
 * prefix is prepended to filenames in the output (e.g. "subdir/").
 *
 * Uses a two-pass approach: first collects all entries and computes
 * sanitised names, then detects collisions before emitting mv commands.
 *
 * If stats is non-NULL (stats mode), renames are recorded there instead of
 * being printed as mv commands.
 */
static void scan_directory(const char *path, bool recursive, const char *prefix, int *looked_at, int *renamable,
                           int *dirs_renamable, RenameStats *stats) {
  DIR *d = opendir(path);
  if (d == NULL) {
    fprintf(stderr, "Could not open directory: %s\n", path);
    return;
  }

  // Phase 1: Read all directory entries
  DirEntry *entries = NULL;
  size_t entry_count = 0;
  size_t entry_cap = 0;

  struct dirent *dir;
  while ((dir = readdir(d)) != NULL) {
    if (dir->d_name[0] == '.')
      continue;

    // Windows Zone.Identifier artifacts (NTFS alternate data streams that
    // materialise as separate files on non-NTFS filesystems, e.g.
    // "report.pdf:Zone.Identifier") are junk metadata: never renamed.
    if (strstr(dir->d_name, "Zone.Identifier") != NULL)
      continue;

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, dir->d_name);

    struct stat path_stat;
    if (lstat(full_path, &path_stat) != 0)
      continue;
    if (!S_ISDIR(path_stat.st_mode) && !S_ISREG(path_stat.st_mode))
      continue;

    // Skip filenames containing control characters (newline, carriage return, tab, etc.) —
    // they break the generated mv commands, since a control byte embedded in the quoted
    // source name gets mangled when the output is copy-pasted back into a shell. The common
    // case is the macOS custom-folder-icon file, literally named "Icon\r" (trailing CR).
    bool has_control_char = false;
    for (const char *p = dir->d_name; *p; p++) {
      if ((unsigned char)*p < 0x20) {
        has_control_char = true;
        break;
      }
    }
    if (has_control_char) {
      fprintf(stderr, "Warning: skipping entry with control character in name: %s/%s\n", path, dir->d_name);
      continue;
    }

    if (entry_count >= entry_cap) {
      entry_cap = entry_cap ? entry_cap * 2 : 32;
      DirEntry *tmp = realloc(entries, entry_cap * sizeof(DirEntry));
      if (!tmp) {
        fprintf(stderr, "Out of memory\n");
        for (size_t j = 0; j < entry_count; j++)
          free(entries[j].name);
        free(entries);
        return;
      }
      entries = tmp;
    }
    entries[entry_count].name = strdup(dir->d_name);
    entries[entry_count].is_dir = S_ISDIR(path_stat.st_mode);
    entry_count++;
  }
  closedir(d);

  // Phase 2: Recurse into subdirectories (depth-first, before renaming)
  if (recursive) {
    for (size_t i = 0; i < entry_count; i++) {
      if (!entries[i].is_dir || is_excluded_dir(entries[i].name))
        continue;
      char full_path[PATH_MAX];
      snprintf(full_path, sizeof(full_path), "%s/%s", path, entries[i].name);
      char new_prefix[PATH_MAX];
      snprintf(new_prefix, sizeof(new_prefix), "%s%s/", prefix, entries[i].name);
      scan_directory(full_path, recursive, new_prefix, looked_at, renamable, dirs_renamable, stats);
    }
  }

  // Phase 3: Compute sanitised names and build the claimed-names set
  NameSet claimed;
  nameset_init(&claimed);

  char **sanitised = calloc(entry_count, sizeof(char *));

  for (size_t i = 0; i < entry_count; i++) {
    if (entries[i].is_dir) {
      // Excluded library dirs keep their name verbatim: name == sanitised means
      // no mv is emitted, while the name still claims its slot in the collision set.
      sanitised[i] = (recursive && !is_excluded_dir(entries[i].name)) ? sanitise_dirname(entries[i].name)
                                                                      : strdup(entries[i].name);
    } else {
      sanitised[i] = sanitise(entries[i].name);
      (*looked_at)++;
    }

    // Names that won't change are immediately claimed
    if (strcmp(entries[i].name, sanitised[i]) == 0) {
      nameset_add(&claimed, entries[i].name);
    }
  }

  // Phase 4: Emit mv commands — files first, then directories
  for (size_t i = 0; i < entry_count; i++) {
    if (entries[i].is_dir)
      continue;
    if (strcmp(entries[i].name, sanitised[i]) == 0)
      continue;

    char *target = resolve_collision(&claimed, sanitised[i], true);
    nameset_add(&claimed, target);

    if (!stats && strcmp(sanitised[i], target) != 0) {
      fprintf(stderr, "Warning: collision avoided: '%s' -> '%s' (instead of '%s')\n", entries[i].name, target,
              sanitised[i]);
    }

    char old_prefixed[PATH_MAX], new_prefixed[PATH_MAX];
    snprintf(old_prefixed, sizeof(old_prefixed), "%s%s", prefix, entries[i].name);
    snprintf(new_prefixed, sizeof(new_prefixed), "%s%s", prefix, target);
    if (stats) {
      stats_record(stats, old_prefixed, new_prefixed);
    } else {
      emit_mv(old_prefixed, new_prefixed, entries[i].name, target);
    }
    (*renamable)++;
    free(target);
  }

  if (recursive) {
    for (size_t i = 0; i < entry_count; i++) {
      if (!entries[i].is_dir)
        continue;
      if (strcmp(entries[i].name, sanitised[i]) == 0)
        continue;

      char *target = resolve_collision(&claimed, sanitised[i], false);
      nameset_add(&claimed, target);

      if (!stats && strcmp(sanitised[i], target) != 0) {
        fprintf(stderr, "Warning: collision avoided: '%s' -> '%s' (instead of '%s')\n", entries[i].name, target,
                sanitised[i]);
      }

      char old_prefixed[PATH_MAX], new_prefixed[PATH_MAX];
      snprintf(old_prefixed, sizeof(old_prefixed), "%s%s", prefix, entries[i].name);
      snprintf(new_prefixed, sizeof(new_prefixed), "%s%s", prefix, target);
      if (stats) {
        stats_record(stats, old_prefixed, new_prefixed);
      } else {
        emit_mv(old_prefixed, new_prefixed, entries[i].name, target);
      }
      (*dirs_renamable)++;
      free(target);
    }
  }

  // Cleanup
  nameset_free(&claimed);
  for (size_t i = 0; i < entry_count; i++) {
    free(entries[i].name);
    free(sanitised[i]);
  }
  free(entries);
  free(sanitised);
}

static int compare_names(const void *a, const void *b) { return strcmp(*(const char **)a, *(const char **)b); }

/**
 * @brief Stats mode: for each folder one level down, report how many renames
 *        a recursive run (-r) would produce inside it, plus a random sample
 *        of up to STATS_SAMPLE_MAX of those renames. Intended for a quick
 *        survey from the top of a large drive (e.g. an NFS mount).
 */
static int run_stats(void) {
  DIR *d = opendir(".");
  if (d == NULL) {
    fprintf(stderr, "Could not open current directory\n");
    return 1;
  }

  // Collect top-level directory names, applying the usual skips
  char **dirs = NULL;
  size_t dir_count = 0, dir_cap = 0;
  struct dirent *dir;
  while ((dir = readdir(d)) != NULL) {
    if (dir->d_name[0] == '.' || is_excluded_dir(dir->d_name))
      continue;
    struct stat path_stat;
    if (lstat(dir->d_name, &path_stat) != 0 || !S_ISDIR(path_stat.st_mode))
      continue;
    if (dir_count >= dir_cap) {
      dir_cap = dir_cap ? dir_cap * 2 : 32;
      char **tmp = realloc(dirs, dir_cap * sizeof(char *));
      if (!tmp) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
      }
      dirs = tmp;
    }
    dirs[dir_count++] = strdup(dir->d_name);
  }
  closedir(d);

  if (dir_count == 0) {
    fprintf(stderr, "No subdirectories found in the working directory\n");
    free(dirs);
    return 0;
  }

  qsort(dirs, dir_count, sizeof(char *), compare_names);
  srand((unsigned)time(NULL));

  for (size_t i = 0; i < dir_count; i++) {
    RenameStats stats = {0};
    int looked_at = 0, renamable = 0, dirs_renamable = 0;

    char prefix[PATH_MAX];
    snprintf(prefix, sizeof(prefix), "%s/", dirs[i]);
    scan_directory(dirs[i], true, prefix, &looked_at, &renamable, &dirs_renamable, &stats);

    printf("%s: %d rename%s\n", dirs[i], stats.count, stats.count == 1 ? "" : "s");
    for (int s = 0; s < stats.sample_count; s++) {
      printf("  %s\n", stats.samples[s]);
    }

    stats_free(&stats);
    free(dirs[i]);
  }
  free(dirs);
  return 0;
}

/**
 * @brief Main function to find files and generate rename commands.
 */
int main(int argc, char *argv[]) {
  bool recursive = false;

  if (argc == 2 && strcmp(argv[1], "-r") == 0) {
    recursive = true;
  } else if (argc == 2 && (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "--stats") == 0)) {
    return run_stats();
  } else if (argc != 1) {
    print_usage(stderr, argv[0]);
    return 2;
  }

  int looked_at = 0;
  int renamable = 0;
  int dirs_renamable = 0;

  scan_directory(".", recursive, "", &looked_at, &renamable, &dirs_renamable, NULL);

  // If no regular files and no directory renames were printed
  if (!looked_at && !dirs_renamable) {
    fprintf(stderr, "There seem to be no regular files in the working directory%s\n",
            recursive ? "" : " (use -r to recurse into subdirectories)");
    return 0;
  }

  // If regular files but no candidates for renaming
  if (looked_at && !renamable && !dirs_renamable) {
    char *working_dir = getcwd(NULL, 0);
    fprintf(stderr, "All %d regular files %s%s seem to have sensible names already\n", looked_at,
            recursive ? "under " : "in ", working_dir);
    free(working_dir);
  }

  return 0;
}

#endif