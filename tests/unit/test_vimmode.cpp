#include "editor/vimmode.h"
#include "vim_oracle_cases.h"
#include "vim_oracle_cases_fx.h"
#include "vim_oracle_cases_wb.h"
#include <QApplication>
#include <QClipboard>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
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

// Like Vim's cursor(), a column past the end lands on the last character.
void placeCursor(QPlainTextEdit *editor, int line, int col) {
  QTextCursor cursor(editor->document());
  const QTextBlock block = editor->document()->findBlockByNumber(line);
  cursor.setPosition(block.position() +
                     qBound(0, col, qMax(0, block.length() - 2)));
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
  void testModifiedKeyTokensRoundTrip();
  void testMacroReplaysModifiedKeys();
  void testMacroAbortsOnFailedMotion();
  void testExRecursionIsLimited();
  void testPutFromUnsetRegister();
  void testHugeRepeatIsRefused();
  void testCountedInsertRepeatsText();
  void testMatchesVimWb_data();
  void testMatchesVimWb();
  void testMatchesVimFx_data();
  void testMatchesVimFx();
  void testExpressionRegister();
  void testLetAndExecute();
  void testSubstituteExpression();
  void testFilterCommands();
  void testReadCommand();
  void testAlignCommands();
  void testBlockShift();
  void testBlockVisualYankLine();
  void testTextWidthWrap();
  void testNextMarkMotions();
  void testSearchContinuesAfterMatch();
  void testSearchOffsetsAndChains();
  void testSearchAtoms();
  void testDeleteMarks();
  void testChangeMarks();
  void testExAddressSearchForms();
  void testNormalMappings();
  void testInsertMappingWaitsForTimeout();
  void testOperatorPendingAndVisualMappings();
  void testMappingListingAndUnmap();
  void testAbbreviations();
  void testSourceVimrc();
  void testMappingEdgeCases();
  void testSubstituteConfirm();
  void testFoldsHideLines();
  void testEditorCommandsFromVim();
  void testCountedVisualReselect();

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
  VimMode::clearAllMappings();
  m_vim->setEnabled(false);
  m_editor->clear();
}

void TestVimMode::cleanup() {
  m_vim->setEnabled(false);
  VimMode::clearAllMappings();
}

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
  QCOMPARE(m_vim->registerContent('q'), QString("A;\x1bj"));
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
  QCOMPARE(matches("\\%(ab\\)\\+", "xababy"), QString("abab"));
  QCOMPARE(matches("\\v%(a|b)+", "xabba!"), QString("abba"));
  QCOMPARE(matches("[^]a]", "]ab"), QString("b"));
  QCOMPARE(matches("[]a]", "x]"), QString("]"));
  QCOMPARE(matches("\\_a\\+", "1ab\ncd2"), QString("ab\ncd"));
  QCOMPARE(matches("\\_x\\+", "g1f\n9z"), QString("1f\n9"));
  QCOMPARE(matches("\\_l\\+", "Aab\ncD"), QString("ab\nc"));
  QCOMPARE(matches("\\_u\\+", "aAB\nCd"), QString("AB\nC"));
  QCOMPARE(matches("\\_[xy]\\+", "axy\nyz"), QString("xy\ny"));
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

void TestVimMode::testModifiedKeyTokensRoundTrip() {
  const QList<QPair<int, Qt::KeyboardModifiers>> keys = {
      {Qt::Key_Right, Qt::ControlModifier},
      {Qt::Key_Right, Qt::ShiftModifier},
      {Qt::Key_Down, Qt::ShiftModifier},
      {Qt::Key_End, Qt::ControlModifier},
      {Qt::Key_Home, Qt::ControlModifier | Qt::ShiftModifier},
      {Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier},
      {Qt::Key_Space, Qt::ControlModifier},
      {Qt::Key_X, Qt::AltModifier},
      {Qt::Key_X, Qt::ControlModifier | Qt::AltModifier},
      {Qt::Key_F5, Qt::ControlModifier},
      {Qt::Key_F2, Qt::ShiftModifier},
      {Qt::Key_Tab, Qt::ControlModifier},
      {Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier},
      {Qt::Key_Return, Qt::ControlModifier},
      {Qt::Key_Launch0, Qt::ControlModifier},
  };
  QStringList tokens;
  for (const auto &key : keys) {
    QKeyEvent event(QEvent::KeyPress, key.first, key.second);
    const QString token = VimMode::keyEventToToken(&event);
    QVERIFY(token.startsWith('<') && token.endsWith('>'));
    tokens << token;
    std::unique_ptr<QKeyEvent> synthetic = VimMode::tokenToKeyEvent(token);
    QVERIFY2(synthetic, qPrintable(token));
    QCOMPARE(VimMode::keyEventToToken(synthetic.get()), token);
  }
  QCOMPARE(VimMode::parseKeyNotation(VimMode::tokensToNotation(tokens)),
           tokens);
  QCOMPARE(VimMode::parseKeyNotation("<c-s-A><s-right><C-SPACE><M-x>"),
           QStringList({"<C-S-a>", "<S-Right>", "<C-Space>", "<A-x>"}));
}

void TestVimMode::testMacroReplaysModifiedKeys() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("alpha beta gamma\nsecond line here");
  vim.setEnabled(true);
  vim.feedKeys("qa");
  QKeyEvent ctrlRight(QEvent::KeyPress, Qt::Key_Right, Qt::ControlModifier);
  QVERIFY(vim.processKeyEvent(&ctrlRight));
  vim.feedKeys("q");
  QCOMPARE(vim.registerContent('a'), QString("<C-Right>"));
  vim.feedKeys("0@a");
  QCOMPARE(editor.toPlainText(), QString("alpha beta gamma\nsecond line here"));
  QCOMPARE(editor.textCursor().position(), 6);
  QCOMPARE(vim.mode(), VimEditMode::Normal);
}

void TestVimMode::testMacroAbortsOnFailedMotion() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abc\ndef\nghi");
  vim.setEnabled(true);
  vim.feedKeys("qa0xjq99999999@a");
  QCOMPARE(editor.toPlainText(), QString("bc\nef\nhi"));
  QCOMPARE(editor.textCursor().blockNumber(), 2);
  vim.feedKeys("gg0qbxjq5@b");
  QCOMPARE(editor.toPlainText(), QString("c\nf\ni"));
}

void TestVimMode::testExRecursionIsLimited() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("one\ntwo\nthree");
  vim.setEnabled(true);
  QSignalSpy spy(&vim, &VimMode::statusMessage);
  vim.feedKeys("qa@aq:@a<CR>");
  bool tooRecursive = false;
  for (const QList<QVariant> &args : spy)
    tooRecursive |= args.at(0).toString().startsWith("E169");
  QVERIFY(tooRecursive);
  vim.feedKeys(":s/o/0/<CR>:1@:<CR>:*:<CR>:silent @:<CR>:g/./@:<CR>@:");
  QCOMPARE(editor.toPlainText(), QString("0ne\ntwo\nthree"));
  QCOMPARE(vim.mode(), VimEditMode::Normal);
}

void TestVimMode::testPutFromUnsetRegister() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("l1\nl2\nl3");
  vim.setEnabled(true);
  QSignalSpy spy(&vim, &VimMode::statusMessage);
  vim.feedKeys(":2put x<CR>");
  QCOMPARE(editor.toPlainText(), QString("l1\nl2\nl3"));
  QVERIFY(!spy.isEmpty());
  QCOMPARE(spy.last().at(0).toString(), QString("E353: Nothing in register x"));
  vim.feedKeys("\"xyy:3put x<CR>");
  QCOMPARE(editor.toPlainText(), QString("l1\nl2\nl3\nl1"));
}

void TestVimMode::testHugeRepeatIsRefused() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abc\ndef");
  vim.setEnabled(true);
  QSignalSpy spy(&vim, &VimMode::statusMessage);
  vim.feedKeys("yy99999999p");
  QCOMPARE(editor.toPlainText(), QString("abc\ndef"));
  QVERIFY(!spy.isEmpty());
  QCOMPARE(spy.last().at(0).toString(),
           QString("E1240: Resulting text too long"));
  vim.feedKeys("yl99999999P");
  QCOMPARE(editor.toPlainText(), QString("abc\ndef"));
  vim.feedKeys("yy3p");
  QCOMPARE(editor.toPlainText(), QString("abc\nabc\nabc\nabc\ndef"));
}

void TestVimMode::testCountedInsertRepeatsText() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("xy");
  vim.setEnabled(true);
  vim.feedKeys("2000iab<Esc>");
  QCOMPARE(editor.toPlainText(), QString("ab").repeated(2000) + "xy");
  QCOMPARE(editor.textCursor().position(), 3999);
  vim.feedKeys("u");
  QCOMPARE(editor.toPlainText(), QString("xy"));
}

void TestVimMode::testMatchesVimWb_data() {
  QTest::addColumn<int>("index");
  const int count =
      int(sizeof(kVimOracleWbCases) / sizeof(kVimOracleWbCases[0]));
  for (int i = 0; i < count; ++i) {
    QTest::newRow(qPrintable(QString("%1: %2").arg(i).arg(
        QString::fromUtf8(kVimOracleWbCases[i].keys))))
        << i;
  }
}

void TestVimMode::testMatchesVimWb() {
  QFETCH(int, index);
  const VimOracleCase &c = kVimOracleWbCases[index];
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText(QString::fromUtf8(kVimOracleWbTexts[c.text]));
  placeCursor(&editor, c.line, c.col);
  vim.setEnabled(true);
  vim.feedKeys(QString::fromUtf8(c.keys));
  if (vim.mode() != VimEditMode::Normal || !vim.pendingKeys().isEmpty())
    vim.feedKeys("<Esc>");

  QCOMPARE(editor.toPlainText().replace(QChar(0), QChar('\n')),
           QString::fromUtf8(c.expectedText));
  QTextCursor cursor = editor.textCursor();
  QCOMPARE(cursor.blockNumber(), c.expectedLine);
  QCOMPARE(cursor.positionInBlock(), c.expectedCol);
  if (c.expectedRegister)
    QCOMPARE(vim.registerContent('"'), QString::fromUtf8(c.expectedRegister));
}

void TestVimMode::testMatchesVimFx_data() {
  QTest::addColumn<int>("index");
  const int count =
      int(sizeof(kVimOracleFxCases) / sizeof(kVimOracleFxCases[0]));
  for (int i = 0; i < count; ++i) {
    QTest::newRow(qPrintable(QString("%1: %2").arg(i).arg(
        QString::fromUtf8(kVimOracleFxCases[i].keys))))
        << i;
  }
}

// Folds, key mappings, abbreviations, Tabs cut by blockwise operators and
// [count]v, compared against Vim 9.1 with remapping enabled.
void TestVimMode::testMatchesVimFx() {
  QFETCH(int, index);
  const VimOracleCase &c = kVimOracleFxCases[index];
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText(QString::fromUtf8(kVimOracleFxTexts[c.text]));
  placeCursor(&editor, c.line, c.col);
  vim.setEnabled(true);
  vim.feedKeys(QString::fromUtf8(c.keys));
  if (vim.mode() != VimEditMode::Normal || !vim.pendingKeys().isEmpty())
    vim.feedKeys("<Esc>");

  QCOMPARE(editor.toPlainText().replace(QChar(0), QChar('\n')),
           QString::fromUtf8(c.expectedText));
  QTextCursor cursor = editor.textCursor();
  QCOMPARE(cursor.blockNumber(), c.expectedLine);
  QCOMPARE(cursor.positionInBlock(), c.expectedCol);
  if (c.expectedRegister)
    QCOMPARE(vim.registerContent('"'), QString::fromUtf8(c.expectedRegister));
}

void TestVimMode::testExpressionRegister() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abc");
  vim.setEnabled(true);
  vim.feedKeys("\"=2*3+1<CR>p");
  QCOMPARE(editor.toPlainText(), QString("a7bc"));
  vim.feedKeys("u\"=\"x\" . 'y'<CR>P");
  QCOMPARE(editor.toPlainText(), QString("xyabc"));
  vim.feedKeys("u:put =toupper('x').repeat('y',2)<CR>");
  QCOMPARE(editor.toPlainText(), QString("abc\nXyy"));
  vim.feedKeys("uA<C-r>=printf('%03d', 7)<CR>!<Esc>");
  QCOMPARE(editor.toPlainText(), QString("abc007!"));
  vim.feedKeys("u:put =[1, 2, 3][1]<CR>");
  QCOMPARE(editor.toPlainText(), QString("abc\n2"));
}

void TestVimMode::testLetAndExecute() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abc def");
  vim.setEnabled(true);
  vim.feedKeys(":let @a=\"w\"<CR>:let @a.=\"x\"<CR>@a");
  QCOMPARE(editor.toPlainText(), QString("abc ef"));
  vim.feedKeys("u:let n = 1<CR>:let n += 2<CR>:exe 'normal gg0' . n . 'x'<CR>");
  QCOMPARE(editor.toPlainText(), QString(" def"));
  vim.feedKeys(":let @/ = 'e'<CR>gg0nx");
  QCOMPARE(editor.toPlainText(), QString(" df"));
}

void TestVimMode::testSubstituteExpression() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("1 and 22 and 3\nx");
  vim.setEnabled(true);
  vim.feedKeys(":1s/\\d\\+/\\=submatch(0)*2/g<CR>");
  QCOMPARE(editor.toPlainText(), QString("2 and 44 and 6\nx"));
  vim.feedKeys(":%s/a/\\=line('.').col('.')/g<CR>");
  QCOMPARE(editor.toPlainText(), QString("2 13nd 44 110nd 6\nx"));
  vim.feedKeys(":2s/x/\\=['p','q']/<CR>");
  QCOMPARE(editor.toPlainText(), QString("2 13nd 44 110nd 6\np\nq\n"));
}

void TestVimMode::testFilterCommands() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("c\nb\na");
  vim.setEnabled(true);
  vim.feedKeys(":%!sort<CR>");
  QCOMPARE(editor.toPlainText(), QString("a\nb\nc"));
  vim.feedKeys("gg!jtr a-z A-Z<CR>");
  QCOMPARE(editor.toPlainText(), QString("A\nB\nc"));
  vim.feedKeys("G!!tr a-z A-Z<CR>");
  QCOMPARE(editor.toPlainText(), QString("A\nB\nC"));
  vim.feedKeys("ggVj!tac<CR>");
  QCOMPARE(editor.toPlainText(), QString("B\nA\nC"));
  vim.feedKeys(":1,2!tr A-Z a-z|tac<CR>");
  QCOMPARE(editor.toPlainText(), QString("a\nb\nC"));
  vim.feedKeys(":%!true<CR>");
  QCOMPARE(editor.toPlainText(), QString(""));
}

void TestVimMode::testReadCommand() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("c\nb");
  vim.setEnabled(true);
  vim.feedKeys(":r !echo hi<CR>");
  QCOMPARE(editor.toPlainText(), QString("c\nhi\nb"));
  QCOMPARE(editor.textCursor().blockNumber(), 1);
  vim.feedKeys(":0r !printf 'x\\ny'<CR>");
  QCOMPARE(editor.toPlainText(), QString("x\ny\nc\nhi\nb"));
  QCOMPARE(editor.textCursor().blockNumber(), 1);
}

void TestVimMode::testAlignCommands() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abc\nde");
  vim.setEnabled(true);
  vim.feedKeys(":%center 10<CR>");
  QCOMPARE(editor.toPlainText(), QString("   abc\n    de"));
  vim.feedKeys(":%right 10<CR>");
  QCOMPARE(editor.toPlainText(), QString("       abc\n        de"));
  vim.feedKeys(":%left 3<CR>");
  QCOMPARE(editor.toPlainText(), QString("   abc\n   de"));
  QCOMPARE(editor.textCursor().positionInBlock(), 3);
}

void TestVimMode::testBlockShift() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abcd\nefgh");
  vim.setEnabled(true);
  vim.feedKeys("l<C-v>j>");
  QCOMPARE(editor.toPlainText(), QString("a    bcd\ne    fgh"));
  QCOMPARE(editor.textCursor().positionInBlock(), 1);
  vim.feedKeys("<C-v>j<");
  QCOMPARE(editor.toPlainText(), QString("abcd\nefgh"));
}

void TestVimMode::testBlockVisualYankLine() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abcd\nefgh");
  vim.setEnabled(true);
  vim.feedKeys("l<C-v>jY");
  QCOMPARE(vim.registerContent('"'), QString("b\nf"));
  QVERIFY(vim.registerValue('"').blockwise);
  vim.feedKeys("l<C-v>jSX<Esc>");
  QCOMPARE(editor.toPlainText(), QString("X"));
}

void TestVimMode::testTextWidthWrap() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("foo bar baz qux quux");
  vim.setEnabled(true);
  vim.feedKeys(":set tw=10<CR>A zz<Esc>");
  QCOMPARE(editor.toPlainText(), QString("foo bar\nbaz qux\nquux zz"));
  QCOMPARE(editor.textCursor().blockNumber(), 2);
  editor.setPlainText("- one two three four five six");
  vim.feedKeys(":set tw=12<CR>gqq");
  QCOMPARE(editor.toPlainText(), QString("- one two\n  three four\nfive six"));
}

void TestVimMode::testNextMarkMotions() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("  a\n  b\n  c\n  d\n  e");
  vim.setEnabled(true);
  vim.feedKeys("jmajjmbgg]'x");
  QCOMPARE(editor.toPlainText(), QString("  a\n  \n  c\n  d\n  e"));
  vim.feedKeys("]`x");
  QCOMPARE(editor.toPlainText(), QString("  a\n  \n  c\n d\n  e"));
  vim.feedKeys("G['x");
  QCOMPARE(editor.toPlainText(), QString("  a\n  \n  c\n \n  e"));
}

void TestVimMode::testSearchContinuesAfterMatch() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("aaaa\nfoo bar baz");
  vim.setEnabled(true);
  vim.feedKeys("/aa<CR>x");
  QCOMPARE(editor.toPlainText(), QString("aaa\nfoo bar baz"));
  QCOMPARE(editor.textCursor().positionInBlock(), 2);
  vim.feedKeys("/\\w\\+<CR>");
  QCOMPARE(editor.textCursor().blockNumber(), 1);
  QCOMPARE(editor.textCursor().positionInBlock(), 0);
  vim.feedKeys("/\\w\\+<CR>");
  QCOMPARE(editor.textCursor().positionInBlock(), 4);
}

void TestVimMode::testSearchOffsetsAndChains() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abc abc abc");
  vim.setEnabled(true);
  vim.feedKeys("/b/e+1<CR>");
  QCOMPARE(editor.textCursor().positionInBlock(), 2);
  vim.feedKeys("gg0/b/b+1<CR>");
  QCOMPARE(editor.textCursor().positionInBlock(), 2);
  vim.feedKeys("gg0/a/b+1<CR>");
  QCOMPARE(editor.textCursor().positionInBlock(), 1);
  vim.feedKeys("gg0/b/;/c<CR>");
  QCOMPARE(editor.textCursor().positionInBlock(), 2);
  vim.feedKeys("gg0:s&b&X&<CR>");
  QCOMPARE(editor.toPlainText(), QString("aXc abc abc"));
}

void TestVimMode::testSearchAtoms() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("foo bar\nfood baz\nbar foo");
  vim.setEnabled(true);
  vim.feedKeys("/\\%2lba<CR>");
  QCOMPARE(editor.textCursor().blockNumber(), 1);
  QCOMPARE(editor.textCursor().positionInBlock(), 5);
  vim.feedKeys("gg0/fo\\%[od]<CR>");
  QCOMPARE(editor.textCursor().blockNumber(), 1);
  vim.feedKeys("gg0/\\(foo\\)\\@<=d<CR>");
  QCOMPARE(editor.textCursor().blockNumber(), 1);
  QCOMPARE(editor.textCursor().positionInBlock(), 3);
  vim.feedKeys("gg0/.*bar\\&.*food<CR>");
  QCOMPARE(editor.toPlainText(), QString("foo bar\nfood baz\nbar foo"));
  vim.feedKeys("gg0/\\%d98<CR>");
  QCOMPARE(editor.textCursor().positionInBlock(), 4);
  vim.feedKeys("gg0/\\v(foo)@=.<CR>x");
  QCOMPARE(editor.toPlainText(), QString("foo bar\nood baz\nbar foo"));
}

void TestVimMode::testDeleteMarks() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("a\nb\nc");
  vim.setEnabled(true);
  vim.feedKeys("jmajmb:delmarks a<CR>'ax");
  QCOMPARE(editor.toPlainText(), QString("a\nb\n"));
  vim.feedKeys("ggmcjmd:delm!<CR>");
  vim.feedKeys("'bx");
  QCOMPARE(editor.toPlainText(), QString("a\n\n"));
}

void TestVimMode::testChangeMarks() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abc");
  vim.setEnabled(true);
  vim.feedKeys("ixyz<Esc>`[iQ<Esc>");
  QCOMPARE(editor.toPlainText(), QString("Qxyzabc"));
  vim.feedKeys("`]iR<Esc>");
  QCOMPARE(editor.toPlainText(), QString("QRxyzabc"));
}

void TestVimMode::testExAddressSearchForms() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("a\nb\nfoo\nfoo\nz");
  vim.setEnabled(true);
  vim.feedKeys(":/foo//foo/d<CR>");
  QCOMPARE(editor.toPlainText(), QString("a\nb\nfoo\nz"));
  vim.feedKeys("gg:/b/;+1d<CR>");
  QCOMPARE(editor.toPlainText(), QString("a\nz"));
  editor.setPlainText("x\nfoo\ny");
  vim.feedKeys("2G/foo<CR>gg:\\/d<CR>");
  QCOMPARE(editor.toPlainText(), QString("x\ny"));
}

void TestVimMode::testNormalMappings() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("one two three\nfour five");
  vim.setEnabled(true);
  vim.feedKeys(":nnoremap Q dw<CR>Q");
  QCOMPARE(editor.toPlainText(), QString("two three\nfour five"));
  // Counts apply to the mapped command, and recursive maps expand again.
  vim.feedKeys(":nmap X Q<CR>2X");
  QCOMPARE(editor.toPlainText(), QString("\nfour five"));
  vim.feedKeys("u:let mapleader = ','<CR>:nnoremap <lt>leader>d dd<CR>,d");
  QCOMPARE(editor.toPlainText(), QString("four five"));
  // A noremap rhs is not remapped, a map rhs starting with its lhs is safe.
  editor.setPlainText("abc");
  vim.feedKeys("gg0:nnoremap x l<CR>:nnoremap l x<CR>x");
  QCOMPARE(editor.toPlainText(), QString("abc"));
  QCOMPARE(editor.textCursor().positionInBlock(), 1);
  vim.feedKeys(":nmap j jx<CR>j");
  QCOMPARE(editor.toPlainText(), QString("abc"));
  // Character arguments are never remapped.
  vim.feedKeys(":nunmap x<CR>fl");
  QCOMPARE(editor.textCursor().positionInBlock(), 1);
  vim.feedKeys(":nmap a b<CR>:nmap b a<CR>a");
  QVERIFY(editor.toPlainText() == "abc");
  // :normal! ignores mappings while :normal uses them.
  editor.setPlainText("abc");
  vim.feedKeys("gg0:nnoremap x dd<CR>:normal! x<CR>");
  QCOMPARE(editor.toPlainText(), QString("bc"));
  vim.feedKeys(":normal x<CR>");
  QCOMPARE(editor.toPlainText(), QString(""));
}

void TestVimMode::testInsertMappingWaitsForTimeout() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("");
  vim.setEnabled(true);
  vim.feedKeys(":inoremap jk <lt>Esc><CR>");
  QKeyEvent i(QEvent::KeyPress, Qt::Key_I, Qt::NoModifier, "i");
  vim.processKeyEvent(&i);
  QKeyEvent j(QEvent::KeyPress, Qt::Key_J, Qt::NoModifier, "j");
  QKeyEvent k(QEvent::KeyPress, Qt::Key_K, Qt::NoModifier, "k");
  QKeyEvent x(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, "x");
  QVERIFY(vim.processKeyEvent(&j));
  QCOMPARE(editor.toPlainText(), QString(""));
  QCOMPARE(vim.pendingKeys(), QString("j"));
  QVERIFY(vim.processKeyEvent(&k));
  QCOMPARE(vim.mode(), VimEditMode::Normal);
  QCOMPARE(editor.toPlainText(), QString(""));

  vim.processKeyEvent(&i);
  vim.processKeyEvent(&j);
  vim.processKeyEvent(&x);
  QCOMPARE(editor.toPlainText(), QString("jx"));
  QCOMPARE(vim.mode(), VimEditMode::Insert);

  vim.feedKeys("<Esc>:set timeoutlen=20<CR>a");
  vim.processKeyEvent(&j);
  QTRY_COMPARE_WITH_TIMEOUT(editor.toPlainText(), QString("jxj"), 2000);
  QCOMPARE(vim.mode(), VimEditMode::Insert);
  QCOMPARE(vim.pendingKeys(), QString());
}

void TestVimMode::testOperatorPendingAndVisualMappings() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("foo(bar, baz) end");
  vim.setEnabled(true);
  vim.feedKeys(":onoremap p i(<CR>fbdp");
  QCOMPARE(editor.toPlainText(), QString("foo() end"));
  vim.feedKeys("u0:xnoremap L e<CR>vLd");
  QCOMPARE(editor.toPlainText(), QString("(bar, baz) end"));
  // Normal-mode mappings do not fire in Operator-pending mode.
  editor.setPlainText("a b c");
  vim.feedKeys("gg0:nnoremap w x<CR>dw");
  QCOMPARE(editor.toPlainText(), QString("b c"));
  vim.feedKeys(":nnoremap <expr> W 'x'<CR>W");
  QCOMPARE(editor.toPlainText(), QString(" c"));
  vim.feedKeys(":nnoremap Z <lt>Nop><CR>Z");
  QCOMPARE(editor.toPlainText(), QString(" c"));
}

void TestVimMode::testMappingListingAndUnmap() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  vim.setEnabled(true);
  QStringList messages;
  connect(&vim, &VimMode::statusMessage,
          [&messages](const QString &m) { messages << m; });
  vim.feedKeys(":nnoremap <lt>Space>w :w<lt>CR><CR>:nmap<CR>");
  QVERIFY(messages.last().contains("n   "));
  QVERIFY(messages.last().contains("*:w<CR>"));
  vim.feedKeys(":nunmap <lt>Space>w<CR>:nmap<CR>");
  QCOMPARE(messages.last(), QString("No mapping found"));
  vim.feedKeys(":nunmap Q<CR>");
  QCOMPARE(messages.last(), QString("E31: No such mapping"));
  vim.feedKeys(":map! zz yy<CR>:imap<CR>");
  QVERIFY(messages.last().startsWith("i  zz"));
  vim.feedKeys(":mapclear!<CR>:imap<CR>");
  QCOMPARE(messages.last(), QString("No mapping found"));
}

void TestVimMode::testAbbreviations() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("");
  vim.setEnabled(true);
  vim.feedKeys(":iabbrev teh the<CR>iteh cat tehx steh.<Esc>");
  QCOMPARE(editor.toPlainText(), QString("the cat tehx steh."));
  vim.feedKeys(":iab ret return<CR>o  ret<C-]>;<Esc>");
  QCOMPARE(editor.toPlainText(), QString("the cat tehx steh.\n  return;"));
  vim.feedKeys(":iunabbrev teh<CR>oteh <Esc>");
  QCOMPARE(editor.toPlainText(),
           QString("the cat tehx steh.\n  return;\nteh "));
  vim.feedKeys(":cabbrev W w<CR>");
  QSignalSpy executed(&vim, &VimMode::commandExecuted);
  vim.feedKeys(":W<CR>");
  QCOMPARE(executed.count(), 1);
  QCOMPARE(executed.first().first().toString(), QString("save"));
}

void TestVimMode::testSourceVimrc() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QFile rc(dir.filePath("vimrc"));
  QVERIFY(rc.open(QIODevice::WriteOnly));
  rc.write("\" comment line\n"
           "set sw=2\n"
           "let mapleader = ' '\n"
           "nnoremap <leader>x dd\n"
           "if has('nvim')\n"
           "  nnoremap Q gg\n"
           "else\n"
           "  nnoremap Q G\n"
           "endif\n"
           "function! Foo()\n"
           "  echo 'x'\n"
           "endfunction\n"
           "inoremap jj\n"
           "  \\ <Esc>\n");
  rc.close();
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("a\nb\nc");
  vim.setEnabled(true);
  QString error;
  QVERIFY(vim.sourceFile(rc.fileName(), &error));
  vim.feedKeys(" x>>");
  QCOMPARE(editor.toPlainText(), QString("  b\nc"));
  vim.feedKeys("Q");
  QCOMPARE(editor.textCursor().blockNumber(), 1);
  vim.feedKeys("Ajj");
  QCOMPARE(vim.mode(), VimEditMode::Normal);
  QVERIFY(!vim.sourceFile(dir.filePath("missing"), &error));
  QVERIFY(error.startsWith("E484"));
  vim.feedKeys(QString(":source %1<CR>").arg(rc.fileName()));
  QCOMPARE(vim.mode(), VimEditMode::Normal);
}

void TestVimMode::testMappingEdgeCases() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abcdef");
  vim.setEnabled(true);
  QStringList messages;
  connect(&vim, &VimMode::statusMessage,
          [&messages](const QString &m) { messages << m; });
  // Mutually recursive mappings stop with E223 instead of hanging.
  vim.feedKeys(":nmap a b<CR>:nmap b a<CR>a");
  QVERIFY(messages.contains("E223: Recursive mapping"));
  QCOMPARE(editor.toPlainText(), QString("abcdef"));
  // Plugin mappings do nothing, and the rest of the mapping is dropped.
  vim.feedKeys(":nmap Q <lt>Plug>(foo)x<CR>Q");
  QCOMPARE(editor.toPlainText(), QString("abcdef"));
  // Macros record the typed keys, not what the mapping expanded to.
  vim.feedKeys(":nnoremap Z x<CR>qaZq");
  QCOMPARE(editor.toPlainText(), QString("bcdef"));
  QCOMPARE(vim.registerContent('a'), QString("Z"));
  vim.feedKeys(":nunmap Z<CR>@a<Esc>");
  QCOMPARE(editor.toPlainText(), QString("bcdef"));
  // A failing command inside a mapping abandons the rest of it.
  vim.feedKeys(":nnoremap W fzx<CR>0W");
  QCOMPARE(editor.toPlainText(), QString("bcdef"));
  // Comments and blank lines are ignored on the command line.
  messages.clear();
  vim.feedKeys(":\" just a comment<CR>");
  QVERIFY(messages.isEmpty());
}

void TestVimMode::testSubstituteConfirm() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("a1 a2 a3\na4");
  vim.setEnabled(true);
  QStringList messages;
  connect(&vim, &VimMode::statusMessage,
          [&messages](const QString &m) { messages << m; });
  vim.feedKeys(":%s/a/X/gc<CR>");
  QVERIFY(messages.last().startsWith("replace with X (y/n/a/q/l"));
  QCOMPARE(editor.textCursor().selectedText(), QString("a"));
  vim.feedKeys("yn");
  QCOMPARE(editor.toPlainText(), QString("X1 a2 a3\na4"));
  QCOMPARE(editor.textCursor().selectionStart(), 6);
  vim.feedKeys("y");
  QCOMPARE(editor.toPlainText(), QString("X1 a2 X3\na4"));
  // Keys other than the answers are ignored while asking.
  vim.feedKeys("dd");
  QCOMPARE(editor.toPlainText(), QString("X1 a2 X3\na4"));
  vim.feedKeys("q");
  QVERIFY(!editor.textCursor().hasSelection());
  QCOMPARE(editor.textCursor().blockNumber(), 1);
  // Every accepted replacement is undone in one step.
  vim.feedKeys("u");
  QCOMPARE(editor.toPlainText(), QString("a1 a2 a3\na4"));

  vim.feedKeys(":%s/a/X/gc<CR>na");
  QCOMPARE(editor.toPlainText(), QString("a1 X2 X3\nX4"));
  vim.feedKeys("u:%s/a/X/gc<CR>l");
  QCOMPARE(editor.toPlainText(), QString("X1 a2 a3\na4"));
  QCOMPARE(editor.textCursor().positionInBlock(), 0);
  vim.feedKeys("u:%s/a/X/gc<CR><Esc>");
  QCOMPARE(editor.toPlainText(), QString("a1 a2 a3\na4"));
  vim.feedKeys("x");
  QCOMPARE(editor.toPlainText(), QString("1 a2 a3\na4"));
}

void TestVimMode::testFoldsHideLines() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("l1\nl2\nl3\nl4\nl5");
  vim.setEnabled(true);
  auto visible = [&editor](int line) {
    return editor.document()->findBlockByNumber(line).isVisible();
  };
  vim.feedKeys("jzfj");
  QVERIFY(visible(1));
  QVERIFY(!visible(2));
  QVERIFY(visible(3));
  vim.feedKeys("zo");
  QVERIFY(visible(2));
  vim.feedKeys("zc");
  QVERIFY(!visible(2));
  vim.feedKeys("zn");
  QVERIFY(visible(2));
  vim.feedKeys("zN");
  QVERIFY(!visible(2));
  // A nested fold stays hidden inside a closed outer one.
  vim.feedKeys("zcggzfGzo");
  QVERIFY(!visible(2));
  QVERIFY(visible(4));
  vim.feedKeys("zR");
  QVERIFY(visible(2));
  vim.feedKeys("zM");
  QVERIFY(!visible(1));
  QVERIFY(!visible(4));
  vim.feedKeys("zE");
  for (int line = 0; line < 5; ++line)
    QVERIFY(visible(line));
  // Deleting a closed fold removes it together with its lines.
  vim.feedKeys(":2,3fold<CR>:2d<CR>");
  QCOMPARE(editor.toPlainText(), QString("l1\nl4\nl5"));
  for (int line = 0; line < 3; ++line)
    QVERIFY(visible(line));
  QStringList messages;
  connect(&vim, &VimMode::statusMessage,
          [&messages](const QString &m) { messages << m; });
  vim.feedKeys("zd");
  QCOMPARE(messages.last(), QString("E490: No fold found"));
}

void TestVimMode::testEditorCommandsFromVim() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("see https://example.com/a.b, ok\nx");
  vim.setEnabled(true);
  QStringList commands;
  connect(&vim, &VimMode::commandExecuted,
          [&commands](const QString &c) { commands << c; });
  vim.feedKeys("3gt");
  QCOMPARE(commands.last(), QString("tab:3"));
  vim.feedKeys("gt");
  QCOMPARE(commands.last(), QString("nextTab"));
  vim.feedKeys("<C-^>");
  QCOMPARE(commands.last(), QString("alternateTab"));
  vim.feedKeys(":tabn 2<CR>");
  QCOMPARE(commands.last(), QString("tab:2"));
  vim.feedKeys(":b 4<CR>");
  QCOMPARE(commands.last(), QString("tab:4"));
  vim.feedKeys(":b main<CR>");
  QCOMPARE(commands.last(), QString("bufferName:main"));
  vim.feedKeys(":tablast<CR>");
  QCOMPARE(commands.last(), QString("lastTab"));
  vim.feedKeys(":ls<CR>");
  QCOMPARE(commands.last(), QString("listBuffers"));
  vim.feedKeys(":e #<CR>");
  QCOMPARE(commands.last(), QString("alternateTab"));
  vim.feedKeys(":w copy.txt<CR>");
  QCOMPARE(commands.last(), QString("writeAs:copy.txt"));
  vim.feedKeys(":sav ~/new.txt<CR>");
  QCOMPARE(commands.last(), QString("saveAs:%1/new.txt").arg(QDir::homePath()));
  vim.feedKeys(":e $MYVIMRC<CR>");
  QCOMPARE(commands.last(),
           QString("edit:%1").arg(VimMode::defaultVimrcPath()));
  vim.feedKeys(":set nonumber<CR>");
  QCOMPARE(commands.last(), QString("option:number:off"));
  vim.feedKeys(":set wrap<CR>");
  QCOMPARE(commands.last(), QString("option:wrap:on"));
  vim.feedKeys("K");
  QCOMPARE(commands.last(), QString("hover"));
  vim.feedKeys("0fhgx");
  QCOMPARE(commands.last(), QString("openUrl:https://example.com/a.b"));

  QStringList messages;
  connect(&vim, &VimMode::statusMessage,
          [&messages](const QString &m) { messages << m; });
  vim.feedKeys("Gggx:changes<CR>");
  QVERIFY(messages.last().contains("1:0"));
  vim.feedKeys(":jumps<CR>");
  QVERIFY(messages.last().contains("2:0"));
}

void TestVimMode::testCountedVisualReselect() {
  QPlainTextEdit editor;
  VimMode vim(&editor);
  editor.setPlainText("abcdefghij\nklmnopqrst");
  vim.setEnabled(true);
  // Without an earlier Visual operation [count]v selects count characters.
  vim.feedKeys("3vd");
  QCOMPARE(editor.toPlainText(), QString("defghij\nklmnopqrst"));
  // Afterwards it reselects count times the size of the last one.
  vim.feedKeys("2vd");
  QCOMPARE(editor.toPlainText(), QString("j\nklmnopqrst"));
  // The reselection keeps the earlier mode, even when started with V.
  vim.feedKeys("u3Vd");
  QCOMPARE(editor.toPlainText(), QString("klmnopqrst"));
}

QTEST_MAIN(TestVimMode)
#include "test_vimmode.moc"
