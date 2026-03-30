#include <iostream>
#include <string>
#include <vector>

class Greeter {
public:
  explicit Greeter(std::string name) : name_(std::move(name)) {}
  std::string greet() const { return "Hello, " + name_ + "!"; }

private:
  std::string name_;
};

int main() {
  std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
  for (const auto &name : names) {
    Greeter g(name);
    std::cout << g.greet() << std::endl;
  }
  return 0;
}
