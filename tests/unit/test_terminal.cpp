#include "ui/panels/shellprofile.h"
#include "ui/panels/terminal.h"
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

  void testShellStartedSignal();

  void testMultipleStopCalls();
  void testRestartAfterStop();

  void testShellProfiles();
  void testScrollbackLines();
  void testLinkDetection();
  void testSendText();
  void testCloseButtonConfiguration();
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

void TestTerminal::testCloseButtonConfiguration() {
  Terminal terminal;
  terminal.stopShell();
  QTest::qWait(200);

  QToolButton *closeButton = terminal.findChild<QToolButton *>("closeButton");
  QVERIFY(closeButton != nullptr);
  QCOMPARE(closeButton->text(), QStringLiteral("\u00D7"));
  QVERIFY(!closeButton->isCheckable());
  QVERIFY(closeButton->autoRaise());
}

QTEST_MAIN(TestTerminal)
#include "test_terminal.moc"
