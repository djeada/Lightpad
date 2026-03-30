"""Sample Python project for editor testing."""


class Calculator:
    """A simple calculator class."""

    def __init__(self):
        self.history = []

    def add(self, a: float, b: float) -> float:
        result = a + b
        self.history.append(f"{a} + {b} = {result}")
        return result

    def subtract(self, a: float, b: float) -> float:
        result = a - b
        self.history.append(f"{a} - {b} = {result}")
        return result

    def multiply(self, a: float, b: float) -> float:
        result = a * b
        self.history.append(f"{a} * {b} = {result}")
        return result

    def divide(self, a: float, b: float) -> float:
        if b == 0:
            raise ValueError("Cannot divide by zero")
        result = a / b
        self.history.append(f"{a} / {b} = {result}")
        return result


def main():
    calc = Calculator()
    print(calc.add(2, 3))
    print(calc.subtract(10, 4))
    print(calc.multiply(3, 7))
    print(calc.divide(15, 3))
    print("History:", calc.history)


if __name__ == "__main__":
    main()
