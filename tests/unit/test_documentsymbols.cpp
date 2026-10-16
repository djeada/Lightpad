#include "core/editor/documentsymbols.h"
#include <QtTest/QtTest>

using namespace DocumentSymbols;

class TestDocumentSymbols : public QObject {
  Q_OBJECT

private:
  static QStringList names(const QList<Symbol> &s) {
    QStringList n;
    for (const Symbol &x : s) {
      n << x.name;
    }
    return n;
  }

private slots:
  void pythonIgnoresCallsAndFindsMethods() {
    const QString src = "import os\n"
                        "def fib(n):\n"
                        "    return fib(n-1) + fib(n-2)\n"
                        "class Greeter:\n"
                        "    def __init__(self, name):\n"
                        "        print(name)\n"
                        "    def hi(self):\n"
                        "        pass\n"
                        "if __name__ == '__main__':\n"
                        "    Greeter('x').hi()\n";
    const auto s = scan(src, "py");
    QCOMPARE(names(s), QStringList({"fib", "Greeter", "__init__", "hi"}));
    QCOMPARE(s[2].kind, Kind::Method);
    QCOMPARE(s[0].kind, Kind::Function);
  }

  void cppFindsDefinitionsNotCalls() {
    const QString src = "#include <iostream>\n"
                        "struct Point { int x; int y; };\n"
                        "int add(int a, int b) {\n"
                        "    return a + b;\n"
                        "}\n"
                        "int main() {\n"
                        "    std::cout << add(1, 2) << \"\\n\";\n"
                        "    undefined_fn(3);\n"
                        "    for (int i = 0; i < 3; ++i) { }\n"
                        "    return 0;\n"
                        "}\n"
                        "void Foo::bar(int x) {\n"
                        "    if (x > 0) { baz(x); }\n"
                        "}\n";
    const auto s = scan(src, "cpp");
    QCOMPARE(names(s), QStringList({"Point", "add", "main", "Foo::bar"}));
  }

  void cppClassMethods() {
    const QString src = "class Widget {\n"
                        "public:\n"
                        "  Widget();\n"
                        "  void paint(int x) {\n"
                        "    helper(x);\n"
                        "  }\n"
                        "};\n";
    const auto s = scan(src, "cpp");
    QVERIFY(names(s).contains("Widget"));
    QVERIFY(names(s).contains("paint"));
    QVERIFY(!names(s).contains("helper"));
  }

  void javascriptFunctionsAndArrows() {
    const QString src = "function greet(name) { return `hi ${name}`; }\n"
                        "const add = (a, b) => a + b;\n"
                        "class A {\n"
                        "  run() {\n"
                        "    greet('x');\n"
                        "  }\n"
                        "}\n";
    const auto s = scan(src, "js");
    QCOMPARE(names(s), QStringList({"greet", "add", "A", "run"}));
  }

  void markdownHeadings() {
    const QString src = "# Title\ntext\n```\n# not a heading\n```\n## Sub\n";
    QCOMPARE(names(scan(src, "md")), QStringList({"Title", "Sub"}));
  }

  void unknownLanguageIsEmptyForData() {
    QVERIFY(scan("{\"a\": 1}", "json").isEmpty());
  }
};

QTEST_MAIN(TestDocumentSymbols)
#include "test_documentsymbols.moc"
