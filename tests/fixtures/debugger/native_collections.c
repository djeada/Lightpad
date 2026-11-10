#include <stdlib.h>
#include <string.h>

int main(void) {
  int fixed[3] = {10, 20, 30};
  char first[] = "alpha";
  char second[] = "beta";
  char *words[2] = {first, second};
  int *heap = malloc(4 * sizeof(*heap));
  char *buffer = malloc(4);
  if (!heap || !buffer) {
    free(heap);
    free(buffer);
    return 1;
  }
  for (int i = 0; i < 4; ++i)
    heap[i] = i + 40;
  memcpy(buffer, "A\0B\n", 4);
  int firstRow[3] = {1, 2, 3};
  int secondRow[3] = {10, 20, 30};
  int *matrix[2] = {firstRow, secondRow};
  int **matrixPointers = matrix;
  int checkpoint = fixed[0];
  free(heap);
  free(buffer);
  return checkpoint == 10 && words[1][0] == 'b' ? 0 : 2;
}
