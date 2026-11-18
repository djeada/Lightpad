#ifndef DBCONNECTION_H
#define DBCONNECTION_H

#include "dbcatalog.h"
#include "dbstatistics.h"
#include "idbsession.h"

#include <QDeadlineTimer>
#include <QObject>
#include <QQueue>
#include <memory>

enum class DbConnectionState { Disconnected, Connecting, Connected, Failed };

class DbConnection : public QObject {
  Q_OBJECT

public:
  explicit DbConnection(const DbConnectionProfile &profile,
                        QObject *parent = nullptr);
  ~DbConnection() override;

  const DbConnectionProfile &profile() const { return m_profile; }

  void setProfile(const DbConnectionProfile &profile);

  DbConnectionState state() const { return m_state; }
  bool isConnected() const { return m_state == DbConnectionState::Connected; }
  bool isBusy() const { return m_userRequestActive; }
  QString lastError() const { return m_lastError; }

  bool hasSessionPassword() const { return m_hasPassword; }

  void rememberPassword(const QString &password) {
    m_password = password;
    m_hasPassword = true;
  }

  const DbSchema &schema() const { return m_schema; }
  bool schemaLoading() const { return m_schemaLoading; }
  QString lastSchemaError() const { return m_schemaError; }
  QStringList databases() const { return m_databases; }
  QString currentDatabase() const { return m_profile.database; }
  const DbStatistics &statistics() const { return m_statistics; }
  bool statisticsLoading() const { return m_statisticsLoading; }
  QString lastStatisticsError() const { return m_statisticsError; }

  void connectToServer(const QString &password);

  // Connects with the remembered password, or the one the profile names (an
  // environment variable, or none at all). False when a password must be
  // asked for first.
  bool reconnect();
  // Fills in the password from the profile's source when it does not need
  // to be asked for. False for "ask" profiles.
  bool resolvePassword();
  void disconnectFromServer();

  quint64 execute(const QStringList &statements, bool stopOnError = true);
  void cancel();

  void refreshSchema();
  void refreshStatistics(bool includeLargeTables = false);
  void cancelStatistics();
  void setExactRowCount(const QString &schema, const QString &table,
                        qint64 rows);
  void switchDatabase(const QString &database);

  void setMaxRows(int rows);

signals:
  void stateChanged(DbConnectionState state);
  void requestStarted(quint64 requestId);
  void statementFinished(quint64 requestId, int index,
                         const DbStatementResult &result);
  void requestFinished(quint64 requestId, bool cancelled);
  void schemaLoaded();
  void schemaError(const QString &message);
  void databasesLoaded();
  void databaseChanged(const QString &database);
  void statisticsChanged();

private:
  enum class Purpose {
    User,
    Schema,
    SchemaDatabases,
    Databases,
    UseDatabase,
    Statistics
  };
  struct Request {
    quint64 id = 0;
    Purpose purpose = Purpose::User;
    QStringList statements;
    bool stopOnError = true;
    QString argument;
  };

  void createSession();
  void destroySession(bool later = true);
  void setState(DbConnectionState state, const QString &error = QString());
  void enqueue(const Request &request);
  void pump();
  void onOpened();
  void onOpenFailed(const QString &message);
  void onClosed(const QString &reason);
  void onStatementFinished(quint64 id, int index, const DbStatementResult &r);
  void onRequestFinished(quint64 id, bool cancelled);
  void failPendingRequests(const QString &message);

  DbConnectionProfile m_profile;
  std::unique_ptr<IDbSession> m_session;
  DbConnectionState m_state = DbConnectionState::Disconnected;
  QString m_lastError;
  QString m_password;
  bool m_hasPassword = false;

  DbSchema m_schema;
  QString m_schemaError;
  QStringList m_databases;
  bool m_schemaLoading = false;
  bool m_schemaDirty = false;
  DbStatistics m_statistics;
  DbStatistics m_pendingStatistics;
  QMap<QString, QString> m_sqliteFiles;
  QStringList m_sqliteSchemas;
  QString m_statisticsError;
  QStringList m_statisticsErrors;
  bool m_statisticsLoading = false;
  bool m_statisticsDirty = false;

  QQueue<Request> m_queue;
  Request m_active;
  bool m_hasActive = false;
  bool m_userRequestActive = false;
  DbResultSet m_metaRows;
  bool m_metaFailed = false;
  int m_maxRows = 10000;
};

#endif
