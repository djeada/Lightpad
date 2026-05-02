#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QProgressBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeWidget>
#include <QtTest>

#include "test_templates/autotestrunner.h"
#include "test_templates/testconfiguration.h"
#include "test_templates/testdiscovery.h"
#include "test_templates/testrunmanager.h"
#include "ui/panels/testpanel.h"

Q_DECLARE_METATYPE(TestResult)
Q_DECLARE_METATYPE(DiscoveredTest)

class TestTestPanel : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testInitialCounts();
  void testInitialTreeEmpty();
  void testConfigurationComboRefreshesAfterTemplateLoad();
  void testCtestDiscoveryUsesBuildDirectory();

  void testDiscoveryPopulatesTree();
  void testDiscoveryWithSuites();
  void testDiscoveryEmptyList();
  void testDiscoveryReplacesPrevious();

  void testOnRunStartedClearsState();
  void testOnRunFinishedUpdatesCounts();
  void testOnTestFinishedAddsItem();
  void testCountsChangedSignal();

  void testFilterAllShowsEverything();
  void testFilterFailedHidesPassed();

  void testAutoRunToggle();
  void testAutoRunModeCombo();

  void testSetDiscoveryAdapter();

  void testSearchEditExists();
  void testRunDetailsCollapsedByDefault();
  void testClickingTestExpandsDetailsPane();
  void testProgressBarHiddenInitially();
  void testProgressBarVisibleDuringRun();
  void testEmptyStateLabelVisibleInitially();
  void testEmptyStateHiddenAfterDiscovery();
  void testSearchFilterByName();
  void testDirectoryRunShowsRunDetailsAndUsesWorkspaceRoot();
  void testDirectoryRunScopesGoogleTestsToSelectedSubdir();

  void testDoubleClickEmitsLocationClicked();
  void testDoubleClickPreservesDiscoveryFilePath();
  void testDoubleClickPytestPreservesFilePath();
  void testDoubleClickCtestIdMatchesFallback();
  void testDoubleClickCtestUsesWorkspaceSourceFallback();
  void testDoubleClickOnDetailChildNavigatesToParent();
  void testDoubleClickNameFallback();

  void testSuiteItemCollapsedByDefault();
  void testTestItemFoldedByDefault();
  void testTestItemHasDetailChildrenOnFailure();
  void testDetailChildrenClearedOnRerun();

  void testStatusLabelShowsRunningDuringRun();
  void testStatusLabelShowsLatestStdoutLine();
  void testStatusLabelTruncatesLongStdoutLine();
  void testStatusLabelIgnoresStderrLines();
  void testStatusLabelClearedOnNewRun();

private:
  QTemporaryDir m_tempDir;
  QTreeWidget *findTree(TestPanel &panel);
  QLabel *findStatusLabel(TestPanel &panel);
  QComboBox *findFilterCombo(TestPanel &panel);
  QComboBox *findConfigCombo(TestPanel &panel);
  TestRunManager *findRunManager(TestPanel &panel);
  QLineEdit *findSearchEdit(TestPanel &panel);
  QProgressBar *findProgressBar(TestPanel &panel);
  QLabel *findEmptyStateLabel(TestPanel &panel);
  QTextEdit *findDetailPane(TestPanel &panel);
  QToolButton *findDetailToggle(TestPanel &panel);
};

void TestTestPanel::initTestCase() {
  qRegisterMetaType<TestResult>("TestResult");
  qRegisterMetaType<DiscoveredTest>("DiscoveredTest");
  qRegisterMetaType<QList<DiscoveredTest>>("QList<DiscoveredTest>");
  QVERIFY(m_tempDir.isValid());
}

void TestTestPanel::cleanupTestCase() {}

QTreeWidget *TestTestPanel::findTree(TestPanel &panel) {
  return panel.findChild<QTreeWidget *>("testTree");
}

QLabel *TestTestPanel::findStatusLabel(TestPanel &panel) {
  return panel.findChild<QLabel *>("statusLabel");
}

QComboBox *TestTestPanel::findFilterCombo(TestPanel &panel) {
  return panel.findChild<QComboBox *>("filterCombo");
}

QComboBox *TestTestPanel::findConfigCombo(TestPanel &panel) {
  return panel.findChild<QComboBox *>("configCombo");
}

TestRunManager *TestTestPanel::findRunManager(TestPanel &panel) {
  return panel.findChild<TestRunManager *>();
}

QLineEdit *TestTestPanel::findSearchEdit(TestPanel &panel) {
  return panel.findChild<QLineEdit *>("testSearchEdit");
}

QProgressBar *TestTestPanel::findProgressBar(TestPanel &panel) {
  return panel.findChild<QProgressBar *>("testProgressBar");
}

QLabel *TestTestPanel::findEmptyStateLabel(TestPanel &panel) {
  return panel.findChild<QLabel *>("testEmptyState");
}

QTextEdit *TestTestPanel::findDetailPane(TestPanel &panel) {
  return panel.findChild<QTextEdit *>("testDetailPane");
}

QToolButton *TestTestPanel::findDetailToggle(TestPanel &panel) {
  return panel.findChild<QToolButton *>("testDetailToggle");
}

void TestTestPanel::testInitialCounts() {
  TestPanel panel;
  QCOMPARE(panel.passedCount(), 0);
  QCOMPARE(panel.failedCount(), 0);
  QCOMPARE(panel.skippedCount(), 0);
  QCOMPARE(panel.erroredCount(), 0);
}

void TestTestPanel::testInitialTreeEmpty() {
  TestPanel panel;
  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 0);
}

void TestTestPanel::testConfigurationComboRefreshesAfterTemplateLoad() {
  TestPanel panel;
  QComboBox *configCombo = findConfigCombo(panel);
  QVERIFY(configCombo != nullptr);
  QCOMPARE(configCombo->count(), 0);

  QVERIFY(TestConfigurationManager::instance().loadTemplates());
  QVERIFY(configCombo->count() > 0);
}

void TestTestPanel::testCtestDiscoveryUsesBuildDirectory() {
  TestPanel panel;
  QVERIFY(TestConfigurationManager::instance().loadTemplates());

  QComboBox *configCombo = findConfigCombo(panel);
  QVERIFY(configCombo != nullptr);
  const int gtestIndex = configCombo->findData("gtest_cmake");
  QVERIFY(gtestIndex >= 0);
  configCombo->setCurrentIndex(gtestIndex);

  class CapturingDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString lastPath;

    QString adapterId() const override { return "ctest"; }
    void discover(const QString &path) override {
      lastPath = path;
      emit discoveryFinished({});
    }
    void cancel() override {}
  };

  CapturingDiscoveryAdapter adapter;
  panel.setDiscoveryAdapter(&adapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  panel.discoverTests();

  QCOMPARE(adapter.lastPath, QDir(m_tempDir.path()).absoluteFilePath("build"));
}

void TestTestPanel::testDiscoveryPopulatesTree() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest t1;
      t1.name = "test_alpha";
      t1.id = "test_alpha";
      t1.suite = "";
      tests.append(t1);

      DiscoveredTest t2;
      t2.name = "test_beta";
      t2.id = "test_beta";
      t2.suite = "";
      tests.append(t2);

      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  mockAdapter.discover(m_tempDir.path());

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 2);
  QCOMPARE(tree->topLevelItem(0)->text(0), "test_alpha");
  QCOMPARE(tree->topLevelItem(1)->text(0), "test_beta");
}

void TestTestPanel::testDiscoveryWithSuites() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest t1;
      t1.name = "test_one";
      t1.id = "suite::test_one";
      t1.suite = "MySuite";
      t1.filePath = "/path/test.py";
      t1.line = 10;
      tests.append(t1);

      DiscoveredTest t2;
      t2.name = "test_two";
      t2.id = "suite::test_two";
      t2.suite = "MySuite";
      tests.append(t2);

      DiscoveredTest t3;
      t3.name = "standalone";
      t3.id = "standalone";
      t3.suite = "";
      tests.append(t3);

      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  mockAdapter.discover(m_tempDir.path());

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);

  QCOMPARE(tree->topLevelItemCount(), 2);

  QTreeWidgetItem *suiteItem = nullptr;
  QTreeWidgetItem *standaloneItem = nullptr;
  for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    if (tree->topLevelItem(i)->text(0) == "MySuite")
      suiteItem = tree->topLevelItem(i);
    else if (tree->topLevelItem(i)->text(0) == "standalone")
      standaloneItem = tree->topLevelItem(i);
  }

  QVERIFY(suiteItem != nullptr);
  QVERIFY(standaloneItem != nullptr);
  QCOMPARE(suiteItem->childCount(), 2);
  QCOMPARE(suiteItem->child(0)->text(0), "test_one");
  QCOMPARE(suiteItem->child(1)->text(0), "test_two");
}

void TestTestPanel::testDiscoveryEmptyList() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      emit discoveryFinished(QList<DiscoveredTest>());
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  mockAdapter.discover(m_tempDir.path());

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 0);
}

void TestTestPanel::testDiscoveryReplacesPrevious() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QList<DiscoveredTest> batch;
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override { emit discoveryFinished(batch); }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  DiscoveredTest t1;
  t1.name = "old_test";
  t1.id = "old_test";
  mockAdapter.batch = {t1};
  mockAdapter.discover(m_tempDir.path());

  QTreeWidget *tree = findTree(panel);
  QCOMPARE(tree->topLevelItemCount(), 1);

  DiscoveredTest t2;
  t2.name = "new_test_a";
  t2.id = "new_test_a";
  DiscoveredTest t3;
  t3.name = "new_test_b";
  t3.id = "new_test_b";
  mockAdapter.batch = {t2, t3};
  mockAdapter.discover(m_tempDir.path());

  QCOMPARE(tree->topLevelItemCount(), 2);
  QCOMPARE(tree->topLevelItem(0)->text(0), "new_test_a");
}

void TestTestPanel::testOnRunStartedClearsState() {
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  QCOMPARE(panel.passedCount(), 0);
  QCOMPARE(panel.failedCount(), 0);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 0);
}

void TestTestPanel::testOnRunFinishedUpdatesCounts() {
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runFinished(5, 2, 1, 0);

  QCOMPARE(panel.passedCount(), 5);
  QCOMPARE(panel.failedCount(), 2);
  QCOMPARE(panel.skippedCount(), 1);
  QCOMPARE(panel.erroredCount(), 0);
}

void TestTestPanel::testOnTestFinishedAddsItem() {
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test1";
  result.name = "test_example";
  result.suite = "";
  result.status = TestStatus::Passed;
  result.durationMs = 42;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 1);
  QCOMPARE(tree->topLevelItem(0)->text(0), "test_example");
  QCOMPARE(tree->topLevelItem(0)->text(1), "Passed");
}

void TestTestPanel::testCountsChangedSignal() {
  TestPanel panel;
  QSignalSpy countsSpy(&panel, &TestPanel::countsChanged);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runFinished(3, 1, 0, 1);

  QCOMPARE(countsSpy.count(), 1);
  QList<QVariant> args = countsSpy.at(0);
  QCOMPARE(args.at(0).toInt(), 3);
  QCOMPARE(args.at(1).toInt(), 1);
  QCOMPARE(args.at(2).toInt(), 0);
  QCOMPARE(args.at(3).toInt(), 1);
}

void TestTestPanel::testFilterAllShowsEverything() {
  TestPanel panel;
  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult r1;
  r1.id = "pass1";
  r1.name = "pass_test";
  r1.suite = "";
  r1.status = TestStatus::Passed;
  emit runMgr->testFinished(r1);

  TestResult r2;
  r2.id = "fail1";
  r2.name = "fail_test";
  r2.suite = "";
  r2.status = TestStatus::Failed;
  emit runMgr->testFinished(r2);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);

  QComboBox *filter = findFilterCombo(panel);
  QVERIFY(filter != nullptr);
  filter->setCurrentIndex(0);

  QCOMPARE(tree->topLevelItem(0)->isHidden(), false);
  QCOMPARE(tree->topLevelItem(1)->isHidden(), false);
}

void TestTestPanel::testFilterFailedHidesPassed() {
  TestPanel panel;
  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult r1;
  r1.id = "pass1";
  r1.name = "pass_test";
  r1.suite = "";
  r1.status = TestStatus::Passed;
  emit runMgr->testFinished(r1);

  TestResult r2;
  r2.id = "fail1";
  r2.name = "fail_test";
  r2.suite = "";
  r2.status = TestStatus::Failed;
  emit runMgr->testFinished(r2);

  QComboBox *filter = findFilterCombo(panel);
  QVERIFY(filter != nullptr);
  filter->setCurrentIndex(1);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);

  QCOMPARE(tree->topLevelItem(0)->isHidden(), true);
  QCOMPARE(tree->topLevelItem(1)->isHidden(), false);
}

void TestTestPanel::testAutoRunToggle() {
  TestPanel panel;
  AutoTestRunner *autoRunner = panel.autoTestRunner();
  QVERIFY(autoRunner != nullptr);

  QCOMPARE(autoRunner->isEnabled(), false);

  QAction *autoRunAction = panel.findChild<QAction *>("autoRunAction");
  QVERIFY(autoRunAction != nullptr);

  autoRunAction->setChecked(true);
  QCOMPARE(autoRunner->isEnabled(), true);

  autoRunAction->setChecked(false);
  QCOMPARE(autoRunner->isEnabled(), false);
}

void TestTestPanel::testAutoRunModeCombo() {
  TestPanel panel;
  AutoTestRunner *autoRunner = panel.autoTestRunner();
  QVERIFY(autoRunner != nullptr);

  QComboBox *modeCombo = panel.findChild<QComboBox *>("autoRunModeCombo");
  QVERIFY(modeCombo != nullptr);

  QAction *autoRunAction = panel.findChild<QAction *>("autoRunAction");
  QVERIFY(autoRunAction != nullptr);
  autoRunAction->setChecked(true);

  modeCombo->setCurrentIndex(1);
  QCOMPARE(autoRunner->mode(), AutoRunMode::CurrentFileOnSave);

  modeCombo->setCurrentIndex(2);
  QCOMPARE(autoRunner->mode(), AutoRunMode::LastSelection);
}

void TestTestPanel::testSetDiscoveryAdapter() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    bool discoverCalled = false;
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      discoverCalled = true;
      emit discoveryFinished(QList<DiscoveredTest>());
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  panel.discoverTests();
  QVERIFY(mockAdapter.discoverCalled);
}

void TestTestPanel::testSearchEditExists() {
  TestPanel panel;
  QLineEdit *searchEdit = findSearchEdit(panel);
  QVERIFY(searchEdit != nullptr);
  QVERIFY(searchEdit->placeholderText().contains("Filter"));
}

void TestTestPanel::testRunDetailsCollapsedByDefault() {
  TestPanel panel;
  panel.show();

  QToolButton *detailToggle = findDetailToggle(panel);
  QTextEdit *detailPane = findDetailPane(panel);
  QVERIFY(detailToggle != nullptr);
  QVERIFY(detailPane != nullptr);
  QCOMPARE(detailToggle->isChecked(), false);
  QCOMPARE(detailPane->isHidden(), true);
}

void TestTestPanel::testClickingTestExpandsDetailsPane() {
  TestPanel panel;
  panel.show();

  QToolButton *detailToggle = findDetailToggle(panel);
  QTextEdit *detailPane = findDetailPane(panel);
  QVERIFY(detailToggle != nullptr);
  QVERIFY(detailPane != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);
  emit runMgr->runStarted();

  TestResult result;
  result.id = "details";
  result.name = "test_details";
  result.status = TestStatus::Failed;
  result.message = "Expected 1 but got 2";
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  emit tree->itemClicked(item, 0);

  QCOMPARE(detailToggle->isChecked(), true);
  QCOMPARE(detailPane->isHidden(), false);
  QVERIFY(detailPane->toPlainText().contains("Expected 1 but got 2"));
}

void TestTestPanel::testProgressBarHiddenInitially() {
  TestPanel panel;
  panel.show();
  QProgressBar *progressBar = findProgressBar(panel);
  QVERIFY(progressBar != nullptr);
  QCOMPARE(progressBar->isVisible(), false);
}

void TestTestPanel::testProgressBarVisibleDuringRun() {
  TestPanel panel;
  panel.show();
  QProgressBar *progressBar = findProgressBar(panel);
  QVERIFY(progressBar != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();
  QCOMPARE(progressBar->isVisible(), true);

  emit runMgr->runFinished(1, 0, 0, 0);
  QCOMPARE(progressBar->isVisible(), false);
}

void TestTestPanel::testEmptyStateLabelVisibleInitially() {
  TestPanel panel;
  panel.show();
  QLabel *emptyState = findEmptyStateLabel(panel);
  QVERIFY(emptyState != nullptr);
  QCOMPARE(emptyState->isVisible(), true);
}

void TestTestPanel::testEmptyStateHiddenAfterDiscovery() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest t1;
      t1.name = "test_one";
      t1.id = "test_one";
      t1.suite = "";
      tests.append(t1);
      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  mockAdapter.discover(m_tempDir.path());

  QLabel *emptyState = findEmptyStateLabel(panel);
  QVERIFY(emptyState != nullptr);
  QCOMPARE(emptyState->isVisible(), false);
}

void TestTestPanel::testSearchFilterByName() {
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult r1;
  r1.id = "alpha";
  r1.name = "test_alpha";
  r1.suite = "";
  r1.status = TestStatus::Passed;
  emit runMgr->testFinished(r1);

  TestResult r2;
  r2.id = "beta";
  r2.name = "test_beta";
  r2.suite = "";
  r2.status = TestStatus::Passed;
  emit runMgr->testFinished(r2);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 2);

  QLineEdit *searchEdit = findSearchEdit(panel);
  QVERIFY(searchEdit != nullptr);
  searchEdit->setText("alpha");

  QCOMPARE(tree->topLevelItem(0)->isHidden(), false);
  QCOMPARE(tree->topLevelItem(1)->isHidden(), true);

  searchEdit->setText("");
  QCOMPARE(tree->topLevelItem(0)->isHidden(), false);
  QCOMPARE(tree->topLevelItem(1)->isHidden(), false);
}

void TestTestPanel::testDirectoryRunShowsRunDetailsAndUsesWorkspaceRoot() {
  TestPanel panel;
  panel.setWorkspaceFolder(m_tempDir.path());

  const QString testsDir = QDir(m_tempDir.path()).absoluteFilePath("tests");
  QDir().mkpath(testsDir);
  QFile sourceFile(QDir(testsDir).absoluteFilePath("sample_test.cpp"));
  QVERIFY(sourceFile.open(QIODevice::WriteOnly | QIODevice::Text));
  sourceFile.write("int sample_test() { return 0; }\n");
  sourceFile.close();

  TestConfiguration config;
  config.id = "panel_echo_directory_run";
  config.name = "Panel Echo Directory Run";
  config.language = "C++";
  config.extensions = {"cpp"};
  config.command = "/bin/echo";
  config.args = {"workspace=${workspaceFolder}", "target=${file}"};
  config.workingDirectory = "${workspaceFolder}";
  config.outputFormat = "tap";

  auto &manager = TestConfigurationManager::instance();
  manager.addConfiguration(config);

  QComboBox *configCombo = findConfigCombo(panel);
  QVERIFY(configCombo != nullptr);
  const int configIndex = configCombo->findData(config.id);
  QVERIFY(configIndex >= 0);
  configCombo->setCurrentIndex(configIndex);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);
  QSignalSpy processSpy(runMgr, &TestRunManager::processStarted);

  panel.runTestsForPath(testsDir);

  QVERIFY(processSpy.count() > 0 || processSpy.wait(1000));
  QCOMPARE(processSpy.count(), 1);
  QCOMPARE(processSpy.at(0).at(0).toString(), QString("/bin/echo"));
  QCOMPARE(processSpy.at(0).at(2).toString(), m_tempDir.path());

  QTextEdit *detailPane = findDetailPane(panel);
  QVERIFY(detailPane != nullptr);
  QTRY_VERIFY(detailPane->toPlainText().contains(
      "Configuration: Panel Echo Directory Run"));
  const QString detailText = detailPane->toPlainText();
  QVERIFY(detailText.contains("Scope: Directory"));
  QVERIFY(detailText.contains("Target: " + testsDir));
  QVERIFY(detailText.contains("Workspace: " + m_tempDir.path()));
  QVERIFY(detailText.contains("Working directory: " + m_tempDir.path()));
  QVERIFY(detailText.contains("Command: /bin/echo"));
  QVERIFY(detailText.contains("workspace=" + m_tempDir.path()));
  QVERIFY(detailText.contains("target=" + testsDir));
  QVERIFY(detailText.contains("Result: No tests were reported for this run."));

  QLabel *emptyState = findEmptyStateLabel(panel);
  QVERIFY(emptyState != nullptr);
  QVERIFY(emptyState->text().contains("No tests were reported"));

  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);
  QTRY_VERIFY(statusLabel->text().contains("No tests were reported"));

  manager.removeConfiguration(config.id);
}

void TestTestPanel::testDirectoryRunScopesGoogleTestsToSelectedSubdir() {
  TestPanel panel;
  panel.setWorkspaceFolder(m_tempDir.path());

  const QString dbDir = QDir(m_tempDir.path()).absoluteFilePath("tests/db");
  const QString renderDir =
      QDir(m_tempDir.path()).absoluteFilePath("tests/render");
  QDir().mkpath(dbDir);
  QDir().mkpath(renderDir);

  QFile dbFile(QDir(dbDir).absoluteFilePath("frame_profile_test.cpp"));
  QVERIFY(dbFile.open(QIODevice::WriteOnly | QIODevice::Text));
  dbFile.write("#include <gtest/gtest.h>\n"
               "TEST(FrameProfileTest, ResetZeroes) {}\n"
               "TEST(FrameProfileTest, DisabledProfileIgnoresWrites) {}\n");
  dbFile.close();

  QFile renderFile(QDir(renderDir).absoluteFilePath("render_profile_test.cpp"));
  QVERIFY(renderFile.open(QIODevice::WriteOnly | QIODevice::Text));
  renderFile.write("#include <gtest/gtest.h>\n"
                   "TEST(RenderProfileTest, DrawsOverlay) {}\n");
  renderFile.close();

  const QString outputPath = m_tempDir.filePath("scoped-pattern.txt");
  const QString scriptPath = m_tempDir.filePath("capture_pattern.sh");
  QFile scriptFile(scriptPath);
  QVERIFY(scriptFile.open(QIODevice::WriteOnly | QIODevice::Text));
  scriptFile.write("#!/bin/sh\nprintf '%s' \"$1\" > \"$2\"\n");
  scriptFile.close();
  scriptFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                            QFileDevice::ExeOwner);

  TestConfiguration config;
  config.id = "panel_scoped_gtest";
  config.name = "Panel Scoped GTest";
  config.language = "C++";
  config.extensions = {"cpp"};
  config.command = "/bin/sh";
  config.args = {"-c", "exit 99"};
  config.runFailed.args = {scriptPath, "${testName}", outputPath};
  config.outputFormat = "ctest";

  auto &manager = TestConfigurationManager::instance();
  manager.addConfiguration(config);

  QComboBox *configCombo = findConfigCombo(panel);
  QVERIFY(configCombo != nullptr);
  const int configIndex = configCombo->findData(config.id);
  QVERIFY(configIndex >= 0);
  configCombo->setCurrentIndex(configIndex);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);
  QSignalSpy processSpy(runMgr, &TestRunManager::processStarted);

  panel.runTestsForPath(dbDir);

  QVERIFY(processSpy.count() > 0 || processSpy.wait(1000));
  QCOMPARE(processSpy.count(), 1);

  QTRY_VERIFY(QFileInfo::exists(outputPath));
  QFile outputFile(outputPath);
  QVERIFY(outputFile.open(QIODevice::ReadOnly));
  const QString pattern = QString::fromUtf8(outputFile.readAll());
  QVERIFY(pattern.contains("FrameProfileTest\\.ResetZeroes"));
  QVERIFY(pattern.contains("FrameProfileTest\\.DisabledProfileIgnoresWrites"));
  QVERIFY(!pattern.contains("RenderProfileTest\\.DrawsOverlay"));

  manager.removeConfiguration(config.id);
}

void TestTestPanel::testDoubleClickEmitsLocationClicked() {
  TestPanel panel;
  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  const QString sourcePath =
      QDir(m_tempDir.path()).absoluteFilePath("src/test_example.cpp");
  QDir().mkpath(QFileInfo(sourcePath).absolutePath());
  QFile sourceFile(sourcePath);
  QVERIFY(sourceFile.open(QIODevice::WriteOnly | QIODevice::Text));
  sourceFile.write("int main() { return 0; }\n");
  sourceFile.close();

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test1";
  result.name = "test_example";
  result.suite = "";
  result.status = TestStatus::Passed;
  result.filePath = sourcePath;
  result.line = 42;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  emit tree->itemDoubleClicked(item, 0);

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), sourcePath);
  QCOMPARE(spy.at(0).at(1).toInt(), 42);
}

void TestTestPanel::testDoubleClickPreservesDiscoveryFilePath() {
  TestPanel panel;
  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString sourcePath;

    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest dt;
      dt.name = "test_example";
      dt.id = "test1";
      dt.suite = "";
      dt.filePath = sourcePath;
      dt.line = 10;
      tests.append(dt);
      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  mockAdapter.sourcePath =
      QDir(m_tempDir.path()).absoluteFilePath("tests/discovered_test.cpp");
  QDir().mkpath(QFileInfo(mockAdapter.sourcePath).absolutePath());
  QFile discoveredFile(mockAdapter.sourcePath);
  QVERIFY(discoveredFile.open(QIODevice::WriteOnly | QIODevice::Text));
  discoveredFile.write("void discovered_test() {}\n");
  discoveredFile.close();
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  mockAdapter.discover(m_tempDir.path());

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test1";
  result.name = "test_example";
  result.suite = "";
  result.status = TestStatus::Passed;

  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  emit tree->itemDoubleClicked(item, 0);

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), mockAdapter.sourcePath);
  QCOMPARE(spy.at(0).at(1).toInt(), 10);
}

void TestTestPanel::testDoubleClickPytestPreservesFilePath() {

  TestPanel panel;
  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  class MockPytestDiscovery : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock_pytest"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest dt;
      dt.name = "test_add";
      dt.suite = "TestArithmetic";
      dt.filePath = "tests/test_math.py";
      dt.id = "tests/test_math.py::TestArithmetic::test_add";
      tests.append(dt);
      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockPytestDiscovery mockAdapter;
  const QString pytestPath =
      QDir(m_tempDir.path()).absoluteFilePath("tests/test_math.py");
  QDir().mkpath(QFileInfo(pytestPath).absolutePath());
  QFile pytestFile(pytestPath);
  QVERIFY(pytestFile.open(QIODevice::WriteOnly | QIODevice::Text));
  pytestFile.write("def test_add():\n    assert 1 + 1 == 2\n");
  pytestFile.close();
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  mockAdapter.discover(m_tempDir.path());

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);
  emit runMgr->runStarted();

  TestResult result;
  result.id = "tests/test_math.py::TestArithmetic::test_add";
  result.name = "TestArithmetic::test_add";
  result.suite = "tests/test_math.py";
  result.filePath = "tests/test_math.py";
  result.status = TestStatus::Passed;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);

  QVERIFY(tree->topLevelItemCount() > 0);
  QTreeWidgetItem *suiteItem = tree->topLevelItem(0);
  QVERIFY(suiteItem != nullptr);
  QVERIFY(suiteItem->childCount() > 0);
  QTreeWidgetItem *item = suiteItem->child(0);
  QVERIFY(item != nullptr);

  emit tree->itemDoubleClicked(item, 0);

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), pytestPath);
}

void TestTestPanel::testDoubleClickCtestIdMatchesFallback() {

  TestPanel panel;
  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  class MockCtestDiscovery : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock_ctest"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest dt;
      dt.name = "LoggerTests";
      dt.id = "1";
      dt.filePath = "/project/build";
      tests.append(dt);
      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockCtestDiscovery mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  mockAdapter.discover(m_tempDir.path());

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);
  emit runMgr->runStarted();

  TestResult result;
  result.id = "1";
  result.name = "LoggerTests";
  result.status = TestStatus::Passed;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  emit tree->itemDoubleClicked(item, 0);

  QCOMPARE(spy.count(), 0);
}

void TestTestPanel::testDoubleClickCtestUsesWorkspaceSourceFallback() {
  TestPanel panel;
  QVERIFY(TestConfigurationManager::instance().loadTemplates());

  QComboBox *configCombo = findConfigCombo(panel);
  QVERIFY(configCombo != nullptr);
  const int gtestIndex = configCombo->findData("gtest_cmake");
  QVERIFY(gtestIndex >= 0);
  configCombo->setCurrentIndex(gtestIndex);

  const QString sourcePath =
      QDir(m_tempDir.path()).absoluteFilePath("tests/test_serialization.cpp");
  QDir().mkpath(QFileInfo(sourcePath).absolutePath());
  QFile sourceFile(sourcePath);
  QVERIFY(sourceFile.open(QIODevice::WriteOnly | QIODevice::Text));
  sourceFile.write("#include <gtest/gtest.h>\n"
                   "\n"
                   "TEST(SerializationTest, EntitySerializationBasic) {\n"
                   "  SUCCEED();\n"
                   "}\n");
  sourceFile.close();

  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  class MockCtestDiscovery : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock_ctest"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest dt;
      dt.name = "SerializationTest.EntitySerializationBasic";
      dt.id = "1";
      dt.filePath = "/project/build/tests";
      tests.append(dt);
      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockCtestDiscovery mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  mockAdapter.discover(m_tempDir.path());

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);
  emit runMgr->runStarted();

  TestResult result;
  result.id = "1";
  result.name = "SerializationTest.EntitySerializationBasic";
  result.status = TestStatus::Passed;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  emit tree->itemDoubleClicked(item, 0);

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), sourcePath);
  QCOMPARE(spy.at(0).at(1).toInt(), 3);
}

void TestTestPanel::testDoubleClickOnDetailChildNavigatesToParent() {

  TestPanel panel;
  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  const QString sourcePath =
      QDir(m_tempDir.path()).absoluteFilePath("src/test_fail.cpp");
  QDir().mkpath(QFileInfo(sourcePath).absolutePath());
  QFile sourceFile(sourcePath);
  QVERIFY(sourceFile.open(QIODevice::WriteOnly | QIODevice::Text));
  sourceFile.write("void test_fail() {}\n");
  sourceFile.close();

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test_fail";
  result.name = "test_fail";
  result.suite = "";
  result.status = TestStatus::Failed;
  result.filePath = sourcePath;
  result.line = 10;
  result.message = "Expected 1 but got 2";
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *testItem = tree->topLevelItem(0);
  QVERIFY(testItem != nullptr);

  QVERIFY(testItem->childCount() > 0);
  QTreeWidgetItem *detailChild = testItem->child(0);
  QVERIFY(detailChild != nullptr);

  emit tree->itemDoubleClicked(detailChild, 0);

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), sourcePath);
  QCOMPARE(spy.at(0).at(1).toInt(), 10);
}

void TestTestPanel::testDoubleClickNameFallback() {

  TestPanel panel;
  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  class MockNameDiscovery : public ITestDiscoveryAdapter {
  public:
    QString sourcePath;

    QString adapterId() const override { return "mock_name"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest dt;
      dt.name = "test_example";
      dt.id = "discovered.test_example";
      dt.suite = "";
      dt.filePath = sourcePath;
      dt.line = 5;
      tests.append(dt);
      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockNameDiscovery mockAdapter;
  mockAdapter.sourcePath =
      QDir(m_tempDir.path()).absoluteFilePath("src/discovered_test.cpp");
  QDir().mkpath(QFileInfo(mockAdapter.sourcePath).absolutePath());
  QFile nameFallbackFile(mockAdapter.sourcePath);
  QVERIFY(nameFallbackFile.open(QIODevice::WriteOnly | QIODevice::Text));
  nameFallbackFile.write("void test_example() {}\n");
  nameFallbackFile.close();
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  mockAdapter.discover(m_tempDir.path());

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);
  emit runMgr->runStarted();

  TestResult result;
  result.id = "runner.test_example";
  result.name = "test_example";
  result.suite = "";
  result.status = TestStatus::Passed;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  emit tree->itemDoubleClicked(item, 0);

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), mockAdapter.sourcePath);
  QCOMPARE(spy.at(0).at(1).toInt(), 5);
}

void TestTestPanel::testSuiteItemCollapsedByDefault() {

  TestPanel panel;
  panel.show();

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "MySuite::test_one";
  result.name = "test_one";
  result.suite = "MySuite";
  result.status = TestStatus::Passed;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 1);

  QTreeWidgetItem *suiteItem = tree->topLevelItem(0);
  QVERIFY(suiteItem != nullptr);
  QCOMPARE(suiteItem->text(0), QString("MySuite"));

  QCOMPARE(suiteItem->isExpanded(), true);
}

void TestTestPanel::testTestItemFoldedByDefault() {

  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test_fail";
  result.name = "test_fail";
  result.suite = "";
  result.status = TestStatus::Failed;
  result.message = "Expected 1 but got 2";
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  QVERIFY(item->childCount() > 0);
  QCOMPARE(item->isExpanded(), false);
}

void TestTestPanel::testTestItemHasDetailChildrenOnFailure() {

  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test_fail";
  result.name = "test_fail";
  result.suite = "";
  result.status = TestStatus::Failed;
  result.message = "Expected 1 but got 2";
  result.stackTrace = "at test_fail() line 10";
  result.stdoutOutput = "some output line";
  result.stderrOutput = "some error line";
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  QCOMPARE(item->childCount(), 4);

  QStringList childTexts;
  for (int i = 0; i < item->childCount(); ++i)
    childTexts << item->child(i)->text(0);

  QVERIFY(childTexts.at(0).contains("Message:"));
  QVERIFY(childTexts.at(1).contains("Stack Trace:"));
  QVERIFY(childTexts.at(2).contains("stdout:"));
  QVERIFY(childTexts.at(3).contains("stderr:"));

  emit runMgr->runStarted();

  QCOMPARE(tree->topLevelItemCount(), 0);

  TestResult passedResult;
  passedResult.id = "test_pass";
  passedResult.name = "test_pass";
  passedResult.suite = "";
  passedResult.status = TestStatus::Passed;
  emit runMgr->testFinished(passedResult);

  QCOMPARE(tree->topLevelItemCount(), 1);
  QTreeWidgetItem *passedItem = tree->topLevelItem(0);
  QVERIFY(passedItem != nullptr);
  QCOMPARE(passedItem->childCount(), 0);
}

void TestTestPanel::testDetailChildrenClearedOnRerun() {

  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult firstRun;
  firstRun.id = "test1";
  firstRun.name = "test_example";
  firstRun.suite = "";
  firstRun.status = TestStatus::Failed;
  firstRun.message = "first failure message";
  emit runMgr->testFinished(firstRun);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);
  QCOMPARE(item->childCount(), 1);
  QVERIFY(item->child(0)->text(0).contains("first failure message"));

  emit runMgr->runStarted();

  QCOMPARE(tree->topLevelItemCount(), 0);

  TestResult secondRun;
  secondRun.id = "test1";
  secondRun.name = "test_example";
  secondRun.suite = "";
  secondRun.status = TestStatus::Failed;
  secondRun.message = "second failure message";
  emit runMgr->testFinished(secondRun);

  item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  QCOMPARE(item->childCount(), 1);
  QVERIFY(item->child(0)->text(0).contains("second failure message"));
}

void TestTestPanel::testStatusLabelShowsRunningDuringRun() {
  TestPanel panel;
  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  QVERIFY(statusLabel->text().contains("Running tests"));
}

void TestTestPanel::testStatusLabelShowsLatestStdoutLine() {
  TestPanel panel;
  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();
  emit runMgr->outputLine("Building project...", false);

  QVERIFY(statusLabel->text().contains("Building project..."));
  QVERIFY(statusLabel->text().contains("Running tests"));
}

void TestTestPanel::testStatusLabelTruncatesLongStdoutLine() {
  TestPanel panel;
  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  const QString longLine = QString("x").repeated(200);
  emit runMgr->outputLine(longLine, false);

  const QString labelText = statusLabel->text();

  QVERIFY(!labelText.contains(longLine));

  QVERIFY(labelText.contains(QChar(0x2026)));
}

void TestTestPanel::testStatusLabelIgnoresStderrLines() {
  TestPanel panel;
  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  emit runMgr->outputLine("some error text", true);

  const QString labelText = statusLabel->text();
  QVERIFY(!labelText.contains("some error text"));
}

void TestTestPanel::testStatusLabelClearedOnNewRun() {
  TestPanel panel;
  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();
  emit runMgr->outputLine("first run output", false);
  QVERIFY(statusLabel->text().contains("first run output"));

  emit runMgr->runStarted();
  QVERIFY(!statusLabel->text().contains("first run output"));
  QVERIFY(statusLabel->text().contains("Running tests"));
}

QTEST_MAIN(TestTestPanel)
#include "test_testpanel.moc"
