#include "core/editor/multicursor.h"
#include <QPlainTextEdit>
#include <QtTest/QtTest>

class TestMultiCursor : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testInitialState();
  void testCursorCountInitial();
  void testAddCursorBelow();
  void testAddCursorAbove();
  void testAddCursorBelowAtLastLine();
  void testAddCursorAboveAtFirstLine();
  void testClearExtraCursors();
  void testHasMultipleCursors();
  void testAddCursorAtNextOccurrence();
  void testAddCursorsToAllOccurrences();
  void testApplyToAllCursors();
  void testLastSelectedWord();
  void testMergeOverlappingCursors();
  void testNullEditor();

private:
  QPlainTextEdit *m_editor = nullptr;
  MultiCursorHandler *m_handler = nullptr;
};

void TestMultiCursor::init() {
  m_editor = new QPlainTextEdit();
  m_editor->setPlainText("line one\nline two\nline three\nline four\n");
  m_handler = new MultiCursorHandler(m_editor);
}

void TestMultiCursor::cleanup() {
  delete m_handler;
  delete m_editor;
  m_handler = nullptr;
  m_editor = nullptr;
}

void TestMultiCursor::testInitialState() {
  QVERIFY(!m_handler->hasMultipleCursors());
  QVERIFY(m_handler->extraCursors().isEmpty());
  QVERIFY(m_handler->lastSelectedWord().isEmpty());
}

void TestMultiCursor::testCursorCountInitial() {
  QCOMPARE(m_handler->cursorCount(), 1);
}

void TestMultiCursor::testAddCursorBelow() {
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  m_handler->addCursorBelow();

  QVERIFY(m_handler->hasMultipleCursors());
  QCOMPARE(m_handler->cursorCount(), 2);
}

void TestMultiCursor::testAddCursorAbove() {
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::NextBlock);
  m_editor->setTextCursor(cursor);

  m_handler->addCursorAbove();

  QVERIFY(m_handler->hasMultipleCursors());
  QCOMPARE(m_handler->cursorCount(), 2);
}

void TestMultiCursor::testAddCursorBelowAtLastLine() {
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::End);
  m_editor->setTextCursor(cursor);

  int countBefore = m_handler->cursorCount();
  m_handler->addCursorBelow();

  QCOMPARE(m_handler->cursorCount(), countBefore);
}

void TestMultiCursor::testAddCursorAboveAtFirstLine() {
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  int countBefore = m_handler->cursorCount();
  m_handler->addCursorAbove();

  QCOMPARE(m_handler->cursorCount(), countBefore);
}

void TestMultiCursor::testClearExtraCursors() {
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  m_handler->addCursorBelow();
  QVERIFY(m_handler->hasMultipleCursors());

  m_handler->clearExtraCursors();
  QVERIFY(!m_handler->hasMultipleCursors());
  QCOMPARE(m_handler->cursorCount(), 1);
}

void TestMultiCursor::testHasMultipleCursors() {
  QVERIFY(!m_handler->hasMultipleCursors());

  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  m_handler->addCursorBelow();
  QVERIFY(m_handler->hasMultipleCursors());
}

void TestMultiCursor::testAddCursorAtNextOccurrence() {
  m_editor->setPlainText("word hello word bye word");
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 4);
  m_editor->setTextCursor(cursor);

  m_handler->addCursorAtNextOccurrence();

  QVERIFY(m_handler->hasMultipleCursors());
  QCOMPARE(m_handler->lastSelectedWord(), QString("word"));
}

void TestMultiCursor::testAddCursorsToAllOccurrences() {
  m_editor->setPlainText("word hello word bye word");
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 4);
  m_editor->setTextCursor(cursor);

  m_handler->addCursorsToAllOccurrences();

  QCOMPARE(m_handler->cursorCount(), 3);
  QCOMPARE(m_handler->lastSelectedWord(), QString("word"));
}

void TestMultiCursor::testApplyToAllCursors() {
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  m_handler->addCursorBelow();
  QCOMPARE(m_handler->cursorCount(), 2);

  int callCount = 0;
  m_handler->applyToAllCursors([&callCount](QTextCursor &) { callCount++; });

  QCOMPARE(callCount, 2);
}

void TestMultiCursor::testLastSelectedWord() {
  m_editor->setPlainText("test test test");
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 4);
  m_editor->setTextCursor(cursor);

  m_handler->addCursorAtNextOccurrence();

  QCOMPARE(m_handler->lastSelectedWord(), QString("test"));
}

void TestMultiCursor::testMergeOverlappingCursors() {
  m_editor->setPlainText("hello world");
  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  m_handler->applyToAllCursors([](QTextCursor &) {});

  QCOMPARE(m_handler->cursorCount(), 1);
}

void TestMultiCursor::testNullEditor() {
  MultiCursorHandler nullHandler(nullptr);

  QVERIFY(!nullHandler.hasMultipleCursors());
  QCOMPARE(nullHandler.cursorCount(), 1);

  nullHandler.addCursorAbove();
  nullHandler.addCursorBelow();
  nullHandler.addCursorAtNextOccurrence();
  nullHandler.addCursorsToAllOccurrences();
  nullHandler.applyToAllCursors([](QTextCursor &) {});

  QCOMPARE(nullHandler.cursorCount(), 1);
}

QTEST_MAIN(TestMultiCursor)
#include "test_multicursor.moc"
