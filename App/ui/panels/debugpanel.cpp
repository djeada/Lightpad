#include "debugpanel.h"
#include "../../core/logging/logger.h"
#include "../uistylehelper.h"

#include <QAction>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QStyle>
#include <QTabBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

namespace {
constexpr int MAX_DEBUG_CONSOLE_BLOCKS = 2000;
constexpr int MAX_DEBUG_CONSOLE_ENTRY_CHARS = 8192;
constexpr int MAX_EAGER_SCOPE_LOADS = 1;
constexpr int MAX_STACK_FRAMES_PER_REFRESH = 64;

void applyTreePalette(QTreeWidget *tree, const Theme &theme) {
  if (!tree) {
    return;
  }

  QPalette palette = tree->palette();
  palette.setColor(QPalette::Base, theme.backgroundColor);
  palette.setColor(QPalette::AlternateBase, theme.surfaceAltColor);
  palette.setColor(QPalette::Text, theme.foregroundColor);
  palette.setColor(QPalette::Highlight, theme.accentSoftColor);
  palette.setColor(QPalette::HighlightedText, theme.foregroundColor);
  tree->setPalette(palette);
}

QList<QPair<QString, QString>> parseInfoLocalsOutput(const QString &raw) {
  QList<QPair<QString, QString>> entries;
  QString currentName;
  QString currentValue;

  const auto flush = [&]() {
    if (!currentName.isEmpty()) {
      entries.append({currentName, currentValue.trimmed()});
      currentName.clear();
      currentValue.clear();
    }
  };

  const QStringList lines = raw.split('\n');
  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
      continue;
    }

    const int eqPos = line.indexOf('=');
    if (eqPos > 0) {
      const QString candidateName = line.left(eqPos).trimmed();
      if (!candidateName.isEmpty() && !candidateName.contains(' ')) {
        flush();
        currentName = candidateName;
        currentValue = line.mid(eqPos + 1).trimmed();
        continue;
      }
    }

    if (!currentName.isEmpty()) {
      if (!currentValue.isEmpty()) {
        currentValue += " ";
      }
      currentValue += trimmed;
    }
  }

  flush();
  return entries;
}
} // namespace

DebugPanel::DebugPanel(QWidget *parent)
    : QWidget(parent), m_dapClient(nullptr), m_debugStatusLabel(nullptr),
      m_currentThreadId(0), m_currentFrameId(0),
      m_programmaticVariablesExpand(false),
      m_variablesNameColumnAutofitPending(false), m_stepInProgress(false),
      m_expectStopEvent(true), m_hasLastStopEvent(false),
      m_lastStoppedThreadId(0), m_lastStoppedReason(DapStoppedReason::Unknown),
      m_localsFallbackPending(false), m_localsFallbackFrameId(-1),
      m_localsFallbackScopeRef(0), m_localsFallbackRequestNonce(0),
      m_themeInitialized(false) {
  setupUI();
  updateToolbarState();

  connect(&BreakpointManager::instance(), &BreakpointManager::breakpointAdded,
          this, &DebugPanel::refreshBreakpointList);
  connect(&BreakpointManager::instance(), &BreakpointManager::breakpointRemoved,
          this, [this](int, const QString &, int) { refreshBreakpointList(); });
  connect(&BreakpointManager::instance(), &BreakpointManager::breakpointChanged,
          this, &DebugPanel::refreshBreakpointList);
  connect(&BreakpointManager::instance(),
          &BreakpointManager::allBreakpointsCleared, this,
          &DebugPanel::refreshBreakpointList);

  refreshBreakpointList();

  connect(&WatchManager::instance(), &WatchManager::watchAdded, this,
          &DebugPanel::onWatchAdded);
  connect(&WatchManager::instance(), &WatchManager::watchRemoved, this,
          &DebugPanel::onWatchRemoved);
  connect(&WatchManager::instance(), &WatchManager::watchUpdated, this,
          &DebugPanel::onWatchUpdated);
  connect(&WatchManager::instance(), &WatchManager::watchChildrenReceived, this,
          &DebugPanel::onWatchChildrenReceived);

  for (const WatchExpression &w : WatchManager::instance().allWatches()) {
    onWatchAdded(w);
  }
}

DebugPanel::~DebugPanel() {}

void DebugPanel::applyTheme(const Theme &theme) {
  m_theme = theme;
  m_themeInitialized = true;

  setStyleSheet(QString("QWidget#debugPanel {"
                        "  background: %1;"
                        "  color: %2;"
                        "}")
                    .arg(theme.backgroundColor.name())
                    .arg(theme.foregroundColor.name()));

  const QString treeStyle = UIStyleHelper::treeWidgetStyle(theme);
  for (QTreeWidget *tree :
       {m_callStackTree, m_variablesTree, m_watchTree, m_breakpointsTree}) {
    if (!tree) {
      continue;
    }
    tree->setStyleSheet(treeStyle);
    applyTreePalette(tree, theme);
  }

  if (m_toolbar) {
    m_toolbar->setStyleSheet(QString("QToolBar {"
                                     "  background: %1;"
                                     "  border-bottom: 1px solid %2;"
                                     "  spacing: 4px;"
                                     "  padding: 2px 4px;"
                                     "}"
                                     "QToolButton {"
                                     "  color: %3;"
                                     "  background: %7;"
                                     "  border: 1px solid %2;"
                                     "  border-radius: 5px;"
                                     "  padding: 5px 9px;"
                                     "  margin: 0 1px;"
                                     "  font-weight: 600;"
                                     "  qproperty-cursor: PointingHandCursor;"
                                     "}"
                                     "QToolButton:hover {"
                                     "  background: %4;"
                                     "  border-color: %8;"
                                     "}"
                                     "QToolButton:pressed {"
                                     "  background: %5;"
                                     "}"
                                     "QToolButton:disabled {"
                                     "  color: %6;"
                                     "  background: %1;"
                                     "  border-color: %2;"
                                     "}"
                                     "QComboBox {"
                                     "  min-height: 24px;"
                                     "  padding: 2px 8px;"
                                     "  border: 1px solid %2;"
                                     "  border-radius: 4px;"
                                     "}"
                                     "QLabel#debugStatusLabel {"
                                     "  color: %3;"
                                     "  padding-left: 8px;"
                                     "  font-weight: 600;"
                                     "}")
                                 .arg(theme.surfaceColor.name())
                                 .arg(theme.borderColor.name())
                                 .arg(theme.foregroundColor.name())
                                 .arg(theme.hoverColor.name())
                                 .arg(theme.pressedColor.name())
                                 .arg(theme.singleLineCommentFormat.name())
                                 .arg(theme.surfaceAltColor.name())
                                 .arg(theme.accentColor.name()));
  }

  if (m_tabWidget) {
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setUsesScrollButtons(true);
    if (m_tabWidget->tabBar()) {
      m_tabWidget->tabBar()->setExpanding(false);
      m_tabWidget->tabBar()->setElideMode(Qt::ElideRight);
    }
    m_tabWidget->setStyleSheet(QString("QTabWidget::pane {"
                                       "  border: 1px solid %1;"
                                       "  background: %2;"
                                       "  border-radius: 6px;"
                                       "  top: -1px;"
                                       "}"
                                       "QTabBar::tab {"
                                       "  background: %3;"
                                       "  color: %4;"
                                       "  border: 1px solid %1;"
                                       "  border-bottom: none;"
                                       "  border-top-left-radius: 5px;"
                                       "  border-top-right-radius: 5px;"
                                       "  padding: 7px 11px;"
                                       "  margin-right: 2px;"
                                       "}"
                                       "QTabBar::tab:selected {"
                                       "  background: %2;"
                                       "  color: %5;"
                                       "  border-color: %6;"
                                       "}"
                                       "QTabBar::tab:hover {"
                                       "  background: %6;"
                                       "}")
                                   .arg(theme.borderColor.name())
                                   .arg(theme.backgroundColor.name())
                                   .arg(theme.surfaceColor.name())
                                   .arg(theme.singleLineCommentFormat.name())
                                   .arg(theme.foregroundColor.name())
                                   .arg(theme.hoverColor.name()));
  }

  if (m_mainSplitter) {
    m_mainSplitter->setStyleSheet(QString("QSplitter::handle {"
                                          "  background: %1;"
                                          "}"
                                          "QSplitter::handle:hover {"
                                          "  background: %2;"
                                          "}")
                                      .arg(theme.borderColor.name())
                                      .arg(theme.accentColor.name()));
  }

  if (m_threadSelector) {
    m_threadSelector->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }
  if (m_watchInput) {
    m_watchInput->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  }
  if (m_consoleInput) {
    m_consoleInput->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  }

  if (m_consoleOutput) {
    m_consoleOutput->setStyleSheet(QString("QTextEdit {"
                                           "  background: %1;"
                                           "  color: %2;"
                                           "  border: 1px solid %3;"
                                           "  border-radius: 4px;"
                                           "  selection-background-color: %4;"
                                           "  selection-color: %2;"
                                           "}")
                                       .arg(theme.backgroundColor.name())
                                       .arg(theme.foregroundColor.name())
                                       .arg(theme.borderColor.name())
                                       .arg(theme.accentSoftColor.name()));

    QPalette consolePalette = m_consoleOutput->palette();
    consolePalette.setColor(QPalette::Base, theme.backgroundColor);
    consolePalette.setColor(QPalette::Text, theme.foregroundColor);
    consolePalette.setColor(QPalette::Highlight, theme.accentSoftColor);
    consolePalette.setColor(QPalette::HighlightedText, theme.foregroundColor);
    m_consoleOutput->setPalette(consolePalette);
  }
}

void DebugPanel::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  setupToolbar();
  mainLayout->addWidget(m_toolbar);

  m_mainSplitter = new QSplitter(Qt::Vertical, this);
  m_mainSplitter->setChildrenCollapsible(false);
  m_mainSplitter->setHandleWidth(5);

  m_tabWidget = new QTabWidget(this);

  setupVariables();
  setupWatches();
  setupCallStack();
  setupBreakpoints();

  m_tabWidget->addTab(m_variablesTree, tr("Variables"));
  m_tabWidget->addTab(m_watchTree->parentWidget(), tr("Watch"));
  m_tabWidget->addTab(m_callStackTree, tr("Call Stack"));
  m_tabWidget->addTab(m_breakpointsTree, tr("Breakpoints"));

  m_mainSplitter->addWidget(m_tabWidget);

  QWidget *consoleWidget = new QWidget(this);
  QVBoxLayout *consoleLayout = new QVBoxLayout(consoleWidget);
  consoleLayout->setContentsMargins(0, 0, 0, 0);
  consoleLayout->setSpacing(2);

  setupConsole();
  consoleLayout->addWidget(m_consoleOutput);
  consoleLayout->addWidget(m_consoleInput);

  m_mainSplitter->addWidget(consoleWidget);
  m_mainSplitter->setSizes({360, 140});

  mainLayout->addWidget(m_mainSplitter);
}

void DebugPanel::setupToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setIconSize(QSize(16, 16));
  m_toolbar->setMovable(false);
  m_toolbar->setFloatable(false);
  m_toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
  m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  const auto configureAction = [this](QAction *action, const QString &toolTip,
                                      const QString &helpText) {
    if (!action) {
      return;
    }
    action->setToolTip(toolTip);
    action->setStatusTip(toolTip);
    action->setWhatsThis(helpText);
    if (QWidget *button = m_toolbar->widgetForAction(action)) {
      button->setCursor(Qt::PointingHandCursor);
    }
  };

  m_continueAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_MediaPlay), tr("Start"));
  m_continueAction->setShortcut(QKeySequence(Qt::Key_F5));
  configureAction(m_continueAction, tr("Start or continue debugging (F5)"),
                  tr("Starts a debug session when idle, or continues execution "
                     "when paused."));
  connect(m_continueAction, &QAction::triggered, this, &DebugPanel::onContinue);

  m_pauseAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_MediaPause), tr("Pause"));
  m_pauseAction->setShortcut(QKeySequence(Qt::Key_F6));
  configureAction(
      m_pauseAction, tr("Pause execution (F6)"),
      tr("Interrupts a running debug session at the next safe point."));
  connect(m_pauseAction, &QAction::triggered, this, &DebugPanel::onPause);

  m_toolbar->addSeparator();

  m_stepOverAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_ArrowRight), tr("Over"));
  m_stepOverAction->setShortcut(QKeySequence(Qt::Key_F10));
  configureAction(
      m_stepOverAction, tr("Step over current line (F10)"),
      tr("Executes the current line without entering called functions."));
  connect(m_stepOverAction, &QAction::triggered, this, &DebugPanel::onStepOver);

  m_stepIntoAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_ArrowDown), tr("Into"));
  m_stepIntoAction->setShortcut(QKeySequence(Qt::Key_F11));
  configureAction(
      m_stepIntoAction, tr("Step into function call (F11)"),
      tr("Advances into the function being called on the current line."));
  connect(m_stepIntoAction, &QAction::triggered, this, &DebugPanel::onStepInto);

  m_stepOutAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_ArrowUp), tr("Out"));
  m_stepOutAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F11));
  configureAction(m_stepOutAction,
                  tr("Step out of current function (Shift+F11)"),
                  tr("Runs until the current function returns to its caller."));
  connect(m_stepOutAction, &QAction::triggered, this, &DebugPanel::onStepOut);

  m_toolbar->addSeparator();

  m_restartAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_BrowserReload), tr("Restart"));
  m_restartAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5));
  configureAction(m_restartAction, tr("Restart debugging (Ctrl+Shift+F5)"),
                  tr("Stops and relaunches the current debug session."));
  connect(m_restartAction, &QAction::triggered, this, &DebugPanel::onRestart);

  m_stopAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_MediaStop), tr("Stop"));
  m_stopAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
  configureAction(
      m_stopAction, tr("Stop debugging (Shift+F5)"),
      tr("Terminates debugging and clears the current debug context."));
  connect(m_stopAction, &QAction::triggered, this, &DebugPanel::onStop);

  m_toolbar->addSeparator();

  m_threadSelector = new QComboBox(this);
  m_threadSelector->setToolTip(tr("Select active thread"));
  m_threadSelector->setStatusTip(tr("Select active thread"));
  m_threadSelector->setMinimumWidth(150);
  m_threadSelector->setEnabled(false);
  m_threadSelector->setCursor(Qt::PointingHandCursor);
  connect(m_threadSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &DebugPanel::onThreadSelected);
  m_toolbar->addWidget(m_threadSelector);

  m_toolbar->addSeparator();

  m_debugStatusLabel = new QLabel(tr("Ready: press Start (F5)"), this);
  m_debugStatusLabel->setObjectName("debugStatusLabel");
  m_debugStatusLabel->setMinimumWidth(260);
  m_debugStatusLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_toolbar->addWidget(m_debugStatusLabel);
}

void DebugPanel::setupCallStack() {
  m_callStackTree = new QTreeWidget(this);
  m_callStackTree->setHeaderLabels({tr("Function"), tr("File"), tr("Line")});
  m_callStackTree->setRootIsDecorated(false);
  m_callStackTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_callStackTree->setAlternatingRowColors(true);
  m_callStackTree->setUniformRowHeights(true);
  m_callStackTree->setAllColumnsShowFocus(true);

  m_callStackTree->header()->setStretchLastSection(false);
  m_callStackTree->header()->setHighlightSections(false);
  m_callStackTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_callStackTree->header()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_callStackTree->header()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);

  connect(m_callStackTree, &QTreeWidget::itemClicked, this,
          &DebugPanel::onCallStackItemClicked);
  connect(m_callStackTree, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
            if (current) {
              onCallStackItemClicked(current, 0);
            }
          });
}

void DebugPanel::setupVariables() {
  m_variablesTree = new QTreeWidget(this);
  m_variablesTree->setHeaderLabels({tr("Name"), tr("Value"), tr("Type")});
  m_variablesTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_variablesTree->setAlternatingRowColors(true);
  m_variablesTree->setUniformRowHeights(true);
  m_variablesTree->setAllColumnsShowFocus(true);
  m_variablesTree->setIndentation(14);

  m_variablesTree->header()->setStretchLastSection(false);
  m_variablesTree->header()->setHighlightSections(false);
  m_variablesTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_variablesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_variablesTree->header()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);

  connect(m_variablesTree, &QTreeWidget::itemExpanded, this,
          &DebugPanel::onVariableItemExpanded);
}

void DebugPanel::setupWatches() {

  QWidget *watchContainer = new QWidget(this);
  QVBoxLayout *watchLayout = new QVBoxLayout(watchContainer);
  watchLayout->setContentsMargins(0, 0, 0, 0);
  watchLayout->setSpacing(2);

  m_watchTree = new QTreeWidget(watchContainer);
  m_watchTree->setHeaderLabels({tr("Expression"), tr("Value"), tr("Type")});
  m_watchTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_watchTree->setAlternatingRowColors(true);
  m_watchTree->setRootIsDecorated(true);
  m_watchTree->setUniformRowHeights(true);
  m_watchTree->setAllColumnsShowFocus(true);
  m_watchTree->setIndentation(14);

  m_watchTree->header()->setStretchLastSection(false);
  m_watchTree->header()->setHighlightSections(false);
  m_watchTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_watchTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_watchTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

  connect(m_watchTree, &QTreeWidget::itemExpanded, this,
          &DebugPanel::onWatchItemExpanded);

  m_watchTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_watchTree, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            QTreeWidgetItem *item = m_watchTree->itemAt(pos);
            if (!item || item->parent())
              return;

            QMenu menu;
            QAction *removeAction = menu.addAction(tr("Remove Watch"));
            QAction *chosen = menu.exec(m_watchTree->mapToGlobal(pos));
            if (chosen == removeAction) {
              int watchId = item->data(0, Qt::UserRole).toInt();
              WatchManager::instance().removeWatch(watchId);
            }
          });

  watchLayout->addWidget(m_watchTree);

  QHBoxLayout *inputLayout = new QHBoxLayout();
  inputLayout->setContentsMargins(2, 0, 2, 2);

  m_watchInput = new QLineEdit(watchContainer);
  m_watchInput->setPlaceholderText(tr("Add watch expression..."));
  m_watchInput->setClearButtonEnabled(true);
  connect(m_watchInput, &QLineEdit::returnPressed, this,
          &DebugPanel::onAddWatch);

  inputLayout->addWidget(m_watchInput);
  watchLayout->addLayout(inputLayout);
}

void DebugPanel::setupBreakpoints() {
  m_breakpointsTree = new QTreeWidget(this);
  m_breakpointsTree->setHeaderLabels({tr(""), tr("Location"), tr("Condition")});
  m_breakpointsTree->setRootIsDecorated(false);
  m_breakpointsTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_breakpointsTree->setAlternatingRowColors(true);
  m_breakpointsTree->setUniformRowHeights(true);
  m_breakpointsTree->setAllColumnsShowFocus(true);

  m_breakpointsTree->header()->setStretchLastSection(true);
  m_breakpointsTree->header()->setHighlightSections(false);
  m_breakpointsTree->header()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  m_breakpointsTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

  connect(m_breakpointsTree, &QTreeWidget::itemDoubleClicked, this,
          &DebugPanel::onBreakpointItemDoubleClicked);
}

void DebugPanel::setupConsole() {
  m_consoleOutput = new QTextEdit(this);
  m_consoleOutput->setReadOnly(true);
  m_consoleOutput->setUndoRedoEnabled(false);
  m_consoleOutput->document()->setMaximumBlockCount(MAX_DEBUG_CONSOLE_BLOCKS);
  QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  fixedFont.setPointSize(9);
  m_consoleOutput->setFont(fixedFont);
  m_consoleOutput->setPlaceholderText(tr("Debug console output..."));

  m_consoleInput = new QLineEdit(this);
  m_consoleInput->setFont(fixedFont);
  m_consoleInput->setPlaceholderText(tr("Evaluate expression..."));
  m_consoleInput->setClearButtonEnabled(true);

  connect(m_consoleInput, &QLineEdit::returnPressed, this,
          &DebugPanel::onConsoleInput);
}

void DebugPanel::setDapClient(DapClient *client) {
  if (m_dapClient) {
    disconnect(m_dapClient, nullptr, this, nullptr);
  }

  m_dapClient = client;
  m_expectStopEvent = true;
  m_hasLastStopEvent = false;
  m_lastStoppedThreadId = 0;
  m_lastStoppedReason = DapStoppedReason::Unknown;
  m_pendingConsoleEvaluations.clear();

  if (m_dapClient) {
    connect(m_dapClient, &DapClient::stateChanged, this,
            &DebugPanel::updateToolbarState);
    connect(m_dapClient, &DapClient::stopped, this, &DebugPanel::onStopped);
    connect(m_dapClient, &DapClient::continued, this,
            [this](int, bool) { onContinued(); });
    connect(m_dapClient, &DapClient::terminated, this,
            &DebugPanel::onTerminated);
    connect(m_dapClient, &DapClient::threadsReceived, this,
            &DebugPanel::onThreadsReceived);
    connect(m_dapClient, &DapClient::stackTraceReceived, this,
            &DebugPanel::onStackTraceReceived);
    connect(m_dapClient, &DapClient::scopesReceived, this,
            &DebugPanel::onScopesReceived);
    connect(m_dapClient, &DapClient::variablesReceived, this,
            &DebugPanel::onVariablesReceived);
    connect(m_dapClient, &DapClient::output, this,
            &DebugPanel::onOutputReceived);
    connect(m_dapClient, &DapClient::evaluateResult, this,
            &DebugPanel::onEvaluateResult);
    connect(m_dapClient, &DapClient::evaluateError, this,
            &DebugPanel::onEvaluateError);

    WatchManager::instance().setDapClient(m_dapClient);
  }

  updateToolbarState();
}

void DebugPanel::clearAll() {
  m_callStackTree->clear();
  m_variablesTree->clear();
  m_variableRefToItem.clear();
  m_pendingScopeVariableLoads.clear();
  m_pendingVariableRequests.clear();
  clearLocalsFallbackState();
  m_programmaticVariablesExpand = false;
  m_variablesNameColumnAutofitPending = false;
  m_stepInProgress = false;
  m_expectStopEvent = true;
  m_hasLastStopEvent = false;
  m_lastStoppedThreadId = 0;
  m_lastStoppedReason = DapStoppedReason::Unknown;
  m_pendingConsoleEvaluations.clear();
  m_consoleOutput->clear();
  m_threads.clear();
  m_stackFrames.clear();
  m_currentThreadId = 0;
  m_currentFrameId = 0;
  m_threadSelector->clear();
  m_threadSelector->setEnabled(false);
}

void DebugPanel::setCurrentFrame(int frameId) {
  m_currentFrameId = frameId;

  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    m_dapClient->getScopes(frameId);
  }
}

void DebugPanel::onStopped(const DapStoppedEvent &event) {
  if (!m_expectStopEvent && event.allThreadsStopped) {
    LOG_DEBUG("DebugPanel: Ignoring redundant allThreadsStopped event");
    return;
  }

  const int eventThreadId =
      event.threadId > 0 ? event.threadId : m_currentThreadId;
  const bool duplicateStop = !m_expectStopEvent && m_hasLastStopEvent &&
                             eventThreadId == m_lastStoppedThreadId &&
                             event.reason == m_lastStoppedReason;
  if (duplicateStop) {
    LOG_DEBUG("DebugPanel: Ignoring duplicate stopped event");
    return;
  }

  m_expectStopEvent = false;
  m_hasLastStopEvent = true;
  m_lastStoppedThreadId = eventThreadId;
  m_lastStoppedReason = event.reason;
  m_stepInProgress = false;

  if (event.threadId > 0) {
    m_currentThreadId = event.threadId;
  }

  QString reasonText;
  switch (event.reason) {
  case DapStoppedReason::Breakpoint:
    reasonText = tr("Breakpoint hit");
    break;
  case DapStoppedReason::Step:
    reasonText = tr("Step completed");
    break;
  case DapStoppedReason::Exception:
    reasonText = tr("Exception: %1").arg(event.description);
    break;
  case DapStoppedReason::Pause:
    reasonText = tr("Paused");
    break;
  case DapStoppedReason::Entry:
    reasonText = tr("Entry point");
    break;
  default:
    reasonText = tr("Stopped");
  }

  appendConsoleLine(reasonText, consoleInfoColor());

  m_variablesTree->clear();
  m_variableRefToItem.clear();
  m_pendingScopeVariableLoads.clear();
  m_pendingVariableRequests.clear();
  clearLocalsFallbackState();

  if (m_dapClient) {
    const bool canFastRefreshOnStep =
        event.reason == DapStoppedReason::Step && m_currentThreadId > 0;
    if (canFastRefreshOnStep) {
      m_dapClient->getStackTrace(m_currentThreadId, 0,
                                 MAX_STACK_FRAMES_PER_REFRESH);
    } else {
      m_dapClient->getThreads();
    }
  }

  updateToolbarState();
}

void DebugPanel::onContinued() {
  m_stepInProgress = false;
  m_variablesTree->clear();
  m_variableRefToItem.clear();
  m_pendingScopeVariableLoads.clear();
  m_pendingVariableRequests.clear();
  clearLocalsFallbackState();
  updateToolbarState();
}

void DebugPanel::onTerminated() {
  clearAll();
  appendConsoleLine(tr("Debug session ended."), consoleMutedColor());
  updateToolbarState();
}

void DebugPanel::onContinue() {
  if (m_stepInProgress)
    return;
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    m_expectStopEvent = true;
    m_stepInProgress = true;
    updateToolbarState();
    m_dapClient->continueExecution(activeThreadId());
  } else {
    emit startDebugRequested();
  }
}

void DebugPanel::onPause() {
  if (m_dapClient && m_dapClient->state() == DapClient::State::Running) {
    m_expectStopEvent = true;
    m_dapClient->pause(activeThreadId());
  }
}

void DebugPanel::onStepOver() {
  if (m_stepInProgress)
    return;
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    const int threadId = activeThreadId();
    if (threadId > 0) {
      m_expectStopEvent = true;
      m_stepInProgress = true;
      updateToolbarState();
      m_dapClient->stepOver(threadId);
    }
  }
}

void DebugPanel::onStepInto() {
  if (m_stepInProgress)
    return;
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    const int threadId = activeThreadId();
    if (threadId > 0) {
      m_expectStopEvent = true;
      m_stepInProgress = true;
      updateToolbarState();
      m_dapClient->stepInto(threadId);
    }
  }
}

void DebugPanel::onStepOut() {
  if (m_stepInProgress)
    return;
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    const int threadId = activeThreadId();
    if (threadId > 0) {
      m_expectStopEvent = true;
      m_stepInProgress = true;
      updateToolbarState();
      m_dapClient->stepOut(threadId);
    }
  }
}

void DebugPanel::onRestart() {
  emit restartDebugRequested();
}

void DebugPanel::onStop() {
  if (m_dapClient && m_dapClient->isDebugging()) {
    appendConsoleLine(tr("Stopping debug session..."), consoleMutedColor());
  }
  emit stopDebugRequested();
}

void DebugPanel::onThreadsReceived(const QList<DapThread> &threads) {
  m_threads = threads;

  bool hasCurrentThread = false;

  m_threadSelector->blockSignals(true);
  m_threadSelector->clear();
  for (const DapThread &thread : threads) {
    m_threadSelector->addItem(
        QString("Thread %1: %2").arg(thread.id).arg(thread.name), thread.id);
    if (thread.id == m_currentThreadId) {
      m_threadSelector->setCurrentIndex(m_threadSelector->count() - 1);
      hasCurrentThread = true;
    }
  }

  if (!threads.isEmpty() && !hasCurrentThread) {
    m_currentThreadId = threads.first().id;
    m_threadSelector->setCurrentIndex(0);
    hasCurrentThread = true;
  }

  m_threadSelector->setEnabled(!threads.isEmpty());
  m_threadSelector->blockSignals(false);

  if (m_dapClient && hasCurrentThread) {
    m_dapClient->getStackTrace(m_currentThreadId, 0,
                               MAX_STACK_FRAMES_PER_REFRESH);
  }
  updateToolbarState();
}

void DebugPanel::onStackTraceReceived(int threadId,
                                      const QList<DapStackFrame> &frames,
                                      int totalFrames) {
  Q_UNUSED(totalFrames);

  if (threadId != m_currentThreadId) {
    return;
  }

  m_stackFrames = frames;
  m_callStackTree->clear();

  for (const DapStackFrame &frame : frames) {
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, frame.name);
    item->setText(1, frame.source.name);
    item->setText(2, QString::number(frame.line));
    item->setData(0, Qt::UserRole, frame.id);
    item->setData(0, Qt::UserRole + 1, frame.source.path);
    item->setData(0, Qt::UserRole + 2, frame.line);
    item->setData(0, Qt::UserRole + 3, frame.column);

    if (frame.presentationHint == "subtle") {
      item->setForeground(0, Qt::gray);
    }

    m_callStackTree->addTopLevelItem(item);
  }

  if (!frames.isEmpty()) {
    int activeIndex = 0;
    for (int i = 0; i < frames.size(); ++i) {
      const DapStackFrame &frame = frames.at(i);
      if (frame.presentationHint != "subtle" && !frame.source.path.isEmpty()) {
        activeIndex = i;
        break;
      }
    }

    const DapStackFrame &activeFrame = frames.at(activeIndex);

    m_callStackTree->blockSignals(true);
    m_callStackTree->setCurrentItem(m_callStackTree->topLevelItem(activeIndex));
    m_callStackTree->blockSignals(false);
    setCurrentFrame(activeFrame.id);

    if (!activeFrame.source.path.isEmpty()) {
      emit locationClicked(activeFrame.source.path, activeFrame.line,
                           activeFrame.column);
    }

    WatchManager::instance().evaluateAll(activeFrame.id);
  }
}

void DebugPanel::onScopesReceived(int frameId, const QList<DapScope> &scopes) {

  if (frameId != m_currentFrameId) {
    return;
  }

  m_variablesTree->clear();
  m_variableRefToItem.clear();
  m_pendingScopeVariableLoads.clear();
  m_pendingVariableRequests.clear();
  clearLocalsFallbackState();
  m_variablesNameColumnAutofitPending = true;

  QList<int> eagerScopeRefs;
  for (const DapScope &scope : scopes) {
    if (scope.variablesReference <= 0 || scope.expensive) {
      continue;
    }
    const QString lowered = scope.name.trimmed().toLower();
    if (lowered.contains("register") || lowered.contains("local")) {
      continue;
    }

    if (lowered.contains("argument") || lowered == QLatin1String("args")) {
      eagerScopeRefs.append(scope.variablesReference);
      break;
    }
  }
  if (eagerScopeRefs.isEmpty()) {
    for (const DapScope &scope : scopes) {
      if (scope.variablesReference <= 0 || scope.expensive) {
        continue;
      }
      const QString lowered = scope.name.trimmed().toLower();
      if (lowered.contains("register") || lowered.contains("local")) {
        continue;
      }
      eagerScopeRefs.append(scope.variablesReference);
      break;
    }
  }
  int eagerLoadsRemaining = MAX_EAGER_SCOPE_LOADS;
  const bool preferLocalsFallback = hasLocalsFallbackCommand();

  for (const DapScope &scope : scopes) {
    const QString lowered = scope.name.trimmed().toLower();
    const bool registerScope = lowered.contains("register");
    const bool localScope = lowered.contains("local");
    const bool autoLoadScope =
        eagerScopeRefs.contains(scope.variablesReference) &&
        eagerLoadsRemaining > 0;
    const bool shouldLoadLocals =
        localScope && !scope.expensive && !registerScope &&
        !preferLocalsFallback;
    const bool shouldLoadScopeVariables =
        shouldLoadLocals ||
        (!scope.expensive && !registerScope && !localScope && autoLoadScope);
    const bool deferScope = !shouldLoadScopeVariables;

    QTreeWidgetItem *scopeItem = new QTreeWidgetItem();
    scopeItem->setText(0, scope.name);
    scopeItem->setData(0, Qt::UserRole, scope.variablesReference);
    scopeItem->setFirstColumnSpanned(true);

    QFont font = scopeItem->font(0);
    font.setBold(true);
    scopeItem->setFont(0, font);

    if (scope.variablesReference > 0 && (localScope || shouldLoadScopeVariables)) {
      scopeItem->setExpanded(true);
    }

    m_variablesTree->addTopLevelItem(scopeItem);

    if (scope.variablesReference > 0) {
      if (localScope && preferLocalsFallback && !scope.expensive &&
          !registerScope) {
        m_variableRefToItem[scope.variablesReference] = scopeItem;
        scopeItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        requestLocalsFallback(scope.variablesReference);
      } else if (shouldLoadScopeVariables) {
        m_variableRefToItem[scope.variablesReference] = scopeItem;
        m_pendingScopeVariableLoads.insert(scope.variablesReference);
        m_pendingVariableRequests.insert(scope.variablesReference);
        m_dapClient->getVariables(scope.variablesReference);
        if (!localScope) {
          --eagerLoadsRemaining;
        }
      } else if (deferScope) {
        scopeItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
      }
    }
  }

  if (m_pendingScopeVariableLoads.isEmpty()) {
    resizeVariablesNameColumnOnce();
    m_variablesNameColumnAutofitPending = false;
  }
}

void DebugPanel::onVariablesReceived(int variablesReference,
                                     const QList<DapVariable> &variables) {
  m_pendingVariableRequests.remove(variablesReference);
  QTreeWidgetItem *parentItem = m_variableRefToItem.value(variablesReference);
  if (!parentItem) {
    return;
  }

  while (parentItem->childCount() > 0) {
    delete parentItem->takeChild(0);
  }

  for (const DapVariable &var : variables) {
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, var.name);
    item->setText(1, var.value);
    item->setText(2, var.type);
    item->setData(0, Qt::UserRole, var.variablesReference);
    item->setIcon(0, variableIcon(var));

    if (var.variablesReference > 0) {
      item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }

    parentItem->addChild(item);
  }

  bool requestedLocalFallback = false;
  if (variables.isEmpty() && !parentItem->parent()) {
    const QString scopeName = parentItem->text(0).trimmed().toLower();
    if (scopeName.contains(QLatin1String("local")) &&
        hasLocalsFallbackCommand()) {
      requestLocalsFallback(variablesReference);
      requestedLocalFallback = true;
    }
  }

  if (!requestedLocalFallback) {
    parentItem->setExpanded(true);
  }

  if (m_variablesNameColumnAutofitPending &&
      m_pendingScopeVariableLoads.remove(variablesReference) &&
      m_pendingScopeVariableLoads.isEmpty()) {
    m_variablesNameColumnAutofitPending = false;
    QTimer::singleShot(0, this, [this]() { resizeVariablesNameColumnOnce(); });
  }
}

void DebugPanel::requestLocalsFallback(int scopeVariablesReference) {
  if (scopeVariablesReference <= 0) {
    return;
  }

  QTreeWidgetItem *scopeItem =
      m_variableRefToItem.value(scopeVariablesReference);
  if (!scopeItem) {
    return;
  }

  if (!m_dapClient || m_dapClient->state() != DapClient::State::Stopped ||
      m_currentFrameId < 0) {
    showLocalsFallbackMessage(scopeVariablesReference,
                              tr("<locals unavailable in current state>"));
    return;
  }

  if (m_localsFallbackPending) {
    return;
  }

  while (scopeItem->childCount() > 0) {
    delete scopeItem->takeChild(0);
  }

  QTreeWidgetItem *loadingItem = new QTreeWidgetItem();
  loadingItem->setText(0, tr("<loading locals...>"));
  loadingItem->setFirstColumnSpanned(true);
  loadingItem->setForeground(0, consoleMutedColor());
  scopeItem->addChild(loadingItem);
  scopeItem->setExpanded(true);

  const DebugEvaluateRequest localsRequest =
      DebugExpressionTranslator::localsFallbackRequest(
          m_dapClient ? m_dapClient->adapterId() : QString(),
          m_dapClient ? m_dapClient->adapterType() : QString());
  const QString baseExpr = localsRequest.expression.trimmed();
  if (baseExpr.isEmpty()) {
    showLocalsFallbackMessage(scopeVariablesReference,
                              tr("<locals fallback unavailable for debugger>"));
    return;
  }

  const int padSpaces = (m_localsFallbackRequestNonce++ % 7) + 1;
  const QString requestExpr = baseExpr + QString(padSpaces, QLatin1Char(' '));
  const QString evalContext =
      localsRequest.context.isEmpty() ? QStringLiteral("repl")
                                      : localsRequest.context;

  m_localsFallbackPending = true;
  m_localsFallbackFrameId = m_currentFrameId;
  m_localsFallbackScopeRef = scopeVariablesReference;
  m_localsFallbackPendingExpression = requestExpr;
  m_dapClient->evaluate(requestExpr, m_currentFrameId, evalContext);
}

bool DebugPanel::hasLocalsFallbackCommand() const {
  if (!m_dapClient) {
    return false;
  }

  const DebugEvaluateRequest localsRequest =
      DebugExpressionTranslator::localsFallbackRequest(m_dapClient->adapterId(),
                                                       m_dapClient->adapterType());
  return !localsRequest.expression.trimmed().isEmpty();
}

void DebugPanel::populateLocalsFromGdbEvaluate(int scopeVariablesReference,
                                               const QString &rawResult) {
  QTreeWidgetItem *scopeItem =
      m_variableRefToItem.value(scopeVariablesReference);
  if (!scopeItem) {
    return;
  }

  while (scopeItem->childCount() > 0) {
    delete scopeItem->takeChild(0);
  }

  const QList<QPair<QString, QString>> entries =
      parseInfoLocalsOutput(rawResult);
  if (entries.isEmpty()) {
    showLocalsFallbackMessage(scopeVariablesReference,
                              tr("<no locals available at this location>"));
    return;
  }

  for (const auto &entry : entries) {
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, entry.first);
    item->setText(1, entry.second);
    item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    scopeItem->addChild(item);
  }

  scopeItem->setExpanded(true);
  QTimer::singleShot(0, this, [this]() { resizeVariablesNameColumnOnce(); });
}

void DebugPanel::showLocalsFallbackMessage(int scopeVariablesReference,
                                           const QString &message,
                                           bool isError) {
  QTreeWidgetItem *scopeItem =
      m_variableRefToItem.value(scopeVariablesReference);
  if (!scopeItem) {
    return;
  }

  while (scopeItem->childCount() > 0) {
    delete scopeItem->takeChild(0);
  }

  QTreeWidgetItem *hintItem = new QTreeWidgetItem();
  hintItem->setText(0, message);
  hintItem->setFirstColumnSpanned(true);
  hintItem->setForeground(0,
                          isError ? consoleErrorColor() : consoleMutedColor());
  scopeItem->addChild(hintItem);
  scopeItem->setExpanded(true);
}

void DebugPanel::clearLocalsFallbackState() {
  m_localsFallbackPending = false;
  m_localsFallbackFrameId = -1;
  m_localsFallbackScopeRef = 0;
  m_localsFallbackPendingExpression.clear();
}

void DebugPanel::onOutputReceived(const DapOutputEvent &event) {
  QColor color = palette().color(QPalette::Text);
  if (event.category == "stderr") {
    color = consoleErrorColor();
  } else if (event.category == "important") {
    color = consoleInfoColor();
  }
  appendConsoleLine(event.output, color);
}

void DebugPanel::onCallStackItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item) {
    return;
  }

  int frameId = item->data(0, Qt::UserRole).toInt();
  QString filePath = item->data(0, Qt::UserRole + 1).toString();
  int line = item->data(0, Qt::UserRole + 2).toInt();
  int col = item->data(0, Qt::UserRole + 3).toInt();

  setCurrentFrame(frameId);
  WatchManager::instance().evaluateAll(frameId);

  if (!filePath.isEmpty()) {
    emit locationClicked(filePath, line, col);
  }
}

void DebugPanel::onVariableItemExpanded(QTreeWidgetItem *item) {
  if (!item || m_programmaticVariablesExpand) {
    return;
  }

  if (!item->parent()) {
    const QString scopeName = item->text(0).trimmed().toLower();
    if (scopeName.contains(QLatin1String("local"))) {
      const int localScopeRef = item->data(0, Qt::UserRole).toInt();
      if (localScopeRef > 0 && item->childCount() == 0) {
        if (hasLocalsFallbackCommand()) {
          requestLocalsFallback(localScopeRef);
        } else if (m_dapClient &&
                   !m_pendingVariableRequests.contains(localScopeRef)) {
          m_variableRefToItem[localScopeRef] = item;
          m_pendingVariableRequests.insert(localScopeRef);
          m_dapClient->getVariables(localScopeRef);
        }
      }
      return;
    }
  }

  int varRef = item->data(0, Qt::UserRole).toInt();

  if (varRef > 0 && item->childCount() == 0 && m_dapClient &&
      !m_pendingVariableRequests.contains(varRef)) {
    m_variableRefToItem[varRef] = item;
    m_pendingVariableRequests.insert(varRef);
    m_dapClient->getVariables(varRef);
  }
}

void DebugPanel::onBreakpointItemDoubleClicked(QTreeWidgetItem *item,
                                               int column) {
  Q_UNUSED(column);

  QString filePath = item->data(0, Qt::UserRole).toString();
  int line = item->data(0, Qt::UserRole + 1).toInt();

  if (!filePath.isEmpty()) {
    emit locationClicked(filePath, line, 0);
  }
}

void DebugPanel::onConsoleInput() {
  QString expr = m_consoleInput->text().trimmed();
  if (expr.isEmpty()) {
    return;
  }

  m_consoleInput->clear();

  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    const QList<DebugEvaluateRequest> attempts =
        DebugExpressionTranslator::buildConsoleEvaluationPlan(
            expr, m_dapClient->adapterId(), m_dapClient->adapterType());
    if (attempts.isEmpty()) {
      appendConsoleLine(tr("Cannot evaluate: expression is empty"),
                        consoleErrorColor());
      return;
    }

    appendConsoleLine(QString("> %1").arg(expr), consoleMutedColor());
    PendingConsoleEvaluation pending;
    pending.userExpression = expr;
    pending.attempts = attempts;
    pending.activeAttemptIndex = 0;
    m_pendingConsoleEvaluations.append(pending);
    dispatchPendingConsoleEvaluation(m_pendingConsoleEvaluations.size() - 1);
  } else {
    appendConsoleLine(QString("> %1").arg(expr), consoleMutedColor());
    appendConsoleLine(tr("Cannot evaluate: not stopped at breakpoint"),
                      consoleErrorColor());
  }
}

void DebugPanel::updateToolbarState() {
  DapClient::State state =
      m_dapClient ? m_dapClient->state() : DapClient::State::Disconnected;

  const bool isDebugging = (state == DapClient::State::Running ||
                            state == DapClient::State::Stopped);
  const bool isStopped = (state == DapClient::State::Stopped &&
                          activeThreadId() > 0 && !m_stepInProgress);
  const bool isRunning =
      (state == DapClient::State::Running || m_stepInProgress);
  const bool isStarting = state == DapClient::State::Connecting ||
                          state == DapClient::State::Initializing;
  const bool canStart = !isRunning && !isStarting;
  const bool canStop = isDebugging || isStarting;

  m_continueAction->setEnabled(canStart);
  m_pauseAction->setEnabled(isRunning && isDebugging);
  m_stepOverAction->setEnabled(isStopped);
  m_stepIntoAction->setEnabled(isStopped);
  m_stepOutAction->setEnabled(isStopped);
  m_restartAction->setEnabled(isDebugging);
  m_stopAction->setEnabled(canStop);

  if (isDebugging) {
    m_continueAction->setText(tr("Continue"));
    m_continueAction->setToolTip(tr("Continue execution (F5)"));
    m_continueAction->setStatusTip(tr("Continue execution (F5)"));
  } else {
    m_continueAction->setText(tr("Start"));
    m_continueAction->setToolTip(tr("Start debugging current file (F5)"));
    m_continueAction->setStatusTip(tr("Start debugging current file (F5)"));
  }

  if (m_debugStatusLabel) {
    QString statusText;
    switch (state) {
    case DapClient::State::Disconnected:
    case DapClient::State::Ready:
    case DapClient::State::Terminated:
      statusText = tr("Ready: press Start (F5)");
      break;
    case DapClient::State::Connecting:
    case DapClient::State::Initializing:
      statusText = tr("Starting debugger...");
      break;
    case DapClient::State::Running:
      statusText = m_stepInProgress
                       ? tr("Stepping... waiting for next stop")
                       : tr("Running: Pause (F6) or Stop (Shift+F5)");
      break;
    case DapClient::State::Stopped:
      statusText = tr("Paused: Step (F10/F11) or Continue (F5)");
      break;
    case DapClient::State::Error:
      statusText = tr("Debugger error: Stop and restart");
      break;
    }
    m_debugStatusLabel->setText(statusText);
    m_debugStatusLabel->setToolTip(statusText);
  }

  m_consoleInput->setEnabled(isStopped);
}

int DebugPanel::activeThreadId() const {
  if (m_currentThreadId > 0) {
    return m_currentThreadId;
  }
  if (!m_threads.isEmpty()) {
    return m_threads.first().id;
  }
  return 0;
}

void DebugPanel::refreshBreakpointList() {
  m_breakpointsTree->clear();

  QList<Breakpoint> breakpoints =
      BreakpointManager::instance().allBreakpoints();

  for (const Breakpoint &bp : breakpoints) {
    QTreeWidgetItem *item = new QTreeWidgetItem();

    item->setCheckState(0, bp.enabled ? Qt::Checked : Qt::Unchecked);

    QFileInfo fi(bp.filePath);
    QString location = QString("%1:%2").arg(fi.fileName()).arg(bp.line);
    item->setText(1, location);
    item->setToolTip(1, bp.filePath);

    if (bp.isLogpoint) {
      item->setText(2, QString("log: %1").arg(bp.logMessage));
    } else if (!bp.condition.isEmpty()) {
      item->setText(2, bp.condition);
    }

    item->setData(0, Qt::UserRole, bp.filePath);
    item->setData(0, Qt::UserRole + 1, bp.line);
    item->setData(0, Qt::UserRole + 2, bp.id);

    if (bp.verified) {
      item->setIcon(0, style()->standardIcon(QStyle::SP_DialogApplyButton));
    } else if (!bp.verificationMessage.isEmpty()) {
      item->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
      item->setToolTip(0, bp.verificationMessage);
    }

    m_breakpointsTree->addTopLevelItem(item);
  }
}

void DebugPanel::onThreadSelected(int index) {
  if (index < 0 || !m_dapClient)
    return;

  int threadId = m_threadSelector->itemData(index).toInt();
  if (threadId > 0 && threadId != m_currentThreadId) {
    m_currentThreadId = threadId;
    m_dapClient->getStackTrace(m_currentThreadId, 0,
                               MAX_STACK_FRAMES_PER_REFRESH);
  }
}

void DebugPanel::onAddWatch() {
  QString expr = m_watchInput->text().trimmed();
  if (expr.isEmpty())
    return;

  m_watchInput->clear();
  int id = WatchManager::instance().addWatch(expr);

  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped &&
      m_currentFrameId > 0) {
    WatchManager::instance().evaluateWatch(id, m_currentFrameId);
  }
}

void DebugPanel::onRemoveWatch() {
  QTreeWidgetItem *item = m_watchTree->currentItem();
  if (!item || item->parent())
    return;

  int watchId = item->data(0, Qt::UserRole).toInt();
  WatchManager::instance().removeWatch(watchId);
}

void DebugPanel::onWatchAdded(const WatchExpression &watch) {
  QTreeWidgetItem *item = new QTreeWidgetItem();
  item->setText(0, watch.expression);
  item->setText(1, watch.value.isEmpty() ? tr("<not evaluated>") : watch.value);
  item->setText(2, watch.type);
  item->setData(0, Qt::UserRole, watch.id);
  item->setData(0, Qt::UserRole + 1, watch.variablesReference);

  if (watch.variablesReference > 0) {
    item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    item->setExpanded(true);
  }

  m_watchTree->addTopLevelItem(item);
  m_watchTree->expandAll();
  m_watchIdToItem[watch.id] = item;
}

void DebugPanel::onWatchRemoved(int id) {
  QTreeWidgetItem *item = m_watchIdToItem.take(id);
  if (item) {
    delete item;
  }
}

void DebugPanel::onWatchUpdated(const WatchExpression &watch) {
  QTreeWidgetItem *item = m_watchIdToItem.value(watch.id);
  if (!item)
    return;

  if (watch.isError) {
    item->setText(1, watch.errorMessage.isEmpty() ? tr("Evaluation failed")
                                                  : watch.errorMessage);
    item->setForeground(1, m_themeInitialized ? m_theme.errorColor
                                              : QColor(Qt::red));
  } else {
    item->setText(1, watch.value);
    item->setForeground(1, palette().color(QPalette::Text));
  }
  item->setText(2, watch.type);
  item->setData(0, Qt::UserRole + 1, watch.variablesReference);

  if (watch.variablesReference > 0) {
    item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    item->setExpanded(true);
  } else {
    item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

    while (item->childCount() > 0) {
      delete item->takeChild(0);
    }
  }
  m_watchTree->expandAll();
}

void DebugPanel::onWatchItemExpanded(QTreeWidgetItem *item) {

  if (!item || item->parent())
    return;

  int watchId = item->data(0, Qt::UserRole).toInt();
  int varRef = item->data(0, Qt::UserRole + 1).toInt();

  if (varRef > 0 && item->childCount() == 0) {
    WatchManager::instance().getWatchChildren(watchId, varRef);
  }
}

void DebugPanel::onWatchChildrenReceived(int watchId,
                                         const QList<DapVariable> &children) {
  QTreeWidgetItem *parentItem = m_watchIdToItem.value(watchId);
  if (!parentItem)
    return;

  while (parentItem->childCount() > 0) {
    delete parentItem->takeChild(0);
  }

  for (const DapVariable &var : children) {
    QTreeWidgetItem *childItem = new QTreeWidgetItem();
    childItem->setText(0, var.name);
    childItem->setText(1, var.value);
    childItem->setText(2, var.type);
    childItem->setData(0, Qt::UserRole + 1, var.variablesReference);
    childItem->setIcon(0, variableIcon(var));

    if (var.variablesReference > 0) {
      childItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }

    parentItem->addChild(childItem);
  }

  parentItem->setExpanded(true);
  m_watchTree->expandAll();
}

void DebugPanel::onEvaluateResult(const QString &expression,
                                  const QString &result, const QString &type,
                                  int variablesReference) {
  Q_UNUSED(variablesReference);

  if (m_localsFallbackPending &&
      expression == m_localsFallbackPendingExpression) {
    const int scopeRef = m_localsFallbackScopeRef;
    const bool staleFrame = m_localsFallbackFrameId != m_currentFrameId;
    clearLocalsFallbackState();
    if (!staleFrame) {
      populateLocalsFromGdbEvaluate(scopeRef, result);
    }
    return;
  }

  const int pendingIndex = findPendingConsoleEvaluationIndex(expression);
  QString displayExpression = expression;
  if (pendingIndex >= 0) {
    displayExpression = m_pendingConsoleEvaluations.at(pendingIndex).userExpression;
    m_pendingConsoleEvaluations.removeAt(pendingIndex);
  }

  QString line = QString("%1 = %2").arg(displayExpression, result);
  if (!type.isEmpty()) {
    line += QString(" (%1)").arg(type);
  }
  appendConsoleLine(line, palette().color(QPalette::Text), true);
}

void DebugPanel::onEvaluateError(const QString &expression,
                                 const QString &errorMessage) {
  if (m_localsFallbackPending &&
      expression == m_localsFallbackPendingExpression) {
    const int scopeRef = m_localsFallbackScopeRef;
    const bool staleFrame = m_localsFallbackFrameId != m_currentFrameId;
    clearLocalsFallbackState();
    if (!staleFrame) {
      showLocalsFallbackMessage(
          scopeRef, tr("<locals unavailable; use Watches/REPL>"), true);
    }
    return;
  }

  const int pendingIndex = findPendingConsoleEvaluationIndex(expression);
  if (pendingIndex >= 0) {
    PendingConsoleEvaluation &pending = m_pendingConsoleEvaluations[pendingIndex];
    const int nextAttempt = pending.activeAttemptIndex + 1;
    if (nextAttempt < pending.attempts.size()) {
      pending.activeAttemptIndex = nextAttempt;
      dispatchPendingConsoleEvaluation(pendingIndex);
      return;
    }

    const QString displayExpression = pending.userExpression;
    m_pendingConsoleEvaluations.removeAt(pendingIndex);
    appendConsoleLine(QString("%1: %2").arg(displayExpression, errorMessage),
                      consoleErrorColor(), true);
    return;
  }

  appendConsoleLine(QString("%1: %2").arg(expression, errorMessage),
                    consoleErrorColor(), true);
}

void DebugPanel::appendConsoleLine(const QString &text, const QColor &color,
                                   bool bold) {
  if (!m_consoleOutput) {
    return;
  }

  QString output = text;
  output.replace("\r\n", "\n");
  output.replace('\r', '\n');
  if (output.size() > MAX_DEBUG_CONSOLE_ENTRY_CHARS) {
    const int truncated = output.size() - MAX_DEBUG_CONSOLE_ENTRY_CHARS;
    output = output.left(MAX_DEBUG_CONSOLE_ENTRY_CHARS);
    output += tr(" ... [truncated %1 chars]").arg(truncated);
  }

  QTextCursor cursor(m_consoleOutput->document());
  cursor.movePosition(QTextCursor::End);

  QTextCharFormat format;
  format.setForeground(color);
  format.setFontWeight(bold ? QFont::DemiBold : QFont::Normal);
  cursor.insertText(output, format);
  if (!output.endsWith('\n')) {
    cursor.insertBlock();
  }

  m_consoleOutput->setTextCursor(cursor);
  m_consoleOutput->ensureCursorVisible();
}

int DebugPanel::findPendingConsoleEvaluationIndex(
    const QString &requestExpression) const {
  for (int i = 0; i < m_pendingConsoleEvaluations.size(); ++i) {
    const PendingConsoleEvaluation &pending = m_pendingConsoleEvaluations.at(i);
    if (pending.activeAttemptIndex < 0 ||
        pending.activeAttemptIndex >= pending.attempts.size()) {
      continue;
    }
    if (pending.attempts.at(pending.activeAttemptIndex).expression ==
        requestExpression) {
      return i;
    }
  }
  return -1;
}

void DebugPanel::dispatchPendingConsoleEvaluation(int pendingIndex) {
  if (!m_dapClient || pendingIndex < 0 ||
      pendingIndex >= m_pendingConsoleEvaluations.size()) {
    return;
  }

  const PendingConsoleEvaluation &pending =
      m_pendingConsoleEvaluations.at(pendingIndex);
  if (pending.activeAttemptIndex < 0 ||
      pending.activeAttemptIndex >= pending.attempts.size()) {
    return;
  }

  const DebugEvaluateRequest &attempt =
      pending.attempts.at(pending.activeAttemptIndex);
  if (attempt.expression.trimmed().isEmpty()) {
    return;
  }

  const QString context =
      attempt.context.isEmpty() ? QStringLiteral("repl") : attempt.context;
  m_dapClient->evaluate(attempt.expression, m_currentFrameId, context);
}

void DebugPanel::resizeVariablesNameColumnOnce() {
  if (!m_variablesTree) {
    return;
  }

  m_variablesTree->resizeColumnToContents(0);
  const int measured = m_variablesTree->columnWidth(0);
  const int padded = measured + 18;
  const int minWidth = 180;
  const int maxWidth = qMax(
      minWidth, static_cast<int>(m_variablesTree->viewport()->width() * 0.55));
  m_variablesTree->setColumnWidth(0, qBound(minWidth, padded, maxWidth));
}

QColor DebugPanel::consoleErrorColor() const {
  if (m_themeInitialized) {
    return m_theme.errorColor;
  }
  const bool darkBackground = palette().color(QPalette::Base).lightness() < 128;
  return darkBackground ? QColor("#ff7b72") : QColor("#b42318");
}

QColor DebugPanel::consoleMutedColor() const {
  if (m_themeInitialized) {
    return m_theme.singleLineCommentFormat;
  }
  QColor muted = palette().color(QPalette::PlaceholderText);
  if (!muted.isValid()) {
    muted = palette().color(QPalette::Mid);
  }
  return muted;
}

QColor DebugPanel::consoleInfoColor() const {
  if (m_themeInitialized) {
    return m_theme.accentColor;
  }
  QColor info = palette().color(QPalette::Link);
  if (!info.isValid()) {
    info = palette().color(QPalette::Highlight);
  }
  return info;
}

QString DebugPanel::formatVariable(const DapVariable &var) const {
  if (var.type.isEmpty()) {
    return var.value;
  }
  return QString("%1 (%2)").arg(var.value).arg(var.type);
}

QIcon DebugPanel::variableIcon(const DapVariable &var) const {
  if (var.variablesReference > 0) {

    return style()->standardIcon(QStyle::SP_DirIcon);
  } else {

    return style()->standardIcon(QStyle::SP_FileIcon);
  }
}
