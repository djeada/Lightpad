#include <cstdlib>
#include <string>
#include <vector>

struct Record {
  int id;
  char label[8];
};

struct Node {
  int key;
  Node *next;
};

int main() {
  int fixed[6] = {65, 66, 67, 0, 255, -1};
  int matrix[2][3] = {{1, 2, 3}, {4, 5, 6}};
  char characters[6] = {'A', '\0', 'B', '\n', '\\', '\''};
  const char *message = "hello";
  const char *words[3] = {"alpha", "beta", ""};
  const char **wordPointers = words;
  int *heap = static_cast<int *>(std::malloc(240 * sizeof(int)));
  if (!heap)
    return 1;
  for (int i = 0; i < 240; ++i)
    heap[i] = i * 3;
  void *untyped = heap;
  int *missing = nullptr;
  std::vector<int> values = {12, 24, 36, 48};
  std::vector<std::vector<int>> rows = {{7, 8}, {9, 10, 11}};
  std::string text("A\0B\n", 4);
  std::vector<std::string> names = {"alpha", "", "beta"};
  std::vector<int> empty;
  Record records[2] = {{1, "first"}, {2, "second"}};
  Node tail = {22, nullptr};
  Node head = {11, &tail};
  Node *node = &head;
  int checkpoint = fixed[0];
  heap[1] = 999;
  values[0] = 77;
  text[0] = 'Z';
  std::free(heap);
  return checkpoint == 65 ? 0 : 2;
}
