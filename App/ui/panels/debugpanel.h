#ifndef DEBUGPANEL_H
#define DEBUGPANEL_H

#include <QButtonGroup>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSet>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "../../dap/breakpointmanager.h"
#include "../../dap/dapclient.h"
#include "../../dap/expressiontranslator.h"
#include "../../dap/watchmanager.h"
#include "../../settings/theme.h"

class DapClient;

class DebugPanel : public QWidget {
  Q_OBJECT

public:
  explicit DebugPanel(QWidget *parent = nullptr);
  ~DebugPanel();

  void setDapClient(DapClient *client);

  DapClient *dapClient() const { return m_dapClient; }

  void clearAll();

  void setCurrentFrame(int frameId);
  void applyTheme(const Theme &theme);

signals:

  void locationClicked(const QString &filePath, int line, int column);

  void startDebugRequested();

  void stopDebugRequested();

  void restartDebugRequested();

public slots:

  void onStopped(const DapStoppedEvent &event);

  void onContinued();

  void onTerminated();

private slots:

  void onContinue();
  void onPause();
  void onStepOver();
  void onStepInto();
  void onStepOut();
  void onRestart();
  void onStop();

  void onThreadsReceived(const QList<DapThread> &threads);
  void onStackTraceReceived(int threadId, const QList<DapStackFrame> &frames,
                            int totalFrames);
  void onScopesReceived(int frameId, const QList<DapScope> &scopes);
  void onVariablesReceived(int variablesReference,
                           const QList<DapVariable> &variables);
  void onEvaluateResult(int requestSeq, const QString &expression,
                        const QString &result, const QString &type,
                        int variablesReference);
  void onOutputReceived(const DapOutputEvent &event);

  void onCallStackItemClicked(QTreeWidgetItem *item, int column);
  void onVariableItemExpanded(QTreeWidgetItem *item);
  void onBreakpointItemDoubleClicked(QTreeWidgetItem *item, int column);
  void onBreakpointItemChanged(QTreeWidgetItem *item, int column);
  void onBreakpointsContextMenuRequested(const QPoint &pos);
  void onConsoleInput();
  void onThreadSelected(int index);

  void onAddWatch();
  void onRemoveWatch();
  void onWatchAdded(const WatchExpression &watch);
  void onWatchRemoved(int id);
  void onWatchUpdated(const WatchExpression &watch);
  void onWatchItemExpanded(QTreeWidgetItem *item);
  void onWatchChildrenReceived(int watchId, const QList<DapVariable> &children);
  void onEvaluateError(int requestSeq, const QString &expression,
                       const QString &errorMessage);

private:
  void setupUI();
  void setupToolbar();
  void setupCallStack();
  void setupVariables();
  void setupWatches();
  void setupBreakpoints();
  void setupConsole();
  QToolButton *createToolbarButton(QAction *action,
                                   const QString &role = QString());
  QWidget *createToolbarDivider();
  QToolButton *createInspectorTabButton(const QString &label, int index);
  QWidget *createInspectorPage(QWidget *content,
                               QWidget *headerActions = nullptr,
                               QWidget *footer = nullptr);
  void setCurrentInspectorTab(int index);
  void setInspectorTabLabel(int index, const QString &label);

  void updateToolbarState();
  void updateSectionSummaries();
  void rebuildExceptionBreakpointMenu();
  QJsonArray currentExceptionBreakpointFilters() const;
  int activeThreadId() const;
  void updateCallStack(int threadId);
  void populateScopeVariables(QTreeWidgetItem *scopeItem,
                              int variablesReference);
  void refreshBreakpointList();
  void appendConsoleLine(const QString &text, const QColor &color,
                         bool bold = false);
  int findPendingConsoleEvaluationIndex(int requestSeq) const;
  void dispatchPendingConsoleEvaluation(int pendingIndex);
  void resizeVariablesNameColumnOnce();
  bool hasLocalsFallbackCommand() const;
  void requestLocalsFallback(int scopeVariablesReference);
  void populateLocalsFromGdbEvaluate(int scopeVariablesReference,
                                     const QString &rawResult);
  void showLocalsFallbackMessage(int scopeVariablesReference,
                                 const QString &message, bool isError = false);
  void clearLocalsFallbackState();
  QColor consoleErrorColor() const;
  QColor consoleMutedColor() const;
  QColor consoleInfoColor() const;

  QString formatVariable(const DapVariable &var) const;
  QIcon variableIcon(const DapVariable &var) const;

  DapClient *m_dapClient;

  QWidget *m_toolbar;
  QAction *m_continueAction;
  QAction *m_pauseAction;
  QAction *m_stepOverAction;
  QAction *m_stepIntoAction;
  QAction *m_stepOutAction;
  QAction *m_restartAction;
  QAction *m_stopAction;
  QLabel *m_debugStatusLabel;

  QWidget *m_inspectorShell;
  QWidget *m_inspectorTabBar;
  QButtonGroup *m_inspectorTabGroup;
  QList<QToolButton *> m_inspectorTabButtons;
  QStackedWidget *m_inspectorStack;

  QTreeWidget *m_callStackTree;

  QTreeWidget *m_variablesTree;
  QMap<int, QTreeWidgetItem *> m_variableRefToItem;

  QTreeWidget *m_breakpointsTree;
  QToolButton *m_addFunctionBreakpointButton;
  QToolButton *m_exceptionBreakpointsButton;
  QMenu *m_exceptionBreakpointsMenu;

  QTreeWidget *m_watchTree;
  QLineEdit *m_watchInput;
  QMap<int, QTreeWidgetItem *> m_watchIdToItem;

  QComboBox *m_threadSelector;

  QPlainTextEdit *m_consoleOutput;
  QLineEdit *m_consoleInput;

  int m_currentThreadId;
  int m_currentFrameId;
  QList<DapThread> m_threads;
  QList<DapStackFrame> m_stackFrames;
  QSet<int> m_pendingScopeVariableLoads;
  QSet<int> m_pendingVariableRequests;
  bool m_programmaticVariablesExpand;
  bool m_variablesNameColumnAutofitPending;
  bool m_stepInProgress;
  bool m_expectStopEvent;
  bool m_hasLastStopEvent;
  int m_lastStoppedThreadId;
  DapStoppedReason m_lastStoppedReason;
  struct PendingConsoleEvaluation {
    QString userExpression;
    QList<DebugEvaluateRequest> attempts;
    int activeAttemptIndex = 0;
    int activeRequestSeq = 0;
  };
  QList<PendingConsoleEvaluation> m_pendingConsoleEvaluations;
  bool m_localsFallbackPending;
  int m_localsFallbackFrameId;
  int m_localsFallbackScopeRef;
  int m_localsFallbackRequestSeq;
  Theme m_theme;
  bool m_themeInitialized;
};

#endif
