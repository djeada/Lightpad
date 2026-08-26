#include "settings/theme.h"
#include "syntax/pluginbasedsyntaxhighlighter.h"
#include "syntax/pythonsyntaxplugin.h"
#include "syntax/shellsyntaxplugin.h"
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QtTest/QtTest>

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
  void testShellCommentsOverrideKeywords();
  void testShellStringsOverrideKeywords();
  void testPythonMultilineBlocksOverrideKeywords();
  void testPythonDefinitionsDecoratorsAndMembersAreSemantic();
  void testPythonBuiltinsAndEscapesStayDistinct();
  void testPythonFStringExpressionsAreHighlighted();
  void testPythonModuleDocstringDoesNotSwallowFollowingCode();
  void testPythonMultilineStateSurvivesViewportSkippedBlocks();
  void testPythonMainThreadLoopsSurviveDirectViewportJump();
};

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

QTEST_MAIN(TestPluginBasedSyntaxHighlighter)
#include "test_pluginbasedsyntaxhighlighter.moc"
