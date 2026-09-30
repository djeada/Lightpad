#include "databasemanager.h"

#include "sqlstatementsplitter.h"

#include <QCoreApplication>
#include <QPointer>

DatabaseManager::DatabaseManager(const QString &connectionsFile,
                                 const QString &historyFile, QObject *parent)
    : QObject(parent), m_store(connectionsFile), m_history(historyFile) {
  m_store.load();
  m_history.load();
  for (const DbConnectionProfile &p : m_store.profiles()) {
    makeConnection(p);
  }
  m_activeId = m_store.activeId();
  if (!m_connections.contains(m_activeId)) {
    m_activeId = m_store.profiles().isEmpty() ? QString()
                                              : m_store.profiles().first().id;
  }
}

DatabaseManager::~DatabaseManager() { flush(); }

DatabaseManager &DatabaseManager::instance() {

  static QPointer<DatabaseManager> manager;
  if (!manager) {
    manager =
        new DatabaseManager(QString(), QString(), QCoreApplication::instance());
  }
  return *manager;
}

DbConnection *DatabaseManager::makeConnection(const DbConnectionProfile &p) {
  auto *conn = new DbConnection(p, this);
  conn->setMaxRows(m_maxRows);
  const QString id = p.id;
  connect(
      conn, &DbConnection::stateChanged, this,
      [this, id](DbConnectionState s) { emit connectionStateChanged(id, s); });
  connect(conn, &DbConnection::schemaLoaded, this,
          [this, id]() { emit schemaChanged(id); });
  connect(conn, &DbConnection::databaseChanged, this,
          [this, id](const QString &) { emit schemaChanged(id); });
  m_connections.insert(id, conn);
  return conn;
}

DbConnection *DatabaseManager::connection(const QString &id) const {
  return m_connections.value(id, nullptr);
}

DbConnection *DatabaseManager::connectionByName(const QString &name) const {
  for (DbConnection *c : m_connections) {
    if (c->profile().name.compare(name, Qt::CaseInsensitive) == 0) {
      return c;
    }
  }
  return nullptr;
}

QVector<DbConnection *> DatabaseManager::connections() const {
  QVector<DbConnection *> out;
  for (const DbConnectionProfile &p : m_store.profiles()) {
    if (DbConnection *c = m_connections.value(p.id)) {
      out.append(c);
    }
  }
  return out;
}

QString DatabaseManager::saveProfile(const DbConnectionProfile &profile) {
  DbConnectionProfile p = profile;
  const bool isNew = p.id.isEmpty() || !m_connections.contains(p.id);
  const QString id = m_store.upsert(p);
  p.id = id;
  if (isNew) {
    makeConnection(p);
  } else {
    m_connections.value(id)->setProfile(p);
  }
  if (m_activeId.isEmpty()) {
    m_activeId = id;
    m_store.setActiveId(id);
  }
  m_store.save();
  emit profilesChanged();
  if (isNew && m_activeId == id) {
    emit activeChanged(id);
  }
  return id;
}

void DatabaseManager::removeProfile(const QString &id) {
  DbConnection *conn = m_connections.take(id);
  if (conn) {
    conn->disconnectFromServer();
    conn->deleteLater();
  }
  m_store.remove(id);
  if (m_activeId == id) {
    m_activeId = m_store.profiles().isEmpty() ? QString()
                                              : m_store.profiles().first().id;
    m_store.setActiveId(m_activeId);
    emit activeChanged(m_activeId);
  }
  m_store.save();
  emit profilesChanged();
}

void DatabaseManager::setActive(const QString &id) {
  if (id == m_activeId || (!id.isEmpty() && !m_connections.contains(id))) {
    return;
  }
  m_activeId = id;
  m_store.setActiveId(id);
  m_store.save();
  emit activeChanged(id);
}

void DatabaseManager::recordHistory(const DbConnection *conn,
                                    const DbStatementResult &r) {
  if (SqlStatementSplitter::stripComments(r.sql).trimmed().isEmpty()) {
    return;
  }
  QueryHistoryEntry e;
  e.connection = conn ? conn->profile().name : QString();
  e.engine = conn ? DbEngineInfo::id(conn->profile().engine) : QString();
  e.sql = r.sql;
  e.ok = r.ok;
  e.error = r.error;
  e.elapsedMs = r.elapsedMs;
  if (!r.resultSets.isEmpty()) {
    e.rows = r.resultSets.first().totalRows;
  } else {
    e.rows = r.rowsAffected;
  }
  m_history.add(e);
  emit historyChanged();
}

void DatabaseManager::flush() {
  m_history.save();
  m_store.save();
}

void DatabaseManager::setMaxRows(int rows) {
  m_maxRows = qMax(100, rows);
  for (DbConnection *c : m_connections) {
    c->setMaxRows(m_maxRows);
  }
}
