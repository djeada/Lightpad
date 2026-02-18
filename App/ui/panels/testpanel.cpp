#include "testpanel.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QHeaderView>
#include <QMenu>
#include <QSettings>
#include <QStyle>

TestPanel::TestPanel(QWidget *parent) : QWidget(parent) {
  setObjectName("TestPanel");
  m_runManager = new TestRunManager(this);
  m_ctestDiscovery = new CTestDiscoveryAdapter(this);
  setupUI();

  connect(m_runManager, &TestRunManager::testStarted, this,
          &TestPanel::onTestStarted);
  connect(m_runManager, &TestRunManager::testFinished, this,
          &TestPanel::onTestFinished);
  connect(m_runManager, &TestRunManager::runStarted, this,
          &TestPanel::onRunStarted);
  connect(m_runManager, &TestRunManager::runFinished, this,
          &TestPanel::onRunFinished);
  connect(m_ctestDiscovery, &CTestDiscoveryAdapter::discoveryFinished, this,
          &TestPanel::onDiscoveryFinished);
  connect(m_ctestDiscovery, &CTestDiscoveryAdapter::discoveryError, this,
          &TestPanel::onDiscoveryError);

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
  m_toolbar->setMovable(false);
  m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  m_runAllAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_MediaPlay), tr("Run All"), this,
      &TestPanel::runAll);
  m_runAllAction->setToolTip(tr("Run All Tests"));

  m_runFailedAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_BrowserReload), tr("Run Failed"), this,
      &TestPanel::runFailed);
  m_runFailedAction->setToolTip(tr("Re-run Failed Tests"));

  m_stopAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_MediaStop), tr("Stop"), this,
      &TestPanel::stopTests);
  m_stopAction->setToolTip(tr("Stop"));
  m_stopAction->setEnabled(false);

  m_discoverAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_FileDialogContentsView), tr("Discover"),
      this, &TestPanel::discoverTests);
  m_discoverAction->setToolTip(tr("Discover Tests"));

  m_clearAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_DialogResetButton), tr("Clear"), this,
      [this]() {
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
  m_filterCombo->setMinimumWidth(110);
  m_toolbar->addWidget(m_filterCombo);

  m_toolbar->addSeparator();

  m_configCombo = new QComboBox(this);
  m_configCombo->setMinimumWidth(210);
  connect(m_configCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &TestPanel::onConfigChanged);
  m_toolbar->addWidget(m_configCombo);

  layout->addWidget(m_toolbar);

  // Splitter with tree and detail pane
  m_splitter = new QSplitter(Qt::Vertical, this);

  m_tree = new QTreeWidget(this);
  m_tree->setHeaderLabels({tr("Test"), tr("Status"), tr("Duration")});
  m_tree->setRootIsDecorated(true);
  m_tree->setAlternatingRowColors(false);
  m_tree->setUniformRowHeights(true);
  m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tree->setAllColumnsShowFocus(true);
  m_tree->header()->setStretchLastSection(false);
  m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
          &TestPanel::onItemDoubleClicked);
  connect(m_tree, &QTreeWidget::itemClicked, this,
          &TestPanel::onItemClicked);
  connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
          &TestPanel::onContextMenu);

  m_detailPane = new QTextEdit(this);
  m_detailPane->setReadOnly(true);
  m_detailPane->setMinimumHeight(120);
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

  const QColor panelBg =
      theme.surfaceColor.isValid() ? theme.surfaceColor : QColor("#111827");
  const QColor textColor =
      theme.foregroundColor.isValid() ? theme.foregroundColor : QColor("#e5e7eb");
  const QColor borderColor =
      theme.borderColor.isValid() ? theme.borderColor : QColor("#334155");
  const QColor treeBg =
      theme.backgroundColor.isValid() ? theme.backgroundColor : QColor("#0b1220");
  const QColor hoverBg = panelBg.lighter(115);
  const QColor selectedBg =
      theme.accentSoftColor.isValid() ? theme.accentSoftColor
      : (theme.highlightColor.isValid() ? theme.highlightColor
                                        : QColor("#1f4b7a"));
  const QColor selectedText = QColor("#ffffff");
  const QColor mutedText = textColor.darker(130);

  setStyleSheet(QString(
                    "QWidget#TestPanel {"
                    "  background-color: %1;"
                    "  color: %2;"
                    "}"
                    "QToolBar {"
                    "  background-color: %1;"
                    "  border: 0;"
                    "  border-bottom: 1px solid %3;"
                    "  padding: 4px 6px;"
                    "  spacing: 4px;"
                    "}"
                    "QToolButton {"
                    "  color: %2;"
                    "  border: 1px solid transparent;"
                    "  border-radius: 4px;"
                    "  padding: 3px 8px;"
                    "}"
                    "QToolButton:hover {"
                    "  background-color: %4;"
                    "  border-color: %3;"
                    "}"
                    "QToolButton:disabled {"
                    "  color: %7;"
                    "}"
                    "QComboBox {"
                    "  min-height: 24px;"
                    "  border: 1px solid %3;"
                    "  border-radius: 4px;"
                    "  padding: 2px 8px;"
                    "  background-color: %5;"
                    "  color: %2;"
                    "}"
                    "QTreeWidget {"
                    "  background-color: %5;"
                    "  border: 1px solid %3;"
                    "  outline: none;"
                    "  padding: 2px;"
                    "}"
                    "QTreeWidget::item {"
                    "  height: 26px;"
                    "}"
                    "QTreeWidget::item:hover {"
                    "  background-color: %4;"
                    "}"
                    "QTreeWidget::item:selected {"
                    "  background-color: %6;"
                    "  color: %8;"
                    "}"
                    "QHeaderView::section {"
                    "  background-color: %1;"
                    "  color: %7;"
                    "  border: 0;"
                    "  border-bottom: 1px solid %3;"
                    "  padding: 6px 8px;"
                    "  font-weight: 600;"
                    "}"
                    "QTextEdit {"
                    "  background-color: %5;"
                    "  color: %2;"
                    "  border: 1px solid %3;"
                    "  padding: 6px;"
                    "}"
                    "QLabel {"
                    "  color: %2;"
                    "}")
                    .arg(panelBg.name(), textColor.name(), borderColor.name(),
                         hoverBg.name(), treeBg.name(), selectedBg.name(),
                         mutedText.name(), selectedText.name()));

  m_statusLabel->setStyleSheet(
      QString("QLabel { border-top: 1px solid %1; padding: 4px 8px; }")
          .arg(borderColor.name()));
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
  TestConfiguration config = currentConfiguration();
  if (!config.isValid())
    return;
  m_runManager->runFailed(config, m_workspaceFolder);
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
    item->setText(1, tr("Passed"));
    break;
  case TestStatus::Failed:
    item->setText(1, tr("Failed"));
    break;
  case TestStatus::Skipped:
    item->setText(1, tr("Skipped"));
    break;
  case TestStatus::Errored:
    item->setText(1, tr("Error"));
    break;
  default:
    item->setText(1, "");
    break;
  }

  if (result.durationMs >= 0)
    item->setText(2, QString::number(result.durationMs) + " ms");

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

  // Check if this is a suite item (has children)
  bool isSuite = (item->childCount() > 0);

  if (isSuite) {
    menu.addAction(tr("Run Suite"), [this, testName]() {
      TestConfiguration config = currentConfiguration();
      if (!config.isValid())
        return;
      m_runManager->runSuite(config, m_workspaceFolder, testName);
    });
  } else {
    QAction *runAction =
        menu.addAction(tr("Run This Test"), [this, testName, filePath]() {
          TestConfiguration config = currentConfiguration();
          if (!config.isValid())
            return;
          m_runManager->runSingleTest(config, m_workspaceFolder, testName,
                                      filePath);
        });
    Q_UNUSED(runAction)
  }

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
  QString text = tr("Passed: %1    Failed: %2    Skipped: %3    Errors: %4    "
                    "Total: %5")
                     .arg(m_passedCount)
                     .arg(m_failedCount)
                     .arg(m_skippedCount)
                     .arg(m_erroredCount)
                     .arg(total);
  m_statusLabel->setText(text);
}

void TestPanel::updateTreeItemIcon(QTreeWidgetItem *item, TestStatus status) {
  QColor color;
  QStyle::StandardPixmap statusIcon = QStyle::SP_FileIcon;
  switch (status) {
  case TestStatus::Passed:
    color = QColor("#3fb950");
    statusIcon = QStyle::SP_DialogApplyButton;
    break;
  case TestStatus::Failed:
    color = QColor("#f85149");
    statusIcon = QStyle::SP_MessageBoxCritical;
    break;
  case TestStatus::Skipped:
    color = QColor("#d29922");
    statusIcon = QStyle::SP_DialogCancelButton;
    break;
  case TestStatus::Errored:
    color = QColor("#f0883e");
    statusIcon = QStyle::SP_MessageBoxWarning;
    break;
  case TestStatus::Running:
    color = QColor("#58a6ff");
    statusIcon = QStyle::SP_BrowserReload;
    break;
  case TestStatus::Queued:
    color = QColor("#8b949e");
    statusIcon = QStyle::SP_ArrowRight;
    break;
  }
  item->setIcon(0, style()->standardIcon(statusIcon));
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

void TestPanel::discoverTests() {
  if (m_workspaceFolder.isEmpty())
    return;

  m_statusLabel->setText(tr("Discovering tests..."));
  m_discoverAction->setEnabled(false);

  // Try CTest discovery using the build directory
  QString buildDir = QDir(m_workspaceFolder).filePath("build");
  if (!QDir(buildDir).exists())
    buildDir = m_workspaceFolder;

  m_ctestDiscovery->discover(buildDir);
}

void TestPanel::onDiscoveryFinished(const QList<DiscoveredTest> &tests) {
  m_discoverAction->setEnabled(true);
  populateTreeFromDiscovery(tests);
  m_statusLabel->setText(
      tr("Discovered %1 tests").arg(tests.size()));
}

void TestPanel::onDiscoveryError(const QString &message) {
  m_discoverAction->setEnabled(true);
  m_statusLabel->setText(tr("Discovery error: %1").arg(message));
}

void TestPanel::populateTreeFromDiscovery(
    const QList<DiscoveredTest> &tests) {
  m_tree->clear();
  m_suiteItems.clear();
  m_testItems.clear();

  for (const DiscoveredTest &test : tests) {
    QTreeWidgetItem *suiteItem = findOrCreateSuiteItem(test.suite);

    auto *item = new QTreeWidgetItem();
    item->setText(0, test.name);
    item->setData(0, Qt::UserRole, test.id);
    if (!test.filePath.isEmpty())
      item->setData(0, Qt::UserRole + 1, test.filePath);
    if (test.line >= 0)
      item->setData(0, Qt::UserRole + 2, test.line);
    updateTreeItemIcon(item, TestStatus::Queued);
    item->setText(1, tr("Not Run"));

    if (suiteItem)
      suiteItem->addChild(item);
    else
      m_tree->addTopLevelItem(item);
    m_testItems[test.id] = item;
  }
}

void TestPanel::saveState() const {
  QSettings settings;
  settings.beginGroup("TestPanel");
  settings.setValue("lastConfiguration", m_configCombo->currentText());
  settings.setValue("lastFilter", m_filterCombo->currentIndex());
  settings.endGroup();
}

void TestPanel::restoreState() {
  QSettings settings;
  settings.beginGroup("TestPanel");
  QString lastConfig = settings.value("lastConfiguration").toString();
  int lastFilter = settings.value("lastFilter", 0).toInt();
  settings.endGroup();

  if (!lastConfig.isEmpty()) {
    int idx = m_configCombo->findText(lastConfig);
    if (idx >= 0)
      m_configCombo->setCurrentIndex(idx);
  }
  if (lastFilter >= 0 && lastFilter < m_filterCombo->count())
    m_filterCombo->setCurrentIndex(lastFilter);
}
