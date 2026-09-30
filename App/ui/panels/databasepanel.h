#ifndef DATABASEPANEL_H
#define DATABASEPANEL_H

#include "../../database/databasemanager.h"
#include "../../settings/theme.h"
#include "dbinsightsview.h"
#include "dbresultview.h"
#include "sqlconsoleedit.h"

#include <QComboBox>
#include <QElapsedTimer>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QWidget>

class QueryHistoryDialog;

class DatabasePanel : public QWidget {
  Q_OBJECT

public:
  explicit DatabasePanel(DatabaseManager *manager, QWidget *parent = nullptr);
  ~DatabasePanel() override;

  DatabaseManager *manager() const { return m_manager; }
  void applyTheme(const Theme &theme);
  void saveState() const;
  void restoreState();

  void runFromEditor(const QString &script, int caretOffset,
                     const QString &selection, bool runAll,
                     bool explain = false);

  DbConnection *connectionForScript(const QString &script) const;

  void runSql(const QString &sql);
  void setConsoleText(const QString &sql, bool focus = true);
  void focusConsole();

  bool isRunning() const;
  SqlConsoleEdit *console() const { return m_console; }
  QTreeWidget *tree() const { return m_tree; }
  QTabWidget *resultTabs() const { return m_resultTabs; }
  QTabWidget *outputTabs() const { return m_outputTabs; }
  QPlainTextEdit *messages() const { return m_messages; }
  DbInsightsView *insights() const { return m_insights; }
  QComboBox *connectionCombo() const { return m_connectionCombo; }

signals:
  void openInEditorRequested(const QString &title, const QString &sql);
  void statusMessage(const QString &text);

public slots:
  void newConnection();
  void editConnection(const QString &id);
  void duplicateConnection(const QString &id);
  void removeConnection(const QString &id);
  void connectConnection(const QString &id, bool userInitiated = true);
  void disconnectConnection(const QString &id);
  void refreshSchema(const QString &id = QString());
  void runStatement();
  void runScript();
  void explainStatement();
  void stopRunning();
  void showHistory();

private slots:
  void onProfilesChanged();
  void onActiveChanged();
  void onConnectionState(const QString &id, DbConnectionState state);
  void onSchemaChanged(const QString &id);
  void onTreeDoubleClicked(QTreeWidgetItem *item, int column);
  void onTreeContextMenu(const QPoint &pos);
  void onTreeItemExpanded(QTreeWidgetItem *item);
  void onTreeItemCollapsed(QTreeWidgetItem *item);
  void onConnectionComboActivated(int index);
  void onDatabaseComboActivated(int index);
  void onFilterChanged(const QString &text);
  void updateRunningLabel();

private:
  enum ItemKind {
    KindConnection = 1,
    KindSchema,
    KindTable,
    KindColumn,
    KindHint,
    KindPlaceholder
  };
  enum ItemRole {
    RoleKind = Qt::UserRole + 1,
    RoleConnection,
    RoleSchema,
    RoleName,
    RoleKey
  };

  struct Run {
    quint64 id = 0;
    QPointer<DbConnection> conn;
    QString connectionName;
    DbEngine engine = DbEngine::PostgreSql;
    QString tableName;
    bool profileAfter = false;
    bool explain = false;
    int statements = 0;
    int finished = 0;
    int results = 0;
    int errors = 0;
    qint64 rows = 0;
    qint64 elapsedMs = 0;
    QElapsedTimer timer;
  };

  void buildUi();
  void buildToolbar();
  void hookConnection(DbConnection *conn);
  void rebuildTree();
  void populateConnectionItem(QTreeWidgetItem *item, DbConnection *conn);
  void populateColumns(QTreeWidgetItem *tableItem);
  QTreeWidgetItem *addTableItem(QTreeWidgetItem *parent, DbConnection *conn,
                                const DbTableInfo &table);
  void refreshCombos();
  void refreshToolbar();
  QIcon stateIcon(DbConnectionState state, const QString &color) const;
  QColor stateColor(DbConnectionState state) const;

  DbConnection *activeConnection() const;
  DbConnection *connectionFromItem(QTreeWidgetItem *item) const;
  const DbTableInfo *tableFromItem(QTreeWidgetItem *item,
                                   DbConnection **conn = nullptr) const;

  bool ensurePassword(DbConnection *conn);
  bool confirmStatements(DbConnection *conn, const QStringList &statements);
  quint64 execute(DbConnection *conn, const QStringList &statements,
                  bool stopOnError, const QString &tableName = QString(),
                  bool profileAfter = false, bool explain = false);
  void previewTable(DbConnection *conn, const DbTableInfo &table);
  void showTableColumns(DbConnection *conn, const DbTableInfo &table);
  void generateSql(DbConnection *conn, const DbTableInfo &table,
                   const QString &what);

  void onStatementFinished(DbConnection *conn, quint64 id, int index,
                           const DbStatementResult &result);
  void onRequestFinished(DbConnection *conn, quint64 id, bool cancelled);
  void addResultTab(const DbResultSet &rs, const Run &run, const QString &title,
                    qint64 elapsedMs);
  void clearResults();
  void logMessage(const QString &text, bool isError,
                  const QString &detail = QString());
  void setStatus(const QString &text);
  void updateTabTitles();

  DatabaseManager *m_manager;
  Theme m_theme;

  QToolBar *m_toolbar = nullptr;
  QComboBox *m_connectionCombo = nullptr;
  QComboBox *m_databaseCombo = nullptr;
  QAction *m_databaseComboAction = nullptr;
  QAction *m_runAction = nullptr;
  QAction *m_runAllAction = nullptr;
  QAction *m_stopAction = nullptr;
  QAction *m_explainAction = nullptr;
  QAction *m_connectAction = nullptr;
  QAction *m_refreshAction = nullptr;
  QAction *m_historyAction = nullptr;
  QAction *m_addAction = nullptr;
  QAction *m_manageAction = nullptr;

  QSplitter *m_mainSplitter = nullptr;
  QSplitter *m_rightSplitter = nullptr;
  QLineEdit *m_filter = nullptr;
  QTreeWidget *m_tree = nullptr;
  QWidget *m_colorStrip = nullptr;
  SqlConsoleEdit *m_console = nullptr;
  QTabWidget *m_outputTabs = nullptr;
  QStackedWidget *m_resultStack = nullptr;
  QLabel *m_resultsEmpty = nullptr;
  QTabWidget *m_resultTabs = nullptr;
  QPlainTextEdit *m_messages = nullptr;
  DbInsightsView *m_insights = nullptr;
  QProgressBar *m_progress = nullptr;
  QLabel *m_status = nullptr;
  QLabel *m_runningLabel = nullptr;
  QTimer m_runningTimer;

  QSet<QString> m_expanded;
  QSet<QString> m_hooked;
  QHash<quint64, Run> m_runs;
  Run m_pendingRun;
  QHash<QString, quint64> m_activeRunByConnection;
  QPointer<QueryHistoryDialog> m_historyDialog;
  int m_historyCursor = -1;
  QString m_historyDraft;
  bool m_rebuildingTree = false;
  bool m_userConnect = false;
  int m_messageCount = 0;
  int m_errorCount = 0;
};

#endif
