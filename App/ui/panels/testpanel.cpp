#include "testpanel.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>

TestPanel::TestPanel(QWidget *parent) : QWidget(parent) {
  m_runManager = new TestRunManager(this);
  setupUI();

  connect(m_runManager, &TestRunManager::testStarted, this,
          &TestPanel::onTestStarted);
  connect(m_runManager, &TestRunManager::testFinished, this,
          &TestPanel::onTestFinished);
  connect(m_runManager, &TestRunManager::runStarted, this,
          &TestPanel::onRunStarted);
  connect(m_runManager, &TestRunManager::runFinished, this,
          &TestPanel::onRunFinished);

  refreshConfigurations();
}

TestPanel::~TestPanel() {}

void TestPanel::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Toolbar
  m_toolbar = new QToolBar(this);
  m_toolbar->setIconSize(QSize(16, 16));

  m_runAllAction = m_toolbar->addAction(QString::fromUtf8("\u25B6"), this,
                                        &TestPanel::runAll);
  m_runAllAction->setToolTip(tr("Run All Tests"));

  m_runFailedAction = m_toolbar->addAction(
      QString::fromUtf8("\u21BB"), this, &TestPanel::runFailed);
  m_runFailedAction->setToolTip(tr("Re-run Failed Tests"));

  m_stopAction = m_toolbar->addAction(QString::fromUtf8("\u25A0"), this,
                                      &TestPanel::stopTests);
  m_stopAction->setToolTip(tr("Stop"));
  m_stopAction->setEnabled(false);

  m_clearAction = m_toolbar->addAction(
      QString::fromUtf8("\u2715"), this, [this]() {
        m_tree->clear();
        m_detailPane->clear();
        m_suiteItems.clear();
        m_testItems.clear();
        m_testResults.clear();
        m_passedCount = m_failedCount = m_skippedCount = m_erroredCount = 0;
        updateStatusLabel();
      });
  m_clearAction->setToolTip(tr("Clear Results"));

  m_toolbar->addSeparator();

  m_filterCombo = new QComboBox(this);
  m_filterCombo->addItem(tr("All"));
  m_filterCombo->addItem(tr("Failed"));
  m_filterCombo->addItem(tr("Passed"));
  m_filterCombo->addItem(tr("Skipped"));
  connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &TestPanel::onFilterChanged);
  m_toolbar->addWidget(m_filterCombo);

  m_toolbar->addSeparator();

  m_configCombo = new QComboBox(this);
  m_configCombo->setMinimumWidth(150);
  connect(m_configCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &TestPanel::onConfigChanged);
  m_toolbar->addWidget(m_configCombo);

  layout->addWidget(m_toolbar);

  // Splitter with tree and detail pane
  m_splitter = new QSplitter(Qt::Vertical, this);

  m_tree = new QTreeWidget(this);
  m_tree->setHeaderLabels({tr("Test"), tr("Status"), tr("Duration")});
  m_tree->setColumnWidth(0, 400);
  m_tree->setColumnWidth(1, 80);
  m_tree->setColumnWidth(2, 80);
  m_tree->setRootIsDecorated(true);
  m_tree->setAlternatingRowColors(true);
  m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
          &TestPanel::onItemDoubleClicked);
  connect(m_tree, &QTreeWidget::itemClicked, this,
          &TestPanel::onItemClicked);
  connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
          &TestPanel::onContextMenu);

  m_detailPane = new QTextEdit(this);
  m_detailPane->setReadOnly(true);
  m_detailPane->setPlaceholderText(
      tr("Select a test to view its output and details"));

  m_splitter->addWidget(m_tree);
  m_splitter->addWidget(m_detailPane);
  m_splitter->setStretchFactor(0, 3);
  m_splitter->setStretchFactor(1, 1);

  layout->addWidget(m_splitter);

  // Status label
  m_statusLabel = new QLabel(this);
  m_statusLabel->setContentsMargins(4, 2, 4, 2);
  layout->addWidget(m_statusLabel);

  updateStatusLabel();
}

void TestPanel::applyTheme(const Theme &theme) {
  m_theme = theme;

  QString bg = theme.surfaceColor.name();
  QString fg = theme.foregroundColor.name();
  QString border = theme.borderColor.name();

  setStyleSheet(
      QString("QWidget { background-color: %1; color: %2; }"
              "QTreeWidget { border: 1px solid %3; }"
              "QTextEdit { border: 1px solid %3; }")
          .arg(bg, fg, border));

  m_detailPane->setStyleSheet(
      QString("QTextEdit { background-color: %1; color: %2; }")
          .arg(theme.backgroundColor.name(), fg));
}

void TestPanel::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
  TestConfigurationManager::instance().setWorkspaceFolder(folder);
  TestConfigurationManager::instance().loadUserConfigurations(folder);
  refreshConfigurations();
}

void TestPanel::runAll() {
  TestConfiguration config = currentConfiguration();
  if (!config.isValid())
    return;
  m_runManager->runAll(config, m_workspaceFolder);
}

void TestPanel::runFailed() {
  // Re-run with same config — the user can filter to only failed
  runAll();
}

void TestPanel::runCurrentFile(const QString &filePath) {
  TestConfiguration config = currentConfiguration();
  if (!config.isValid())
    return;
  m_runManager->runAll(config, m_workspaceFolder, filePath);
}

bool TestPanel::runWithConfigurationId(const QString &configId,
                                       const QString &filePath) {
  const int configIndex = m_configCombo->findData(configId);
  if (configIndex < 0) {
    return false;
  }

  if (m_configCombo->currentIndex() != configIndex) {
    m_configCombo->setCurrentIndex(configIndex);
  }

  if (filePath.isEmpty()) {
    runAll();
  } else {
    runCurrentFile(filePath);
  }
  return true;
}

void TestPanel::stopTests() { m_runManager->stop(); }

void TestPanel::onTestStarted(const TestResult &result) {
  QTreeWidgetItem *suiteItem = findOrCreateSuiteItem(result.suite);

  QTreeWidgetItem *item = findTestItem(result.id);
  if (!item) {
    item = new QTreeWidgetItem();
    if (suiteItem)
      suiteItem->addChild(item);
    else
      m_tree->addTopLevelItem(item);
    m_testItems[result.id] = item;
  }

  item->setText(0, result.name);
  item->setData(0, Qt::UserRole, result.id);
  updateTreeItemIcon(item, TestStatus::Running);
  item->setText(1, tr("Running"));
  item->setText(2, "");

  m_testResults[result.id] = result;
}

void TestPanel::onTestFinished(const TestResult &result) {
  QTreeWidgetItem *suiteItem = findOrCreateSuiteItem(result.suite);

  QTreeWidgetItem *item = findTestItem(result.id);
  if (!item) {
    item = new QTreeWidgetItem();
    if (suiteItem)
      suiteItem->addChild(item);
    else
      m_tree->addTopLevelItem(item);
    m_testItems[result.id] = item;
  }

  item->setText(0, result.name);
  item->setData(0, Qt::UserRole, result.id);
  item->setData(0, Qt::UserRole + 1, result.filePath);
  item->setData(0, Qt::UserRole + 2, result.line);

  updateTreeItemIcon(item, result.status);

  switch (result.status) {
  case TestStatus::Passed:
    item->setText(1, QString::fromUtf8("\u2705 Passed"));
    break;
  case TestStatus::Failed:
    item->setText(1, QString::fromUtf8("\u274C Failed"));
    break;
  case TestStatus::Skipped:
    item->setText(1, QString::fromUtf8("\u23ED Skipped"));
    break;
  case TestStatus::Errored:
    item->setText(1, QString::fromUtf8("\u26A0 Error"));
    break;
  default:
    item->setText(1, "");
    break;
  }

  if (result.durationMs >= 0)
    item->setText(2, QString::number(result.durationMs) + "ms");

  m_testResults[result.id] = result;
  applyFilter();
}

void TestPanel::onRunStarted() {
  m_tree->clear();
  m_detailPane->clear();
  m_suiteItems.clear();
  m_testItems.clear();
  m_testResults.clear();
  m_passedCount = m_failedCount = m_skippedCount = m_erroredCount = 0;

  m_stopAction->setEnabled(true);
  m_runAllAction->setEnabled(false);
  m_runFailedAction->setEnabled(false);
  m_statusLabel->setText(tr("Running tests..."));
}

void TestPanel::onRunFinished(int passed, int failed, int skipped,
                              int errored) {
  m_passedCount = passed;
  m_failedCount = failed;
  m_skippedCount = skipped;
  m_erroredCount = errored;

  m_stopAction->setEnabled(false);
  m_runAllAction->setEnabled(true);
  m_runFailedAction->setEnabled(failed > 0 || errored > 0);

  updateStatusLabel();
  emit countsChanged(passed, failed, skipped, errored);
}

void TestPanel::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item)
    return;

  QString filePath = item->data(0, Qt::UserRole + 1).toString();
  int line = item->data(0, Qt::UserRole + 2).toInt();

  if (!filePath.isEmpty())
    emit locationClicked(filePath, line, 0);
}

void TestPanel::onItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item)
    return;

  QString testId = item->data(0, Qt::UserRole).toString();
  if (m_testResults.contains(testId)) {
    const TestResult &result = m_testResults[testId];
    QString detail;
    if (!result.message.isEmpty())
      detail += tr("Message: ") + result.message + "\n\n";
    if (!result.stackTrace.isEmpty())
      detail += tr("Stack Trace:\n") + result.stackTrace + "\n\n";
    if (!result.stdoutOutput.isEmpty())
      detail += tr("stdout:\n") + result.stdoutOutput + "\n\n";
    if (!result.stderrOutput.isEmpty())
      detail += tr("stderr:\n") + result.stderrOutput + "\n";
    m_detailPane->setPlainText(detail.isEmpty() ? tr("No details available")
                                                : detail);
  }
}

void TestPanel::onFilterChanged(int index) {
  Q_UNUSED(index)
  applyFilter();
}

void TestPanel::onConfigChanged(int index) { Q_UNUSED(index) }

void TestPanel::onContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_tree->itemAt(pos);
  if (!item)
    return;

  QMenu menu(this);

  QString testId = item->data(0, Qt::UserRole).toString();
  QString testName = item->text(0);
  QString filePath = item->data(0, Qt::UserRole + 1).toString();

  QAction *runAction =
      menu.addAction(tr("Run This Test"), [this, testName, filePath]() {
        TestConfiguration config = currentConfiguration();
        if (!config.isValid())
          return;
        m_runManager->runSingleTest(config, m_workspaceFolder, testName,
                                    filePath);
      });
  Q_UNUSED(runAction)

  if (!filePath.isEmpty()) {
    int line = item->data(0, Qt::UserRole + 2).toInt();
    menu.addAction(tr("Go to Source"), [this, filePath, line]() {
      emit locationClicked(filePath, line, 0);
    });
  }

  menu.addAction(tr("Copy Name"), [testName]() {
    QApplication::clipboard()->setText(testName);
  });

  menu.addSeparator();

  if (m_testResults.contains(testId)) {
    menu.addAction(tr("Show Output"), [this, testId]() {
      const TestResult &result = m_testResults[testId];
      QString detail;
      if (!result.message.isEmpty())
        detail += result.message + "\n\n";
      if (!result.stackTrace.isEmpty())
        detail += result.stackTrace + "\n\n";
      if (!result.stdoutOutput.isEmpty())
        detail += result.stdoutOutput;
      m_detailPane->setPlainText(detail);
    });
  }

  menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void TestPanel::updateStatusLabel() {
  int total = m_passedCount + m_failedCount + m_skippedCount + m_erroredCount;
  QString text =
      QString::fromUtf8("\u2705 %1 passed  \u274C %2 failed  "
                        "\u23ED %3 skipped  \u26A0 %4 errored  "
                        "(%5 total)")
          .arg(m_passedCount)
          .arg(m_failedCount)
          .arg(m_skippedCount)
          .arg(m_erroredCount)
          .arg(total);
  m_statusLabel->setText(text);
}

void TestPanel::updateTreeItemIcon(QTreeWidgetItem *item, TestStatus status) {
  QColor color;
  switch (status) {
  case TestStatus::Passed:
    color = QColor("#3fb950");
    break;
  case TestStatus::Failed:
    color = QColor("#f85149");
    break;
  case TestStatus::Skipped:
    color = QColor("#d29922");
    break;
  case TestStatus::Errored:
    color = QColor("#f0883e");
    break;
  case TestStatus::Running:
    color = QColor("#58a6ff");
    break;
  case TestStatus::Queued:
    color = QColor("#8b949e");
    break;
  }
  item->setForeground(1, QBrush(color));
}

QTreeWidgetItem *TestPanel::findOrCreateSuiteItem(const QString &suite) {
  if (suite.isEmpty())
    return nullptr;

  if (m_suiteItems.contains(suite))
    return m_suiteItems[suite];

  auto *item = new QTreeWidgetItem();
  item->setText(0, suite);
  item->setData(0, Qt::UserRole, suite);
  item->setExpanded(true);
  m_tree->addTopLevelItem(item);
  m_suiteItems[suite] = item;
  return item;
}

QTreeWidgetItem *TestPanel::findTestItem(const QString &id) {
  if (m_testItems.contains(id))
    return m_testItems[id];
  return nullptr;
}

void TestPanel::applyFilter() {
  int filterIndex = m_filterCombo->currentIndex();

  for (auto it = m_testItems.begin(); it != m_testItems.end(); ++it) {
    QTreeWidgetItem *item = it.value();
    if (!m_testResults.contains(it.key())) {
      item->setHidden(false);
      continue;
    }

    const TestResult &result = m_testResults[it.key()];
    bool visible = true;

    switch (filterIndex) {
    case 1: // Failed
      visible = (result.status == TestStatus::Failed ||
                 result.status == TestStatus::Errored);
      break;
    case 2: // Passed
      visible = (result.status == TestStatus::Passed);
      break;
    case 3: // Skipped
      visible = (result.status == TestStatus::Skipped);
      break;
    default:
      visible = true;
      break;
    }

    item->setHidden(!visible);
  }

  // Hide suite items that have no visible children
  for (auto it = m_suiteItems.begin(); it != m_suiteItems.end(); ++it) {
    QTreeWidgetItem *suite = it.value();
    bool hasVisible = false;
    for (int i = 0; i < suite->childCount(); ++i) {
      if (!suite->child(i)->isHidden()) {
        hasVisible = true;
        break;
      }
    }
    suite->setHidden(!hasVisible);
  }
}

void TestPanel::refreshConfigurations() {
  m_configCombo->clear();
  auto configs = TestConfigurationManager::instance().allConfigurations();
  for (const TestConfiguration &cfg : configs)
    m_configCombo->addItem(cfg.name, cfg.id);

  QString defaultName =
      TestConfigurationManager::instance().defaultConfigurationName();
  if (!defaultName.isEmpty()) {
    int idx = m_configCombo->findText(defaultName);
    if (idx >= 0)
      m_configCombo->setCurrentIndex(idx);
  }
}

TestConfiguration TestPanel::currentConfiguration() const {
  QString name = m_configCombo->currentText();
  return TestConfigurationManager::instance().configurationByName(name);
}
