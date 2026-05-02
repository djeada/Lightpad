#include "testpanel.h"
#include "../dialogs/testconfigurationdialog.h"
#include "../uistylehelper.h"
#include "../dialogs/themedmessagebox.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>

namespace {
constexpr int TestIdRole = Qt::UserRole;
constexpr int FilePathRole = Qt::UserRole + 1;
constexpr int LineNumberRole = Qt::UserRole + 2;
constexpr int IsSuiteRole = Qt::UserRole + 3;
constexpr int IsDetailItemRole = Qt::UserRole + 4;

void appendUnique(QStringList &values, const QString &value) {
  const QString trimmed = value.trimmed();
  if (!trimmed.isEmpty() && !values.contains(trimmed))
    values.append(trimmed);
}

QStringList googleTestTokenCandidates(const QString &value) {
  QStringList candidates;
  appendUnique(candidates, value);

  const int firstSlash = value.indexOf('/');
  if (firstSlash > 0)
    appendUnique(candidates, value.left(firstSlash));

  const int lastSlash = value.lastIndexOf('/');
  if (lastSlash > 0 && lastSlash + 1 < value.size())
    appendUnique(candidates, value.mid(lastSlash + 1));

  return candidates;
}

QString toRegexAlternation(const QStringList &values) {
  QStringList escaped;
  escaped.reserve(values.size());
  for (const QString &value : values)
    escaped.append(QRegularExpression::escape(value));
  return escaped.join('|');
}

QString formatCommandArgument(const QString &arg) {
  if (arg.isEmpty())
    return "\"\"";

  QString escaped = arg;
  escaped.replace('\\', "\\\\");
  escaped.replace('"', "\\\"");

  static const QRegularExpression needsQuoting(R"([\s"])");
  if (needsQuoting.match(arg).hasMatch())
    return "\"" + escaped + "\"";
  return escaped;
}

QStringList extractGoogleTestNames(const QString &content) {
  QStringList names;
  static const QRegularExpression testRegex(
      R"(\b(?:TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)\s*\(\s*([^\s,()]+)\s*,\s*([^\s,()]+)\s*\))");

  QRegularExpressionMatchIterator it = testRegex.globalMatch(content);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    appendUnique(names, match.captured(1).trimmed() + "." +
                            match.captured(2).trimmed());
  }

  return names;
}

bool resolveNavigableFilePath(const QString &candidatePath,
                              const QString &workspaceFolder,
                              QString *resolvedPath) {
  if (candidatePath.isEmpty() || !resolvedPath)
    return false;

  QString absolutePath = candidatePath;
  QFileInfo info(absolutePath);
  if (info.isRelative() && !workspaceFolder.isEmpty()) {
    absolutePath = QDir(workspaceFolder).absoluteFilePath(absolutePath);
    info = QFileInfo(absolutePath);
  }

  if (!info.exists() || !info.isFile())
    return false;

  *resolvedPath = info.absoluteFilePath();
  return true;
}

bool findGoogleTestMacroLocation(const QString &content,
                                 const QStringList &suiteCandidates,
                                 const QStringList &testCandidates,
                                 int *line) {
  if (suiteCandidates.isEmpty() || testCandidates.isEmpty())
    return false;

  const QString pattern =
      QString(R"(\b(?:TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)\s*\(\s*(?:%1)\s*,\s*(?:%2)\s*\))")
          .arg(toRegexAlternation(suiteCandidates), toRegexAlternation(testCandidates));
  const QRegularExpression regex(pattern);
  const QRegularExpressionMatch match = regex.match(content);
  if (!match.hasMatch())
    return false;

  if (line)
    *line = content.left(match.capturedStart()).count('\n') + 1;
  return true;
}

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

  auto &configManager = TestConfigurationManager::instance();
  connect(&configManager, &TestConfigurationManager::templatesLoaded, this,
          &TestPanel::refreshConfigurations);
  connect(&configManager, &TestConfigurationManager::configurationsChanged,
          this, &TestPanel::refreshConfigurations);

  connect(m_runManager, &TestRunManager::testStarted, this,
          &TestPanel::onTestStarted);
  connect(m_runManager, &TestRunManager::testFinished, this,
          &TestPanel::onTestFinished);
  connect(m_runManager, &TestRunManager::runStarted, this,
          &TestPanel::onRunStarted);
  connect(m_runManager, &TestRunManager::runFinished, this,
          &TestPanel::onRunFinished);
  connect(m_runManager, &TestRunManager::outputLine, this,
          &TestPanel::onOutputLine);
  connect(m_runManager, &TestRunManager::processStarted, this,
          &TestPanel::onProcessStarted);
  connect(m_runManager, &TestRunManager::processFinished, this,
          &TestPanel::onProcessFinished);

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

  m_headerShell = new QWidget(this);
  m_headerShell->setObjectName("testHeaderShell");
  auto *headerLayout = new QVBoxLayout(m_headerShell);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(0);

  setupToolbar();
  headerLayout->addWidget(m_toolbar);

  m_searchEdit = new QLineEdit(m_headerShell);
  m_searchEdit->setObjectName("testSearchEdit");
  m_searchEdit->setPlaceholderText(tr("Filter tests by name..."));
  m_searchEdit->setClearButtonEnabled(true);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &TestPanel::onSearchTextChanged);
  headerLayout->addWidget(m_searchEdit);

  layout->addWidget(m_headerShell);

  m_progressBar = new QProgressBar(this);
  m_progressBar->setObjectName("testProgressBar");
  m_progressBar->setMaximumHeight(3);
  m_progressBar->setTextVisible(false);
  m_progressBar->setRange(0, 0);
  m_progressBar->setVisible(false);
  layout->addWidget(m_progressBar);

  m_splitter = new QSplitter(Qt::Vertical, this);

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

  m_emptyStateLabel = new QLabel(treeContainer);
  m_emptyStateLabel->setObjectName("testEmptyState");
  m_emptyStateLabel->setAlignment(Qt::AlignCenter);
  m_emptyStateLabel->setText(
      tr("No tests discovered yet.\nUse \"Discover\" to find tests or "
         "\"Run All\" to execute them."));
  m_emptyStateLabel->setWordWrap(true);

  treeLayout->addWidget(m_tree);
  treeLayout->addWidget(m_emptyStateLabel);

  m_detailSection = new QWidget(this);
  m_detailSection->setObjectName("testDetailSection");
  auto *detailLayout = new QVBoxLayout(m_detailSection);
  detailLayout->setContentsMargins(0, 0, 0, 0);
  detailLayout->setSpacing(0);

  m_detailToggleButton = new QToolButton(m_detailSection);
  m_detailToggleButton->setObjectName("testDetailToggle");
  m_detailToggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_detailToggleButton->setText(tr("Run Details"));
  m_detailToggleButton->setCheckable(true);
  m_detailToggleButton->setChecked(false);
  m_detailToggleButton->setArrowType(Qt::RightArrow);
  detailLayout->addWidget(m_detailToggleButton);

  m_detailPane = new QTextEdit(m_detailSection);
  m_detailPane->setObjectName("testDetailPane");
  m_detailPane->setReadOnly(true);
  m_detailPane->setMinimumHeight(100);
  m_detailPane->setPlaceholderText(
      tr("Select a test to view its output and details"));
  detailLayout->addWidget(m_detailPane);

  connect(m_detailToggleButton, &QToolButton::toggled, this,
          [this](bool checked) { setDetailsExpanded(checked); });

  m_splitter->addWidget(treeContainer);
  m_splitter->addWidget(m_detailSection);
  m_splitter->setStretchFactor(0, 3);
  m_splitter->setStretchFactor(1, 1);
  setDetailsExpanded(false);

  layout->addWidget(m_splitter);

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
                              resetRunContext();
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

  m_toolbar->addSeparator();

  m_configureAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("Configure"),
      this, &TestPanel::openConfigurationDialog);
  m_configureAction->setToolTip(tr("Edit Test Configurations"));
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

  QColor panelSurface = blend(theme.backgroundColor, theme.surfaceColor, 0.08);
  QColor toolbarShell = blend(theme.backgroundColor, theme.surfaceColor, 0.14);
  QColor treeBg = theme.backgroundColor;
  QColor textColor = theme.foregroundColor;
  QColor borderColor = theme.borderColor;
  QColor mutedText = blend(theme.foregroundColor, theme.backgroundColor, 0.24);
  QColor subtleText = blend(theme.foregroundColor, theme.backgroundColor, 0.40);
  QColor hoverBg = blend(theme.backgroundColor, theme.foregroundColor, 0.06);
  QColor selectedBg = theme.accentSoftColor.isValid() ? theme.accentSoftColor
                                                      : theme.accentColor;
  QColor accentColor =
      theme.accentColor.isValid() ? theme.accentColor : theme.foregroundColor;
  QColor successColor =
      theme.successColor.isValid() ? theme.successColor : theme.successColor;
  QColor errorColor =
      theme.errorColor.isValid() ? theme.errorColor : theme.errorColor;

  if (auto *delegate =
          dynamic_cast<TestTreeDelegate *>(m_tree->itemDelegate())) {
    delegate->setTheme(theme);
  }

  setStyleSheet(QString("QWidget#TestPanel {"
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
                        "QWidget#testDetailSection {"
                        "  background: %8;"
                        "  border-top: 1px solid %4;"
                        "}"
                        "QToolButton#testDetailToggle {"
                        "  background: %3;"
                        "  border: none;"
                        "  border-bottom: 1px solid %4;"
                        "  border-radius: 0;"
                        "  padding: 6px 10px;"
                        "  text-align: left;"
                        "  font-size: 11px;"
                        "  font-weight: 600;"
                        "}"
                        "QToolButton#testDetailToggle:hover {"
                        "  background: %5;"
                        "  border-color: %4;"
                        "}"
                        "QToolButton#testDetailToggle:checked {"
                        "  background: %3;"
                        "  border-color: %4;"
                        "}"
                        "QTextEdit#testDetailPane {"
                        "  background: %8;"
                        "  color: %2;"
                        "  border: none;"
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
                    .arg(panelSurface.name(), textColor.name(),
                         toolbarShell.name(), borderColor.name(),
                         hoverBg.name(), selectedBg.name(), mutedText.name(),
                         treeBg.name(), accentColor.name()));

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
  recordRunRequest(config, tr("Workspace"));
  m_runManager->runAll(config, m_workspaceFolder);
}

void TestPanel::runFailed() {
  TestConfiguration config = currentConfiguration();
  if (!config.isValid())
    return;
  recordRunRequest(config, tr("Failed tests"));
  m_runManager->runFailed(config, m_workspaceFolder);
}

void TestPanel::runCurrentFile(const QString &filePath) {
  TestConfiguration config = currentConfiguration();
  if (!config.isValid())
    return;
  if (runGoogleTestScopedPath(config, filePath))
    return;
  recordRunRequest(config, tr("File"), filePath);
  m_runManager->runFile(config, m_workspaceFolder, filePath);
}

void TestPanel::runTestsForPath(const QString &path) {
  QFileInfo fi(path);
  const TestConfiguration preferredConfig =
      TestConfigurationManager::instance().preferredConfigurationForPath(path);
  if (preferredConfig.isValid()) {
    const int idx = m_configCombo->findData(preferredConfig.id);
    if (idx >= 0)
      m_configCombo->setCurrentIndex(idx);
  }

  if (fi.isFile()) {
    runCurrentFile(path);
  } else if (fi.isDir()) {

    TestConfiguration config = currentConfiguration();
    if (!config.isValid())
      return;
    if (runGoogleTestScopedPath(config, path))
      return;
    recordRunRequest(config, tr("Directory"), path);
    m_runManager->runAll(config, m_workspaceFolder, path);
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
    QFileInfo fileInfo(filePath);
    TestConfiguration config = currentConfiguration();
    if (!config.isValid())
      return false;
    if (runGoogleTestScopedPath(config, filePath)) {
      return true;
    } else if (fileInfo.isDir()) {
      recordRunRequest(config, tr("Directory"), filePath);
      m_runManager->runAll(config, m_workspaceFolder, filePath);
    } else {
      runCurrentFile(filePath);
    }
  }
  return true;
}

void TestPanel::stopTests() { m_runManager->stop(); }

void TestPanel::openConfigurationDialog() {
  if (m_workspaceFolder.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Test Configurations"),
        tr("Open a project or workspace first to configure test settings."));
    return;
  }

  TestConfigurationManager::instance().setWorkspaceFolder(m_workspaceFolder);
  TestConfigurationManager::instance().loadUserConfigurations(m_workspaceFolder);

  if (auto *existing = window()->findChild<TestConfigurationDialog *>()) {
    if (existing->isVisible()) {
      existing->raise();
      existing->activateWindow();
      return;
    }
  }

  auto *dialog = new TestConfigurationDialog(
      this, m_configCombo->currentData().toString());
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->applyTheme(m_theme);
  dialog->show();
}

void TestPanel::onTestStarted(const TestResult &result) {
  m_lastRunProducedResults = true;
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
  m_lastRunProducedResults = true;
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
  if (!result.filePath.isEmpty()) {
    item->setData(0, FilePathRole, result.filePath);
    if (result.line >= 0)
      item->setData(0, LineNumberRole, result.line);
  } else if (m_discoveredTests.contains(result.id)) {
    const DiscoveredTest &dt = m_discoveredTests[result.id];
    if (!dt.filePath.isEmpty()) {
      item->setData(0, FilePathRole, dt.filePath);
      if (dt.line >= 0)
        item->setData(0, LineNumberRole, dt.line);
    }
  } else {
    // Name-based fallback: search discovered tests by name when ID doesn't match
    if (const DiscoveredTest *dt = findDiscoveredTestByName(result.name)) {
      item->setData(0, FilePathRole, dt->filePath);
      if (dt->line >= 0)
        item->setData(0, LineNumberRole, dt->line);
    }
  }

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

  // Remove any stale detail children (e.g. from a re-run of the same test)
  while (item->childCount() > 0)
    delete item->takeChild(0);

  // Add collapsible detail children so the user can expand a result to see
  // all execution info without leaving the tree.
  auto addDetailChild = [&](const QString &label, const QString &content) {
    if (content.isEmpty())
      return;
    auto *child = new QTreeWidgetItem(item);
    const QString firstLine = content.split('\n').first().trimmed();
    child->setText(0, label + firstLine);
    child->setData(0, TestIdRole, result.id);
    child->setData(0, IsDetailItemRole, true);
    child->setToolTip(0, content);
  };

  if (!result.message.isEmpty())
    addDetailChild(tr("Message: "), result.message);
  if (!result.stackTrace.isEmpty())
    addDetailChild(tr("Stack Trace: "), result.stackTrace);
  if (!result.stdoutOutput.isEmpty())
    addDetailChild(tr("stdout: "), result.stdoutOutput);
  if (!result.stderrOutput.isEmpty())
    addDetailChild(tr("stderr: "), result.stderrOutput);
  // item stays collapsed by default; expand arrow appears when children exist

  applyFilter();
}

void TestPanel::onRunStarted() {
  m_tree->clear();
  m_suiteItems.clear();
  m_testItems.clear();
  m_testResults.clear();
  m_passedCount = m_failedCount = m_skippedCount = m_erroredCount = 0;
  m_latestStdoutLine.clear();
  m_lastRunProducedResults = false;

  m_stopAction->setEnabled(true);
  m_runAllAction->setEnabled(false);
  m_runFailedAction->setEnabled(false);
  m_progressBar->setRange(0, 0);
  m_progressBar->setVisible(true);
  updateRunningStatusLabel();
  refreshRunDetails();
  updateEmptyState();
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
  refreshRunDetails();
  updateEmptyState();
  emit countsChanged(passed, failed, skipped, errored);
}

void TestPanel::onOutputLine(const QString &line, bool isError) {
  if (!line.isEmpty()) {
    if (!m_lastRunOutput.isEmpty() && !m_lastRunOutput.endsWith('\n') &&
        !line.startsWith('\n')) {
      m_lastRunOutput += '\n';
    }
    m_lastRunOutput += line;
    if (!m_lastRunOutput.endsWith('\n'))
      m_lastRunOutput += '\n';
  }

  refreshRunDetails();

  if (isError)
    return;
  const QString trimmed = line.trimmed();
  if (trimmed.isEmpty())
    return;
  m_latestStdoutLine = trimmed;
  updateRunningStatusLabel();
}

void TestPanel::onProcessStarted(const QString &command, const QStringList &args,
                                 const QString &workingDirectory) {
  m_lastRunCommand = command;
  m_lastRunArgs = args;
  m_lastRunWorkingDirectory = workingDirectory;
  refreshRunDetails();
}

void TestPanel::onProcessFinished(int exitCode, bool normalExit) {
  m_lastRunExitCode = exitCode;
  m_lastRunExitedNormally = normalExit;
  refreshRunDetails();
}

void TestPanel::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  QString filePath;
  int line = 0;
  if (resolveSourceLocation(item, &filePath, &line))
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
    setDetailsExpanded(true);
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

  // Detail child items have their own role; skip the context menu for them
  if (item->data(0, IsDetailItemRole).toBool())
    return;

  QString testId = item->data(0, TestIdRole).toString();
  QString testName = item->text(0);
  QString filePath = item->data(0, FilePathRole).toString();

  // Distinguish real suite items from test items that have detail children
  bool isSuite = item->data(0, IsSuiteRole).toBool();

  if (isSuite) {
    menu.addAction(tr("Run Suite"), [this, testName]() {
      TestConfiguration config = currentConfiguration();
      if (!config.isValid())
        return;
      recordRunRequest(config, tr("Suite"), testName);
      m_runManager->runSuite(config, m_workspaceFolder, testName);
    });
  } else {
    QAction *runAction =
        menu.addAction(tr("Run This Test"), [this, testName, filePath]() {
          TestConfiguration config = currentConfiguration();
          if (!config.isValid())
            return;
          recordRunRequest(config, tr("Test"), filePath);
          m_runManager->runSingleTest(config, m_workspaceFolder, testName,
                                      filePath);
        });
    Q_UNUSED(runAction)
  }

  QString sourceFilePath;
  int sourceLine = 0;
  if (resolveSourceLocation(item, &sourceFilePath, &sourceLine)) {
    menu.addAction(tr("Go to Source"), [this, sourceFilePath, sourceLine]() {
      emit locationClicked(sourceFilePath, sourceLine, 0);
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
      setDetailsExpanded(true);
    });
  }

  menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void TestPanel::updateStatusLabel() {
  int total = m_passedCount + m_failedCount + m_skippedCount + m_erroredCount;

  if (total == 0 && m_hasRunContext) {
    QString label = tr("No tests were reported");
    if (!m_runManager || !m_runManager->isRunning())
      label += tr(" (exit %1)").arg(m_lastRunExitCode);
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setText(label);
    return;
  }

  QColor successColor = m_theme.successColor.isValid() ? m_theme.successColor
                                                       : m_theme.successColor;
  QColor errorColor =
      m_theme.errorColor.isValid() ? m_theme.errorColor : m_theme.errorColor;
  QColor warningColor = m_theme.warningColor.isValid() ? m_theme.warningColor
                                                       : m_theme.accentColor;
  QColor mutedColor = m_theme.foregroundColor.isValid()
                          ? m_theme.foregroundColor.darker(140)
                          : m_theme.singleLineCommentFormat;

  QString text = QString("<span style='color:%6'>"
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

void TestPanel::updateRunningStatusLabel() {
  constexpr int kMaxLen = 120;
  QString label = tr("Running tests...");
  if (!m_latestStdoutLine.isEmpty()) {
    QString display = m_latestStdoutLine;
    if (display.length() > kMaxLen)
      display = display.left(kMaxLen) + QChar(0x2026); // "…"
    label += QString(QChar(0x20)) + QChar(0x2502) + QChar(0x20) + display; // " │ "
  }
  m_statusLabel->setTextFormat(Qt::PlainText);
  m_statusLabel->setText(label);
}

void TestPanel::updateTreeItemIcon(QTreeWidgetItem *item, TestStatus status) {
  QColor color;
  QStyle::StandardPixmap statusIcon = QStyle::SP_FileIcon;
  switch (status) {
  case TestStatus::Passed:
    color = m_theme.successColor.isValid() ? m_theme.successColor
                                           : m_theme.successColor;
    statusIcon = QStyle::SP_DialogApplyButton;
    break;
  case TestStatus::Failed:
    color =
        m_theme.errorColor.isValid() ? m_theme.errorColor : m_theme.errorColor;
    statusIcon = QStyle::SP_MessageBoxCritical;
    break;
  case TestStatus::Skipped:
    color = m_theme.warningColor.isValid() ? m_theme.warningColor
                                           : m_theme.accentColor;
    statusIcon = QStyle::SP_DialogCancelButton;
    break;
  case TestStatus::Errored:
    color = m_theme.warningColor.isValid() ? m_theme.warningColor.lighter(120)
                                           : m_theme.accentColor;
    statusIcon = QStyle::SP_MessageBoxWarning;
    break;
  case TestStatus::Running:
    color = m_theme.accentColor;
    statusIcon = QStyle::SP_BrowserReload;
    break;
  case TestStatus::Queued:
    color = m_theme.singleLineCommentFormat;
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
  item->setData(0, IsSuiteRole, true);
  m_tree->addTopLevelItem(item);
  item->setExpanded(true);
  m_suiteItems[suite] = item;
  return item;
}

QTreeWidgetItem *TestPanel::findTestItem(const QString &id) {
  if (m_testItems.contains(id))
    return m_testItems[id];
  return nullptr;
}

const DiscoveredTest *
TestPanel::findDiscoveredTestByName(const QString &name) const {
  for (auto it = m_discoveredTests.cbegin(); it != m_discoveredTests.cend();
       ++it) {
    if (it.value().name == name && !it.value().filePath.isEmpty())
      return &it.value();
  }
  return nullptr;
}

bool TestPanel::supportsGoogleTestPathScoping(
    const TestConfiguration &config) const {
  if (config.outputFormat != "ctest")
    return false;

  const QString templateKey =
      config.templateId.isEmpty() ? config.id : config.templateId;
  return config.language.compare("C++", Qt::CaseInsensitive) == 0 ||
         templateKey == "gtest_cmake" || templateKey == "gtest_make";
}

bool TestPanel::runGoogleTestScopedPath(const TestConfiguration &config,
                                        const QString &path) {
  if (!supportsGoogleTestPathScoping(config))
    return false;

  QFileInfo info(path);
  if (!info.exists())
    return false;

  const QString scopeLabel = info.isDir() ? tr("Directory") : tr("File");
  recordRunRequest(config, scopeLabel, path);

  const QStringList testNames = googleTestNamesForPath(config, path);
  if (testNames.isEmpty()) {
    m_lastRunCommand = tr("No command executed");
    m_lastRunOutput =
        tr("No matching Google Test cases were found under:\n%1").arg(path);
    refreshRunDetails();
    updateStatusLabel();
    updateEmptyState();
    return true;
  }

  const QString pattern = QString("^(%1)$").arg(toRegexAlternation(testNames));
  m_runManager->runPattern(config, m_workspaceFolder, pattern, path);
  return true;
}

QStringList TestPanel::googleTestNamesForPath(const TestConfiguration &config,
                                              const QString &path) const {
  QStringList names;
  QFileInfo pathInfo(path);
  if (!pathInfo.exists())
    return names;

  QStringList extensions = config.extensions;
  const QStringList commonCppExtensions = {"cpp", "cc",  "cxx", "c++", "h",
                                           "hh",  "hpp", "hxx", "ipp", "inl"};
  for (const QString &extension : commonCppExtensions)
    appendUnique(extensions, extension);

  auto processFile = [&names](const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      return;
    const QString content = QString::fromUtf8(file.readAll());
    const QStringList fileNames = extractGoogleTestNames(content);
    for (const QString &name : fileNames)
      appendUnique(names, name);
  };

  if (pathInfo.isFile()) {
    processFile(pathInfo.absoluteFilePath());
    return names;
  }

  QStringList nameFilters;
  for (const QString &extension : extensions)
    nameFilters.append(QString("*.%1").arg(extension));

  QDirIterator it(pathInfo.absoluteFilePath(), nameFilters, QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext())
    processFile(it.next());

  return names;
}

void TestPanel::resetRunContext() {
  m_lastRunConfigurationName.clear();
  m_lastRunScope.clear();
  m_lastRunTargetPath.clear();
  m_lastRunCommand.clear();
  m_lastRunArgs.clear();
  m_lastRunWorkingDirectory.clear();
  m_lastRunOutput.clear();
  m_lastRunExitCode = 0;
  m_lastRunExitedNormally = true;
  m_hasRunContext = false;
  m_lastRunProducedResults = false;
}

void TestPanel::recordRunRequest(const TestConfiguration &config,
                                 const QString &scope,
                                 const QString &targetPath) {
  m_lastRunConfigurationName = config.name;
  m_lastRunScope = scope;
  m_lastRunTargetPath = targetPath;
  m_lastRunCommand.clear();
  m_lastRunArgs.clear();
  m_lastRunWorkingDirectory.clear();
  m_lastRunOutput.clear();
  m_lastRunExitCode = 0;
  m_lastRunExitedNormally = true;
  m_hasRunContext = true;
  m_lastRunProducedResults = false;
  refreshRunDetails();
  updateEmptyState();
}

void TestPanel::refreshRunDetails() {
  if (!m_hasRunContext)
    return;

  QTreeWidgetItem *currentItem = m_tree->currentItem();
  if (currentItem && currentItem->data(0, IsDetailItemRole).toBool())
    currentItem = currentItem->parent();
  if (currentItem) {
    const QString testId = currentItem->data(0, TestIdRole).toString();
    if (m_testResults.contains(testId))
      return;
  }

  m_detailPane->setPlainText(buildRunDetailsText());
}

QString TestPanel::buildRunDetailsText() const {
  QStringList lines;
  lines << tr("Run Details");
  if (!m_lastRunConfigurationName.isEmpty())
    lines << tr("Configuration: %1").arg(m_lastRunConfigurationName);
  if (!m_lastRunScope.isEmpty())
    lines << tr("Scope: %1").arg(m_lastRunScope);
  if (!m_lastRunTargetPath.isEmpty())
    lines << tr("Target: %1").arg(m_lastRunTargetPath);
  if (!m_workspaceFolder.isEmpty())
    lines << tr("Workspace: %1").arg(m_workspaceFolder);
  if (!m_lastRunWorkingDirectory.isEmpty())
    lines << tr("Working directory: %1").arg(m_lastRunWorkingDirectory);

  QString commandLine = m_lastRunCommand;
  for (const QString &arg : m_lastRunArgs) {
    if (!commandLine.isEmpty())
      commandLine += ' ';
    commandLine += formatCommandArgument(arg);
  }
  lines << tr("Command: %1")
               .arg(commandLine.isEmpty() ? tr("Preparing...") : commandLine);

  QString state = m_runManager && m_runManager->isRunning() ? tr("Running")
                                                            : tr("Finished");
  if (!m_runManager || !m_runManager->isRunning()) {
    state += tr(" (exit code %1%2)")
                 .arg(m_lastRunExitCode)
                 .arg(m_lastRunExitedNormally ? QString() : tr(", abnormal"));
  }
  lines << tr("State: %1").arg(state);

  if (!m_runManager || !m_runManager->isRunning()) {
    if (m_lastRunProducedResults) {
      lines << tr("Result: %1 test item(s) reported.")
                   .arg(m_testResults.size());
    } else {
      lines << tr("Result: No tests were reported for this run.");
    }
  }

  if (!m_lastRunOutput.trimmed().isEmpty()) {
    lines << QString();
    lines << tr("Raw output:");
    lines << m_lastRunOutput.trimmed();
  }

  return lines.join('\n');
}

void TestPanel::setDetailsExpanded(bool expanded) {
  if (m_detailToggleButton->isChecked() != expanded)
    m_detailToggleButton->setChecked(expanded);

  m_detailToggleButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
  m_detailPane->setVisible(expanded);

  QList<int> sizes = m_splitter->sizes();
  if (sizes.size() != 2)
    return;

  if (expanded) {
    const int total = qMax(m_splitter->sizes().value(0) +
                               m_splitter->sizes().value(1),
                           1);
    const int desiredDetails = qMax(140, total / 5);
    if (sizes[1] < desiredDetails) {
      sizes[1] = desiredDetails;
      sizes[0] = qMax(0, total - desiredDetails);
      m_splitter->setSizes(sizes);
    }
  } else {
    const int collapsedHeight = m_detailToggleButton->sizeHint().height() + 2;
    const int total = qMax(sizes[0] + sizes[1], collapsedHeight);
    sizes[1] = collapsedHeight;
    sizes[0] = qMax(0, total - collapsedHeight);
    m_splitter->setSizes(sizes);
  }
}

bool TestPanel::resolveSourceLocation(QTreeWidgetItem *item, QString *filePath,
                                      int *line) {
  if (!item || !filePath || !line)
    return false;

  if (item->data(0, IsDetailItemRole).toBool()) {
    item = item->parent();
    if (!item)
      return false;
  }

  if (item->data(0, IsSuiteRole).toBool())
    return false;

  QString resolvedPath;
  const QString itemPath = item->data(0, FilePathRole).toString();
  const int itemLine = item->data(0, LineNumberRole).toInt();
  if (resolveNavigableFilePath(itemPath, m_workspaceFolder, &resolvedPath)) {
    *filePath = resolvedPath;
    *line = itemLine >= 0 ? itemLine : 0;
    if (resolvedPath != itemPath)
      item->setData(0, FilePathRole, resolvedPath);
    return true;
  }

  if (const DiscoveredTest *dt = findDiscoveredTestByName(item->text(0))) {
    if (resolveNavigableFilePath(dt->filePath, m_workspaceFolder,
                                 &resolvedPath)) {
      *filePath = resolvedPath;
      *line = dt->line >= 0 ? dt->line : 0;
      item->setData(0, FilePathRole, resolvedPath);
      item->setData(0, LineNumberRole, *line);
      return true;
    }
  }

  if (!resolveWorkspaceSourceLocation(item, &resolvedPath, line))
    return false;

  *filePath = resolvedPath;
  item->setData(0, FilePathRole, resolvedPath);
  item->setData(0, LineNumberRole, *line);
  return true;
}

bool TestPanel::resolveWorkspaceSourceLocation(QTreeWidgetItem *item,
                                               QString *filePath,
                                               int *line) const {
  if (!item || !filePath || !line || m_workspaceFolder.isEmpty())
    return false;

  const TestConfiguration config = currentConfiguration();
  const bool shouldUseCppLookup =
      config.outputFormat == "ctest" ||
      config.language.compare("C++", Qt::CaseInsensitive) == 0;
  if (!shouldUseCppLookup)
    return false;

  QString suiteName;
  QString testName = item->text(0).trimmed();
  if (QTreeWidgetItem *parent = item->parent();
      parent && parent->data(0, IsSuiteRole).toBool()) {
    suiteName = parent->text(0).trimmed();
  }

  const QString testId = item->data(0, TestIdRole).toString();
  if (suiteName.isEmpty() && m_testResults.contains(testId))
    suiteName = m_testResults.value(testId).suite.trimmed();
  if (suiteName.isEmpty() && m_discoveredTests.contains(testId))
    suiteName = m_discoveredTests.value(testId).suite.trimmed();

  if (suiteName.isEmpty()) {
    const int dotIndex = testName.indexOf('.');
    if (dotIndex > 0 && dotIndex + 1 < testName.size()) {
      suiteName = testName.left(dotIndex).trimmed();
      testName = testName.mid(dotIndex + 1).trimmed();
    }
  }

  QStringList suiteCandidates = googleTestTokenCandidates(suiteName);
  QStringList testCandidates = googleTestTokenCandidates(testName);
  if (suiteCandidates.isEmpty() || testCandidates.isEmpty())
    return false;

  QStringList extensions = config.extensions;
  const QStringList commonCppExtensions = {"cpp", "cc",  "cxx", "c++", "h",
                                           "hh",  "hpp", "hxx", "ipp", "inl"};
  for (const QString &extension : commonCppExtensions)
    appendUnique(extensions, extension);

  QStringList nameFilters;
  nameFilters.reserve(extensions.size());
  for (const QString &extension : extensions)
    nameFilters.append(QString("*.%1").arg(extension));

  const QString workspaceRoot = QDir(m_workspaceFolder).absolutePath();
  const QString buildRoot = QDir(workspaceRoot).absoluteFilePath("build") + '/';
  const QString gitRoot = QDir(workspaceRoot).absoluteFilePath(".git") + '/';
  const QString lightpadRoot =
      QDir(workspaceRoot).absoluteFilePath(".lightpad") + '/';

  QDirIterator it(workspaceRoot, nameFilters, QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString candidatePath = it.next();
    if (candidatePath.startsWith(buildRoot) || candidatePath.startsWith(gitRoot) ||
        candidatePath.startsWith(lightpadRoot)) {
      continue;
    }

    const QFileInfo info(candidatePath);
    if (info.size() > 1024 * 1024)
      continue;

    QFile file(candidatePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    const QString content = QString::fromUtf8(file.readAll());
    int matchedLine = 0;
    if (!findGoogleTestMacroLocation(content, suiteCandidates, testCandidates,
                                     &matchedLine)) {
      continue;
    }

    *filePath = info.absoluteFilePath();
    *line = matchedLine;
    return true;
  }

  return false;
}

void TestPanel::applyFilter() { applySearchFilter(); }

void TestPanel::refreshConfigurations() {
  const QString previousId = m_configCombo->currentData().toString();
  const QString previousName = m_configCombo->currentText();
  const QString defaultId =
      TestConfigurationManager::instance().defaultConfigurationId();
  const QString defaultName =
      TestConfigurationManager::instance().defaultConfigurationName();

  QSignalBlocker blocker(m_configCombo);
  m_configCombo->clear();
  auto configs = TestConfigurationManager::instance().allConfigurations();
  for (const TestConfiguration &cfg : configs)
    m_configCombo->addItem(cfg.name, cfg.id);

  int idx = -1;
  if (!previousId.isEmpty())
    idx = m_configCombo->findData(previousId);
  if (idx < 0 && !previousName.isEmpty())
    idx = m_configCombo->findText(previousName);
  if (idx < 0 && !defaultId.isEmpty())
    idx = m_configCombo->findData(defaultId);
  if (idx < 0 && !defaultName.isEmpty())
    idx = m_configCombo->findText(defaultName);
  if (idx < 0 && m_configCombo->count() > 0)
    idx = 0;
  if (idx >= 0)
    m_configCombo->setCurrentIndex(idx);

  syncAutoRunConfiguration();
  updateDiscoveryAdapterForConfig();
}

TestConfiguration TestPanel::currentConfiguration() const {
  const QString selectedId = m_configCombo->currentData().toString();
  if (!selectedId.isEmpty()) {
    const TestConfiguration cfg =
        TestConfigurationManager::instance().configurationById(selectedId);
    if (cfg.isValid())
      return cfg;
  }

  return TestConfigurationManager::instance().configurationByName(
      m_configCombo->currentText());
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
  if (!config.isValid()) {
    if (m_discoveryAdapter && m_ownsDiscoveryAdapter) {
      disconnect(m_discoveryAdapter, nullptr, this, nullptr);
      delete m_discoveryAdapter;
      m_discoveryAdapter = nullptr;
      m_ownsDiscoveryAdapter = false;
    }
    return;
  }

  if (m_discoveryAdapter && !m_ownsDiscoveryAdapter)
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

QString TestPanel::discoveryRootForConfiguration(
    const TestConfiguration &config) const {
  QString discoveryRoot;
  if (!config.discoveryDirectory.isEmpty()) {
    discoveryRoot = TestConfigurationManager::substituteVariables(
        config.discoveryDirectory, QString(), m_workspaceFolder);
  } else if (m_discoveryAdapter && m_discoveryAdapter->adapterId() == "ctest" &&
             !m_workspaceFolder.isEmpty()) {
    discoveryRoot = QDir(m_workspaceFolder).absoluteFilePath("build");
  } else {
    discoveryRoot = m_workspaceFolder;
  }

  if (!discoveryRoot.isEmpty() && QFileInfo(discoveryRoot).isRelative() &&
      !m_workspaceFolder.isEmpty()) {
    discoveryRoot = QDir(m_workspaceFolder).absoluteFilePath(discoveryRoot);
  }

  return discoveryRoot;
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

  m_discoveryAdapter->discover(
      discoveryRootForConfiguration(currentConfiguration()));
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
  m_discoveredTests.clear();

  for (const DiscoveredTest &test : tests) {
    m_discoveredTests[test.id] = test;
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
  settings.setValue("lastConfigurationId", m_configCombo->currentData());
  settings.setValue("lastConfiguration", m_configCombo->currentText());
  settings.setValue("lastFilter", m_filterCombo->currentIndex());
  settings.endGroup();
}

void TestPanel::restoreState() {
  QSettings settings;
  settings.beginGroup("TestPanel");
  QString lastConfigId = settings.value("lastConfigurationId").toString();
  QString lastConfig = settings.value("lastConfiguration").toString();
  int lastFilter = settings.value("lastFilter", 0).toInt();
  settings.endGroup();

  int idx = -1;
  if (!lastConfigId.isEmpty())
    idx = m_configCombo->findData(lastConfigId);
  if (idx < 0 && !lastConfig.isEmpty())
    idx = m_configCombo->findText(lastConfig);
  if (idx >= 0)
    m_configCombo->setCurrentIndex(idx);
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

    if (visible && !searchText.isEmpty()) {
      visible = item->text(0).contains(searchText, Qt::CaseInsensitive);
    }

    item->setHidden(!visible);
  }

  for (auto it = m_suiteItems.begin(); it != m_suiteItems.end(); ++it) {
    QTreeWidgetItem *suite = it.value();
    bool hasVisible = false;

    if (!searchText.isEmpty() &&
        suite->text(0).contains(searchText, Qt::CaseInsensitive)) {

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
  bool hasItems = m_tree->topLevelItemCount() > 0 || !m_testItems.isEmpty();
  if (!hasItems) {
    if (m_runManager && m_runManager->isRunning()) {
      m_emptyStateLabel->setText(
          tr("Running tests...\nSee the details pane below for the active "
             "command and live output."));
    } else if (m_hasRunContext) {
      m_emptyStateLabel->setText(
          tr("No tests were reported for the last run.\nSee the details pane "
             "below for the executed command, working directory, and raw "
             "output."));
    } else {
      m_emptyStateLabel->setText(
          tr("No tests discovered yet.\nUse \"Discover\" to find tests or "
             "\"Run All\" to execute them."));
    }
  }
  m_emptyStateLabel->setVisible(!hasItems);
  m_tree->setVisible(hasItems);
}
