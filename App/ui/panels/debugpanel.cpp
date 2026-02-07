#include "debugpanel.h"
#include "../../core/logging/logger.h"

#include <QAction>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QStyle>

DebugPanel::DebugPanel(QWidget *parent)
    : QWidget(parent), m_dapClient(nullptr), m_currentThreadId(0),
      m_currentFrameId(0) {
  setupUI();
  updateToolbarState();

  // Connect to breakpoint manager
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

  // Connect to watch manager
  connect(&WatchManager::instance(), &WatchManager::watchAdded, this,
          &DebugPanel::onWatchAdded);
  connect(&WatchManager::instance(), &WatchManager::watchRemoved, this,
          &DebugPanel::onWatchRemoved);
  connect(&WatchManager::instance(), &WatchManager::watchUpdated, this,
          &DebugPanel::onWatchUpdated);
  connect(&WatchManager::instance(), &WatchManager::watchChildrenReceived, this,
          &DebugPanel::onWatchChildrenReceived);

  // Populate existing watches
  for (const WatchExpression &w : WatchManager::instance().allWatches()) {
    onWatchAdded(w);
  }
}

DebugPanel::~DebugPanel() {}

void DebugPanel::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  setupToolbar();
  mainLayout->addWidget(m_toolbar);

  // Main splitter for panels
  m_mainSplitter = new QSplitter(Qt::Vertical, this);

  // Tab widget for different debug views
  m_tabWidget = new QTabWidget(this);

  // Setup individual panels
  setupVariables();
  setupWatches();
  setupCallStack();
  setupBreakpoints();

  m_tabWidget->addTab(m_variablesTree, tr("Variables"));
  m_tabWidget->addTab(m_watchTree->parentWidget(), tr("Watch"));
  m_tabWidget->addTab(m_callStackTree, tr("Call Stack"));
  m_tabWidget->addTab(m_breakpointsTree, tr("Breakpoints"));

  m_mainSplitter->addWidget(m_tabWidget);

  // Debug console
  QWidget *consoleWidget = new QWidget(this);
  QVBoxLayout *consoleLayout = new QVBoxLayout(consoleWidget);
  consoleLayout->setContentsMargins(0, 0, 0, 0);
  consoleLayout->setSpacing(2);

  setupConsole();
  consoleLayout->addWidget(m_consoleOutput);
  consoleLayout->addWidget(m_consoleInput);

  m_mainSplitter->addWidget(consoleWidget);
  m_mainSplitter->setSizes({300, 100});

  mainLayout->addWidget(m_mainSplitter);
}

void DebugPanel::setupToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setIconSize(QSize(16, 16));

  m_continueAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_MediaPlay), tr("Continue"));
  m_continueAction->setShortcut(QKeySequence(Qt::Key_F5));
  m_continueAction->setToolTip(tr("Continue (F5)"));
  connect(m_continueAction, &QAction::triggered, this, &DebugPanel::onContinue);

  m_pauseAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_MediaPause), tr("Pause"));
  m_pauseAction->setShortcut(QKeySequence(Qt::Key_F6));
  m_pauseAction->setToolTip(tr("Pause (F6)"));
  connect(m_pauseAction, &QAction::triggered, this, &DebugPanel::onPause);

  m_toolbar->addSeparator();

  m_stepOverAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_ArrowRight), tr("Step Over"));
  m_stepOverAction->setShortcut(QKeySequence(Qt::Key_F10));
  m_stepOverAction->setToolTip(tr("Step Over (F10)"));
  connect(m_stepOverAction, &QAction::triggered, this, &DebugPanel::onStepOver);

  m_stepIntoAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_ArrowDown), tr("Step Into"));
  m_stepIntoAction->setShortcut(QKeySequence(Qt::Key_F11));
  m_stepIntoAction->setToolTip(tr("Step Into (F11)"));
  connect(m_stepIntoAction, &QAction::triggered, this, &DebugPanel::onStepInto);

  m_stepOutAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_ArrowUp), tr("Step Out"));
  m_stepOutAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F11));
  m_stepOutAction->setToolTip(tr("Step Out (Shift+F11)"));
  connect(m_stepOutAction, &QAction::triggered, this, &DebugPanel::onStepOut);

  m_toolbar->addSeparator();

  m_restartAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_BrowserReload), tr("Restart"));
  m_restartAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5));
  m_restartAction->setToolTip(tr("Restart (Ctrl+Shift+F5)"));
  connect(m_restartAction, &QAction::triggered, this, &DebugPanel::onRestart);

  m_stopAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_MediaStop), tr("Stop"));
  m_stopAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
  m_stopAction->setToolTip(tr("Stop (Shift+F5)"));
  connect(m_stopAction, &QAction::triggered, this, &DebugPanel::onStop);

  m_toolbar->addSeparator();

  // Thread selector
  m_threadSelector = new QComboBox(this);
  m_threadSelector->setToolTip(tr("Select Thread"));
  m_threadSelector->setMinimumWidth(120);
  m_threadSelector->setEnabled(false);
  connect(m_threadSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &DebugPanel::onThreadSelected);
  m_toolbar->addWidget(m_threadSelector);
}

void DebugPanel::setupCallStack() {
  m_callStackTree = new QTreeWidget(this);
  m_callStackTree->setHeaderLabels({tr("Function"), tr("File"), tr("Line")});
  m_callStackTree->setRootIsDecorated(false);
  m_callStackTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_callStackTree->setAlternatingRowColors(true);

  m_callStackTree->header()->setStretchLastSection(false);
  m_callStackTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_callStackTree->header()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_callStackTree->header()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);

  connect(m_callStackTree, &QTreeWidget::itemClicked, this,
          &DebugPanel::onCallStackItemClicked);
}

void DebugPanel::setupVariables() {
  m_variablesTree = new QTreeWidget(this);
  m_variablesTree->setHeaderLabels({tr("Name"), tr("Value"), tr("Type")});
  m_variablesTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_variablesTree->setAlternatingRowColors(true);

  m_variablesTree->header()->setStretchLastSection(false);
  m_variablesTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_variablesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_variablesTree->header()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);

  connect(m_variablesTree, &QTreeWidget::itemExpanded, this,
          &DebugPanel::onVariableItemExpanded);
}

void DebugPanel::setupWatches() {
  // Container widget with tree + input
  QWidget *watchContainer = new QWidget(this);
  QVBoxLayout *watchLayout = new QVBoxLayout(watchContainer);
  watchLayout->setContentsMargins(0, 0, 0, 0);
  watchLayout->setSpacing(2);

  m_watchTree = new QTreeWidget(watchContainer);
  m_watchTree->setHeaderLabels({tr("Expression"), tr("Value"), tr("Type")});
  m_watchTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_watchTree->setAlternatingRowColors(true);
  m_watchTree->setRootIsDecorated(true);

  m_watchTree->header()->setStretchLastSection(false);
  m_watchTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_watchTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_watchTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

  connect(m_watchTree, &QTreeWidget::itemExpanded, this,
          &DebugPanel::onWatchItemExpanded);

  // Context menu for removing watches
  m_watchTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_watchTree, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            QTreeWidgetItem *item = m_watchTree->itemAt(pos);
            if (!item || item->parent())
              return; // Only top-level items

            QMenu menu;
            QAction *removeAction = menu.addAction(tr("Remove Watch"));
            QAction *chosen = menu.exec(m_watchTree->mapToGlobal(pos));
            if (chosen == removeAction) {
              int watchId = item->data(0, Qt::UserRole).toInt();
              WatchManager::instance().removeWatch(watchId);
            }
          });

  watchLayout->addWidget(m_watchTree);

  // Watch input
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

  m_breakpointsTree->header()->setStretchLastSection(true);
  m_breakpointsTree->header()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  m_breakpointsTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

  connect(m_breakpointsTree, &QTreeWidget::itemDoubleClicked, this,
          &DebugPanel::onBreakpointItemDoubleClicked);
}

void DebugPanel::setupConsole() {
  m_consoleOutput = new QTextEdit(this);
  m_consoleOutput->setReadOnly(true);
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
            [this](const QString &expr, const QString &result,
                   const QString &type, int) {
              m_consoleOutput->append(QString("<b>%1</b> = %2 <i>(%3)</i>")
                                          .arg(expr.toHtmlEscaped())
                                          .arg(result.toHtmlEscaped())
                                          .arg(type.toHtmlEscaped()));
            });
    connect(m_dapClient, &DapClient::evaluateError, this,
            &DebugPanel::onEvaluateError);

    // Connect WatchManager to the DAP client
    WatchManager::instance().setDapClient(m_dapClient);
  }

  updateToolbarState();
}

void DebugPanel::clearAll() {
  m_callStackTree->clear();
  m_variablesTree->clear();
  m_variableRefToItem.clear();
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

  // Request scopes for the selected frame
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    m_dapClient->getScopes(frameId);
  }
}

void DebugPanel::onStopped(const DapStoppedEvent &event) {
  m_currentThreadId = event.threadId;

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

  m_consoleOutput->append(
      QString("<font color='blue'>%1</font>").arg(reasonText));

  // Request threads and stack trace
  if (m_dapClient) {
    m_dapClient->getThreads();
    m_dapClient->getStackTrace(m_currentThreadId);
  }

  updateToolbarState();
}

void DebugPanel::onContinued() {
  m_variablesTree->clear();
  m_variableRefToItem.clear();
  updateToolbarState();
}

void DebugPanel::onTerminated() {
  clearAll();
  m_consoleOutput->append(tr("<font color='gray'>Debug session ended.</font>"));
  updateToolbarState();
}

void DebugPanel::onContinue() {
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    m_dapClient->continueExecution(m_currentThreadId);
  } else {
    emit startDebugRequested();
  }
}

void DebugPanel::onPause() {
  if (m_dapClient && m_dapClient->state() == DapClient::State::Running) {
    m_dapClient->pause(m_currentThreadId);
  }
}

void DebugPanel::onStepOver() {
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    m_dapClient->stepOver(m_currentThreadId);
  }
}

void DebugPanel::onStepInto() {
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    m_dapClient->stepInto(m_currentThreadId);
  }
}

void DebugPanel::onStepOut() {
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    m_dapClient->stepOut(m_currentThreadId);
  }
}

void DebugPanel::onRestart() {
  if (m_dapClient && m_dapClient->isDebugging()) {
    m_dapClient->restart();
  }
  emit restartDebugRequested();
}

void DebugPanel::onStop() {
  if (m_dapClient && m_dapClient->isDebugging()) {
    m_dapClient->terminate();
  }
  emit stopDebugRequested();
}

void DebugPanel::onThreadsReceived(const QList<DapThread> &threads) {
  m_threads = threads;

  // Update thread selector
  m_threadSelector->blockSignals(true);
  m_threadSelector->clear();
  for (const DapThread &thread : threads) {
    m_threadSelector->addItem(
        QString("Thread %1: %2").arg(thread.id).arg(thread.name), thread.id);
    if (thread.id == m_currentThreadId) {
      m_threadSelector->setCurrentIndex(m_threadSelector->count() - 1);
    }
  }
  m_threadSelector->setEnabled(!threads.isEmpty());
  m_threadSelector->blockSignals(false);
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

  // Select first frame and get its variables
  if (!frames.isEmpty()) {
    m_callStackTree->setCurrentItem(m_callStackTree->topLevelItem(0));
    setCurrentFrame(frames.first().id);

    // Evaluate all watch expressions in the current frame context
    WatchManager::instance().evaluateAll(frames.first().id);
  }
}

void DebugPanel::onScopesReceived(int frameId, const QList<DapScope> &scopes) {
  Q_UNUSED(frameId);

  m_variablesTree->clear();
  m_variableRefToItem.clear();

  for (const DapScope &scope : scopes) {
    QTreeWidgetItem *scopeItem = new QTreeWidgetItem();
    scopeItem->setText(0, scope.name);
    scopeItem->setData(0, Qt::UserRole, scope.variablesReference);
    scopeItem->setFirstColumnSpanned(true);
    scopeItem->setExpanded(true);

    // Make scope names bold
    QFont font = scopeItem->font(0);
    font.setBold(true);
    scopeItem->setFont(0, font);

    m_variablesTree->addTopLevelItem(scopeItem);

    // Request variables for this scope
    if (scope.variablesReference > 0) {
      m_variableRefToItem[scope.variablesReference] = scopeItem;
      m_dapClient->getVariables(scope.variablesReference);
    }
  }
}

void DebugPanel::onVariablesReceived(int variablesReference,
                                     const QList<DapVariable> &variables) {
  QTreeWidgetItem *parentItem = m_variableRefToItem.value(variablesReference);
  if (!parentItem) {
    return;
  }

  // Clear any placeholder
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

    // If variable is structured, add placeholder for expansion
    if (var.variablesReference > 0) {
      item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }

    parentItem->addChild(item);
  }
}

void DebugPanel::onOutputReceived(const DapOutputEvent &event) {
  QString color;
  if (event.category == "stderr") {
    color = "red";
  } else if (event.category == "important") {
    color = "blue";
  } else {
    color = "black";
  }

  QString text = event.output;
  text = text.toHtmlEscaped();
  text.replace("\n", "<br>");

  m_consoleOutput->append(
      QString("<font color='%1'>%2</font>").arg(color).arg(text));
}

void DebugPanel::onCallStackItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);

  int frameId = item->data(0, Qt::UserRole).toInt();
  QString filePath = item->data(0, Qt::UserRole + 1).toString();
  int line = item->data(0, Qt::UserRole + 2).toInt();
  int col = item->data(0, Qt::UserRole + 3).toInt();

  setCurrentFrame(frameId);

  if (!filePath.isEmpty()) {
    emit locationClicked(filePath, line, col);
  }
}

void DebugPanel::onVariableItemExpanded(QTreeWidgetItem *item) {
  int varRef = item->data(0, Qt::UserRole).toInt();

  // Only request if we haven't already loaded children
  if (varRef > 0 && item->childCount() == 0 && m_dapClient) {
    m_variableRefToItem[varRef] = item;
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
  m_consoleOutput->append(
      QString("<font color='gray'>&gt; %1</font>").arg(expr.toHtmlEscaped()));

  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    m_dapClient->evaluate(expr, m_currentFrameId, "repl");
  } else {
    m_consoleOutput->append(tr(
        "<font color='red'>Cannot evaluate: not stopped at breakpoint</font>"));
  }
}

void DebugPanel::updateToolbarState() {
  DapClient::State state =
      m_dapClient ? m_dapClient->state() : DapClient::State::Disconnected;

  bool isDebugging = (state == DapClient::State::Running ||
                      state == DapClient::State::Stopped);
  bool isStopped = (state == DapClient::State::Stopped);
  bool isRunning = (state == DapClient::State::Running);

  m_continueAction->setEnabled(!isRunning);
  m_pauseAction->setEnabled(isRunning);
  m_stepOverAction->setEnabled(isStopped);
  m_stepIntoAction->setEnabled(isStopped);
  m_stepOutAction->setEnabled(isStopped);
  m_restartAction->setEnabled(isDebugging);
  m_stopAction->setEnabled(isDebugging);

  m_consoleInput->setEnabled(isStopped);
}

void DebugPanel::refreshBreakpointList() {
  m_breakpointsTree->clear();

  QList<Breakpoint> breakpoints =
      BreakpointManager::instance().allBreakpoints();

  for (const Breakpoint &bp : breakpoints) {
    QTreeWidgetItem *item = new QTreeWidgetItem();

    // Checkbox for enabled state
    item->setCheckState(0, bp.enabled ? Qt::Checked : Qt::Unchecked);

    // Location
    QFileInfo fi(bp.filePath);
    QString location = QString("%1:%2").arg(fi.fileName()).arg(bp.line);
    item->setText(1, location);
    item->setToolTip(1, bp.filePath);

    // Condition or log message
    if (bp.isLogpoint) {
      item->setText(2, QString("log: %1").arg(bp.logMessage));
    } else if (!bp.condition.isEmpty()) {
      item->setText(2, bp.condition);
    }

    // Store data for navigation
    item->setData(0, Qt::UserRole, bp.filePath);
    item->setData(0, Qt::UserRole + 1, bp.line);
    item->setData(0, Qt::UserRole + 2, bp.id);

    // Visual feedback for verification
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
    m_dapClient->getStackTrace(m_currentThreadId);
  }
}

void DebugPanel::onAddWatch() {
  QString expr = m_watchInput->text().trimmed();
  if (expr.isEmpty())
    return;

  m_watchInput->clear();
  int id = WatchManager::instance().addWatch(expr);

  // If currently stopped, evaluate immediately
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
  }

  m_watchTree->addTopLevelItem(item);
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
    item->setText(1, watch.errorMessage.isEmpty() ? tr("<error>")
                                                  : watch.errorMessage);
    item->setForeground(1, Qt::red);
  } else {
    item->setText(1, watch.value);
    item->setForeground(1, palette().color(QPalette::Text));
  }
  item->setText(2, watch.type);
  item->setData(0, Qt::UserRole + 1, watch.variablesReference);

  // Update expansion capability
  if (watch.variablesReference > 0) {
    item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  } else {
    item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
    // Remove any existing children
    while (item->childCount() > 0) {
      delete item->takeChild(0);
    }
  }
}

void DebugPanel::onWatchItemExpanded(QTreeWidgetItem *item) {
  // Only handle top-level watch items
  if (!item)
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

  // Clear existing children
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
}

void DebugPanel::onEvaluateError(const QString &expression,
                                 const QString &errorMessage) {
  m_consoleOutput->append(
      QString("<font color='red'><b>%1</b>: %2</font>")
          .arg(expression.toHtmlEscaped())
          .arg(errorMessage.toHtmlEscaped()));
}

QString DebugPanel::formatVariable(const DapVariable &var) const {
  if (var.type.isEmpty()) {
    return var.value;
  }
  return QString("%1 (%2)").arg(var.value).arg(var.type);
}

QIcon DebugPanel::variableIcon(const DapVariable &var) const {
  if (var.variablesReference > 0) {
    // Structured variable (object, array, etc.)
    return style()->standardIcon(QStyle::SP_DirIcon);
  } else {
    // Primitive value
    return style()->standardIcon(QStyle::SP_FileIcon);
  }
}
