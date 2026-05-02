#define private public
#include "ui/panels/shellprofile.h"
#include "ui/panels/terminal.h"
#undef private
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSignalSpy>
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

  void testShellStartedSignal();

  void testMultipleStopCalls();
  void testRestartAfterStop();

  void testShellProfiles();
  void testScrollbackLines();
  void testLinkDetection();
  void testSendText();
  void testInterruptActiveRunProcess();
  void testExternalRunCrashDoesNotBreakTerminal();
  void testNoEmbeddedCloseButton();
  void testCwdLabelExists();
  void testCwdLabelUpdatesOnDirectoryChange();
  void testContextMenuExists();
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
  void testPtyMouseClickDoesNotMoveInputCursor();
  void testRunProcessAcceptsInteractiveInput();
  void testRunInputIndicatorVisibility();
  void testLooksLikeInputPromptPatterns();
  void testIndicatorNotShownForNonPromptOutput();
  void testIndicatorNotShownOnProcessStart();
  void testIndicatorHiddenAfterNonPromptOutput();
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
  terminal.appendOutput("\x1b[D"
                        "b");

  QCOMPARE(textEdit->toPlainText(), QString("$ abc"));
}

void TestTerminal::testPtyMouseClickDoesNotMoveInputCursor() {
  Terminal terminal;
  QPlainTextEdit *textEdit = terminal.findChild<QPlainTextEdit *>("textEdit");
  QVERIFY(textEdit != nullptr);

  QTRY_VERIFY_WITH_TIMEOUT(terminal.isRunning(), 3000);
  terminal.clear();
  terminal.appendOutput("old output\n$ ");
  textEdit->moveCursor(QTextCursor::End);

  terminal.show();
  textEdit->setFocus();
  QTest::qWait(50);
  QTest::mouseClick(textEdit->viewport(), Qt::LeftButton, Qt::NoModifier,
                    QPoint(4, 4));

  QTextCursor endCursor(textEdit->document());
  endCursor.movePosition(QTextCursor::End);
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
  // No trailing newline — always a prompt
  QVERIFY(Terminal::looksLikeInputPrompt("Enter your name: "));
  QVERIFY(Terminal::looksLikeInputPrompt("Password:"));
  QVERIFY(Terminal::looksLikeInputPrompt(">>> "));
  QVERIFY(Terminal::looksLikeInputPrompt("Continue? "));

  // Common prompt suffix on last line (output ends with newline): the
  // last non-empty line is kept as-is (no trimming) so trailing spaces are
  // preserved when matching the suffix patterns.
  QVERIFY(Terminal::looksLikeInputPrompt("some output\nEnter value: \n"));

  // Yes/no confirmation patterns
  QVERIFY(Terminal::looksLikeInputPrompt("Are you sure? [y/n]\n"));
  QVERIFY(Terminal::looksLikeInputPrompt("Overwrite? [Y/n]\n"));
  QVERIFY(Terminal::looksLikeInputPrompt("Continue (yes/no)\n"));

  // Shell-style prompts (with trailing space before newline)
  QVERIFY(Terminal::looksLikeInputPrompt("user@host:~$ \n"));
  QVERIFY(Terminal::looksLikeInputPrompt("root@host:~# \n"));

  // Normal output with trailing newline — NOT a prompt
  QVERIFY(!Terminal::looksLikeInputPrompt("Hello, World!\n"));
  QVERIFY(!Terminal::looksLikeInputPrompt("Processing complete.\n"));
  QVERIFY(!Terminal::looksLikeInputPrompt("file1.txt\nfile2.txt\nfile3.txt\n"));

  // Empty / whitespace-only — NOT a prompt
  QVERIFY(!Terminal::looksLikeInputPrompt(""));
  QVERIFY(!Terminal::looksLikeInputPrompt("\n"));
  QVERIFY(!Terminal::looksLikeInputPrompt("\n\n\n"));
}

void TestTerminal::testIndicatorNotShownForNonPromptOutput() {
  // Run a process that produces only normal output (ends with newline) and
  // verify that the input indicator is never shown.
  Terminal terminal;
  QSignalSpy finishedSpy(&terminal, &Terminal::processFinished);
  QVERIFY(finishedSpy.isValid());

  terminal.executeCommand("sh",
                          QStringList() << "-c"
                                        << "printf 'line1\\nline2\\n'",
                          QDir::tempPath());

  QTRY_VERIFY_WITH_TIMEOUT(!terminal.hasActiveRunProcess(), 5000);

  // The indicator must stay hidden: no input prompt was produced.
  QVERIFY(terminal.m_runInputIndicator != nullptr);
  QVERIFY(terminal.m_runInputIndicator->isHidden());

  terminal.stopShell();
}

void TestTerminal::testIndicatorNotShownOnProcessStart() {
  // The indicator must be hidden immediately after a process starts (before any
  // output is produced).  Previously a false positive was triggered via the
  // QProcess::started signal.
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  // Ensure indicator is hidden before executing the command.
  QVERIFY(terminal.m_runInputIndicator != nullptr);
  QVERIFY(terminal.m_runInputIndicator->isHidden());

  // Start a process that sleeps briefly so we can sample the indicator state
  // before it exits.
  terminal.executeCommand("sh", QStringList() << "-c"
                                              << "sleep 0.1 && printf 'done\\n'",
                          QDir::tempPath());

  // Allow the process to start but not yet produce output.
  QTest::qWait(30);

  // The indicator must still be hidden — process start is not a prompt.
  QVERIFY(terminal.m_runInputIndicator->isHidden());

  QTRY_VERIFY_WITH_TIMEOUT(!terminal.hasActiveRunProcess(), 5000);
  terminal.stopShell();
}

void TestTerminal::testIndicatorHiddenAfterNonPromptOutput() {
  // When a process emits non-prompt stdout (ends with newline) the indicator
  // must not become visible.  This verifies the debounce + looksLikeInputPrompt
  // path end-to-end.
  Terminal terminal;
  QSignalSpy finishedSpy(&terminal, &Terminal::processFinished);
  QVERIFY(finishedSpy.isValid());

  terminal.executeCommand(
      "sh",
      QStringList() << "-c"
                    << "printf 'output line 1\\n'; printf 'output line 2\\n'",
      QDir::tempPath());

  QTRY_VERIFY_WITH_TIMEOUT(!terminal.hasActiveRunProcess(), 5000);

  // Debounce window has definitely elapsed — indicator must be hidden.
  QTest::qWait(200);
  QVERIFY(terminal.m_runInputIndicator != nullptr);
  QVERIFY(terminal.m_runInputIndicator->isHidden());

  terminal.stopShell();
}

QTEST_MAIN(TestTerminal)
#include "test_terminal.moc"
