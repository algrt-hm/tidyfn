#include <stdbool.h> // stdbool.h for the bool type
#include <stdio.h>   // stdio.h for FILE

// Forward declarations of functions
bool is_ascii(const char *s);
float proportion_block_caps(const char *s);
char *handle_before_dot(const char *s);
char *remove_all_but_last_dot(const char *s);
char *sanitise_core(const char *s);
char *sanitise_final(const char *s);
char *sanitise(const char *s);
char *escape_for_shell(const char *s);
char *replace_substring(const char *str, const char *old, const char *new_sub);

#ifndef TESTING
static void print_usage(FILE *stream, const char *prog_name);
#endif