#include "testpanel.h"
#include "../uistylehelper.h"

#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QSettings>
#include <QStyle>
#include <QStyledItemDelegate>

namespace {
constexpr int TestIdRole = Qt::UserRole;
constexpr int FilePathRole = Qt::UserRole + 1;
constexpr int LineNumberRole = Qt::UserRole + 2;

int autoRunModeToComboIndex(AutoRunMode mode) {
  switch (mode) {
  case AutoRunMode::AllOnSave:
    return 0;
  case AutoRunMode::CurrentFileOnSave:
    return 1;
  case AutoRunMode::LastSelection:
    return 2;
  case AutoRunMode::Off:
  default:
    return 0;
  }
}

AutoRunMode comboIndexToAutoRunMode(int index) {
  switch (index) {
  case 0:
    return AutoRunMode::AllOnSave;
  case 1:
    return AutoRunMode::CurrentFileOnSave;
  case 2:
    return AutoRunMode::LastSelection;
  default:
    return AutoRunMode::AllOnSave;
  }
}

class TestTreeDelegate : public QStyledItemDelegate {
public:
  explicit TestTreeDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void setTheme(const Theme &theme) {
    m_textColor = theme.foregroundColor;
    m_hoverBackground =
        QColor::fromRgbF(theme.hoverColor.redF(), theme.hoverColor.greenF(),
                         theme.hoverColor.blueF(), 0.2);
    m_selectedBackground = QColor::fromRgbF(
        theme.accentSoftColor.redF(), theme.accentSoftColor.greenF(),
        theme.accentSoftColor.blueF(), 0.18);
    m_selectedBorder = theme.accentColor.lighter(102);
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!painter)
      return;

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const bool isSelected = opt.state.testFlag(QStyle::State_Selected);
    const bool isHovered = opt.state.testFlag(QStyle::State_MouseOver);

    QRect rowRect = opt.rect.adjusted(2, 1, -2, -1);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    if (isSelected || isHovered) {
      const QColor bg = isSelected ? m_selectedBackground : m_hoverBackground;
      painter->setPen(Qt::NoPen);
      painter->setBrush(bg);
      painter->drawRect(rowRect);

      if (isSelected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_selectedBorder);
        painter->drawRect(
            QRect(rowRect.left(), rowRect.top(), 2, rowRect.height()));
      }
    }

    opt.rect = rowRect.adjusted(8, 0, -6, 0);
    opt.state &= ~QStyle::State_HasFocus;
    opt.showDecorationSelected = false;
    opt.palette.setColor(QPalette::Highlight, Qt::transparent);
    opt.palette.setColor(QPalette::HighlightedText, m_textColor);
    opt.palette.setColor(QPalette::Text, m_textColor);

    QStyledItemDelegate::paint(painter, opt, index);
    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), 22));
    return size + QSize(0, 2);
  }

private:
  QColor m_textColor = QColor("#dce4ee");
  QColor m_hoverBackground = QColor("#171e27");
  QColor m_selectedBackground = QColor("#17283d");
  QColor m_selectedBorder = QColor("#5da7ff");
};
} // namespace

TestPanel::TestPanel(QWidget *parent) : QWidget(parent) {
  setObjectName("TestPanel");
  m_runManager = new TestRunManager(this);
  m_autoTestRunner = new AutoTestRunner(m_runManager, this);
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

TestPanel::~TestPanel() {
  if (m_ownsDiscoveryAdapter)
    delete m_discoveryAdapter;
}

void TestPanel::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Header shell wrapping the toolbar for consistent styling
  m_headerShell = new QWidget(this);
  m_headerShell->setObjectName("testHeaderShell");
  auto *headerLayout = new QVBoxLayout(m_headerShell);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(0);

  setupToolbar();
  headerLayout->addWidget(m_toolbar);

  // Search/filter bar
  m_searchEdit = new QLineEdit(m_headerShell);
  m_searchEdit->setObjectName("testSearchEdit");
  m_searchEdit->setPlaceholderText(tr("Filter tests by name..."));
  m_searchEdit->setClearButtonEnabled(true);
  m_searchEdit->setContentsMargins(6, 4, 6, 4);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &TestPanel::onSearchTextChanged);
  headerLayout->addWidget(m_searchEdit);

  layout->addWidget(m_headerShell);

  // Progress bar for running tests
  m_progressBar = new QProgressBar(this);
  m_progressBar->setObjectName("testProgressBar");
  m_progressBar->setMaximumHeight(3);
  m_progressBar->setTextVisible(false);
  m_progressBar->setRange(0, 0);
  m_progressBar->setVisible(false);
  layout->addWidget(m_progressBar);

  m_splitter = new QSplitter(Qt::Vertical, this);

  // Tree widget with custom delegate
  auto *treeContainer = new QWidget(this);
  auto *treeLayout = new QVBoxLayout(treeContainer);
  treeLayout->setContentsMargins(0, 0, 0, 0);
  treeLayout->setSpacing(0);

  m_tree = new QTreeWidget(treeContainer);
  m_tree->setObjectName("testTree");
  m_tree->setHeaderLabels({tr("Test"), tr("Status"), tr("Duration")});
  m_tree->setRootIsDecorated(true);
  m_tree->setAlternatingRowColors(false);
  m_tree->setUniformRowHeights(true);
  m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tree->setAllColumnsShowFocus(true);
  m_tree->setMouseTracking(true);
  m_tree->header()->setStretchLastSection(false);
  m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
  m_tree->setItemDelegate(new TestTreeDelegate(m_tree));
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
          &TestPanel::onItemDoubleClicked);
  connect(m_tree, &QTreeWidget::itemClicked, this, &TestPanel::onItemClicked);
  connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
          &TestPanel::onContextMenu);

  // Empty state overlay label
  m_emptyStateLabel = new QLabel(treeContainer);
  m_emptyStateLabel->setObjectName("testEmptyState");
  m_emptyStateLabel->setAlignment(Qt::AlignCenter);
  m_emptyStateLabel->setText(
      tr("No tests discovered yet.\nUse \"Discover\" to find tests or "
         "\"Run All\" to execute them."));
  m_emptyStateLabel->setWordWrap(true);

  treeLayout->addWidget(m_tree);
  treeLayout->addWidget(m_emptyStateLabel);

  m_detailPane = new QTextEdit(this);
  m_detailPane->setObjectName("testDetailPane");
  m_detailPane->setReadOnly(true);
  m_detailPane->setMinimumHeight(100);
  m_detailPane->setPlaceholderText(
      tr("Select a test to view its output and details"));

  m_splitter->addWidget(treeContainer);
  m_splitter->addWidget(m_detailPane);
  m_splitter->setStretchFactor(0, 3);
  m_splitter->setStretchFactor(1, 1);

  layout->addWidget(m_splitter);

  // Status bar with colored count badges
  m_statusLabel = new QLabel(this);
  m_statusLabel->setObjectName("statusLabel");
  m_statusLabel->setContentsMargins(8, 4, 8, 4);
  layout->addWidget(m_statusLabel);

  updateStatusLabel();
  updateEmptyState();
}

void TestPanel::setupToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setObjectName("testToolbar");
  m_toolbar->setIconSize(QSize(16, 16));
  m_toolbar->setMovable(false);
  m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  m_runAllAction =
      m_toolbar->addAction(style()->standardIcon(QStyle::SP_MediaPlay),
                           tr("Run All"), this, &TestPanel::runAll);
  m_runAllAction->setObjectName("runAllAction");
  m_runAllAction->setToolTip(tr("Run All Tests"));

  m_runFailedAction =
      m_toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload),
                           tr("Run Failed"), this, &TestPanel::runFailed);
  m_runFailedAction->setToolTip(tr("Re-run Failed Tests"));

  m_stopAction =
      m_toolbar->addAction(style()->standardIcon(QStyle::SP_MediaStop),
                           tr("Stop"), this, &TestPanel::stopTests);
  m_stopAction->setToolTip(tr("Stop"));
  m_stopAction->setEnabled(false);

  m_discoverAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_FileDialogContentsView), tr("Discover"),
      this, &TestPanel::discoverTests);
  m_discoverAction->setToolTip(tr("Discover Tests"));

  m_clearAction =
      m_toolbar->addAction(style()->standardIcon(QStyle::SP_DialogResetButton),
                           tr("Clear"), this, [this]() {
                             m_tree->clear();
                             m_detailPane->clear();
                             m_suiteItems.clear();
                             m_testItems.clear();
                             m_testResults.clear();
                             m_passedCount = m_failedCount = m_skippedCount =
                                 m_erroredCount = 0;
                             updateStatusLabel();
                             updateEmptyState();
                           });
  m_clearAction->setToolTip(tr("Clear Results"));

  m_toolbar->addSeparator();

  m_autoRunAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_DialogApplyButton), tr("Auto"));
  m_autoRunAction->setObjectName("autoRunAction");
  m_autoRunAction->setCheckable(true);
  m_autoRunAction->setChecked(false);
  m_autoRunAction->setToolTip(tr("Toggle Auto-run Tests on Save"));
  connect(m_autoRunAction, &QAction::toggled, this,
          &TestPanel::onAutoRunToggled);

  m_autoRunModeCombo = new QComboBox(this);
  m_autoRunModeCombo->setObjectName("autoRunModeCombo");
  m_autoRunModeCombo->addItem(tr("All on Save"));
  m_autoRunModeCombo->addItem(tr("File on Save"));
  m_autoRunModeCombo->addItem(tr("Last Selection"));
  m_autoRunModeCombo->setMinimumWidth(130);
  m_autoRunModeCombo->setEnabled(false);
  m_autoRunModeCombo->setToolTip(tr("Auto-run scope"));
  connect(m_autoRunModeCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &TestPanel::onAutoRunModeChanged);
  m_toolbar->addWidget(m_autoRunModeCombo);

  m_toolbar->addSeparator();

  m_filterCombo = new QComboBox(this);
  m_filterCombo->setObjectName("filterCombo");
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
  m_configCombo->setObjectName("configCombo");
  m_configCombo->setMinimumWidth(210);
  connect(m_configCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &TestPanel::onConfigChanged);
  m_toolbar->addWidget(m_configCombo);
}

void TestPanel::applyTheme(const Theme &theme) {
  m_theme = theme;

  const auto blend = [](const QColor &base, const QColor &overlay,
                        qreal ratio) {
    const qreal clamped = qBound(0.0, ratio, 1.0);
    return QColor::fromRgbF(
        base.redF() * (1.0 - clamped) + overlay.redF() * clamped,
        base.greenF() * (1.0 - clamped) + overlay.greenF() * clamped,
        base.blueF() * (1.0 - clamped) + overlay.blueF() * clamped, 1.0);
  };
  const auto withAlpha = [](QColor color, int alpha) {
    color.setAlpha(alpha);
    return color;
  };

  QColor panelSurface = blend(theme.backgroundColor, QColor("#0f141b"), 0.08);
  QColor toolbarShell = blend(theme.backgroundColor, QColor("#10161d"), 0.14);
  QColor treeBg = theme.backgroundColor;
  QColor textColor = theme.foregroundColor;
  QColor borderColor = theme.borderColor;
  QColor mutedText = blend(theme.foregroundColor, theme.backgroundColor, 0.24);
  QColor subtleText = blend(theme.foregroundColor, theme.backgroundColor, 0.40);
  QColor hoverBg = blend(theme.backgroundColor, theme.foregroundColor, 0.06);
  QColor selectedBg =
      theme.accentSoftColor.isValid() ? theme.accentSoftColor : QColor("#1f4b7a");
  QColor accentColor = theme.accentColor.isValid() ? theme.accentColor : QColor("#58a6ff");
  QColor successColor = theme.successColor.isValid() ? theme.successColor : QColor("#3fb950");
  QColor errorColor = theme.errorColor.isValid() ? theme.errorColor : QColor("#f85149");

  // Update tree delegate theme
  if (auto *delegate =
          dynamic_cast<TestTreeDelegate *>(m_tree->itemDelegate())) {
    delegate->setTheme(theme);
  }

  setStyleSheet(
      QString(
          "QWidget#TestPanel {"
          "  background: %1;"
          "  color: %2;"
          "}"
          "QWidget#testHeaderShell {"
          "  background: %3;"
          "  border-bottom: 1px solid %4;"
          "}"
          "QToolBar#testToolbar {"
          "  background: transparent;"
          "  border: 0;"
          "  padding: 4px 6px;"
          "  spacing: 4px;"
          "}"
          "QToolButton {"
          "  color: %2;"
          "  background: transparent;"
          "  border: 1px solid transparent;"
          "  border-radius: 4px;"
          "  padding: 4px 10px;"
          "  font-size: 12px;"
          "  font-weight: 600;"
          "}"
          "QToolButton:hover {"
          "  background: %5;"
          "  border-color: %4;"
          "}"
          "QToolButton:pressed {"
          "  background: %6;"
          "}"
          "QToolButton:disabled {"
          "  color: %7;"
          "}"
          "QToolButton:checked {"
          "  background: %6;"
          "  border-color: %9;"
          "}"
          "QLineEdit#testSearchEdit {"
          "  background: %8;"
          "  color: %2;"
          "  border: none;"
          "  border-top: 1px solid %4;"
          "  padding: 6px 10px;"
          "  font-size: 12px;"
          "}"
          "QLineEdit#testSearchEdit:focus {"
          "  border-bottom: 2px solid %9;"
          "}"
          "QProgressBar#testProgressBar {"
          "  background: %4;"
          "  border: none;"
          "  max-height: 3px;"
          "}"
          "QProgressBar#testProgressBar::chunk {"
          "  background: %9;"
          "}"
          "QTreeWidget#testTree {"
          "  background: %8;"
          "  color: %2;"
          "  border: none;"
          "  outline: none;"
          "  padding: 2px;"
          "}"
          "QTreeWidget#testTree::item {"
          "  height: 28px;"
          "}"
          "QTreeWidget#testTree::item:hover {"
          "  background: transparent;"
          "}"
          "QTreeWidget#testTree::item:selected {"
          "  background: transparent;"
          "  color: %2;"
          "}"
          "QHeaderView::section {"
          "  background: %3;"
          "  color: %7;"
          "  border: 0;"
          "  border-bottom: 1px solid %4;"
          "  padding: 6px 8px;"
          "  font-weight: 600;"
          "  font-size: 11px;"
          "  text-transform: uppercase;"
          "}"
          "QTextEdit#testDetailPane {"
          "  background: %8;"
          "  color: %2;"
          "  border: none;"
          "  border-top: 1px solid %4;"
          "  padding: 8px;"
          "  font-family: monospace;"
          "  font-size: 12px;"
          "}"
          "QLabel#testEmptyState {"
          "  color: %7;"
          "  font-size: 13px;"
          "  padding: 20px;"
          "}"
          "QLabel#statusLabel {"
          "  color: %2;"
          "  border-top: 1px solid %4;"
          "  padding: 5px 10px;"
          "  font-size: 12px;"
          "}"
          "QSplitter::handle {"
          "  background: %4;"
          "  height: 1px;"
          "}"
          "QScrollBar:vertical {"
          "  background: transparent;"
          "  width: 6px;"
          "  margin: 0;"
          "}"
          "QScrollBar::handle:vertical {"
          "  background: %4;"
          "  min-height: 20px;"
          "  border-radius: 3px;"
          "}"
          "QScrollBar::handle:vertical:hover {"
          "  background: %7;"
          "}"
          "QScrollBar::add-line:vertical,"
          "QScrollBar::sub-line:vertical {"
          "  height: 0;"
          "}"
          "QScrollBar:horizontal {"
          "  background: transparent;"
          "  height: 6px;"
          "  margin: 0;"
          "}"
          "QScrollBar::handle:horizontal {"
          "  background: %4;"
          "  min-width: 20px;"
          "  border-radius: 3px;"
          "}"
          "QScrollBar::handle:horizontal:hover {"
          "  background: %7;"
          "}"
          "QScrollBar::add-line:horizontal,"
          "QScrollBar::sub-line:horizontal {"
          "  width: 0;"
          "}")
          .arg(panelSurface.name(), textColor.name(), toolbarShell.name(),
               borderColor.name(), hoverBg.name(), selectedBg.name(),
               mutedText.name(), treeBg.name(), accentColor.name()));

  // Apply UIStyleHelper styles for combo boxes
  QString comboStyle = UIStyleHelper::comboBoxStyle(theme);
  m_filterCombo->setStyleSheet(comboStyle);
  m_configCombo->setStyleSheet(comboStyle);
  m_autoRunModeCombo->setStyleSheet(comboStyle);
}

void TestPanel::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
  TestConfigurationManager::instance().setWorkspaceFolder(folder);
  TestConfigurationManager::instance().loadUserConfigurations(folder);
  refreshConfigurations();
  m_autoTestRunner->setWorkspaceFolder(folder);
  m_autoTestRunner->loadSettings(folder);
  m_autoRunAction->setChecked(m_autoTestRunner->isEnabled());
  m_autoRunModeCombo->setEnabled(m_autoTestRunner->isEnabled());
  if (m_autoTestRunner->mode() != AutoRunMode::Off) {
    int modeIdx = autoRunModeToComboIndex(m_autoTestRunner->mode());
    if (modeIdx >= 0 && modeIdx < m_autoRunModeCombo->count())
      m_autoRunModeCombo->setCurrentIndex(modeIdx);
  }
  syncAutoRunConfiguration();
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

void TestPanel::runTestsForPath(const QString &path) {
  QFileInfo fi(path);

  if (fi.isFile()) {
    QString ext = fi.suffix().toLower();
    QList<TestConfiguration> matches =
        TestConfigurationManager::instance().configurationsForExtension(ext);
    if (!matches.isEmpty()) {

      int idx = m_configCombo->findData(matches.first().id);
      if (idx >= 0)
        m_configCombo->setCurrentIndex(idx);
    }
    runCurrentFile(path);
  } else if (fi.isDir()) {

    TestConfiguration config = currentConfiguration();
    if (!config.isValid())
      return;
    m_runManager->runAll(config, path);
  }
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
  item->setData(0, TestIdRole, result.id);
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
  item->setData(0, TestIdRole, result.id);
  item->setData(0, FilePathRole, result.filePath);
  item->setData(0, LineNumberRole, result.line);

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
  m_progressBar->setRange(0, 0);
  m_progressBar->setVisible(true);
  m_emptyStateLabel->setVisible(false);
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
  m_progressBar->setVisible(false);

  updateStatusLabel();
  updateEmptyState();
  emit countsChanged(passed, failed, skipped, errored);
}

void TestPanel::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item)
    return;

  QString filePath = item->data(0, FilePathRole).toString();
  int line = item->data(0, LineNumberRole).toInt();

  if (!filePath.isEmpty())
    emit locationClicked(filePath, line, 0);
}

void TestPanel::onItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item)
    return;

  QString filePath = item->data(0, FilePathRole).toString();
  int line = item->data(0, LineNumberRole).toInt();
  if (!filePath.isEmpty())
    emit locationClicked(filePath, line, 0);

  QString testId = item->data(0, TestIdRole).toString();
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

void TestPanel::onConfigChanged(int index) {
  Q_UNUSED(index)
  syncAutoRunConfiguration();
  updateDiscoveryAdapterForConfig();
}

void TestPanel::onContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_tree->itemAt(pos);
  if (!item)
    return;

  QMenu menu(this);
  if (m_theme.surfaceColor.isValid()) {
    menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));
  }

  QString testId = item->data(0, TestIdRole).toString();
  QString testName = item->text(0);
  QString filePath = item->data(0, FilePathRole).toString();

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
    int line = item->data(0, LineNumberRole).toInt();
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

  QColor successColor = m_theme.successColor.isValid()
                            ? m_theme.successColor
                            : QColor("#3fb950");
  QColor errorColor =
      m_theme.errorColor.isValid() ? m_theme.errorColor : QColor("#f85149");
  QColor warningColor = m_theme.warningColor.isValid()
                            ? m_theme.warningColor
                            : QColor("#d29922");
  QColor mutedColor = m_theme.foregroundColor.isValid()
                          ? m_theme.foregroundColor.darker(140)
                          : QColor("#8b949e");

  QString text = QString(
      "<span style='color:%6'>"
      "<span style='color:%1'>\u2714 %2</span>"
      " &nbsp; "
      "<span style='color:%3'>\u2718 %4</span>"
      " &nbsp; "
      "<span style='color:%5'>\u25CB %7</span>"
      " &nbsp; "
      "<span style='color:%8'>\u26A0 %9</span>"
      " &nbsp;&nbsp; Total: %10"
      "</span>")
      .arg(successColor.name())
      .arg(m_passedCount)
      .arg(errorColor.name())
      .arg(m_failedCount)
      .arg(warningColor.name())
      .arg(mutedColor.name())
      .arg(m_skippedCount)
      .arg(warningColor.darker(110).name())
      .arg(m_erroredCount)
      .arg(total);
  m_statusLabel->setTextFormat(Qt::RichText);
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
  item->setData(0, TestIdRole, suite);
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
  applySearchFilter();
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

  updateDiscoveryAdapterForConfig();
}

TestConfiguration TestPanel::currentConfiguration() const {
  QString name = m_configCombo->currentText();
  return TestConfigurationManager::instance().configurationByName(name);
}

void TestPanel::setDiscoveryAdapter(ITestDiscoveryAdapter *adapter) {
  if (m_discoveryAdapter) {
    disconnect(m_discoveryAdapter, nullptr, this, nullptr);
    if (m_ownsDiscoveryAdapter)
      delete m_discoveryAdapter;
  }
  m_discoveryAdapter = adapter;
  m_ownsDiscoveryAdapter = false;
  if (m_discoveryAdapter)
    connectDiscoveryAdapter();
}

void TestPanel::connectDiscoveryAdapter() {
  if (!m_discoveryAdapter)
    return;
  connect(m_discoveryAdapter, &ITestDiscoveryAdapter::discoveryFinished, this,
          &TestPanel::onDiscoveryFinished);
  connect(m_discoveryAdapter, &ITestDiscoveryAdapter::discoveryError, this,
          &TestPanel::onDiscoveryError);
}

void TestPanel::updateDiscoveryAdapterForConfig() {
  TestConfiguration config = currentConfiguration();
  if (!config.isValid())
    return;

  QString templateKey =
      config.templateId.isEmpty() ? config.id : config.templateId;

  if (m_discoveryAdapter && m_ownsDiscoveryAdapter) {
    disconnect(m_discoveryAdapter, nullptr, this, nullptr);
    delete m_discoveryAdapter;
    m_discoveryAdapter = nullptr;
    m_ownsDiscoveryAdapter = false;
  }

  ITestDiscoveryAdapter *adapter =
      TestDiscoveryAdapterFactory::createForConfiguration(templateKey, this);
  if (adapter) {
    m_discoveryAdapter = adapter;
    m_ownsDiscoveryAdapter = true;
    connectDiscoveryAdapter();
  }
}

void TestPanel::discoverTests() {
  if (m_workspaceFolder.isEmpty())
    return;

  if (!m_discoveryAdapter) {
    m_statusLabel->setText(tr("No discovery adapter configured"));
    return;
  }

  m_statusLabel->setText(tr("Discovering tests..."));
  m_discoverAction->setEnabled(false);

  m_discoveryAdapter->discover(m_workspaceFolder);
}

void TestPanel::onDiscoveryFinished(const QList<DiscoveredTest> &tests) {
  m_discoverAction->setEnabled(true);
  populateTreeFromDiscovery(tests);
  m_statusLabel->setText(tr("Discovered %1 tests").arg(tests.size()));
}

void TestPanel::onDiscoveryError(const QString &message) {
  m_discoverAction->setEnabled(true);
  m_statusLabel->setText(tr("Discovery error: %1").arg(message));
}

void TestPanel::populateTreeFromDiscovery(const QList<DiscoveredTest> &tests) {
  m_tree->clear();
  m_suiteItems.clear();
  m_testItems.clear();

  for (const DiscoveredTest &test : tests) {
    QTreeWidgetItem *suiteItem = findOrCreateSuiteItem(test.suite);

    auto *item = new QTreeWidgetItem();
    item->setText(0, test.name);
    item->setData(0, TestIdRole, test.id);
    if (!test.filePath.isEmpty())
      item->setData(0, FilePathRole, test.filePath);
    if (test.line >= 0)
      item->setData(0, LineNumberRole, test.line);
    updateTreeItemIcon(item, TestStatus::Queued);
    item->setText(1, tr("Not Run"));

    if (suiteItem)
      suiteItem->addChild(item);
    else
      m_tree->addTopLevelItem(item);
    m_testItems[test.id] = item;
  }
  updateEmptyState();
}

void TestPanel::notifyFileSaved(const QString &filePath) {
  syncAutoRunConfiguration();
  m_autoTestRunner->notifyFileSaved(filePath);
}

void TestPanel::onAutoRunToggled(bool checked) {
  m_autoTestRunner->setEnabled(checked);
  m_autoRunModeCombo->setEnabled(checked);
  if (checked) {
    m_autoTestRunner->setMode(
        comboIndexToAutoRunMode(m_autoRunModeCombo->currentIndex()));
  } else {
    m_autoTestRunner->setMode(AutoRunMode::Off);
  }
  m_autoTestRunner->saveSettings(m_workspaceFolder);
}

void TestPanel::onAutoRunModeChanged(int index) {
  if (m_autoTestRunner->isEnabled()) {
    m_autoTestRunner->setMode(comboIndexToAutoRunMode(index));
    m_autoTestRunner->saveSettings(m_workspaceFolder);
  }
}

void TestPanel::syncAutoRunConfiguration() {
  TestConfiguration config = currentConfiguration();
  m_autoTestRunner->setConfiguration(config);
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

void TestPanel::onSearchTextChanged(const QString &text) {
  Q_UNUSED(text)
  applySearchFilter();
}

void TestPanel::applySearchFilter() {
  QString searchText = m_searchEdit->text().trimmed();
  int filterIndex = m_filterCombo->currentIndex();

  for (auto it = m_testItems.begin(); it != m_testItems.end(); ++it) {
    QTreeWidgetItem *item = it.value();
    bool visible = true;

    // Apply status filter
    if (m_testResults.contains(it.key())) {
      const TestResult &result = m_testResults[it.key()];
      switch (filterIndex) {
      case 1:
        visible = (result.status == TestStatus::Failed ||
                   result.status == TestStatus::Errored);
        break;
      case 2:
        visible = (result.status == TestStatus::Passed);
        break;
      case 3:
        visible = (result.status == TestStatus::Skipped);
        break;
      default:
        visible = true;
        break;
      }
    }

    // Apply search text filter
    if (visible && !searchText.isEmpty()) {
      visible = item->text(0).contains(searchText, Qt::CaseInsensitive);
    }

    item->setHidden(!visible);
  }

  // Update suite visibility based on child visibility
  for (auto it = m_suiteItems.begin(); it != m_suiteItems.end(); ++it) {
    QTreeWidgetItem *suite = it.value();
    bool hasVisible = false;

    // Check if suite name matches search
    if (!searchText.isEmpty() &&
        suite->text(0).contains(searchText, Qt::CaseInsensitive)) {
      // Show entire suite when suite name matches
      suite->setHidden(false);
      for (int i = 0; i < suite->childCount(); ++i) {
        suite->child(i)->setHidden(false);
      }
      continue;
    }

    for (int i = 0; i < suite->childCount(); ++i) {
      if (!suite->child(i)->isHidden()) {
        hasVisible = true;
        break;
      }
    }
    suite->setHidden(!hasVisible);
  }
}

void TestPanel::updateEmptyState() {
  bool hasItems =
      m_tree->topLevelItemCount() > 0 || !m_testItems.isEmpty();
  m_emptyStateLabel->setVisible(!hasItems);
  m_tree->setVisible(hasItems);
}
