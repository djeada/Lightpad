#ifndef DEBUGPANEL_H
#define DEBUGPANEL_H

#include <QComboBox>
#include <QLineEdit>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "../../dap/breakpointmanager.h"
#include "../../dap/dapclient.h"
#include "../../dap/watchmanager.h"

class DapClient;

/**
 * @brief Debug Panel - main debugging UI component
 *
 * Provides a comprehensive debugging interface including:
 * - Debug toolbar (Continue, Step Over, Step Into, Step Out, etc.)
 * - Variables view (locals, watches, registers)
 * - Call stack view
 * - Breakpoints list
 * - Debug console/REPL
 *
 * This panel is language-agnostic and works with any DAP-compliant
 * debug adapter.
 */
class DebugPanel : public QWidget {
  Q_OBJECT

public:
  explicit DebugPanel(QWidget *parent = nullptr);
  ~DebugPanel();

  /**
   * @brief Set the DAP client to use
   */
  void setDapClient(DapClient *client);

  /**
   * @brief Get the current DAP client
   */
  DapClient *dapClient() const { return m_dapClient; }

  /**
   * @brief Clear all debug state
   */
  void clearAll();

  /**
   * @brief Set the current stack frame for variable inspection
   */
  void setCurrentFrame(int frameId);

signals:
  /**
   * @brief Emitted when user clicks on a location in call stack
   */
  void locationClicked(const QString &filePath, int line, int column);

  /**
   * @brief Emitted when user wants to start debugging
   */
  void startDebugRequested();

  /**
   * @brief Emitted when user wants to stop debugging
   */
  void stopDebugRequested();

  /**
   * @brief Emitted when user wants to restart debugging
   */
  void restartDebugRequested();

public slots:
  /**
   * @brief Handle stopped event from debug adapter
   */
  void onStopped(const DapStoppedEvent &event);

  /**
   * @brief Handle continued event
   */
  void onContinued();

  /**
   * @brief Handle debug session terminated
   */
  void onTerminated();

private slots:
  // Toolbar actions
  void onContinue();
  void onPause();
  void onStepOver();
  void onStepInto();
  void onStepOut();
  void onRestart();
  void onStop();

  // Data handlers
  void onThreadsReceived(const QList<DapThread> &threads);
  void onStackTraceReceived(int threadId, const QList<DapStackFrame> &frames,
                            int totalFrames);
  void onScopesReceived(int frameId, const QList<DapScope> &scopes);
  void onVariablesReceived(int variablesReference,
                           const QList<DapVariable> &variables);
  void onOutputReceived(const DapOutputEvent &event);

  // UI interactions
  void onCallStackItemClicked(QTreeWidgetItem *item, int column);
  void onVariableItemExpanded(QTreeWidgetItem *item);
  void onBreakpointItemDoubleClicked(QTreeWidgetItem *item, int column);
  void onConsoleInput();
  void onThreadSelected(int index);

  // Watch interactions
  void onAddWatch();
  void onRemoveWatch();
  void onWatchAdded(const WatchExpression &watch);
  void onWatchRemoved(int id);
  void onWatchUpdated(const WatchExpression &watch);
  void onWatchItemExpanded(QTreeWidgetItem *item);
  void onWatchChildrenReceived(int watchId, const QList<DapVariable> &children);
  void onEvaluateError(const QString &expression, const QString &errorMessage);

private:
  void setupUI();
  void setupToolbar();
  void setupCallStack();
  void setupVariables();
  void setupWatches();
  void setupBreakpoints();
  void setupConsole();

  void updateToolbarState();
  void updateCallStack(int threadId);
  void populateScopeVariables(QTreeWidgetItem *scopeItem,
                              int variablesReference);
  void refreshBreakpointList();

  QString formatVariable(const DapVariable &var) const;
  QIcon variableIcon(const DapVariable &var) const;

  DapClient *m_dapClient;

  // Toolbar
  QToolBar *m_toolbar;
  QAction *m_continueAction;
  QAction *m_pauseAction;
  QAction *m_stepOverAction;
  QAction *m_stepIntoAction;
  QAction *m_stepOutAction;
  QAction *m_restartAction;
  QAction *m_stopAction;

  // Main layout
  QTabWidget *m_tabWidget;
  QSplitter *m_mainSplitter;

  // Call stack view
  QTreeWidget *m_callStackTree;

  // Variables view
  QTreeWidget *m_variablesTree;
  QMap<int, QTreeWidgetItem *> m_variableRefToItem; // For async population

  // Breakpoints list
  QTreeWidget *m_breakpointsTree;

  // Watch expressions
  QTreeWidget *m_watchTree;
  QLineEdit *m_watchInput;
  QMap<int, QTreeWidgetItem *> m_watchIdToItem; // watchId -> tree item

  // Thread selector
  QComboBox *m_threadSelector;

  // Debug console
  QTextEdit *m_consoleOutput;
  QLineEdit *m_consoleInput;

  // State
  int m_currentThreadId;
  int m_currentFrameId;
  QList<DapThread> m_threads;
  QList<DapStackFrame> m_stackFrames;
};

#endif // DEBUGPANEL_H
