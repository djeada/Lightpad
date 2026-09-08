#ifndef DEBUGPANEL_H
#define DEBUGPANEL_H

#include <QButtonGroup>
#include <QColor>
#include <QComboBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
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

  QList<QAction *> transportActions() const;

signals:

  void locationClicked(const QString &filePath, int line, int column);

  void startDebugRequested();

  void stopDebugRequested();

  void restartDebugRequested();

public slots:

  void onStopped(const DapStoppedEvent &event);

  void onContinued();

  void onTerminated();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

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
  void onExceptionInfoReceived(int threadId, const DapExceptionInfo &info);

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
  void fitVariablesNameColumn();
  void applyDebugStatusText();
  void setupConsoleToolbar();
  void toggleConsoleDetached();
  void reattachConsole();
  void applyTransportIcons();
  QString emptyStateTextFor(const QTreeWidget *tree) const;
  void showVariablesContextMenu(const QPoint &pos);
  QString expressionForVariableItem(const QTreeWidgetItem *item) const;
  bool paintTreeEmptyState(QTreeWidget *tree);
  void recallConsoleHistory(int direction);
  void findInConsole(bool backwards);
  void showConsoleContextMenu(const QPoint &pos);
  void navigateToConsoleLocation(const QString &lineText);
  void updateConsoleInputAffordances();
  void requestAggregatePreviews(const QList<DapVariable> &variables,
                                QTreeWidgetItem *parentItem);
  static QString previewForVariables(const QList<DapVariable> &variables);
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

  QPointer<DapClient> m_dapClient;

  QWidget *m_toolbar = nullptr;
  QAction *m_continueAction = nullptr;
  QAction *m_pauseAction = nullptr;
  QAction *m_stepOverAction = nullptr;
  QAction *m_stepIntoAction = nullptr;
  QAction *m_stepOutAction = nullptr;
  QAction *m_restartAction = nullptr;
  QAction *m_stopAction = nullptr;
  QLabel *m_debugStatusLabel = nullptr;

  QWidget *m_inspectorShell = nullptr;
  QWidget *m_inspectorTabBar = nullptr;
  QButtonGroup *m_inspectorTabGroup = nullptr;
  QList<QToolButton *> m_inspectorTabButtons;
  QStackedWidget *m_inspectorStack = nullptr;

  QTreeWidget *m_callStackTree = nullptr;

  QTreeWidget *m_variablesTree = nullptr;
  QMap<int, QTreeWidgetItem *> m_variableRefToItem;

  QTreeWidget *m_breakpointsTree = nullptr;
  QToolButton *m_addFunctionBreakpointButton = nullptr;
  QToolButton *m_exceptionBreakpointsButton = nullptr;
  QMenu *m_exceptionBreakpointsMenu = nullptr;

  QTreeWidget *m_watchTree = nullptr;
  QLineEdit *m_watchInput = nullptr;
  QMap<int, QTreeWidgetItem *> m_watchIdToItem;

  QComboBox *m_threadSelector = nullptr;

  QPlainTextEdit *m_consoleOutput = nullptr;
  QLineEdit *m_consoleInput = nullptr;

  int m_currentThreadId;
  int m_currentFrameId;
  QList<DapThread> m_threads;
  QList<DapStackFrame> m_stackFrames;
  QSet<int> m_pendingScopeVariableLoads;
  QSet<int> m_pendingVariableRequests;
  bool m_programmaticVariablesExpand;

  bool m_variablesNameColumnUserSized;
  bool m_variablesNameColumnAutofitting;
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

  struct ConsoleExpansionNode {
    QString name;
    QString value;
    int reference = 0;
    int depth = 0;
    QList<int> children;
  };
  struct ConsoleExpansion {
    QString expression;
    QString type;
    QList<ConsoleExpansionNode> nodes;
    int pendingRequests = 0;
  };
  QHash<int, ConsoleExpansion> m_consoleExpansions;

  QHash<int, QPair<int, int>> m_consoleExpansionNodeByRef;
  int m_nextConsoleExpansionId = 1;

  void startConsoleExpansion(const QString &expression, const QString &type,
                             int variablesReference);
  bool applyConsoleExpansion(int variablesReference,
                             const QList<DapVariable> &variables);
  QString renderConsoleExpansion(const ConsoleExpansion &expansion,
                                 int nodeIndex) const;

  QHash<int, QTreeWidgetItem *> m_pendingTreeSummaries;

  QHash<QString, QString> m_previousVariableValues;

  QString m_debugStatusText;

  QStringList m_consoleHistory;
  int m_consoleHistoryIndex = 0;
  QString m_consoleHistoryDraft;
  QWidget *m_consoleToolbar = nullptr;

  QWidget *m_consolePage = nullptr;
  QWidget *m_consoleWindow = nullptr;
  QAction *m_consoleDetachAction = nullptr;
  QAction *m_consoleClearAction = nullptr;
  QAction *m_consoleWrapAction = nullptr;
  QLineEdit *m_consoleFindInput = nullptr;

  bool m_localsFallbackPending;
  int m_localsFallbackFrameId;
  int m_localsFallbackScopeRef;
  int m_localsFallbackRequestSeq;
  Theme m_theme;
  bool m_themeInitialized;
};

#endif
