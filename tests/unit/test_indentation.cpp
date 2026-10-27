#include "core/editor/indentation.h"
#include <QTextDocument>
#include <QtTest>

class TestIndentation : public QObject {
  Q_OBJECT
private slots:
  void newline_data() {
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("width");
    auto add = [](const char *name, const QString &before, const QString &after,
                  const QString &language = "py", int width = 4) {
      QTest::newRow(name) << before << after << language << width;
    };
    add("empty", "|", "\n|");
    add("python-function", "def work():|", "def work():\n    |");
    add("python-nested-if", "def work():\n    if ready:|",
        "def work():\n    if ready:\n        |");
    add("python-trailing-comment", "    if ready:  # check:|",
        "    if ready:  # check:\n        |");
    add("python-async", "async with resource() as value:|",
        "async with resource() as value:\n    |");
    add("python-match", "    case {'name': value}:|",
        "    case {'name': value}:\n        |");
    add("two-space-setting", "  if ready:|", "  if ready:\n    |", "py", 2);
    add("eight-space-setting", "        if ready:|",
        "        if ready:\n                |", "py", 8);
    add("same-level", "    work()|", "    work()\n    |");
    add("blank-line", "    |", "    \n    |");
    add("middle-of-code", "    result = |calculate()",
        "    result = \n    |calculate()");
    add("before-indent", "|    work()", "\n|    work()");
    add("string-colon", "    value = 'if ready:'|",
        "    value = 'if ready:'\n    |");
    add("comment-colon", "    # if ready:|", "    # if ready:\n    |");
    add("lambda-colon", "    callback = lambda item:|",
        "    callback = lambda item:\n    |");
    add("dictionary-entry", "values = {\n    'item':|",
        "values = {\n    'item':\n    |");
    add("triple-string", "text = '''\n    if ready:|\n'''",
        "text = '''\n    if ready:\n    |\n'''");
    add("multiline-header", "if (\n    ready\n):|",
        "if (\n    ready\n):\n    |");
    add("nested-multiline-header", "    if (\n        ready\n    ):|",
        "    if (\n        ready\n    ):\n        |");
    add("hanging-list", "values = [|", "values = [\n    |");
    add("hanging-call", "    result = calculate(|",
        "    result = calculate(\n        |");
    add("aligned-call", "calculate(first,|", "calculate(first,\n          |");
    add("continued-call", "calculate(\n    first,|",
        "calculate(\n    first,\n    |");
    add("backslash", "value = first + \\|", "value = first + \\\n    |");
    add("continued-backslash", "value = first + \\\n    second + \\|",
        "value = first + \\\n    second + \\\n    |");
    add("empty-python-call", "calculate(|)", "calculate(\n    |\n)");
    add("empty-python-list", "    values = [|]",
        "    values = [\n        |\n    ]");
    add("pair-with-whitespace", "    values = [|  ] + tail",
        "    values = [\n        |\n    ] + tail");
    add("cpp-block", "    if (ready) { |}",
        "    if (ready) { \n        |\n    }", "cpp");
    add("cpp-comment", "    if (ready) { // comment {|",
        "    if (ready) { // comment {\n        |", "cpp");
    add("cpp-inline-body", "    if (ready) { run();|",
        "    if (ready) { run();\n        |", "cpp");
    add("cpp-string", "    auto value = \"{\";|",
        "    auto value = \"{\";\n    |", "cpp");
    add("cpp-tabs", "\tif (ready) {|}", "\tif (ready) {\n\t\t|\n\t}", "cpp");
    add("before-existing-indent-in-block", "if (ready) {\n|    work();\n}",
        "if (ready) {\n\n|    work();\n}", "cpp");
    add("preserve-blank-line-indent-in-block", "if (ready) {\n  |\n}",
        "if (ready) {\n  \n  |\n}", "cpp");
    add("yaml-mapping", "  service: # comment|", "  service: # comment\n    |",
        "yaml", 2);
    add("plain-text", "  text:|", "  text:\n  |", "plaintext");
  }

  void newline() {
    QFETCH(QString, before);
    QFETCH(QString, after);
    QFETCH(QString, language);
    QFETCH(int, width);
    const int position = before.indexOf('|');
    before.remove(position, 1);
    const int expected = after.indexOf('|');
    after.remove(expected, 1);
    QTextDocument document(before);
    QTextCursor cursor(&document);
    cursor.setPosition(position);
    EditorIndentation::insertNewline(cursor,
                                     {language, width, true, language == "py"});
    QCOMPARE(document.toPlainText(), after);
    QCOMPARE(cursor.position(), expected);
    QVERIFY(!cursor.hasSelection());
    document.undo();
    QCOMPARE(document.toPlainText(), before);
    document.redo();
    QCOMPARE(document.toPlainText(), after);
  }

  void backspace_data() {
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::addColumn<int>("width");
    auto add = [](const char *name, const QString &before, const QString &after,
                  int width = 4) {
      QTest::newRow(name) << before << after << width;
    };
    add("one-indent", "    |work()", "|work()");
    add("nested-indent", "        |work()", "    |work()");
    add("whitespace-only", "previous\n        |", "previous\n    |");
    add("partial-indent", "      |work()", "    |work()");
    add("partial-first-indent", "   |work()", "|work()");
    add("middle-of-indent", "      |  work()", "    |  work()");
    add("one-tab", "\t|work()", "|work()");
    add("two-tabs", "\t\t|work()", "\t|work()");
    add("tab-and-spaces", "\t    |work()", "\t|work()");
    add("tab-and-partial-spaces", "\t  |work()", "\t|work()");
    add("spaces-and-tab", "  \t|work()", "|work()");
    add("two-space-setting", "    |work()", "  |work()", 2);
    add("eight-space-setting", "                |work()", "        |work()", 8);
    add("invalid-width", "  |work()", " |work()", 0);
  }

  void backspace() {
    QFETCH(QString, before);
    QFETCH(QString, after);
    QFETCH(int, width);
    const int position = before.indexOf('|');
    before.remove(position, 1);
    const int expected = after.indexOf('|');
    after.remove(expected, 1);
    QTextDocument document(before);
    QTextCursor cursor(&document);
    cursor.setPosition(position);
    QVERIFY(EditorIndentation::backspace(cursor, width));
    QCOMPARE(document.toPlainText(), after);
    QCOMPARE(cursor.position(), expected);
    document.undo();
    QCOMPARE(document.toPlainText(), before);
    document.redo();
    QCOMPARE(document.toPlainText(), after);
  }

  void normalDeletionIsLeftToTheEditor() {
    for (const QString &text : {QString("work()  "), QString("previous\n"),
                                QString(""), QString("\u00a0")}) {
      QTextDocument document(text);
      const QString original = document.toRawText();
      QTextCursor cursor(&document);
      cursor.movePosition(QTextCursor::End);
      const int position = cursor.position();
      QVERIFY(!EditorIndentation::backspace(cursor, 4));
      QCOMPARE(document.toRawText(), original);
      QCOMPARE(cursor.position(), position);
    }
    QTextDocument document("    work()");
    QTextCursor cursor(&document);
    cursor.setPosition(2);
    cursor.setPosition(4, QTextCursor::KeepAnchor);
    QVERIFY(!EditorIndentation::backspace(cursor, 4));
    QCOMPARE(cursor.selectedText(), QString("  "));
    QCOMPARE(document.toPlainText(), QString("    work()"));
  }

  void selectionReplacementAndUndo() {
    QTextDocument document("    if ready: remove\nother() + tail");
    QTextCursor cursor(&document);
    const int start = QString("    if ready:").size();
    cursor.setPosition(start);
    cursor.setPosition(start + QString(" remove\nother()").size(),
                       QTextCursor::KeepAnchor);
    EditorIndentation::insertNewline(cursor, {"py", 4, true, true});
    QCOMPARE(document.toPlainText(), QString("    if ready:\n         + tail"));
    QCOMPARE(cursor.position(), start + 9);
    document.undo();
    QCOMPARE(document.toPlainText(),
             QString("    if ready: remove\nother() + tail"));
  }

  void disabledAutoIndent() {
    QTextDocument document("    if ready:");
    QTextCursor cursor(&document);
    cursor.movePosition(QTextCursor::End);
    EditorIndentation::insertNewline(cursor, {"py", 4, false, true});
    QCOMPARE(document.toPlainText(), QString("    if ready:\n"));
    document.undo();
    QCOMPARE(document.toPlainText(), QString("    if ready:"));
  }

  void pythonNewIndentUsesSpaces() {
    QTextDocument document("\tif ready:");
    QTextCursor cursor(&document);
    cursor.movePosition(QTextCursor::End);
    EditorIndentation::insertNewline(cursor, {"py", 4, true, true});
    QCOMPARE(document.toPlainText(), QString("\tif ready:\n        "));
    document.undo();
    QCOMPARE(document.toPlainText(), QString("\tif ready:"));
  }
};

QTEST_MAIN(TestIndentation)
#include "test_indentation.moc"
