#include "editor/vimmode.h"
#include <QPlainTextEdit>
#include <QtTest/QtTest>

class TestVimMode : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  void initTestCase();
  void cleanupTestCase();
  void testEnableDisable();
  void testModeNames();
  void testNormalToInsertMode();
  void testInsertToNormalMode();
  void testBasicMotions();
  void testDeleteOperator();
  void testVisualMode();
  void testReplaceMode();
  void testFindCharMotion();
  void testMarks();
  void testTextObjects();
  void testIndent();
  void testToggleCase();
  void testGoToLine();
  void testParagraphMotion();
  void testSetNoVim();
  void testSetVim();
  void testCommandHistory();

private:
  QPlainTextEdit *m_editor;
  VimMode *m_vim;
};

void TestVimMode::initTestCase() {
  m_editor = new QPlainTextEdit();
  m_vim = new VimMode(m_editor);
}

void TestVimMode::cleanupTestCase() {
  delete m_vim;
  delete m_editor;
}

void TestVimMode::init() {

  m_vim->setEnabled(false);
  m_editor->clear();
}

void TestVimMode::cleanup() { m_vim->setEnabled(false); }

void TestVimMode::testEnableDisable() {
  QVERIFY(!m_vim->isEnabled());

  m_vim->setEnabled(true);
  QVERIFY(m_vim->isEnabled());
  QCOMPARE(m_vim->mode(), VimEditMode::Normal);

  m_vim->setEnabled(false);
  QVERIFY(!m_vim->isEnabled());
}

void TestVimMode::testModeNames() {
  m_vim->setEnabled(true);

  QCOMPARE(m_vim->modeName(), QString("NORMAL"));
}

void TestVimMode::testNormalToInsertMode() {
  m_vim->setEnabled(true);
  QCOMPARE(m_vim->mode(), VimEditMode::Normal);

  QKeyEvent keyEvent(QEvent::KeyPress, Qt::Key_I, Qt::NoModifier, "i");
  bool handled = m_vim->processKeyEvent(&keyEvent);

  QVERIFY(handled);
  QCOMPARE(m_vim->mode(), VimEditMode::Insert);
}

void TestVimMode::testInsertToNormalMode() {
  m_vim->setEnabled(true);

  QKeyEvent iKey(QEvent::KeyPress, Qt::Key_I, Qt::NoModifier, "i");
  m_vim->processKeyEvent(&iKey);
  QCOMPARE(m_vim->mode(), VimEditMode::Insert);

  QKeyEvent escKey(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  bool handled = m_vim->processKeyEvent(&escKey);

  QVERIFY(handled);
  QCOMPARE(m_vim->mode(), VimEditMode::Normal);
}

void TestVimMode::testBasicMotions() {
  m_vim->setEnabled(true);
  m_editor->setPlainText("Hello World\nSecond Line\nThird Line");

  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  QKeyEvent lKey(QEvent::KeyPress, Qt::Key_L, Qt::NoModifier, "l");
  m_vim->processKeyEvent(&lKey);

  cursor = m_editor->textCursor();
  QCOMPARE(cursor.position(), 1);

  QKeyEvent jKey(QEvent::KeyPress, Qt::Key_J, Qt::NoModifier, "j");
  m_vim->processKeyEvent(&jKey);

  cursor = m_editor->textCursor();
  QVERIFY(cursor.blockNumber() == 1);
}

void TestVimMode::testDeleteOperator() {
  m_vim->setEnabled(true);
  m_editor->setPlainText("Hello World");

  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  QKeyEvent xKey(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, "x");
  m_vim->processKeyEvent(&xKey);

  QCOMPARE(m_editor->toPlainText(), QString("ello World"));
}

void TestVimMode::testVisualMode() {
  m_vim->setEnabled(true);

  QKeyEvent vKey(QEvent::KeyPress, Qt::Key_V, Qt::NoModifier, "v");
  m_vim->processKeyEvent(&vKey);

  QCOMPARE(m_vim->mode(), VimEditMode::Visual);

  QKeyEvent escKey(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  m_vim->processKeyEvent(&escKey);

  QCOMPARE(m_vim->mode(), VimEditMode::Normal);
}

void TestVimMode::testReplaceMode() {
  m_vim->setEnabled(true);
  m_editor->setPlainText("Hello");

  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  QKeyEvent rKey(QEvent::KeyPress, Qt::Key_R, Qt::ShiftModifier, "R");
  m_vim->processKeyEvent(&rKey);

  QCOMPARE(m_vim->mode(), VimEditMode::Replace);
  QCOMPARE(m_vim->modeName(), QString("REPLACE"));

  QKeyEvent xKey(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, "X");
  m_vim->processKeyEvent(&xKey);

  QCOMPARE(m_editor->toPlainText(), QString("Xello"));

  QKeyEvent escKey(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  m_vim->processKeyEvent(&escKey);

  QCOMPARE(m_vim->mode(), VimEditMode::Normal);
}

void TestVimMode::testFindCharMotion() {
  m_vim->setEnabled(true);
  m_editor->setPlainText("Hello World");

  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  QKeyEvent fKey(QEvent::KeyPress, Qt::Key_F, Qt::NoModifier, "f");
  m_vim->processKeyEvent(&fKey);

  QKeyEvent wKey(QEvent::KeyPress, Qt::Key_W, Qt::ShiftModifier, "W");
  m_vim->processKeyEvent(&wKey);

  cursor = m_editor->textCursor();
  QCOMPARE(cursor.position(), 6);
}

void TestVimMode::testMarks() {
  m_vim->setEnabled(true);
  m_editor->setPlainText("Line 1\nLine 2\nLine 3");

  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::Down);
  m_editor->setTextCursor(cursor);

  int markedPos = cursor.position();

  QKeyEvent mKey(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier, "m");
  m_vim->processKeyEvent(&mKey);

  QKeyEvent aKey(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
  m_vim->processKeyEvent(&aKey);

  QKeyEvent gKey1(QEvent::KeyPress, Qt::Key_G, Qt::NoModifier, "g");
  m_vim->processKeyEvent(&gKey1);
  QKeyEvent gKey2(QEvent::KeyPress, Qt::Key_G, Qt::NoModifier, "g");
  m_vim->processKeyEvent(&gKey2);

  cursor = m_editor->textCursor();
  QCOMPARE(cursor.position(), 0);

  QKeyEvent quoteKey(QEvent::KeyPress, Qt::Key_Apostrophe, Qt::NoModifier, "'");
  m_vim->processKeyEvent(&quoteKey);

  QKeyEvent aKey2(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
  m_vim->processKeyEvent(&aKey2);

  cursor = m_editor->textCursor();
  QCOMPARE(cursor.position(), markedPos);
}

void TestVimMode::testTextObjects() {
  m_vim->setEnabled(true);
  m_editor->setPlainText("Hello (World) Test");

  QTextCursor cursor = m_editor->textCursor();
  cursor.setPosition(8);
  m_editor->setTextCursor(cursor);

  QKeyEvent dKey(QEvent::KeyPress, Qt::Key_D, Qt::NoModifier, "d");
  m_vim->processKeyEvent(&dKey);

  QKeyEvent iKey(QEvent::KeyPress, Qt::Key_I, Qt::NoModifier, "i");
  m_vim->processKeyEvent(&iKey);

  QKeyEvent parenKey(QEvent::KeyPress, Qt::Key_ParenLeft, Qt::NoModifier, "(");
  m_vim->processKeyEvent(&parenKey);

  QCOMPARE(m_editor->toPlainText(), QString("Hello () Test"));
}

void TestVimMode::testIndent() {
  m_vim->setEnabled(true);
  m_editor->setPlainText("Hello");

  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  QKeyEvent greaterKey1(QEvent::KeyPress, Qt::Key_Greater, Qt::ShiftModifier,
                        ">");
  m_vim->processKeyEvent(&greaterKey1);

  QKeyEvent greaterKey2(QEvent::KeyPress, Qt::Key_Greater, Qt::ShiftModifier,
                        ">");
  m_vim->processKeyEvent(&greaterKey2);

  QCOMPARE(m_editor->toPlainText(), QString("    Hello"));
}

void TestVimMode::testToggleCase() {
  m_vim->setEnabled(true);
  m_editor->setPlainText("Hello");

  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  QKeyEvent tildeKey(QEvent::KeyPress, Qt::Key_AsciiTilde, Qt::ShiftModifier,
                     "~");
  m_vim->processKeyEvent(&tildeKey);

  QCOMPARE(m_editor->toPlainText(), QString("hello"));
}

void TestVimMode::testGoToLine() {
  m_vim->setEnabled(true);
  m_editor->setPlainText("Line 1\nLine 2\nLine 3\nLine 4\nLine 5");

  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  QKeyEvent colonKey(QEvent::KeyPress, Qt::Key_Colon, Qt::NoModifier, ":");
  m_vim->processKeyEvent(&colonKey);

  QKeyEvent threeKey(QEvent::KeyPress, Qt::Key_3, Qt::NoModifier, "3");
  m_vim->processKeyEvent(&threeKey);

  QKeyEvent enterKey(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
  m_vim->processKeyEvent(&enterKey);

  cursor = m_editor->textCursor();
  QCOMPARE(cursor.blockNumber(), 2);
}

void TestVimMode::testParagraphMotion() {
  m_vim->setEnabled(true);
  m_editor->setPlainText("Paragraph 1\n\nParagraph 2\n\nParagraph 3");

  QTextCursor cursor = m_editor->textCursor();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  QKeyEvent braceKey(QEvent::KeyPress, Qt::Key_BraceRight, Qt::ShiftModifier,
                     "}");
  m_vim->processKeyEvent(&braceKey);

  cursor = m_editor->textCursor();
  QVERIFY(cursor.blockNumber() > 0);
}

void TestVimMode::testSetNoVim() {
  m_vim->setEnabled(true);
  QSignalSpy spy(m_vim, &VimMode::commandExecuted);

  QKeyEvent colonKey(QEvent::KeyPress, Qt::Key_Colon, Qt::NoModifier, ":");
  m_vim->processKeyEvent(&colonKey);
  QKeyEvent sKey(QEvent::KeyPress, Qt::Key_S, Qt::NoModifier, "s");
  m_vim->processKeyEvent(&sKey);
  QKeyEvent eKey(QEvent::KeyPress, Qt::Key_E, Qt::NoModifier, "e");
  m_vim->processKeyEvent(&eKey);
  QKeyEvent tKey(QEvent::KeyPress, Qt::Key_T, Qt::NoModifier, "t");
  m_vim->processKeyEvent(&tKey);
  QKeyEvent spaceKey(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ");
  m_vim->processKeyEvent(&spaceKey);
  QKeyEvent nKey(QEvent::KeyPress, Qt::Key_N, Qt::NoModifier, "n");
  m_vim->processKeyEvent(&nKey);
  QKeyEvent oKey(QEvent::KeyPress, Qt::Key_O, Qt::NoModifier, "o");
  m_vim->processKeyEvent(&oKey);
  QKeyEvent vKey(QEvent::KeyPress, Qt::Key_V, Qt::NoModifier, "v");
  m_vim->processKeyEvent(&vKey);
  QKeyEvent iKey(QEvent::KeyPress, Qt::Key_I, Qt::NoModifier, "i");
  m_vim->processKeyEvent(&iKey);
  QKeyEvent mKey(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier, "m");
  m_vim->processKeyEvent(&mKey);
  QKeyEvent enterKey(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
  m_vim->processKeyEvent(&enterKey);

  QVERIFY(spy.count() > 0);
  QCOMPARE(spy.takeFirst().at(0).toString(), QString("vim:off"));
}

void TestVimMode::testSetVim() {
  m_vim->setEnabled(true);
  QSignalSpy spy(m_vim, &VimMode::commandExecuted);

  QKeyEvent colonKey(QEvent::KeyPress, Qt::Key_Colon, Qt::NoModifier, ":");
  m_vim->processKeyEvent(&colonKey);
  QKeyEvent sKey(QEvent::KeyPress, Qt::Key_S, Qt::NoModifier, "s");
  m_vim->processKeyEvent(&sKey);
  QKeyEvent eKey(QEvent::KeyPress, Qt::Key_E, Qt::NoModifier, "e");
  m_vim->processKeyEvent(&eKey);
  QKeyEvent tKey(QEvent::KeyPress, Qt::Key_T, Qt::NoModifier, "t");
  m_vim->processKeyEvent(&tKey);
  QKeyEvent spaceKey(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ");
  m_vim->processKeyEvent(&spaceKey);
  QKeyEvent vKey(QEvent::KeyPress, Qt::Key_V, Qt::NoModifier, "v");
  m_vim->processKeyEvent(&vKey);
  QKeyEvent iKey(QEvent::KeyPress, Qt::Key_I, Qt::NoModifier, "i");
  m_vim->processKeyEvent(&iKey);
  QKeyEvent mKey(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier, "m");
  m_vim->processKeyEvent(&mKey);
  QKeyEvent enterKey(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
  m_vim->processKeyEvent(&enterKey);

  QVERIFY(spy.count() > 0);
  QCOMPARE(spy.takeFirst().at(0).toString(), QString("vim:on"));
}

void TestVimMode::testCommandHistory() {
  m_vim->setEnabled(true);

  QKeyEvent colonKey(QEvent::KeyPress, Qt::Key_Colon, Qt::NoModifier, ":");
  m_vim->processKeyEvent(&colonKey);
  QKeyEvent wKey(QEvent::KeyPress, Qt::Key_W, Qt::NoModifier, "w");
  m_vim->processKeyEvent(&wKey);
  QKeyEvent enterKey(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
  m_vim->processKeyEvent(&enterKey);

  QKeyEvent colonKey2(QEvent::KeyPress, Qt::Key_Colon, Qt::NoModifier, ":");
  m_vim->processKeyEvent(&colonKey2);
  QKeyEvent upKey(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
  m_vim->processKeyEvent(&upKey);

  QCOMPARE(m_vim->commandBuffer(), QString("w"));

  QKeyEvent downKey(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
  m_vim->processKeyEvent(&downKey);
  QCOMPARE(m_vim->commandBuffer(), QString(""));
}

QTEST_MAIN(TestVimMode)
#include "test_vimmode.moc"
