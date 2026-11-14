#include <stdio.h>
#include <string.h>
int test_addition(void) {
  puts("C captured success");
  return 0;
}
int test_failure(void) {
  fprintf(stderr, "%s:4: Assertion failed: 2 + 2 == 5\n", __FILE__);
  return 1;
}
int main(int argc, char **argv) {
  if (argc < 2 || strcmp(argv[1], "skip") == 0)
    return 77;
  return strcmp(argv[1], "fail") == 0 ? test_failure() : test_addition();
}
