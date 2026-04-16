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

  const QString text = "\"\"\"\nreturn\n\"\"\"\nreturn";
  document.setPlainText(text);
  highlighter.rehighlight();

  QCOMPARE(formatAt(document, 1, 0).foreground().color(),
           theme.singleLineCommentFormat);
  QCOMPARE(formatAt(document, 3, 0).foreground().color(),
           theme.keywordFormat_0);
}

QTEST_MAIN(TestPluginBasedSyntaxHighlighter)
#include "test_pluginbasedsyntaxhighlighter.moc"
