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

QTEST_MAIN(TestPluginBasedSyntaxHighlighter)
#include "test_pluginbasedsyntaxhighlighter.moc"
