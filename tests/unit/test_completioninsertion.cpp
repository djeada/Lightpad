#include "completion/completioninsertion.h"
#include <QTextDocument>
#include <QtTest>

class TestCompletionInsertion : public QObject {
  Q_OBJECT
private slots:
  void insertion_data() {
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("insert");
    QTest::addColumn<int>("kind");
    QTest::addColumn<bool>("snippet");
    QTest::addColumn<QString>("after");
    QTest::addColumn<QString>("selection");
    auto add = [](const char *name, QString before, QString insert,
                  CompletionItemKind kind, bool snippet, QString after,
                  QString selection = {}) {
      QTest::newRow(name) << before << insert << static_cast<int>(kind)
                          << snippet << after << selection;
    };
    const auto function = CompletionItemKind::Function;
    add("middle-of-expression", "total = pr| + other(3)", "print", function,
        false, "total = print(|) + other(3)");
    add("before-comma", "send(re|, other)", "retry", function, false,
        "send(retry(|), other)");
    add("before-closing-paren", "if (re|) { run(); }", "retry", function, false,
        "if (retry(|)) { run(); }");
    add("after-member-dot", "object.| + tail", "method",
        CompletionItemKind::Method, false, "object.method(|) + tail");
    add("empty-argument-position", "send(a, |, b)", "method", function, false,
        "send(a, method(|), b)");
    add("middle-of-token", "object.fo|obar + tail", "format", function, false,
        "object.format(|) + tail");
    add("existing-arguments", "object.fo|o(value) + tail", "format", function,
        false, "object.format(|value) + tail");
    add("existing-call-with-spacing", "fo|o  (value) + tail", "format",
        function, false, "format  (|value) + tail");
    add("snippet-with-existing-call", "fo|o(value) + tail",
        "format(${1:other})", function, true, "format(|value) + tail");
    add("first-argument-selected", "re| + tail", "retry(${1:count})$0",
        function, true, "retry(|count) + tail", "count");
    add("empty-tabstop", "re|", "retry($1)$0", function, true, "retry(|)");
    add("explicit-final-tabstop", "re|", "retry()$0", function, true,
        "retry()|");
    add("lowest-numbered-tabstop", "re|", "retry(${2:second}, ${1:first})$0",
        function, true, "retry(second, |first)", "first");
    add("nested-tabstop", "re|", "retry(${2:outer ${1:inner}})$0", function,
        true, "retry(outer |inner)", "inner");
    add("ordinary-variable", "left + va| + right", "value",
        CompletionItemKind::Variable, false, "left + value| + right");
    add("keyword", "re|", "return", CompletionItemKind::Keyword, false,
        "return|");
    add("qualified-function", "mv|", "std::move", function, false,
        "std::move(|)");
    add("imported-reference", "from tools import re|", "retry", function, false,
        "from tools import retry|");
    add("function-address", "auto callback = &re|", "retry", function, false,
        "auto callback = &retry|");
    add("constructor", "auto item = new Wi|;", "Widget",
        CompletionItemKind::Constructor, false, "auto item = new Widget(|);");
    add("unicode-identifier", QString::fromUtf8("δο|κιμή + tail"),
        QString::fromUtf8("δοκιμή2"), CompletionItemKind::Variable, false,
        QString::fromUtf8("δοκιμή2| + tail"));
  }

  void insertion() {
    QFETCH(QString, before);
    QFETCH(QString, insert);
    QFETCH(int, kind);
    QFETCH(bool, snippet);
    QFETCH(QString, after);
    QFETCH(QString, selection);
    const int initial = before.indexOf('|');
    before.remove(initial, 1);
    const int expected = after.indexOf('|');
    after.remove(expected, 1);
    QTextDocument document(before);
    QTextCursor cursor(&document);
    cursor.setPosition(initial);
    CompletionItem item;
    item.insertText = insert;
    item.kind = static_cast<CompletionItemKind>(kind);
    item.isSnippet = snippet;
    QVERIFY(CompletionInsertion::apply(cursor, item));
    QCOMPARE(document.toPlainText(), after);
    QCOMPARE(cursor.selectionStart(), expected);
    QCOMPARE(cursor.selectedText(), selection);
    document.undo();
    QCOMPARE(document.toPlainText(), before);
  }

  void serverRangeOverridesWordBoundaries() {
    QTextDocument document("object.oldName + tail");
    QTextCursor cursor(&document);
    cursor.setPosition(10);
    CompletionItem item;
    item.insertText = "service.run($0)";
    item.kind = CompletionItemKind::Function;
    item.isSnippet = true;
    item.textEditRange = CompletionItem::EditRange{0, 0, 0, 14};
    QVERIFY(CompletionInsertion::apply(cursor, item));
    QCOMPARE(document.toPlainText(), QString("service.run() + tail"));
    QCOMPARE(cursor.position(), QString("service.run(").size());
  }

  void serverRangesUseUtf16Columns() {
    QTextDocument document(QString::fromUtf8("😀 va + tail"));
    QTextCursor cursor(&document);
    cursor.setPosition(5);
    CompletionItem item;
    item.insertText = "value";
    item.textEditRange = CompletionItem::EditRange{0, 3, 0, 5};
    QVERIFY(CompletionInsertion::apply(cursor, item));
    QCOMPARE(document.toPlainText(), QString::fromUtf8("😀 value + tail"));
  }

  void invalidOrStaleRangesNeverModifyTheDocument() {
    for (const auto range : {CompletionItem::EditRange{0, 0, 0, 100},
                             CompletionItem::EditRange{-1, 0, 0, 2},
                             CompletionItem::EditRange{0, 2, 0, 0},
                             CompletionItem::EditRange{0, 0, 0, 2}}) {
      QTextDocument document("left + right");
      QTextCursor cursor(&document);
      cursor.setPosition(9);
      CompletionItem item;
      item.insertText = "changed";
      item.textEditRange = range;
      QVERIFY(!CompletionInsertion::apply(cursor, item));
      QCOMPARE(document.toPlainText(), QString("left + right"));
      QCOMPARE(cursor.position(), 9);
    }
  }

  void explicitSelectionIsReplacedInPlace() {
    QTextDocument document("left + old + right");
    QTextCursor cursor(&document);
    cursor.setPosition(7);
    cursor.setPosition(10, QTextCursor::KeepAnchor);
    CompletionItem item;
    item.insertText = "newValue";
    QVERIFY(CompletionInsertion::apply(cursor, item));
    QCOMPARE(document.toPlainText(), QString("left + newValue + right"));
  }

  void prefixOnlyIncludesTextBeforeTheCursor() {
    QTextDocument document("object.foobar + tail");
    QTextCursor cursor(&document);
    cursor.setPosition(9);
    QCOMPARE(CompletionInsertion::prefix(cursor), QString("fo"));
    cursor.setPosition(7);
    QCOMPARE(CompletionInsertion::prefix(cursor), QString());
  }
};

QTEST_MAIN(TestCompletionInsertion)
#include "test_completioninsertion.moc"
