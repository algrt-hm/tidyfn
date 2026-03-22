#include <ctype.h>   // ctype.h for character type functions (isupper, isdigit, tolower, isascii)
#include <dirent.h>  // dirent.h for reading directory
#include <limits.h>  // PATH_MAX
#include <stdbool.h> // stdbool.h for the bool type
#include <stdio.h>   // stdio.h for input/output (printf)
#include <stdlib.h>  // stdlib.h for memory allocation (malloc, free)
#include <string.h>  // string.h for string manipulation (strlen, strcpy, strchr, etc.)
#include <sys/stat.h>
#include <unistd.h> // getcwd (POSIX)

#include "tidyfn.h"

// Global constant equivalent to Python's KEY_NON_ALPHANUMERIC
const char *KEY_NON_ALPHANUMERIC = " .-_";

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

  // If we see '.tar.' in the string we only replace up to the penultimate dot, not the final one
  bool tar_in_str = (strstr(s, ".tar.") != NULL);

  if (tar_in_str) {
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

  // 2. Convert to lowercase if mostly caps (except README.md)
  if (strcmp(temp, "README.md") != 0 && proportion_block_caps(temp) > 0.5) {
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

  // 4. Remove double special characters
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
        continue; // Skip double special char
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

  // 6. Get rid of "_-_" -> "-"
  // This is complex to do in-place, so we use a helper.
  // The previous `result` is consumed by `replace_substring`.
  char *final_result = replace_substring(result, "_-_", "-");
  free(result);

  return final_result;
}

/**
 * @brief Full sanitisation pipeline.
 * @return A new dynamically allocated string. Caller must free.
 */
char *sanitise(const char *s) {
  char *s1 = sanitise_core(s);
  char *s2 = handle_before_dot(s1);
  char *s3 = remove_all_but_last_dot(s2);

  free(s1);
  free(s2);
  return s3;
}

/**
 * @brief Escapes characters that are special to the shell.
 * @return A new dynamically allocated string. Caller must free.
 */
char *escape_for_shell(const char *s) {
  size_t len = strlen(s);
  size_t escaped_len = len;

  // First pass: count how many characters need escaping
  for (size_t i = 0; i < len; i++) {
    if (s[i] == '!' || s[i] == '$' || s[i] == '"' || s[i] == '\\' || s[i] == '`') {
      escaped_len++;
    }
  }

  char *result = malloc(escaped_len + 1);
  if (!result)
    return NULL;

  // Second pass: build the new string
  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] == '!' || s[i] == '$' || s[i] == '"' || s[i] == '\\' || s[i] == '`') {
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

#ifndef TESTING

/**
 * @brief Print usage information.
 */
static void print_usage(FILE *stream, const char *prog_name) {
  const char *usage =
      "Usage: %s [-r]\n"
      "\n"
      "Scans the current directory for regular files and prints safe rename commands. It does not modify "
      "files itself; it only prints shell 'mv' commands that you can review and run.\n"
      "\n"
      "Options:\n"
      "  -r    Recurse into subdirectories\n"
      "\n"
      "Output format:\n"
      "  mv <original file name> <sanitised file name>\n"
      "  Special shell characters '$', '!', '\"', '\\' and '`' in the original file name are escaped.\n"
      "\n"
      "How file names are sanitised:\n"
      "  - Keeps letters, numbers, space, '.', '-' and '_'\n"
      "  - Converts mostly-UPPERCASE names to lowercase (except 'README.md')\n"
      "  - Replaces spaces with underscores\n"
      "  - Collapses repeated special characters (space/dot/dash/underscore)\n"
      "  - Trims leading and trailing special characters\n"
      "  - Removes a separator before the final '.' (e.g., 'name_.txt' -> 'name.txt')\n"
      "  - Replaces all dots except the last one with underscores (exception for e.g. filename.tar.gz)\n"
      "\n";

  fprintf(stream, usage, prog_name);
}

/**
 * @brief Scan a directory for files to rename.
 * If recursive is true, descend into subdirectories.
 * prefix is prepended to filenames in the output (e.g. "subdir/").
 */
static void scan_directory(const char *path, bool recursive, const char *prefix, int *looked_at, int *renamable,
                           int *dirs_renamable) {
  DIR *d = opendir(path);
  if (d == NULL) {
    fprintf(stderr, "Could not open directory: %s\n", path);
    return;
  }

  struct dirent *dir;
  while ((dir = readdir(d)) != NULL) {
    const char *entry_name = dir->d_name;

    // Skip hidden files, "." and ".."
    if (entry_name[0] == '.') {
      continue;
    }

    // Build the full path for stat
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry_name);

    // Use lstat to avoid following symlinks
    struct stat path_stat;
    if (lstat(full_path, &path_stat) != 0) {
      continue;
    }

    if (S_ISDIR(path_stat.st_mode)) {
      if (recursive) {
        // Build the new prefix using the original directory name
        char new_prefix[PATH_MAX];
        snprintf(new_prefix, sizeof(new_prefix), "%s%s/", prefix, entry_name);
        scan_directory(full_path, recursive, new_prefix, looked_at, renamable, dirs_renamable);

        // After processing contents, rename the directory itself if needed
        char *new_dir_name = sanitise_dirname(entry_name);
        if (strcmp(entry_name, new_dir_name) != 0) {
          char old_prefixed[PATH_MAX];
          char new_prefixed[PATH_MAX];
          snprintf(old_prefixed, sizeof(old_prefixed), "%s%s", prefix, entry_name);
          snprintf(new_prefixed, sizeof(new_prefixed), "%s%s", prefix, new_dir_name);

          char *old_escaped = escape_for_shell(old_prefixed);
          char *new_escaped = escape_for_shell(new_prefixed);
          printf("mv \"%s\" \"%s\"\n", old_escaped, new_escaped);
          free(old_escaped);
          free(new_escaped);
          (*dirs_renamable)++;
        }
        free(new_dir_name);
      }
      continue;
    }

    if (!S_ISREG(path_stat.st_mode)) {
      continue;
    }

    // Sanitise only the basename
    char *new_name = sanitise(entry_name);

    if (strcmp(entry_name, new_name) != 0) {
      // Build prefixed old and new names for the mv command
      char old_prefixed[PATH_MAX];
      char new_prefixed[PATH_MAX];
      snprintf(old_prefixed, sizeof(old_prefixed), "%s%s", prefix, entry_name);
      snprintf(new_prefixed, sizeof(new_prefixed), "%s%s", prefix, new_name);

      char *old_escaped = escape_for_shell(old_prefixed);
      char *new_escaped = escape_for_shell(new_prefixed);
      printf("mv \"%s\" \"%s\"\n", old_escaped, new_escaped);
      free(old_escaped);
      free(new_escaped);
      (*renamable)++;
    }

    free(new_name);
    (*looked_at)++;
  }

  closedir(d);
}

/**
 * @brief Main function to find files and generate rename commands.
 */
int main(int argc, char *argv[]) {
  bool recursive = false;

  if (argc == 2 && strcmp(argv[1], "-r") == 0) {
    recursive = true;
  } else if (argc != 1) {
    print_usage(stderr, argv[0]);
    return 2;
  }

  int looked_at = 0;
  int renamable = 0;
  int dirs_renamable = 0;

  scan_directory(".", recursive, "", &looked_at, &renamable, &dirs_renamable);

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