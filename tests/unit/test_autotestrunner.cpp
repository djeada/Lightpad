#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "test_templates/autotestrunner.h"
#include "test_templates/testrunmanager.h"

class TestAutoTestRunner : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testDefaultState();
  void testSetEnabled();
  void testSetMode();
  void testSetDebounceDelay();
  void testSetWorkspaceFolder();
  void testNotifyFileSavedWhenDisabled();
  void testNotifyFileSavedWhenNoConfig();
  void testNotifyFileSavedTriggersDebounce();
  void testDebounceCoalesces();
  void testSaveAndLoadSettings();
  void testSaveSettingsCreatesDir();
  void testLoadMissingSettingsFile();
  void testAutoRunModeAllOnSave();
  void testAutoRunModeCurrentFile();
  void testAutoRunModeLastSelection();
  void testSkipWhenAlreadyRunning();
  void testMinDebounceDelay();

private:
  QTemporaryDir m_tempDir;
};

void TestAutoTestRunner::initTestCase() { QVERIFY(m_tempDir.isValid()); }

void TestAutoTestRunner::cleanupTestCase() {}

void TestAutoTestRunner::testDefaultState() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);

  QCOMPARE(runner.isEnabled(), false);
  QCOMPARE(runner.mode(), AutoRunMode::Off);
  QCOMPARE(runner.debounceDelay(), 1000);
  QVERIFY(runner.workspaceFolder().isEmpty());
  QVERIFY(runner.lastFilePath().isEmpty());
  QCOMPARE(runner.isRunning(), false);
}

void TestAutoTestRunner::testSetEnabled() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);
  QSignalSpy spy(&runner, &AutoTestRunner::enabledChanged);

  runner.setEnabled(true);
  QCOMPARE(runner.isEnabled(), true);
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toBool(), true);

  runner.setEnabled(true);
  QCOMPARE(spy.count(), 1);

  runner.setEnabled(false);
  QCOMPARE(runner.isEnabled(), false);
  QCOMPARE(spy.count(), 2);
}

void TestAutoTestRunner::testSetMode() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);
  QSignalSpy spy(&runner, &AutoTestRunner::modeChanged);

  runner.setMode(AutoRunMode::AllOnSave);
  QCOMPARE(runner.mode(), AutoRunMode::AllOnSave);
  QCOMPARE(spy.count(), 1);

  runner.setMode(AutoRunMode::AllOnSave);
  QCOMPARE(spy.count(), 1);

  runner.setMode(AutoRunMode::CurrentFileOnSave);
  QCOMPARE(runner.mode(), AutoRunMode::CurrentFileOnSave);
  QCOMPARE(spy.count(), 2);
}

void TestAutoTestRunner::testSetDebounceDelay() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);

  runner.setDebounceDelay(2000);
  QCOMPARE(runner.debounceDelay(), 2000);

  runner.setDebounceDelay(500);
  QCOMPARE(runner.debounceDelay(), 500);
}

void TestAutoTestRunner::testSetWorkspaceFolder() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);

  runner.setWorkspaceFolder("/home/test/project");
  QCOMPARE(runner.workspaceFolder(), "/home/test/project");
}

void TestAutoTestRunner::testNotifyFileSavedWhenDisabled() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);
  QSignalSpy spy(&runner, &AutoTestRunner::autoRunTriggered);

  TestConfiguration config;
  config.name = "pytest";
  config.command = "pytest";
  runner.setConfiguration(config);
  runner.setMode(AutoRunMode::AllOnSave);

  runner.notifyFileSaved("/some/file.py");
  QTest::qWait(200);
  QCOMPARE(spy.count(), 0);
}

void TestAutoTestRunner::testNotifyFileSavedWhenNoConfig() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);
  QSignalSpy spy(&runner, &AutoTestRunner::autoRunTriggered);

  runner.setEnabled(true);
  runner.setMode(AutoRunMode::AllOnSave);

  runner.notifyFileSaved("/some/file.py");
  QTest::qWait(200);
  QCOMPARE(spy.count(), 0);
}

void TestAutoTestRunner::testNotifyFileSavedTriggersDebounce() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);
  QSignalSpy spy(&runner, &AutoTestRunner::autoRunTriggered);

  TestConfiguration config;
  config.name = "pytest";
  config.command = "nonexistent-command-for-test";
  runner.setConfiguration(config);
  runner.setWorkspaceFolder(m_tempDir.path());
  runner.setEnabled(true);
  runner.setMode(AutoRunMode::AllOnSave);
  runner.setDebounceDelay(100);

  runner.notifyFileSaved("/some/file.py");

  QCOMPARE(spy.count(), 0);

  QTest::qWait(200);
  QCOMPARE(spy.count(), 1);
}

void TestAutoTestRunner::testDebounceCoalesces() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);
  QSignalSpy spy(&runner, &AutoTestRunner::autoRunTriggered);

  TestConfiguration config;
  config.name = "pytest";
  config.command = "nonexistent-command-for-test";
  runner.setConfiguration(config);
  runner.setWorkspaceFolder(m_tempDir.path());
  runner.setEnabled(true);
  runner.setMode(AutoRunMode::AllOnSave);
  runner.setDebounceDelay(200);

  runner.notifyFileSaved("/some/file1.py");
  QTest::qWait(50);
  runner.notifyFileSaved("/some/file2.py");
  QTest::qWait(50);
  runner.notifyFileSaved("/some/file3.py");

  QTest::qWait(350);
  QCOMPARE(spy.count(), 1);
}

void TestAutoTestRunner::testSaveAndLoadSettings() {
  TestRunManager runManager;

  {
    AutoTestRunner runner(&runManager);
    runner.setEnabled(true);
    runner.setMode(AutoRunMode::CurrentFileOnSave);
    runner.setDebounceDelay(2000);
    runner.saveSettings(m_tempDir.path());
  }

  {
    AutoTestRunner runner(&runManager);
    runner.loadSettings(m_tempDir.path());

    QCOMPARE(runner.isEnabled(), true);
    QCOMPARE(runner.mode(), AutoRunMode::CurrentFileOnSave);
    QCOMPARE(runner.debounceDelay(), 2000);
  }
}

void TestAutoTestRunner::testSaveSettingsCreatesDir() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QDir dir(tempDir.path());
  QVERIFY(!dir.exists(".lightpad"));

  runner.setEnabled(true);
  runner.setMode(AutoRunMode::AllOnSave);
  runner.saveSettings(tempDir.path());

  QVERIFY(dir.exists(".lightpad"));
  QVERIFY(QFile::exists(dir.filePath(".lightpad/autotest.json")));
}

void TestAutoTestRunner::testLoadMissingSettingsFile() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  runner.setEnabled(true);
  runner.loadSettings(tempDir.path());
  QCOMPARE(runner.isEnabled(), true);
}

void TestAutoTestRunner::testAutoRunModeAllOnSave() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);

  TestConfiguration config;
  config.name = "test";
  config.command = "nonexistent-command-for-test";
  runner.setConfiguration(config);
  runner.setWorkspaceFolder(m_tempDir.path());
  runner.setEnabled(true);
  runner.setMode(AutoRunMode::AllOnSave);
  runner.setDebounceDelay(100);

  QSignalSpy spy(&runner, &AutoTestRunner::autoRunTriggered);
  runner.notifyFileSaved("/some/file.py");
  QTest::qWait(200);
  QCOMPARE(spy.count(), 1);
}

void TestAutoTestRunner::testAutoRunModeCurrentFile() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);

  TestConfiguration config;
  config.name = "test";
  config.command = "nonexistent-command-for-test";
  runner.setConfiguration(config);
  runner.setWorkspaceFolder(m_tempDir.path());
  runner.setEnabled(true);
  runner.setMode(AutoRunMode::CurrentFileOnSave);
  runner.setDebounceDelay(100);

  QSignalSpy spy(&runner, &AutoTestRunner::autoRunTriggered);
  runner.notifyFileSaved("/some/file.py");
  QTest::qWait(200);
  QCOMPARE(spy.count(), 1);
  QCOMPARE(runner.lastFilePath(), "/some/file.py");
}

void TestAutoTestRunner::testAutoRunModeLastSelection() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);

  TestConfiguration config;
  config.name = "test";
  config.command = "nonexistent-command-for-test";
  runner.setConfiguration(config);
  runner.setWorkspaceFolder(m_tempDir.path());
  runner.setEnabled(true);
  runner.setMode(AutoRunMode::LastSelection);
  runner.setLastFilePath("/some/previous.py");
  runner.setDebounceDelay(100);

  QSignalSpy spy(&runner, &AutoTestRunner::autoRunTriggered);
  runner.notifyFileSaved("/some/other.py");
  QTest::qWait(200);
  QCOMPARE(spy.count(), 1);
}

void TestAutoTestRunner::testSkipWhenAlreadyRunning() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);

  TestConfiguration config;
  config.name = "test";
  config.command = "sleep";
  config.args = {"10"};
  runner.setConfiguration(config);
  runner.setWorkspaceFolder(m_tempDir.path());
  runner.setEnabled(true);
  runner.setMode(AutoRunMode::AllOnSave);
  runner.setDebounceDelay(100);

  runManager.runAll(config, m_tempDir.path());
  QVERIFY(runManager.isRunning());

  QSignalSpy spy(&runner, &AutoTestRunner::autoRunTriggered);
  runner.notifyFileSaved("/some/file.py");
  QTest::qWait(200);
  QCOMPARE(spy.count(), 0);

  runManager.stop();
}

void TestAutoTestRunner::testMinDebounceDelay() {
  TestRunManager runManager;
  AutoTestRunner runner(&runManager);

  runner.setDebounceDelay(10);
  QCOMPARE(runner.debounceDelay(), 100);

  runner.setDebounceDelay(-5);
  QCOMPARE(runner.debounceDelay(), 100);
}

QTEST_MAIN(TestAutoTestRunner)
#include "test_autotestrunner.moc"
