#include "settings/theme.h"
#include "syntax/cppsyntaxplugin.h"
#include "syntax/glslsyntaxplugin.h"
#include "syntax/gosyntaxplugin.h"
#include "syntax/hlslsyntaxplugin.h"
#include "syntax/javasyntaxplugin.h"
#include "syntax/markdownsyntaxplugin.h"
#include "syntax/metalsyntaxplugin.h"
#include "syntax/pluginbasedsyntaxhighlighter.h"
#include "syntax/pythonsyntaxplugin.h"
#include "syntax/rustsyntaxplugin.h"
#include "syntax/shellsyntaxplugin.h"
#include "syntax/typescriptsyntaxplugin.h"
#include "syntax/wgslsyntaxplugin.h"
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QtTest/QtTest>
#include <memory>

static std::unique_ptr<ISyntaxPlugin>
pluginForLanguage(const QString &language) {
  std::unique_ptr<ISyntaxPlugin> plugin;
  if (language == "java")
    plugin = std::make_unique<JavaSyntaxPlugin>();
  if (language == "typescript")
    plugin = std::make_unique<TypeScriptSyntaxPlugin>();
  if (language == "cpp")
    plugin = std::make_unique<CppSyntaxPlugin>();
  if (language == "rust")
    plugin = std::make_unique<RustSyntaxPlugin>();
  if (language == "go")
    plugin = std::make_unique<GoSyntaxPlugin>();
  if (language == "glsl")
    plugin = std::make_unique<GlslSyntaxPlugin>();
  if (language == "hlsl")
    plugin = std::make_unique<HlslSyntaxPlugin>();
  if (language == "metal")
    plugin = std::make_unique<MetalSyntaxPlugin>();
  if (language == "wgsl")
    plugin = std::make_unique<WgslSyntaxPlugin>();
  return plugin;
}

static QTextCharFormat formatAt(const QTextDocument &document, int blockNumber,
                                int column) {
  QTextBlock block = document.findBlockByNumber(blockNumber);
  if (!block.isValid() || !block.layout()) {
    return QTextCharFormat();
  }

  const auto formats = block.layout()->formats();
  for (const QTextLayout::FormatRange &range : formats) {
    if (column >= range.start && column < range.start + range.length) {
      return range.format;
    }
  }

  return QTextCharFormat();
}

class TestPluginBasedSyntaxHighlighter : public QObject {
  Q_OBJECT

private slots:
  void testMemberAccess_data();
  void testMemberAccess();
  void testCppExpressionsAndExplicitTypeContexts();
  void testCppRawStringProtection();
  void testImportedJavaAndTypeScriptTypes_data();
  void testImportedJavaAndTypeScriptTypes();
  void testCustomTypes_data();
  void testCustomTypes();
  void testTypeIndexIgnoresLiteralsAndComments();
  void testTypeIndexTracksEditsAndViewport();
  void testCppAliasesAndControlFlow();
  void testTypeScriptLiteralsAndDollarNames();
  void testExternalTypesInDeclarations();
  void testShellCommentsOverrideKeywords();
  void testShellStringsOverrideKeywords();
  void testPythonMultilineBlocksOverrideKeywords();
  void testPythonDefinitionsDecoratorsAndMembersAreSemantic();
  void testPythonBuiltinsAndEscapesStayDistinct();
  void testPythonFStringExpressionsAreHighlighted();
  void testPythonModuleDocstringDoesNotSwallowFollowingCode();
  void testPythonMultilineStateSurvivesViewportSkippedBlocks();
  void testPythonMainThreadLoopsSurviveDirectViewportJump();
  void testPythonStringClosingAndReopeningOnSameLine();
  void testCppCommentDelimiterInsideStringIsNotComment();
  void testCppLineCommentHidesBlockCommentStart();
  void testCppBlockCommentAfterStringOnSameLine();
  void testMarkdownCommentInsideCodeFenceDoesNotLeak();
};

void TestPluginBasedSyntaxHighlighter::
    testPythonStringClosingAndReopeningOnSameLine() {
  Theme theme;
  PythonSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text = "x = \"\"\"start\n"
                       "end\"\"\" + \"\"\"again\n"
                       "return\n"
                       "\"\"\"\n"
                       "return";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 1, 0).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 1, 12).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 2, 0).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 4, 0).foreground().color(),
           theme.keywordFormat_0);
}

void TestPluginBasedSyntaxHighlighter::
    testCppCommentDelimiterInsideStringIsNotComment() {
  Theme theme;
  CppSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text = "const char *open = \"/*\";\n"
                       "return 0;";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 0, text.indexOf("/*")).foreground().color(),
           theme.quotationFormat);
  QVERIFY(formatAt(document, 1, 0).foreground().color() !=
          theme.singleLineCommentFormat);
  QCOMPARE(document.findBlockByNumber(0).userState(), 0);
}

void TestPluginBasedSyntaxHighlighter::
    testCppLineCommentHidesBlockCommentStart() {
  Theme theme;
  CppSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text = "int a; // see /* here\n"
                       "return 0;";
  document.setPlainText(text);
  highlighter.rehighlight();

  QVERIFY(formatAt(document, 1, 0).foreground().color() !=
          theme.singleLineCommentFormat);
}

void TestPluginBasedSyntaxHighlighter::
    testCppBlockCommentAfterStringOnSameLine() {
  Theme theme;
  CppSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text = "f(\"*/\"); /* note\n"
                       "still comment */ return 0;";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 0, text.indexOf("note")).foreground().color(),
           theme.singleLineCommentFormat);
  QCOMPARE(formatAt(document, 1, 0).foreground().color(),
           theme.singleLineCommentFormat);
  const QString secondLine = "still comment */ return 0;";
  QVERIFY(formatAt(document, 1, secondLine.indexOf("return"))
              .foreground()
              .color() != theme.singleLineCommentFormat);
}

void TestPluginBasedSyntaxHighlighter::
    testMarkdownCommentInsideCodeFenceDoesNotLeak() {
  Theme theme;
  MarkdownSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text = "```\n"
                       "<!-- inside code\n"
                       "```\n"
                       "after";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 1, 0).foreground().color(),
           theme.singleLineCommentFormat);
  QCOMPARE(document.findBlockByNumber(2).userState(), 0);
  QVERIFY(formatAt(document, 3, 0).foreground().color() !=
          theme.singleLineCommentFormat);
}

void TestPluginBasedSyntaxHighlighter::testShellCommentsOverrideKeywords() {
  Theme theme;
  ShellSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text = "echo test # if then fi";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 0, text.indexOf("echo")).foreground().color(),
           theme.keywordFormat_2);
  QCOMPARE(formatAt(document, 0, text.indexOf("if")).foreground().color(),
           theme.singleLineCommentFormat);
  QCOMPARE(formatAt(document, 0, text.indexOf("then")).foreground().color(),
           theme.singleLineCommentFormat);
  QCOMPARE(formatAt(document, 0, text.indexOf("fi")).foreground().color(),
           theme.singleLineCommentFormat);
}

void TestPluginBasedSyntaxHighlighter::testShellStringsOverrideKeywords() {
  Theme theme;
  ShellSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text = "echo \"if then fi\"";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 0, text.indexOf("echo")).foreground().color(),
           theme.keywordFormat_2);
  QCOMPARE(formatAt(document, 0, text.indexOf("if")).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 0, text.indexOf("then")).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 0, text.indexOf("fi")).foreground().color(),
           theme.quotationFormat);
}

void TestPluginBasedSyntaxHighlighter::
    testPythonMultilineBlocksOverrideKeywords() {
  Theme theme;
  PythonSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text = "r\"\"\"\nreturn\n\"\"\"\nreturn";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 0, 0).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 1, 0).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 3, 0).foreground().color(),
           theme.keywordFormat_0);
}

void TestPluginBasedSyntaxHighlighter::
    testPythonDefinitionsDecoratorsAndMembersAreSemantic() {
  Theme theme;
  PythonSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text = "@pytest.mark.parametrize(\"value\", [1])\n"
                       "class Demo:\n"
                       "    def build_value(self, source):\n"
                       "        return source.value\n";
  document.setPlainText(text);
  highlighter.rehighlight();

  const int decoratorColumn = text.indexOf("pytest");
  QCOMPARE(formatAt(document, 0, decoratorColumn).foreground().color(),
           theme.keywordFormat_1);
  QCOMPARE(
      formatAt(document, 0, text.indexOf("parametrize")).foreground().color(),
      theme.keywordFormat_1);

  const QTextCharFormat classFormat =
      formatAt(document, 1, text.indexOf("Demo") - text.indexOf("class Demo"));
  QCOMPARE(classFormat.foreground().color(), theme.classFormat);
  QCOMPARE(classFormat.fontWeight(), static_cast<int>(QFont::Bold));

  const int functionColumn = text.indexOf("build_value") -
                             text.indexOf("    def build_value(self, source):");
  const QTextCharFormat functionFormat = formatAt(document, 2, functionColumn);
  QCOMPARE(functionFormat.foreground().color(), theme.functionFormat);
  QCOMPARE(functionFormat.fontWeight(), static_cast<int>(QFont::Bold));
  QVERIFY(!functionFormat.fontItalic());

  QCOMPARE(formatAt(document, 2,
                    text.indexOf("self") -
                        text.indexOf("    def build_value(self, source):"))
               .foreground()
               .color(),
           theme.keywordFormat_2);
  QCOMPARE(formatAt(document, 3,
                    text.lastIndexOf("value") -
                        text.indexOf("        return source.value"))
               .foreground()
               .color(),
           theme.keywordFormat_2);
}

void TestPluginBasedSyntaxHighlighter::
    testPythonBuiltinsAndEscapesStayDistinct() {
  Theme theme;
  PythonSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text = "print(len(items), CustomType(), helper())\n"
                       "message = \"line\\n\"";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 0, text.indexOf("print")).foreground().color(),
           theme.keywordFormat_1);
  QCOMPARE(formatAt(document, 0, text.indexOf("len")).foreground().color(),
           theme.keywordFormat_1);
  QCOMPARE(
      formatAt(document, 0, text.indexOf("CustomType")).foreground().color(),
      theme.classFormat);
  const QTextCharFormat helperFormat =
      formatAt(document, 0, text.indexOf("helper"));
  QCOMPARE(helperFormat.foreground().color(), theme.functionFormat);
  QVERIFY(!helperFormat.fontItalic());
  QCOMPARE(formatAt(document, 1,
                    text.indexOf("\\n") - text.indexOf("message = \"line\\n\""))
               .foreground()
               .color(),
           theme.escapeFormat);
}

void TestPluginBasedSyntaxHighlighter::
    testPythonFStringExpressionsAreHighlighted() {
  Theme theme;
  PythonSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text =
      "print(f\"my pid is: {os.getpid()}, returned: {value}\")";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 0, text.indexOf("print")).foreground().color(),
           theme.keywordFormat_1);
  QCOMPARE(formatAt(document, 0, text.indexOf("getpid")).foreground().color(),
           theme.functionFormat);
  QCOMPARE(formatAt(document, 0, text.indexOf("my pid")).foreground().color(),
           theme.quotationFormat);
}

void TestPluginBasedSyntaxHighlighter::
    testPythonModuleDocstringDoesNotSwallowFollowingCode() {
  Theme theme;
  PythonSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text =
      "\"\"\"\n"
      "Reader Writer Pattern\n"
      "\n"
      "shared data <- concurrent access from multiple entities\n"
      "\n"
      "\"\"\"\n"
      "\n"
      "from threading import Thread, Lock\n"
      "import random\n"
      "import time\n"
      "\n"
      "class ReaderWriterLock:\n"
      "\n"
      "    def __init__(self):\n"
      "        self._data = 0\n"
      "        print(f\"READER {reader_id} reads the data: {self._data}\")\n"
      "\n"
      "if __name__ == \"__main__\":\n"
      "    main()\n";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 0, 0).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 1, 0).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 7, 0).foreground().color(),
           theme.keywordFormat_0);
  QCOMPARE(formatAt(document, 11, 0).foreground().color(),
           theme.keywordFormat_0);
  QCOMPARE(formatAt(document, 13, 4).foreground().color(),
           theme.keywordFormat_0);
  QCOMPARE(formatAt(document, 15, 8).foreground().color(),
           theme.keywordFormat_1);
  QCOMPARE(formatAt(document, 15, 17).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 17, 0).foreground().color(),
           theme.keywordFormat_0);
  QCOMPARE(formatAt(document, 17, 3).foreground().color(),
           theme.constantFormat);
}

void TestPluginBasedSyntaxHighlighter::
    testPythonMultilineStateSurvivesViewportSkippedBlocks() {
  Theme theme;
  PythonSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);
  highlighter.setVisibleBlockRange(2, 5);

  const QString text = "def before():\n"
                       "    \"\"\"\n"
                       "    return is still text\n"
                       "    \"\"\"\n"
                       "    return 1\n";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 2, 4).foreground().color(),
           theme.quotationFormat);
  QCOMPARE(formatAt(document, 4, 4).foreground().color(),
           theme.keywordFormat_0);
}

void TestPluginBasedSyntaxHighlighter::
    testPythonMainThreadLoopsSurviveDirectViewportJump() {
  Theme theme;
  PythonSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);

  const QString text =
      "\"\"\"\n"
      "Reader Writer Pattern\n"
      "\n"
      "shared data <- concurrent access from multiple entities\n"
      "\n"
      "writers require exlcusive access\n"
      "readers can read concurrently (improved throughput)\n"
      "\n"
      "use cases:\n"
      "-> caches with rare updates\n"
      "-> config shared between threads\n"
      "-> database like systems with read heavy load\n"
      "\n"
      "\"\"\"\n"
      "\n"
      "from threading import Thread, Lock\n"
      "import random\n"
      "import time\n"
      "\n"
      "class ReaderWriterLock:\n"
      "\n"
      "    def __init__(self):\n"
      "        self._data = 0\n"
      "        self._reader_count = 0\n"
      "        self._reader_lock = Lock()\n"
      "        self._writer_lock = Lock()\n"
      "        \n"
      "    def read(self, reader_id):\n"
      "        print(f\"READER {reader_id} reads the data: {self._data}\")\n"
      "        \n"
      "    def write(self, writer_id, val):\n"
      "        with self._writer_lock:\n"
      "            print(f\"Writer {writer_id} increments the {self._data} by "
      "{val}\")\n"
      "\n"
      "def reader(rw_lock, reader_id, n):\n"
      "    for _ in range(n):\n"
      "        rw_lock.read(reader_id)\n"
      "\n"
      "def writer(rw_lock, writer_id, n):\n"
      "    for i in range(n):\n"
      "        rw_lock.write(writer_id, i)\n"
      "        \n"
      "def main():\n"
      "    num_readers = 5\n"
      "    num_writers = 3\n"
      "    n = 10\n"
      "    \n"
      "    threads = list()\n"
      "    rw_lock = ReaderWriterLock()\n"
      "    \n"
      "    threads.extend([Thread(target=writer, args=(rw_lock, i, n)) for i "
      "in range(num_writers)])\n"
      "    threads.extend([Thread(target=reader, args=(rw_lock, i, n)) for i "
      "in range(num_readers)])\n"
      "\n"
      "    # random.shuffle(threads)\n"
      "    \n"
      "    for thread in threads:\n"
      "        thread.start()\n"
      "        \n"
      "    for thread in threads:\n"
      "        thread.join()    \n"
      "        \n"
      "if __name__ == \"__main__\":\n"
      "    main()\n";
  document.setPlainText(text);

  highlighter.setVisibleBlockRange(55, 62);

  QCOMPARE(formatAt(document, 55, 4).foreground().color(),
           theme.keywordFormat_0);
  QCOMPARE(formatAt(document, 55, 15).foreground().color(),
           theme.keywordFormat_0);
  QCOMPARE(formatAt(document, 56, 15).foreground().color(),
           theme.functionFormat);
  QCOMPARE(formatAt(document, 58, 4).foreground().color(),
           theme.keywordFormat_0);
  QCOMPARE(formatAt(document, 58, 15).foreground().color(),
           theme.keywordFormat_0);
  QCOMPARE(formatAt(document, 59, 15).foreground().color(),
           theme.functionFormat);
  QCOMPARE(formatAt(document, 61, 0).foreground().color(),
           theme.keywordFormat_0);
}

void TestPluginBasedSyntaxHighlighter::testCustomTypes_data() {
  QTest::addColumn<QString>("language");
  QTest::addColumn<QString>("declaration");
  QTest::newRow("cpp-struct")
      << "cpp" << "struct custom_record { int count; };";
  QTest::newRow("cpp-typedef-struct")
      << "cpp" << "typedef struct { int count; } custom_record;";
  QTest::newRow("cpp-callback")
      << "cpp" << "typedef void (*custom_record)(int);";
  QTest::newRow("cpp-aligned-struct")
      << "cpp" << "struct alignas(16) custom_record {};";
  QTest::newRow("cpp-class") << "cpp" << "class custom_record final {};";
  QTest::newRow("cpp-enum") << "cpp" << "enum class custom_record { Ready };";
  QTest::newRow("java-record")
      << "java" << "record custom_record(int count) {}";
  QTest::newRow("typescript-interface")
      << "typescript" << "interface custom_record { count: number }";
  QTest::newRow("typescript-alias")
      << "typescript" << "type custom_record = { count: number };";
  QTest::newRow("rust") << "rust" << "pub struct custom_record(i32);";
  QTest::newRow("go") << "go" << "type custom_record struct { Count int }";
  QTest::newRow("glsl") << "glsl" << "struct custom_record { vec3 normal; };";
  QTest::newRow("hlsl") << "hlsl" << "struct custom_record { float3 normal; };";
  QTest::newRow("metal") << "metal"
                         << "struct custom_record { float3 normal; };";
  QTest::newRow("wgsl") << "wgsl" << "struct custom_record { normal: vec3f }";
}

void TestPluginBasedSyntaxHighlighter::testCustomTypes() {
  QFETCH(QString, language);
  QFETCH(QString, declaration);
  auto plugin = pluginForLanguage(language);
  QVERIFY(plugin);
  Theme theme;
  theme.typeFormat = QColor("#60e0ac");
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(plugin.get(), theme, "", &document);
  document.setPlainText(declaration + "\ncustom_record\ncustom_record ()\n"
                                      "// custom_record\n\"custom_record\"");
  highlighter.rehighlight();
  QCOMPARE(formatAt(document, 0, declaration.indexOf("custom_record"))
               .foreground()
               .color(),
           theme.classFormat);
  QCOMPARE(
      formatAt(document, 0, declaration.indexOf("custom_record")).fontWeight(),
      int(QFont::Bold));
  for (int block : {1, 2}) {
    QCOMPARE(formatAt(document, block, 0).foreground().color(),
             theme.typeFormat);
    QCOMPARE(formatAt(document, block, 0).fontWeight(), int(QFont::Normal));
  }
  QCOMPARE(formatAt(document, 3, 3).foreground().color(),
           theme.singleLineCommentFormat);
  QCOMPARE(formatAt(document, 4, 1).foreground().color(),
           theme.quotationFormat);
}

void TestPluginBasedSyntaxHighlighter::
    testTypeIndexIgnoresLiteralsAndComments() {
  Theme theme;
  theme.keywordFormat_2 = QColor("#ba84d1");
  CppSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);
  document.setPlainText("// struct LineGhost {};\n"
                        "/* struct BlockGhost {}; */\n"
                        "\"struct StringGhost {};\"\n"
                        "R\"tag(struct RawGhost {}; \"quotes\")tag\"\n"
                        "LineGhost BlockGhost StringGhost RawGhost\n"
                        "struct Real {};\nobject.Real; pointer->Real;\n"
                        "/* unfinished\nstruct Hidden {};\n");
  highlighter.rehighlight();
  for (const auto &name :
       {"LineGhost", "BlockGhost", "StringGhost", "RawGhost"}) {
    const QString line = document.findBlockByNumber(4).text();
    QVERIFY(formatAt(document, 4, line.indexOf(name)).foreground().color() !=
            theme.classFormat);
  }
  QVERIFY(formatAt(document, 6, 7).foreground().color() != theme.classFormat);
  QVERIFY(formatAt(document, 6, 22).foreground().color() != theme.classFormat);
}

void TestPluginBasedSyntaxHighlighter::testTypeIndexTracksEditsAndViewport() {
  Theme theme;
  CppSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);
  document.setPlainText("record();\n" + QString(150, '\n') +
                        "struct record {};\n");
  highlighter.setVisibleBlockRange(0, 1);
  highlighter.rehighlight();
  QCOMPARE(formatAt(document, 0, 0).foreground().color(), theme.classFormat);
  QTextCursor cursor(&document);
  cursor.setPosition(document.toPlainText().indexOf("struct record"));
  cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
  QTRY_VERIFY(formatAt(document, 0, 0).foreground().color() !=
              theme.classFormat);
  cursor.insertText("struct record {};\n");
  QTRY_COMPARE(formatAt(document, 0, 0).foreground().color(),
               theme.classFormat);
}

void TestPluginBasedSyntaxHighlighter::testCppAliasesAndControlFlow() {
  Theme theme;
  CppSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);
  document.setPlainText("using UserId = unsigned;\ntypedef unsigned count_t;\n"
                        "UserId id; count_t count;\n"
                        "if (ready) update ();\nwhile(ready) update();\n"
                        "sizeof(UserId);\n");
  highlighter.rehighlight();
  QCOMPARE(formatAt(document, 0, 6).foreground().color(), theme.classFormat);
  QCOMPARE(formatAt(document, 1, 17).foreground().color(), theme.classFormat);
  QCOMPARE(formatAt(document, 2, 0).foreground().color(), theme.classFormat);
  QCOMPARE(formatAt(document, 2, 11).foreground().color(), theme.classFormat);
  QCOMPARE(formatAt(document, 3, 0).foreground().color(),
           theme.keywordFormat_1);
  QCOMPARE(formatAt(document, 4, 0).foreground().color(),
           theme.keywordFormat_1);
  QCOMPARE(formatAt(document, 5, 0).foreground().color(),
           theme.keywordFormat_0);
  QCOMPARE(formatAt(document, 3, 11).foreground().color(),
           theme.functionFormat);
}

void TestPluginBasedSyntaxHighlighter::testExternalTypesInDeclarations() {
  Theme theme;
  CppSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);
  document.setPlainText(
      "External value;\nexternal_t *value;\n"
      "void process(const External &value);\nreturn value;\n");
  highlighter.rehighlight();
  QCOMPARE(formatAt(document, 0, 0).foreground().color(), theme.classFormat);
  QCOMPARE(formatAt(document, 1, 0).foreground().color(), theme.classFormat);
  QCOMPARE(formatAt(document, 2, 19).foreground().color(), theme.classFormat);
  QCOMPARE(formatAt(document, 3, 0).foreground().color(),
           theme.keywordFormat_1);
}

void TestPluginBasedSyntaxHighlighter::testTypeScriptLiteralsAndDollarNames() {
  Theme theme;
  TypeScriptSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);
  document.setPlainText(
      "const label = 'interface Ghost {}';\nGhost();\n"
      "interface $Model { id: number }\nconst model: $Model = {};\n");
  highlighter.rehighlight();
  QCOMPARE(formatAt(document, 1, 0).foreground().color(), theme.functionFormat);
  QCOMPARE(formatAt(document, 2, 10).foreground().color(), theme.classFormat);
  QCOMPARE(formatAt(document, 3, 13).foreground().color(), theme.typeFormat);
}

void TestPluginBasedSyntaxHighlighter::testMemberAccess_data() {
  QTest::addColumn<QString>("language");
  for (const auto &language : {"cpp", "rust", "go", "glsl", "hlsl", "metal",
                               "wgsl", "java", "typescript"})
    QTest::newRow(language) << QString(language);
}

void TestPluginBasedSyntaxHighlighter::testMemberAccess() {
  QFETCH(QString, language);
  auto plugin = pluginForLanguage(language);
  Theme theme;
  theme.keywordFormat_2 = QColor("#ba84d1");
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(plugin.get(), theme, "", &document);
  document.setPlainText("mesh.position.x;\nmesh.update ();\n"
                        "// mesh.position\n\"mesh.position\"\n...spread;\n");
  highlighter.rehighlight();
  QCOMPARE(formatAt(document, 0, 5).foreground().color(),
           theme.keywordFormat_2);
  QCOMPARE(formatAt(document, 0, 14).foreground().color(),
           theme.keywordFormat_2);
  QCOMPARE(formatAt(document, 1, 5).foreground().color(), theme.functionFormat);
  QCOMPARE(formatAt(document, 2, 8).foreground().color(),
           theme.singleLineCommentFormat);
  QCOMPARE(formatAt(document, 3, 6).foreground().color(),
           theme.quotationFormat);
  QVERIFY(formatAt(document, 4, 3).foreground().color() !=
          theme.keywordFormat_2);
}

void TestPluginBasedSyntaxHighlighter::
    testCppExpressionsAndExplicitTypeContexts() {
  Theme theme;
  theme.typeFormat = QColor("#60e0ac");
  CppSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);
  document.setPlainText(
      "left * right;\nflags & mask;\n"
      "new vendor::External ();\nstatic_cast<vendor::External *>(value);\n"
      "struct lower_type {};\nlower_type* value;\n"
      "object->position;\nobject->update ();\n"
      "External *value;\nexternal_t &value;\n");
  highlighter.rehighlight();
  QVERIFY(formatAt(document, 0, 0).foreground().color() != theme.typeFormat);
  QVERIFY(formatAt(document, 1, 0).foreground().color() != theme.typeFormat);
  QCOMPARE(formatAt(document, 2, 12).foreground().color(), theme.typeFormat);
  QCOMPARE(formatAt(document, 3, 20).foreground().color(), theme.typeFormat);
  QCOMPARE(formatAt(document, 3, 0).foreground().color(),
           theme.keywordFormat_1);
  QCOMPARE(formatAt(document, 5, 0).foreground().color(), theme.typeFormat);
  QCOMPARE(formatAt(document, 6, 8).foreground().color(),
           theme.keywordFormat_2);
  QCOMPARE(formatAt(document, 7, 8).foreground().color(), theme.functionFormat);
  QCOMPARE(formatAt(document, 8, 0).foreground().color(), theme.typeFormat);
  QCOMPARE(formatAt(document, 9, 0).foreground().color(), theme.typeFormat);
}

void TestPluginBasedSyntaxHighlighter::testCppRawStringProtection() {
  Theme theme;
  CppSyntaxPlugin plugin;
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(&plugin, theme, "", &document);
  const QString raw =
      R"cpp(auto s = R"tag("quotes" /* mesh.position \n struct Ghost {}; )tag";)cpp";
  document.setPlainText(raw + "\nGhost();\nreturn 0;\n");
  highlighter.rehighlight();
  for (const auto &part : {"quotes", "/*", "position", "\\n", "Ghost"})
    QCOMPARE(formatAt(document, 0, raw.indexOf(part)).foreground().color(),
             theme.quotationFormat);
  QCOMPARE(formatAt(document, 1, 0).foreground().color(), theme.functionFormat);
  QCOMPARE(formatAt(document, 2, 0).foreground().color(),
           theme.keywordFormat_1);
  QCOMPARE(document.firstBlock().userState(), 0);
}

void TestPluginBasedSyntaxHighlighter::
    testImportedJavaAndTypeScriptTypes_data() {
  QTest::addColumn<QString>("language");
  QTest::newRow("java") << "java";
  QTest::newRow("typescript") << "typescript";
}

void TestPluginBasedSyntaxHighlighter::testImportedJavaAndTypeScriptTypes() {
  QFETCH(QString, language);
  auto plugin = pluginForLanguage(language);
  Theme theme;
  theme.typeFormat = QColor("#60e0ac");
  QTextDocument document;
  PluginBasedSyntaxHighlighter highlighter(plugin.get(), theme, "", &document);
  document.setPlainText(
      "new library.External ();\nclass Local extends External {}\n"
      "class Local implements Contract {}\nvalue instanceof External\n");
  highlighter.rehighlight();
  QCOMPARE(formatAt(document, 0, 12).foreground().color(), theme.typeFormat);
  QCOMPARE(formatAt(document, 1, 20).foreground().color(), theme.typeFormat);
  QCOMPARE(formatAt(document, 2, 23).foreground().color(), theme.typeFormat);
  QCOMPARE(formatAt(document, 3, 17).foreground().color(), theme.typeFormat);
}

QTEST_MAIN(TestPluginBasedSyntaxHighlighter)
#include "test_pluginbasedsyntaxhighlighter.moc"
