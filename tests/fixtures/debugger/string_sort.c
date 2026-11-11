#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int lexicographic_sort(const char *a, const char *b) { return strcmp(a, b); }

int lexicographic_sort_reverse(const char *a, const char *b) {
  return strcmp(b, a);
}

static int distinct_letters(const char *text) {
  bool seen[26] = {false};
  int count = 0;
  for (; *text != '\0'; ++text) {
    const int index = *text - 'a';
    if (!seen[index]) {
      seen[index] = true;
      ++count;
    }
  }
  return count;
}

int sort_by_number_of_distinct_characters(const char *a, const char *b) {
  const int difference = distinct_letters(a) - distinct_letters(b);
  return difference == 0 ? strcmp(a, b) : difference;
}

int sort_by_length(const char *a, const char *b) {
  const size_t left = strlen(a);
  const size_t right = strlen(b);
  if (left == right) {
    return strcmp(a, b);
  }
  return left < right ? -1 : 1;
}

void string_sort(char **strings, int count,
                 int (*compare)(const char *, const char *)) {
  for (int i = 1; i < count; ++i) {
    char *value = strings[i];
    int j = i;
    while (j > 0 && compare(strings[j - 1], value) > 0) {
      strings[j] = strings[j - 1];
      --j;
    }
    strings[j] = value;
  }
}

static char **read_strings(int count) {
  char **strings = calloc((size_t)count, sizeof *strings);
  if (strings == NULL) {
    return NULL;
  }
  for (int i = 0; i < count; ++i) {
    char word[2501];
    if (scanf("%2500s", word) == 1) {
      const size_t size = strlen(word) + 1;
      strings[i] = malloc(size);
      if (strings[i] != NULL) {
        memcpy(strings[i], word, size);
        continue;
      }
    }
    for (int j = 0; j < i; ++j) {
      free(strings[j]);
    }
    free(strings);
    return NULL;
  }
  return strings;
}

int main(void) {
  int count;
  if (scanf("%d", &count) != 1 || count <= 0) {
    return 1;
  }
  char **strings = read_strings(count);
  if (strings == NULL) {
    return 1;
  }

  int (*const comparators[])(const char *, const char *) = {
      lexicographic_sort, lexicographic_sort_reverse, sort_by_length,
      sort_by_number_of_distinct_characters};
  for (size_t i = 0; i < sizeof comparators / sizeof comparators[0]; ++i) {
    if (i != 0) {
      putchar('\n');
    }
    string_sort(strings, count, comparators[i]);
    for (int j = 0; j < count; ++j) {
      puts(strings[j]);
    }
  }

  for (int i = 0; i < count; ++i) {
    free(strings[i]);
  }
  free(strings);
  return 0;
}
