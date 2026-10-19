#ifndef DEBUGCONCURRENCYPANEL_H
#define DEBUGCONCURRENCYPANEL_H

#include "../../dap/dapclient.h"
#include "../../settings/theme.h"
#include <QHash>
#include <QPointer>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QPlainTextEdit;
class DebugValueInspector;

class DebugConcurrencyPanel : public QWidget {
  Q_OBJECT
public:
  explicit DebugConcurrencyPanel(QWidget *parent = nullptr);
  void setFrameContext(DapClient *client, int frameId);
  void applyTheme(const Theme &theme);
  bool singleThreadExecution() const;
  static QString asyncTasksExpression();

public slots:
  void refresh();
  void inspectAsyncTasks();

signals:
  void threadSelected(DapClient *client, int threadId);
  void clientSelected(DapClient *client);
  void watchRequested(const QString &expression);

private:
  void rebuildSessions();
  void showSelectedOutput();
  void updateActions();
  QPointer<DapClient> m_client;
  int m_frameId = -1;
  int m_tasksRequest = 0;
  QHash<DapClient *, QList<DapThread>> m_threads;
  QHash<DapClient *, QList<QMetaObject::Connection>> m_connections;
  QList<QMetaObject::Connection> m_contextConnections;
  QTreeWidget *m_sessions = nullptr;
  QPlainTextEdit *m_output = nullptr;
  QCheckBox *m_singleThread = nullptr;
  QPushButton *m_tasksButton = nullptr;
  QLabel *m_status = nullptr;
  DebugValueInspector *m_tasks = nullptr;
};

#endif
