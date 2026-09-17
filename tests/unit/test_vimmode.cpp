#include "editor/vimmode.h"
#include "vim_oracle_cases.h"
#include <QApplication>
#include <QClipboard>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QtTest/QtTest>

namespace {

class VimTestEditor : public QPlainTextEdit {
public:
  VimMode *vim = nullptr;

protected:
  void keyPressEvent(QKeyEvent *event) override {
    if (vim && vim->processKeyEvent(event))
      return;
    QPlainTextEdit::keyPressEvent(event);
  }
};

void placeCursor(QPlainTextEdit *editor, int line, int col) {
  QTextCursor cursor(editor->document());
  cursor.setPosition(editor->document()->findBlockByNumber(line).position() +
                     col);
  editor->setTextCursor(cursor);
}

} // namespace

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
  void testMatchesVim_data();
  void testMatchesVim();
  void testKeyTokens();
  void testKeyNotationRoundTrip();
  void testShortcutOverride();
  void testInsertSessionIsSingleUndoStep();
  void testRealKeyEventsInsertText();
  void testMacroRegisterContent();
  void testEditorCommandSignals();
  void testSearchHighlightSignals();
  void testIncrementalSearchEscapeRestoresCursor();
  void testCommandLineEditing();
  void testVisualBlockRanges();
  void testMouseSelectionEntersVisual();
  void testSetShiftWidth();
  void testClipboardRegister();
  void testRegexTranslation();
  void testPendingKeysDisplay();

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

void TestVimMode::testMatchesVim_data() {
  QTest::addColumn<int>("index");
  const int count = int(sizeof(kVimOracleCases) / sizeof(kVimOracleCases[0]));
  for (int i = 0; i < count; ++i) {
    QTest::newRow(qPrintable(QString("%1: %2").arg(i).arg(
        QString::fromUtf8(kVimOracleCases[i].keys))))
        << i;
  }
}

void TestVimMode::testMatchesVim() {
  QFETCH(int, index);
  const VimOracleCase &c = kVimOracleCases[index];
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText(QString::fromUtf8(kVimOracleTexts[c.text]));
  placeCursor(&editor, c.line, c.col);
  vim.setEnabled(true);
  vim.feedKeys(QString::fromUtf8(c.keys));
  if (vim.mode() != VimEditMode::Normal || !vim.pendingKeys().isEmpty())
    vim.feedKeys("<Esc>");

  QCOMPARE(editor.toPlainText(), QString::fromUtf8(c.expectedText));
  QTextCursor cursor = editor.textCursor();
  QCOMPARE(cursor.blockNumber(), c.expectedLine);
  QCOMPARE(cursor.positionInBlock(), c.expectedCol);
  if (c.expectedRegister)
    QCOMPARE(vim.registerContent('"'), QString::fromUtf8(c.expectedRegister));
}

void TestVimMode::testKeyTokens() {
  QKeyEvent ctrlR(QEvent::KeyPress, Qt::Key_R, Qt::ControlModifier);
  QCOMPARE(VimMode::keyEventToToken(&ctrlR), QString("<C-r>"));
  QKeyEvent shiftA(QEvent::KeyPress, Qt::Key_A, Qt::ShiftModifier, "A");
  QCOMPARE(VimMode::keyEventToToken(&shiftA), QString("A"));
  QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QCOMPARE(VimMode::keyEventToToken(&esc), QString("<Esc>"));
  QKeyEvent enter(QEvent::KeyPress, Qt::Key_Enter, Qt::KeypadModifier);
  QCOMPARE(VimMode::keyEventToToken(&enter), QString("<CR>"));
  QKeyEvent ctrlBracket(QEvent::KeyPress, Qt::Key_BracketLeft,
                        Qt::ControlModifier);
  QCOMPARE(VimMode::keyEventToToken(&ctrlBracket), QString("<C-[>"));
  QKeyEvent shift(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier);
  QVERIFY(VimMode::keyEventToToken(&shift).isEmpty());
  QKeyEvent backtab(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier);
  QCOMPARE(VimMode::keyEventToToken(&backtab), QString("<S-Tab>"));
}

void TestVimMode::testKeyNotationRoundTrip() {
  const QStringList tokens =
      VimMode::parseKeyNotation("d2w<Esc>:s/a/b/<CR><lt>x<C-r>\"");
  QCOMPARE(tokens, QStringList({"d", "2", "w", "<Esc>", ":", "s", "/", "a", "/",
                                "b", "/", "<CR>", "<", "x", "<C-r>", "\""}));
  QCOMPARE(VimMode::tokensToNotation(tokens),
           QString("d2w<Esc>:s/a/b/<CR><lt>x<C-r>\""));
  QCOMPARE(VimMode::parseKeyNotation("a<b"), QStringList({"a", "<", "b"}));
}

void TestVimMode::testShortcutOverride() {
  m_vim->setEnabled(true);
  QKeyEvent ctrlR(QEvent::ShortcutOverride, Qt::Key_R, Qt::ControlModifier);
  QKeyEvent ctrlS(QEvent::ShortcutOverride, Qt::Key_S, Qt::ControlModifier);
  QKeyEvent ctrlW(QEvent::ShortcutOverride, Qt::Key_W, Qt::ControlModifier);
  QVERIFY(m_vim->shouldOverrideShortcut(&ctrlR));
  QVERIFY(!m_vim->shouldOverrideShortcut(&ctrlS));
  m_vim->feedKeys("i");
  QVERIFY(m_vim->shouldOverrideShortcut(&ctrlW));
  m_vim->feedKeys("<Esc>");
  m_vim->setEnabled(false);
  QVERIFY(!m_vim->shouldOverrideShortcut(&ctrlR));
}

void TestVimMode::testInsertSessionIsSingleUndoStep() {
  m_editor->setPlainText("start");
  m_vim->setEnabled(true);
  m_vim->feedKeys("Aone<CR>two<BS>o three<Esc>");
  QCOMPARE(m_editor->toPlainText(), QString("startone\ntwo three"));
  m_vim->feedKeys("ciwX<Esc>");
  QCOMPARE(m_editor->toPlainText(), QString("startone\ntwo X"));
  m_vim->feedKeys("u");
  QCOMPARE(m_editor->toPlainText(), QString("startone\ntwo three"));
  m_vim->feedKeys("u");
  QCOMPARE(m_editor->toPlainText(), QString("start"));
  m_vim->feedKeys("<C-r><C-r>");
  QCOMPARE(m_editor->toPlainText(), QString("startone\ntwo X"));
}

void TestVimMode::testRealKeyEventsInsertText() {
  VimTestEditor editor;
  VimMode vim(&editor);
  editor.vim = &vim;
  editor.setPlainText("abc");
  vim.setEnabled(true);
  QTest::keyClicks(&editor, "A-xyz");
  QTest::keyClick(&editor, Qt::Key_Escape);
  QCOMPARE(editor.toPlainText(), QString("abc-xyz"));
  QCOMPARE(vim.mode(), VimEditMode::Normal);
  QTest::keyClick(&editor, Qt::Key_Period);
  QCOMPARE(editor.toPlainText(), QString("abc-xyz-xyz"));
  QTest::keyClick(&editor, Qt::Key_U);
  QCOMPARE(editor.toPlainText(), QString("abc-xyz"));
  QTest::keyClick(&editor, Qt::Key_R, Qt::ControlModifier);
  QCOMPARE(editor.toPlainText(), QString("abc-xyz-xyz"));
}

void TestVimMode::testMacroRegisterContent() {
  m_editor->setPlainText("a\nb\nc");
  m_vim->setEnabled(true);
  m_vim->feedKeys("qqA;<Esc>jq");
  QCOMPARE(m_vim->registerContent('q'), QString("A;<Esc>j"));
  m_vim->feedKeys("@q");
  QCOMPARE(m_editor->toPlainText(), QString("a;\nb;\nc"));
  m_vim->feedKeys("@@");
  QCOMPARE(m_editor->toPlainText(), QString("a;\nb;\nc;"));
}

void TestVimMode::testEditorCommandSignals() {
  m_editor->setPlainText("text");
  m_vim->setEnabled(true);
  QSignalSpy spy(m_vim, &VimMode::commandExecuted);
  auto run = [this, &spy](const QString &keys) {
    spy.clear();
    m_vim->feedKeys(keys);
    QStringList commands;
    for (const QList<QVariant> &args : spy)
      commands << args.at(0).toString();
    return commands;
  };
  QCOMPARE(run(":w<CR>"), QStringList({"save"}));
  QCOMPARE(run(":wq<CR>"), QStringList({"save", "quit"}));
  QCOMPARE(run("ZZ"), QStringList({"save", "quit"}));
  QCOMPARE(run(":q!<CR>"), QStringList({"closeWithoutSaving"}));
  QCOMPARE(run(":wa<CR>"), QStringList({"saveAll"}));
  QCOMPARE(run(":vsp<CR>"), QStringList({"splitVertical"}));
  QCOMPARE(run("<C-w>s"), QStringList({"splitHorizontal"}));
  QCOMPARE(run("gt"), QStringList({"nextTab"}));
  QCOMPARE(run(":bp<CR>"), QStringList({"prevTab"}));
  QCOMPARE(run(":e other.txt<CR>"), QStringList({"edit:other.txt"}));
  QCOMPARE(run("gd"), QStringList({"goToDefinition"}));
  QCOMPARE(run("zc"), QStringList({"fold"}));
}

void TestVimMode::testSearchHighlightSignals() {
  m_editor->setPlainText("foo bar foo");
  m_vim->setEnabled(true);
  QSignalSpy spy(m_vim, &VimMode::searchHighlightRequested);
  m_vim->feedKeys("/fo\\+<CR>");
  QVERIFY(!spy.isEmpty());
  QVERIFY(spy.last().at(1).toBool());
  QCOMPARE(m_editor->textCursor().position(), 8);
  QCOMPARE(m_vim->searchPattern(), QString("fo\\+"));
  spy.clear();
  m_vim->feedKeys(":noh<CR>");
  QCOMPARE(spy.count(), 1);
  QVERIFY(!spy.last().at(1).toBool());
}

void TestVimMode::testIncrementalSearchEscapeRestoresCursor() {
  m_editor->setPlainText("alpha beta gamma");
  placeCursor(m_editor, 0, 2);
  m_vim->setEnabled(true);
  m_vim->feedKeys("/gam");
  QCOMPARE(m_vim->mode(), VimEditMode::Command);
  QCOMPARE(m_editor->textCursor().selectionStart(), 11);
  m_vim->feedKeys("<Esc>");
  QCOMPARE(m_vim->mode(), VimEditMode::Normal);
  QCOMPARE(m_editor->textCursor().position(), 2);
  QVERIFY(!m_editor->textCursor().hasSelection());
}

void TestVimMode::testCommandLineEditing() {
  m_editor->setPlainText("one");
  m_vim->setEnabled(true);
  m_vim->feedKeys(":abc<Left><BS>");
  QCOMPARE(m_vim->commandBuffer(), QString("ac"));
  QCOMPARE(m_vim->commandCursorPosition(), 1);
  m_vim->feedKeys("<C-u>");
  QCOMPARE(m_vim->commandBuffer(), QString("c"));
  m_vim->feedKeys("<Esc>");
  m_vim->feedKeys("yiw:<C-r>\"");
  QCOMPARE(m_vim->commandBuffer(), QString("one"));
  m_vim->feedKeys("<C-w>set ts=8<CR>");
  m_vim->feedKeys(":se<Up>");
  QCOMPARE(m_vim->commandBuffer(), QString("set ts=8"));
  m_vim->feedKeys("<Esc>/on");
  QCOMPARE(m_vim->commandBuffer(), QString("/on"));
  m_vim->feedKeys("<Esc>");
}

void TestVimMode::testVisualBlockRanges() {
  m_editor->setPlainText("abcd\nefgh\nij");
  m_vim->setEnabled(true);
  m_vim->feedKeys("l<C-v>jjl");
  const QVector<QPair<int, int>> ranges = m_vim->visualBlockRanges();
  QCOMPARE(ranges.size(), 3);
  QCOMPARE(ranges[0], qMakePair(1, 3));
  QCOMPARE(ranges[1], qMakePair(6, 8));
  QCOMPARE(ranges[2], qMakePair(11, 12));
  m_vim->feedKeys("<Esc>");
  QVERIFY(m_vim->visualBlockRanges().isEmpty());
}

void TestVimMode::testMouseSelectionEntersVisual() {
  m_editor->setPlainText("hello world");
  m_vim->setEnabled(true);
  QTextCursor cursor = m_editor->textCursor();
  cursor.setPosition(6);
  cursor.setPosition(11, QTextCursor::KeepAnchor);
  m_editor->setTextCursor(cursor);
  m_vim->feedKeys("d");
  QCOMPARE(m_editor->toPlainText(), QString("hello "));
  QCOMPARE(m_vim->mode(), VimEditMode::Normal);
}

void TestVimMode::testSetShiftWidth() {
  m_editor->setPlainText("x\ny");
  m_vim->setEnabled(true);
  m_vim->feedKeys(":set sw=2<CR>>>j:set noet ts=4 sw=4<CR>>>");
  QCOMPARE(m_editor->toPlainText(), QString("  x\n\ty"));
  m_vim->feedKeys(":set sw=4 et<CR>");
}

void TestVimMode::testClipboardRegister() {
  m_editor->setPlainText("copy me\nnext");
  m_vim->setEnabled(true);
  m_vim->feedKeys("\"+yy");
  QCOMPARE(QApplication::clipboard()->text(), QString("copy me\n"));
  QApplication::clipboard()->setText("pasted");
  m_vim->feedKeys("j\"+P");
  QCOMPARE(m_editor->toPlainText(), QString("copy me\npastednext"));
}

void TestVimMode::testRegexTranslation() {
  auto matches = [](const QString &vimPattern, const QString &text) {
    bool cs = true;
    QRegularExpression re(VimMode::toRegularExpression(vimPattern, &cs),
                          QRegularExpression::MultilineOption);
    if (!cs)
      re.setPatternOptions(re.patternOptions() |
                           QRegularExpression::CaseInsensitiveOption);
    return re.match(text).captured();
  };
  QCOMPARE(matches("\\<foo\\>", "afoo foo"), QString("foo"));
  QCOMPARE(matches("a\\(b\\|c\\)\\+", "xacbd"), QString("acb"));
  QCOMPARE(matches("\\v(ab)+", "ababx"), QString("abab"));
  QCOMPARE(matches("x\\{2,3}", "xxxxx"), QString("xxx"));
  QCOMPARE(matches("x\\{-1,}", "xxxxx"), QString("x"));
  QCOMPARE(matches("\\cFOO", "foo"), QString("foo"));
  QCOMPARE(matches("a.c", "a+c"), QString("a+c"));
  QCOMPARE(matches("\\Va.c", "abc a.c"), QString("a.c"));
  QCOMPARE(matches("foo\\zsbar", "foobar"), QString("bar"));
  QCOMPARE(matches("(x)", "(x)"), QString("(x)"));
  QCOMPARE(VimMode::escapePattern("a.b*c/"), QString("a\\.b\\*c\\/"));
}

void TestVimMode::testPendingKeysDisplay() {
  m_editor->setPlainText("text");
  m_vim->setEnabled(true);
  QSignalSpy spy(m_vim, &VimMode::pendingKeysChanged);
  m_vim->feedKeys("\"a2d");
  QCOMPARE(m_vim->pendingKeys(), QString("\"a2d"));
  m_vim->feedKeys("<Esc>");
  QVERIFY(m_vim->pendingKeys().isEmpty());
  QVERIFY(!spy.isEmpty());
}

QTEST_MAIN(TestVimMode)
#include "test_vimmode.moc"
