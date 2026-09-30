#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "connectionstore.h"
#include "dbconnection.h"
#include "queryhistory.h"

#include <QHash>
#include <QObject>
#include <QPointer>

class DatabaseManager : public QObject {
  Q_OBJECT

public:
  explicit DatabaseManager(const QString &connectionsFile = QString(),
                           const QString &historyFile = QString(),
                           QObject *parent = nullptr);
  ~DatabaseManager() override;

  static DatabaseManager &instance();

  QVector<DbConnectionProfile> profiles() const { return m_store.profiles(); }
  DbConnection *connection(const QString &id) const;
  DbConnection *connectionByName(const QString &name) const;
  QVector<DbConnection *> connections() const;

  QString saveProfile(const DbConnectionProfile &profile);
  void removeProfile(const QString &id);
  QString uniqueName(const QString &wanted,
                     const QString &ignoreId = {}) const {
    return m_store.uniqueName(wanted, ignoreId);
  }

  QString activeId() const { return m_activeId; }
  DbConnection *activeConnection() const { return connection(m_activeId); }
  void setActive(const QString &id);

  QueryHistory &history() { return m_history; }
  const QueryHistory &history() const { return m_history; }
  void recordHistory(const DbConnection *conn, const DbStatementResult &result);
  void flush();

  int maxRows() const { return m_maxRows; }
  void setMaxRows(int rows);

signals:
  void profilesChanged();
  void activeChanged(const QString &id);
  void connectionStateChanged(const QString &id, DbConnectionState state);
  void schemaChanged(const QString &id);
  void historyChanged();

private:
  DbConnection *makeConnection(const DbConnectionProfile &profile);

  ConnectionStore m_store;
  QueryHistory m_history;
  QHash<QString, DbConnection *> m_connections;
  QString m_activeId;
  int m_maxRows = 10000;
};

#endif
