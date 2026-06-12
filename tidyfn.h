#include <stdbool.h> // stdbool.h for the bool type
#include <stddef.h>  // stddef.h for size_t
#include <stdio.h>   // stdio.h for FILE

// Set of claimed names, used for collision detection during renaming
typedef struct {
  char **entries;
  size_t count;
  size_t capacity;
} NameSet;

// Forward declarations of functions
bool is_ascii(const char *s);
float proportion_block_caps(const char *s);
char *handle_before_dot(const char *s);
char *remove_all_but_last_dot(const char *s);
char *sanitise_core(const char *s);
char *sanitise(const char *s);
char *escape_for_shell(const char *s);
char *replace_substring(const char *str, const char *old, const char *new_sub);
char *sanitise_dirname(const char *original);

// Collision detection helpers
void nameset_init(NameSet *ns);
void nameset_free(NameSet *ns);
bool nameset_contains(const NameSet *ns, const char *name);
void nameset_add(NameSet *ns, const char *name);
char *resolve_collision(NameSet *claimed, const char *name, bool is_file);

#ifndef TESTING
static void print_usage(FILE *stream, const char *prog_name);
#endif