

#include "theme/themeengine.h"
#include "ui/panels/shellprofile.h"
#include "ui/panels/terminal.h"
#include <QClipboard>
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTextBlock>
#include <QToolButton>
#include <QtTest/QtTest>

class TestTerminal : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testConstruction();
  void testDestructor();

  void testStartAndStopShell();
  void testIsRunning();

  void testSetWorkingDirectory();

  void testClear();
  void testPtyClearRedrawsPrompt();
  void testTerminalDocumentMargin();
  void testPtyGridFitsViewport();
  void testPtyKeepsStandardWidthBeforeLayout();
  void testPtyWidthFollowsWidgetOnceShown();

  void testShellStartedSignal();

  void testMultipleStopCalls();
  void testRestartAfterStop();

  void testShellProfiles();
  void testScrollbackLines();
  void testLargeSingleLineOutputIsBounded();
  void testLargeMultilineOutputKeepsRecentScrollback();
  void testLinkDetection();
  void testSendText();
  void testInterruptActiveRunProcess();
  void testExternalRunCrashDoesNotBreakTerminal();
  void testNoEmbeddedCloseButton();
  void testCwdLabelExists();
  void testCwdLabelUpdatesOnDirectoryChange();
  void testContextMenuExists();
  void testCopySelectionToClipboard();
  void testFontZoomIn();
  void testFontZoomOut();
  void testFontZoomReset();
  void testFontZoomLimits();
  void testTypingOutsidePromptAppendsToInput();
  void testBackspaceDoesNotDeleteTerminalOutput();
  void testPtyBackspaceErasesEchoedInput();
  void testPtyCarriageReturnCanReplaceLine();
  void testPtyCrLfKeepsOutputHistoryOnSeparateLines();
  void testPtyCursorLeftAllowsMidLineInsert();
  void testPtyDeleteCharactersRemovesLeadingPadding();
  void testAnsiColorsFollowThemePalette();
  void testAnsiReverseVideoSwapsForegroundAndBackground();
  void testPtyAutoWrapsLongLinesAtTerminalWidth();
  void testPtyCharsetDesignatorsDoNotLeakIntoTranscript();
  void testPtyAbsoluteCursorAddressingPlacesTextOnTargetLine();
  void testPtyOutOfBoundsCursorAddressingClampsToVisibleScreen();
  void testPtyCursorStatePersistsAcrossChunks();
  void testPtyAlternateScreenRestoreRemovesTransientUi();
  void testPtyAlternateScreenExitSplitAcrossChunksRestoresPrimaryScreen();
  void testPtyDumbVimStartupDoesNotLeakControlCharacters();
  void testPtyMouseClickDoesNotMoveInputCursor();
  void testRunProcessAcceptsInteractiveInput();
  void testRunProcessAcceptsMultilinePastedInput();
  void testRunInputIndicatorVisibility();
  void testLooksLikeInputPromptPatterns();
  void testIndicatorNotShownForNonPromptOutput();
  void testIndicatorNotShownOnProcessStart();
  void testIndicatorHiddenAfterNonPromptOutput();
  void testPtyCursorAddressingIsRelativeToScreen();
  void testPtyScrollRegionDeleteAndInsertLines();
  void testPtyLineFeedAtRegionBottomScrollsRegion();
  void testPtyReverseIndexAtTopScrollsDown();
  void testPtyLineFeedKeepsColumn();
  void testPtyBackspaceMovesWithoutErasing();
  void testPtyTabMovesToNextStopWithoutMovingText();
  void testPtyUtf8SplitAcrossReadsIsDecoded();
  void testPtyWideCharactersTakeTwoColumns();
  void testPtyWideCharacterWrapsBeforeRightMargin();
  void testPtyAlternateScreenRestoresColors();
  void testPtyClearScreenKeepsScrollback();
  void testPtyPrivateModeSgrIsIgnored();
  void testPtyTrueColorSgr();
  void testScrollbackTrimKeepsScreenAddressable();
  void testPtyKeySequences();
  void testPtyPasteData();
  void testPtyControlKeysOverrideApplicationShortcuts();
  void testPtyInterruptStopsForegroundJob();
  void testNewTerminalStartsInDirectoryWithoutCd();
  void testZoomOverridesApplicationEditorFont();
  void testShrinkingAfterClearKeepsPromptInPlace();
};

void TestTerminal::initTestCase() {}

void TestTerminal::cleanupTestCase() {}

void TestTerminal::testConstruction() {
  Terminal *t = new Terminal();
  QVERIFY(t != nullptr);

  t->stopShell();
  QTest::qWait(200);
  delete t;
}

void TestTerminal::testDestructor() {
  Terminal *t = new Terminal();
  t->stopShell();
  QTest::qWait(200);
  delete t;

  QVERIFY(true);
}

void TestTerminal::testStartAndStopShell() {
  Terminal terminal;

  terminal.stopShell();
  QTest::qWait(200);
  QVERIFY(!terminal.isRunning());

  bool started = terminal.startShell();
  QVERIFY(started);
  QVERIFY(terminal.isRunning());

  terminal.stopShell();
  QTest::qWait(200);
  QVERIFY(!terminal.isRunning());
}

void TestTerminal::testIsRunning() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QVERIFY(!terminal.isRunning());
  terminal.startShell();
  QVERIFY(terminal.isRunning());
  terminal.stopShell();
  QTest::qWait(200);
  QVERIFY(!terminal.isRunning());
}

void TestTerminal::testSetWorkingDirectory() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  terminal.setWorkingDirectory("/tmp");
  bool started = terminal.startShell();

  QVERIFY(started);
  QVERIFY(terminal.isRunning());

  terminal.stopShell();
  QTest::qWait(200);
}

void TestTerminal::testClear() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  terminal.startShell();
  QTest::qWait(200);

  terminal.clear();

  terminal.stopShell();
  QTest::qWait(200);
  QVERIFY(true);
}

void TestTerminal::testPtyClearRedrawsPrompt() {
  Terminal terminal;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  QTRY_VERIFY_WITH_TIMEOUT(terminal.isRunning(), 3000);
  terminal.appendOutput("temporary output\n");
  terminal.clear();

  QTRY_VERIFY_WITH_TIMEOUT(!textEdit->toPlainText().trimmed().isEmpty(), 3000);

  terminal.stopShell();
}

void TestTerminal::testTerminalDocumentMargin() {
  Terminal terminal;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  QCOMPARE(qRound(textEdit->document()->documentMargin()), 12);
}

void TestTerminal::testPtyGridFitsViewport() {
  Terminal terminal;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.resize(900, 320);
  terminal.show();
  QTest::qWait(50);
  terminal.updatePtySize();

  const QFontMetrics metrics(textEdit->font());
  const QRect viewport = textEdit->viewport()->rect();
  QVERIFY(terminal.m_terminalColumns *
                  qMax(1, metrics.horizontalAdvance(QLatin1Char('M'))) +
              textEdit->cursorWidth() <=
          viewport.width());
  QVERIFY(terminal.m_terminalRows * qMax(1, metrics.lineSpacing()) <=
          viewport.height());

  terminal.stopShell();
}

void TestTerminal::testPtyKeepsStandardWidthBeforeLayout() {
  Terminal terminal;
  terminal.updatePtySize();

  QCOMPARE(terminal.m_terminalColumns, 80);
  QCOMPARE(terminal.m_terminalRows, 24);

  terminal.stopShell();
}

void TestTerminal::testPtyWidthFollowsWidgetOnceShown() {
  Terminal terminal;
  terminal.resize(900, 320);
  terminal.show();
  QTest::qWait(50);
  terminal.updatePtySize();

  QVERIFY(terminal.m_terminalColumns > 40);
  QVERIFY(terminal.m_terminalRows >= 4);

  terminal.stopShell();
}

void TestTerminal::testShellStartedSignal() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QSignalSpy spy(&terminal, &Terminal::shellStarted);

  terminal.startShell();

  QVERIFY(spy.count() >= 1);

  terminal.stopShell();
  QTest::qWait(200);
}

void TestTerminal::testMultipleStopCalls() {
  Terminal terminal;

  QVERIFY(terminal.isRunning());

  terminal.stopShell();
  terminal.stopShell();
  terminal.stopShell();

  QVERIFY(!terminal.isRunning());
}

void TestTerminal::testRestartAfterStop() {
  Terminal terminal;

  QVERIFY(terminal.isRunning());

  terminal.stopShell();
  QVERIFY(!terminal.isRunning());

  bool started = terminal.startShell();
  QVERIFY(started);
  QVERIFY(terminal.isRunning());

  terminal.stopShell();
  QVERIFY(!terminal.isRunning());

  started = terminal.startShell();
  QVERIFY(started);
  QVERIFY(terminal.isRunning());

  terminal.stopShell();
}

void TestTerminal::testShellProfiles() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QStringList profiles = terminal.availableShellProfiles();
  QVERIFY(!profiles.isEmpty());

  ShellProfile profile = terminal.shellProfile();
  QVERIFY(profile.isValid());
  QVERIFY(!profile.name.isEmpty());
  QVERIFY(!profile.command.isEmpty());

  if (!profiles.isEmpty()) {
    QString firstName = profiles.first();
    bool result = terminal.setShellProfileByName(firstName);
    QVERIFY(result);
    QCOMPARE(terminal.shellProfile().name, firstName);
  }

  bool result = terminal.setShellProfileByName("NonExistentShell12345");
  QVERIFY(!result);
}

void TestTerminal::testScrollbackLines() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  int defaultLines = terminal.scrollbackLines();
  QVERIFY(defaultLines > 0);

  terminal.setScrollbackLines(5000);
  QCOMPARE(terminal.scrollbackLines(), 5000);

  terminal.setScrollbackLines(0);
  QCOMPARE(terminal.scrollbackLines(), 0);

  terminal.setScrollbackLines(1000);
  QCOMPARE(terminal.scrollbackLines(), 1000);
}

void TestTerminal::testLargeSingleLineOutputIsBounded() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput(QString(3 * 1024 * 1024, QLatin1Char('x')) +
                        "TAIL_MARKER");

  QVERIFY(textEdit->document()->characterCount() <=
          Terminal::kMaxDocumentCharacters + 1);
  QVERIFY(textEdit->toPlainText().size() <=
          Terminal::kMaxOutputChunkCharacters + 4096);
  QVERIFY(textEdit->toPlainText().contains("TAIL_MARKER"));
}

void TestTerminal::testLargeMultilineOutputKeepsRecentScrollback() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.setScrollbackLines(25);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  QString output;
  for (int i = 0; i < 200; ++i) {
    output += QString("line %1\n").arg(i);
  }
  terminal.appendOutput(output);

  QVERIFY(textEdit->document()->blockCount() <= terminal.scrollbackLines());
  QVERIFY(!textEdit->toPlainText().contains("line 0\n"));
  QVERIFY(textEdit->toPlainText().contains("line 199"));
}

void TestTerminal::testLinkDetection() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QVERIFY(terminal.isLinkDetectionEnabled());

  terminal.setLinkDetectionEnabled(false);
  QVERIFY(!terminal.isLinkDetectionEnabled());

  terminal.setLinkDetectionEnabled(true);
  QVERIFY(terminal.isLinkDetectionEnabled());
}

void TestTerminal::testSendText() {
  Terminal terminal;

  QVERIFY(terminal.isRunning());

  terminal.sendText("echo test", false);
  terminal.sendText("ls", true);

  terminal.stopShell();
  QTest::qWait(200);

  terminal.sendText("test", true);

  QVERIFY(true);
}

void TestTerminal::testInterruptActiveRunProcess() {
  Terminal terminal;
  QSignalSpy finishedSpy(&terminal, &Terminal::processFinished);
  QVERIFY(finishedSpy.isValid());

  terminal.executeCommand("sh", QStringList() << "-c" << "sleep 30",
                          QDir::tempPath());

  QTRY_VERIFY_WITH_TIMEOUT(terminal.hasActiveRunProcess(), 3000);
  QVERIFY(terminal.canInterruptActiveProcess());
  QVERIFY(terminal.interruptActiveProcess());
  QTRY_VERIFY_WITH_TIMEOUT(!terminal.hasActiveRunProcess(), 5000);
  QCOMPARE(finishedSpy.count(), 1);
  QCOMPARE(finishedSpy.takeFirst().at(0).toInt(), 130);

  terminal.stopShell();
  QTest::qWait(200);
}

void TestTerminal::testExternalRunCrashDoesNotBreakTerminal() {
  Terminal terminal;
  QSignalSpy finishedSpy(&terminal, &Terminal::processFinished);
  QSignalSpy errorSpy(&terminal, &Terminal::processError);
  QVERIFY(finishedSpy.isValid());
  QVERIFY(errorSpy.isValid());

#ifdef Q_OS_WIN
  terminal.executeCommand("cmd", QStringList() << "/C" << "exit 1",
                          QDir::tempPath());
#else
  terminal.executeCommand("sh", QStringList() << "-c" << "kill -SEGV $$",
                          QDir::tempPath());
#endif

  QTRY_VERIFY_WITH_TIMEOUT(!terminal.hasActiveRunProcess(), 5000);
  QVERIFY(finishedSpy.count() >= 1);
  QVERIFY(terminal.isRunning());

  terminal.stopShell();
  QTest::qWait(200);
}

void TestTerminal::testNoEmbeddedCloseButton() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QToolButton *closeButton = terminal.findChild<QToolButton *>("closeButton");
  QVERIFY(closeButton == nullptr);
}

void TestTerminal::testCwdLabelExists() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QLabel *cwdLabel = terminal.findChild<QLabel *>("cwdLabel");
  QVERIFY(cwdLabel != nullptr);
  QVERIFY(!cwdLabel->isVisible());
}

void TestTerminal::testCwdLabelUpdatesOnDirectoryChange() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  terminal.setWorkingDirectory("/tmp");

  QLabel *cwdLabel = terminal.findChild<QLabel *>("cwdLabel");
  QVERIFY(cwdLabel != nullptr);
  QVERIFY(!cwdLabel->isVisible());
  QVERIFY(cwdLabel->text().contains("/tmp"));
}

void TestTerminal::testContextMenuExists() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);
  QCOMPARE(textEdit->contextMenuPolicy(), Qt::CustomContextMenu);

  QMenu *contextMenu = terminal.findChild<QMenu *>();
  QVERIFY(contextMenu != nullptr);

  QList<QAction *> actions = contextMenu->actions();
  bool hasCopy = false;
  bool hasPaste = false;
  bool hasSelectAll = false;
  bool hasClear = false;
  for (QAction *action : actions) {
    if (action->text().contains("Copy"))
      hasCopy = true;
    if (action->text().contains("Paste"))
      hasPaste = true;
    if (action->text().contains("Select All"))
      hasSelectAll = true;
    if (action->text().contains("Clear"))
      hasClear = true;
  }
  QVERIFY(hasCopy);
  QVERIFY(hasPaste);
  QVERIFY(hasSelectAll);
  QVERIFY(hasClear);
}

void TestTerminal::testCopySelectionToClipboard() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  textEdit->setPlainText("first line\nsecond line");
  QTextCursor cursor = textEdit->textCursor();
  cursor.setPosition(0);
  cursor.setPosition(QString("first line\nsecond").size(),
                     QTextCursor::KeepAnchor);
  textEdit->setTextCursor(cursor);

  terminal.show();
  textEdit->setFocus();
  QApplication::clipboard()->clear();
  QTest::keyClick(textEdit, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);

  QCOMPARE(QApplication::clipboard()->text(), QString("first line\nsecond"));
}

void TestTerminal::testFontZoomIn() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  int originalSize = terminal.currentFontSize();
  terminal.zoomIn();
  QCOMPARE(terminal.currentFontSize(), originalSize + 1);
}

void TestTerminal::testFontZoomOut() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  int originalSize = terminal.currentFontSize();
  terminal.zoomOut();
  QCOMPARE(terminal.currentFontSize(), originalSize - 1);
}

void TestTerminal::testFontZoomReset() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  terminal.zoomIn();
  terminal.zoomIn();
  terminal.zoomIn();
  QVERIFY(terminal.currentFontSize() != 11);

  terminal.zoomReset();
  QCOMPARE(terminal.currentFontSize(), 11);
}

void TestTerminal::testFontZoomLimits() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  for (int i = 0; i < 100; ++i) {
    terminal.zoomIn();
  }
  QVERIFY(terminal.currentFontSize() <= 48);

  for (int i = 0; i < 100; ++i) {
    terminal.zoomOut();
  }
  QVERIFY(terminal.currentFontSize() >= 6);
}

void TestTerminal::testTypingOutsidePromptAppendsToInput() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  const QString transcript = "printed output\n$ ";
  textEdit->setPlainText(transcript);
  terminal.m_inputStartPosition = transcript.size();

  terminal.show();
  textEdit->setFocus();
  QTest::qWait(50);

  QTextCursor cursor = textEdit->textCursor();
  cursor.setPosition(1);
  textEdit->setTextCursor(cursor);

  QTest::keyClicks(textEdit, "echo");

  QCOMPARE(textEdit->toPlainText(), transcript + "echo");
}

void TestTerminal::testBackspaceDoesNotDeleteTerminalOutput() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  const QString transcript = "printed output\n$ cmd";
  textEdit->setPlainText(transcript);
  terminal.m_inputStartPosition = QString("printed output\n$ ").size();

  terminal.show();
  textEdit->setFocus();
  QTest::qWait(50);

  QTextCursor cursor = textEdit->textCursor();
  cursor.setPosition(terminal.m_inputStartPosition);
  textEdit->setTextCursor(cursor);
  QTest::keyClick(textEdit, Qt::Key_Backspace);
  QCOMPARE(textEdit->toPlainText(), transcript);

  cursor.setPosition(2);
  textEdit->setTextCursor(cursor);
  QTest::keyClick(textEdit, Qt::Key_Backspace);
  QCOMPARE(textEdit->toPlainText(), transcript);
}

void TestTerminal::testPtyBackspaceErasesEchoedInput() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("$ abc");
  terminal.appendOutput("\b \b");

  QCOMPARE(textEdit->toPlainText(), QString("$ ab"));
}

void TestTerminal::testPtyCarriageReturnCanReplaceLine() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("downloading 99%");
  terminal.appendOutput("\rready\x1b[K");

  QCOMPARE(textEdit->toPlainText(), QString("ready"));
}

void TestTerminal::testPtyCrLfKeepsOutputHistoryOnSeparateLines() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("$ echo hi\r\nhi\r\n$ ");

  QCOMPARE(textEdit->toPlainText(), QString("$ echo hi\nhi\n$ "));
}

void TestTerminal::testPtyCursorLeftAllowsMidLineInsert() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("$ ac");
  terminal.appendOutput("\x1b[D\x1b[1@"
                        "b");

  QCOMPARE(textEdit->toPlainText(), QString("$ abc"));
}

void TestTerminal::testPtyDeleteCharactersRemovesLeadingPadding() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("    alpha");
  terminal.appendOutput("\x1b[1G\x1b[4P");

  QCOMPARE(textEdit->toPlainText(), QString("alpha"));
}

void TestTerminal::testAnsiColorsFollowThemePalette() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("\x1b[31mR\x1b[32mG\x1b[94mB");

  QTextCursor cursor(textEdit->document());
  cursor.setPosition(0);
  cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
  QCOMPARE(cursor.charFormat().foreground().color(),
           ThemeEngine::instance().activeTheme().colors.ansiRed);

  cursor.setPosition(1);
  cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
  QCOMPARE(cursor.charFormat().foreground().color(),
           ThemeEngine::instance().activeTheme().colors.ansiGreen);

  cursor.setPosition(2);
  cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
  QCOMPARE(cursor.charFormat().foreground().color(),
           ThemeEngine::instance().activeTheme().colors.ansiBrightBlue);
}

void TestTerminal::testAnsiReverseVideoSwapsForegroundAndBackground() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("\x1b[31;47;7mX");

  QTextCursor cursor(textEdit->document());
  cursor.setPosition(0);
  cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
  const QTextCharFormat format = cursor.charFormat();
  QCOMPARE(format.foreground().color(),
           ThemeEngine::instance().activeTheme().colors.ansiWhite);
  QCOMPARE(format.background().color(),
           ThemeEngine::instance().activeTheme().colors.ansiRed);
}

void TestTerminal::testPtyAutoWrapsLongLinesAtTerminalWidth() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalColumns = 10;

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("0123456789X");

  QCOMPARE(textEdit->toPlainText(), QString("0123456789\nX"));
}

void TestTerminal::testPtyCharsetDesignatorsDoNotLeakIntoTranscript() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("\x1b(B\x1b)0plain");

  QCOMPARE(textEdit->toPlainText(), QString("plain"));
}

void TestTerminal::testPtyAbsoluteCursorAddressingPlacesTextOnTargetLine() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("\f\x1b[1;1Htop");
  terminal.appendOutput("\x1b[3;1Hbottom");

  QCOMPARE(textEdit->toPlainText(), QString("top\n\nbottom"));
}

void TestTerminal::testPtyOutOfBoundsCursorAddressingClampsToVisibleScreen() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalRows = 5;
  terminal.m_terminalColumns = 20;

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("\f\x1b[80;1Hstatus");

  QCOMPARE(textEdit->toPlainText(), QString("\n\n\n\nstatus"));
}

void TestTerminal::testPtyCursorStatePersistsAcrossChunks() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("\f\x1b[2;3Hhe");
  terminal.appendOutput("llo");

  QCOMPARE(textEdit->toPlainText(), QString("\n  hello"));
}

void TestTerminal::testPtyAlternateScreenRestoreRemovesTransientUi() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("$ vim README.md\n");
  terminal.appendOutput("\x1b[?1049h\x1b[1;1Hvim screen");
  terminal.appendOutput("\x1b[?1049l");

  QCOMPARE(textEdit->toPlainText(), QString("$ vim README.md\n"));
}

void TestTerminal::
    testPtyAlternateScreenExitSplitAcrossChunksRestoresPrimaryScreen() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("$ vim README.md\n");
  terminal.appendOutput("\x1b[?1049h\x1b[1;1Hvim screen\x1b[?1049");
  terminal.appendOutput("l");

  QCOMPARE(textEdit->toPlainText(), QString("$ vim README.md\n"));
}

void TestTerminal::testPtyDumbVimStartupDoesNotLeakControlCharacters() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  terminal.appendOutput("stale output");
  terminal.appendOutput("\f\x1b[80;1H\x1b[80;1H");

  QVERIFY(textEdit->toPlainText().trimmed().isEmpty());
}

void TestTerminal::testPtyMouseClickDoesNotMoveInputCursor() {
  Terminal terminal;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  QTRY_VERIFY_WITH_TIMEOUT(terminal.isRunning(), 3000);
  QTRY_VERIFY_WITH_TIMEOUT(!textEdit->toPlainText().trimmed().isEmpty(), 3000);
  const QString beforeClick = textEdit->toPlainText();
  QVERIFY(textEdit->isReadOnly());

  terminal.show();
  textEdit->setFocus();
  QTest::qWait(50);
  QTest::mouseClick(textEdit->viewport(), Qt::LeftButton, Qt::NoModifier,
                    QPoint(4, 4));
  QTest::qWait(100);

  QTextCursor endCursor(textEdit->document());
  endCursor.movePosition(QTextCursor::End);
  QCOMPARE(textEdit->toPlainText(), beforeClick);
  QCOMPARE(textEdit->textCursor().position(), endCursor.position());

  terminal.stopShell();
}

void TestTerminal::testRunProcessAcceptsInteractiveInput() {
  Terminal terminal;
  QSignalSpy finishedSpy(&terminal, &Terminal::processFinished);
  QVERIFY(finishedSpy.isValid());

  terminal.executeCommand(
      "sh",
      QStringList() << "-c"
                    << "printf 'Name: '; read name; printf 'Hello %s\\n' "
                       "\"$name\"",
      QDir::tempPath());

  QTRY_VERIFY_WITH_TIMEOUT(terminal.hasActiveRunProcess(), 3000);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);
  terminal.show();
  textEdit->setFocus();
  QTest::qWait(50);

  QTest::keyClicks(textEdit, "Ada");
  QTest::keyClick(textEdit, Qt::Key_Return);

  QTRY_VERIFY_WITH_TIMEOUT(!terminal.hasActiveRunProcess(), 5000);
  QVERIFY(textEdit->toPlainText().contains("Hello Ada"));
  QVERIFY(finishedSpy.count() >= 1);

  terminal.stopShell();
}

void TestTerminal::testRunProcessAcceptsMultilinePastedInput() {
  Terminal terminal;
  QSignalSpy finishedSpy(&terminal, &Terminal::processFinished);
  QVERIFY(finishedSpy.isValid());

  terminal.executeCommand(
      "sh",
      QStringList()
          << "-c"
          << "while IFS= read -r line; do printf '<%s>\\n' \"$line\"; "
             "[ \"$line\" = done ] && break; done",
      QDir::tempPath());

  QTRY_VERIFY_WITH_TIMEOUT(terminal.hasActiveRunProcess(), 3000);

  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);
  terminal.show();
  textEdit->setFocus();
  QTest::qWait(50);

  QApplication::clipboard()->setText("alpha\nbeta\ndone");
  QTest::keyClick(textEdit, Qt::Key_V, Qt::ControlModifier);
  QTest::keyClick(textEdit, Qt::Key_Return);

  QTRY_VERIFY_WITH_TIMEOUT(!terminal.hasActiveRunProcess(), 5000);
  const QString transcript = textEdit->toPlainText();
  QVERIFY(transcript.contains("<alpha>"));
  QVERIFY(transcript.contains("<beta>"));
  QVERIFY(transcript.contains("<done>"));
  QVERIFY(finishedSpy.count() >= 1);

  terminal.stopShell();
}

void TestTerminal::testRunInputIndicatorVisibility() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QVERIFY(terminal.m_runInputIndicator != nullptr);
  QVERIFY(terminal.m_runInputIndicatorTimer != nullptr);
  QVERIFY(terminal.m_runInputIndicator->isHidden());
  QVERIFY(!terminal.m_runInputIndicatorTimer->isActive());

  terminal.setRunInputIndicatorActive(true);
  QVERIFY(!terminal.m_runInputIndicator->isHidden());
  QVERIFY(terminal.m_runInputIndicatorTimer->isActive());
  QVERIFY(terminal.m_runInputIndicator->text().contains("Input ready"));

  terminal.setRunInputIndicatorActive(false);
  QVERIFY(terminal.m_runInputIndicator->isHidden());
  QVERIFY(!terminal.m_runInputIndicatorTimer->isActive());
}

void TestTerminal::testLooksLikeInputPromptPatterns() {

  QVERIFY(Terminal::looksLikeInputPrompt("Enter your name: "));
  QVERIFY(Terminal::looksLikeInputPrompt("Password:"));
  QVERIFY(Terminal::looksLikeInputPrompt(">>> "));
  QVERIFY(Terminal::looksLikeInputPrompt("Continue? "));

  QVERIFY(Terminal::looksLikeInputPrompt("some output\nEnter value: \n"));

  QVERIFY(Terminal::looksLikeInputPrompt("Are you sure? [y/n]\n"));
  QVERIFY(Terminal::looksLikeInputPrompt("Overwrite? [Y/n]\n"));
  QVERIFY(Terminal::looksLikeInputPrompt("Continue (yes/no)\n"));

  QVERIFY(Terminal::looksLikeInputPrompt("user@host:~$ \n"));
  QVERIFY(Terminal::looksLikeInputPrompt("root@host:~# \n"));

  QVERIFY(!Terminal::looksLikeInputPrompt("Hello, World!\n"));
  QVERIFY(!Terminal::looksLikeInputPrompt("Processing complete.\n"));
  QVERIFY(!Terminal::looksLikeInputPrompt("file1.txt\nfile2.txt\nfile3.txt\n"));

  QVERIFY(!Terminal::looksLikeInputPrompt(""));
  QVERIFY(!Terminal::looksLikeInputPrompt("\n"));
  QVERIFY(!Terminal::looksLikeInputPrompt("\n\n\n"));
}

void TestTerminal::testIndicatorNotShownForNonPromptOutput() {

  Terminal terminal;
  QSignalSpy finishedSpy(&terminal, &Terminal::processFinished);
  QVERIFY(finishedSpy.isValid());

  terminal.executeCommand("sh",
                          QStringList() << "-c" << "printf 'line1\\nline2\\n'",
                          QDir::tempPath());

  QTRY_VERIFY_WITH_TIMEOUT(!terminal.hasActiveRunProcess(), 5000);

  QVERIFY(terminal.m_runInputIndicator != nullptr);
  QVERIFY(terminal.m_runInputIndicator->isHidden());

  terminal.stopShell();
}

void TestTerminal::testIndicatorNotShownOnProcessStart() {

  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QVERIFY(terminal.m_runInputIndicator != nullptr);
  QVERIFY(terminal.m_runInputIndicator->isHidden());

  terminal.executeCommand(
      "sh", QStringList() << "-c" << "sleep 0.1 && printf 'done\\n'",
      QDir::tempPath());

  QTest::qWait(30);

  QVERIFY(terminal.m_runInputIndicator->isHidden());

  QTRY_VERIFY_WITH_TIMEOUT(!terminal.hasActiveRunProcess(), 5000);
  terminal.stopShell();
}

void TestTerminal::testIndicatorHiddenAfterNonPromptOutput() {

  Terminal terminal;
  QSignalSpy finishedSpy(&terminal, &Terminal::processFinished);
  QVERIFY(finishedSpy.isValid());

  terminal.executeCommand(
      "sh",
      QStringList() << "-c"
                    << "printf 'output line 1\\n'; printf 'output line 2\\n'",
      QDir::tempPath());

  QTRY_VERIFY_WITH_TIMEOUT(!terminal.hasActiveRunProcess(), 5000);

  QTest::qWait(200);
  QVERIFY(terminal.m_runInputIndicator != nullptr);
  QVERIFY(terminal.m_runInputIndicator->isHidden());

  terminal.stopShell();
}

namespace {
QString withoutWidePlaceholders(QString text) {
  text.remove(QChar(0x200B));
  return text;
}
} // namespace

void TestTerminal::testPtyCursorAddressingIsRelativeToScreen() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalRows = 4;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("1\r\n2\r\n3\r\n4\r\n5\r\n6");
  terminal.onPtyReadyRead("\x1b[1;1HX");

  QCOMPARE(textEdit->toPlainText(), QString("1\n2\nX\n4\n5\n6"));
}

void TestTerminal::testPtyScrollRegionDeleteAndInsertLines() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalRows = 5;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("a\r\nb\r\nc\r\nd\r\ne");

  terminal.onPtyReadyRead("\x1b[2;4r\x1b[2;1H\x1b[M");
  QCOMPARE(textEdit->toPlainText(), QString("a\nc\nd\n\ne"));

  terminal.onPtyReadyRead("\x1b[2;1H\x1b[LB");
  QCOMPARE(textEdit->toPlainText(), QString("a\nB\nc\nd\ne"));
}

void TestTerminal::testPtyLineFeedAtRegionBottomScrollsRegion() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalRows = 4;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("one\r\ntwo\r\nthree\r\nstatus");
  terminal.onPtyReadyRead("\x1b[1;3r\x1b[3;1H\nfour");

  QCOMPARE(textEdit->toPlainText(), QString("two\nthree\nfour\nstatus"));
}

void TestTerminal::testPtyReverseIndexAtTopScrollsDown() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalRows = 3;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("x\r\ny\r\nz");
  terminal.onPtyReadyRead("\x1b[H\x1bMw");

  QCOMPARE(textEdit->toPlainText(), QString("w\nx\ny"));
}

void TestTerminal::testPtyLineFeedKeepsColumn() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("ab\ncd");

  QCOMPARE(textEdit->toPlainText(), QString("ab\n  cd"));
}

void TestTerminal::testPtyBackspaceMovesWithoutErasing() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("abc\bX\r\nabc\b");

  QCOMPARE(textEdit->toPlainText(), QString("abX\nabc"));
}

void TestTerminal::testPtyTabMovesToNextStopWithoutMovingText() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("tab\there");

  QCOMPARE(textEdit->toPlainText(), QString("tab     here"));
}

void TestTerminal::testPtyUtf8SplitAcrossReadsIsDecoded() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead(QByteArray("caf\xc3", 4));
  terminal.onPtyReadyRead(QByteArray("\xa9 \xe2\x9c", 4));
  terminal.onPtyReadyRead(QByteArray("\x93", 1));

  QCOMPARE(textEdit->toPlainText(), QString::fromUtf8("café ✓"));
}

void TestTerminal::testPtyWideCharactersTakeTwoColumns() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalColumns = 20;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead(QString::fromUtf8("日本語").toUtf8());
  terminal.onPtyReadyRead("\x1b[4D\x1b[1@X");

  QCOMPARE(withoutWidePlaceholders(textEdit->toPlainText()),
           QString::fromUtf8("日X本語"));

  textEdit->selectAll();
  terminal.copySelectionToClipboard();
  QCOMPARE(QApplication::clipboard()->text(), QString::fromUtf8("日X本語"));
}

void TestTerminal::testPtyWideCharacterWrapsBeforeRightMargin() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalColumns = 5;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead(QString::fromUtf8("日本語").toUtf8());

  QCOMPARE(withoutWidePlaceholders(textEdit->toPlainText()),
           QString::fromUtf8("日本\n語"));
}

void TestTerminal::testPtyAlternateScreenRestoresColors() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("\x1b[31mred\x1b[0m\r\n");
  terminal.onPtyReadyRead("\x1b[?1049h\x1b[Hfull screen\x1b[?1049l");

  QCOMPARE(textEdit->toPlainText(), QString("red\n"));
  QTextCursor cursor(textEdit->document());
  cursor.setPosition(1);
  QCOMPARE(cursor.charFormat().foreground().color(),
           ThemeEngine::instance().activeTheme().colors.ansiRed);
}

void TestTerminal::testPtyClearScreenKeepsScrollback() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalRows = 3;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("old\r\n$ ");
  terminal.onPtyReadyRead("\x1b[H\x1b[2J$ ");

  QCOMPARE(textEdit->toPlainText(), QString("old\n$ \n$ \n\n"));
  QCOMPARE(terminal.m_screenTop, 2);
  QCOMPARE(terminal.m_ansiRow, 2);

  terminal.onPtyReadyRead("\x1b[H\x1b[2J\x1b[3J$ ");
  QCOMPARE(textEdit->toPlainText(), QString("$ \n\n"));
}

void TestTerminal::testPtyPrivateModeSgrIsIgnored() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("\x1b[>4;2m\x1b[?4mplain");

  QTextCursor cursor(textEdit->document());
  cursor.setPosition(1);
  QVERIFY(!cursor.charFormat().fontUnderline());
  QCOMPARE(cursor.charFormat().foreground().color(),
           QColor(terminal.m_textColor));
}

void TestTerminal::testPtyTrueColorSgr() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("\x1b[38;2;10;20;30mA\x1b[38:2::40:50:60mB");

  QTextCursor cursor(textEdit->document());
  cursor.setPosition(1);
  QCOMPARE(cursor.charFormat().foreground().color(), QColor(10, 20, 30));
  cursor.setPosition(2);
  QCOMPARE(cursor.charFormat().foreground().color(), QColor(40, 50, 60));
}

void TestTerminal::testScrollbackTrimKeepsScreenAddressable() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalRows = 3;
  terminal.setScrollbackLines(5);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  QString lines;
  for (int i = 1; i <= 20; ++i) {
    lines += QString::number(i) + (i < 20 ? "\r\n" : "");
  }
  terminal.onPtyReadyRead(lines.toUtf8());
  QCOMPARE(textEdit->toPlainText(), QString("16\n17\n18\n19\n20"));

  terminal.onPtyReadyRead("\x1b[1;1HX");
  QCOMPARE(textEdit->toPlainText(), QString("16\n17\nX8\n19\n20"));
}

void TestTerminal::testPtyKeySequences() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  auto sequence = [&](int key, Qt::KeyboardModifiers mods,
                      const QString &text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, mods, text);
    return terminal.ptyKeySequence(&event);
  };

  QCOMPARE(sequence(Qt::Key_Up, Qt::NoModifier), QByteArray("\x1b[A"));
  QCOMPARE(sequence(Qt::Key_Left, Qt::ControlModifier),
           QByteArray("\x1b[1;5D"));
  QCOMPARE(sequence(Qt::Key_R, Qt::ControlModifier), QByteArray("\x12"));
  QCOMPARE(sequence(Qt::Key_B, Qt::AltModifier, "b"), QByteArray("\033b"));
  QCOMPARE(sequence(Qt::Key_Backtab, Qt::ShiftModifier), QByteArray("\x1b[Z"));
  QCOMPARE(sequence(Qt::Key_Delete, Qt::NoModifier), QByteArray("\x1b[3~"));
  QCOMPARE(sequence(Qt::Key_F5, Qt::NoModifier), QByteArray("\x1b[15~"));

  terminal.onPtyReadyRead("\x1b[?1h");
  QCOMPARE(sequence(Qt::Key_Up, Qt::NoModifier), QByteArray("\x1bOA"));
  terminal.onPtyReadyRead("\x1b[?1l");
  QCOMPARE(sequence(Qt::Key_Up, Qt::NoModifier), QByteArray("\x1b[A"));
}

void TestTerminal::testPtyPasteData() {
  QCOMPARE(Terminal::ptyPasteData("echo a\necho b\r\n", false),
           QByteArray("echo a\recho b\r"));
  QCOMPARE(Terminal::ptyPasteData("ls\n", true),
           QByteArray("\x1b[200~ls\r\x1b[201~"));

  QCOMPARE(Terminal::ptyPasteData("a\x1b[201~rm -rf x\n", true),
           QByteArray("\x1b[200~arm -rf x\r\x1b[201~"));
}

void TestTerminal::testPtyControlKeysOverrideApplicationShortcuts() {
  Terminal terminal;
  QTRY_VERIFY_WITH_TIMEOUT(terminal.isRunning(), 3000);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  QKeyEvent ctrlU(QEvent::ShortcutOverride, Qt::Key_U, Qt::ControlModifier);
  QApplication::sendEvent(textEdit, &ctrlU);
  QVERIFY(ctrlU.isAccepted());

  QKeyEvent toggle(QEvent::ShortcutOverride, Qt::Key_QuoteLeft,
                   Qt::ControlModifier);
  toggle.ignore();
  QApplication::sendEvent(textEdit, &toggle);
  QVERIFY(!toggle.isAccepted());

  terminal.stopShell();
}

void TestTerminal::testPtyInterruptStopsForegroundJob() {
  Terminal terminal;
  QTRY_VERIFY_WITH_TIMEOUT(terminal.isRunning(), 3000);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QTRY_VERIFY_WITH_TIMEOUT(!textEdit->toPlainText().trimmed().isEmpty(), 3000);

  terminal.sendText("sleep 30", true);
  QTRY_VERIFY_WITH_TIMEOUT(!terminal.isShellInForeground(), 3000);

  QVERIFY(terminal.interruptActiveProcess());
  QTRY_VERIFY_WITH_TIMEOUT(terminal.isShellInForeground(), 3000);

  terminal.stopShell();
}

void TestTerminal::testNewTerminalStartsInDirectoryWithoutCd() {
  const QString directory = QDir::tempPath();
  Terminal terminal(nullptr, directory);
  QTRY_VERIFY_WITH_TIMEOUT(terminal.isRunning(), 3000);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QTRY_VERIFY_WITH_TIMEOUT(!textEdit->toPlainText().trimmed().isEmpty(), 3000);

#ifdef Q_OS_LINUX
  QTRY_COMPARE_WITH_TIMEOUT(
      QDir(terminal.shellCurrentDirectory()).canonicalPath(),
      QDir(directory).canonicalPath(), 3000);
#endif
  terminal.setWorkingDirectory(directory);
  QTest::qWait(300);
  QVERIFY(!textEdit->toPlainText().contains("cd -- "));

  terminal.stopShell();
}

void TestTerminal::testZoomOverridesApplicationEditorFont() {

  const QString previousStyleSheet = qApp->styleSheet();
  qApp->setStyleSheet("QPlainTextEdit { font-size: 30pt; }");

  Terminal terminal;
  QTRY_VERIFY_WITH_TIMEOUT(terminal.isRunning(), 3000);
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  terminal.resize(800, 300);
  terminal.show();
  textEdit->setFocus();
  QTest::qWait(100);
  QCOMPARE(textEdit->font().pointSize(), terminal.currentFontSize());
  const int columnsBefore = terminal.m_terminalColumns;

  QTest::keyClick(textEdit, Qt::Key_Equal, Qt::ControlModifier);
  QCOMPARE(terminal.currentFontSize(), 12);
  QCOMPARE(textEdit->font().pointSize(), 12);

  QVERIFY(terminal.m_terminalColumns < columnsBefore);

  terminal.stopShell();
  qApp->setStyleSheet(previousStyleSheet);
}

void TestTerminal::testShrinkingAfterClearKeepsPromptInPlace() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);
  terminal.m_terminalRows = 10;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");

  terminal.onPtyReadyRead("old\r\n$ \x1b[H\x1b[2J$ ");
  const int promptRow = terminal.m_ansiRow;

  terminal.m_terminalRows = 4;
  terminal.handleScreenResize();

  QCOMPARE(terminal.m_ansiRow, promptRow);
  QCOMPARE(textEdit->document()->blockCount(), promptRow + 1);
  terminal.onPtyReadyRead("ls");
  QCOMPARE(textEdit->document()->lastBlock().text(), QString("$ ls"));
}

QTEST_MAIN(TestTerminal)
#include "test_terminal.moc"
