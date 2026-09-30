#include "dbconnection.h"

#include "clisession.h"
#include "sqlitesession.h"
#include "sqlstatementsplitter.h"

#include <atomic>

namespace {
std::atomic<quint64> g_nextRequestId{1};
}

DbConnection::DbConnection(const DbConnectionProfile &profile, QObject *parent)
    : QObject(parent), m_profile(profile) {}

DbConnection::~DbConnection() { destroySession(false); }

void DbConnection::createSession() {
  destroySession();
  if (m_profile.engine == DbEngine::Sqlite) {
    m_session = std::make_unique<SqliteSession>(m_profile);
  } else {
    m_session = std::make_unique<CliSession>(m_profile);
  }
  m_session->setMaxRows(m_maxRows);
  connect(m_session.get(), &IDbSession::opened, this, &DbConnection::onOpened);
  connect(m_session.get(), &IDbSession::openFailed, this,
          &DbConnection::onOpenFailed);
  connect(m_session.get(), &IDbSession::closed, this, &DbConnection::onClosed);
  connect(m_session.get(), &IDbSession::statementFinished, this,
          &DbConnection::onStatementFinished);
  connect(m_session.get(), &IDbSession::requestFinished, this,
          &DbConnection::onRequestFinished);
}

void DbConnection::destroySession(bool later) {
  if (!m_session) {
    return;
  }
  m_session->disconnect(this);
  if (later) {
    m_session.release()->deleteLater();
  } else {
    m_session.reset();
  }
}

void DbConnection::setMaxRows(int rows) {
  m_maxRows = rows;
  if (m_session) {
    m_session->setMaxRows(rows);
  }
}

void DbConnection::setProfile(const DbConnectionProfile &profile) {
  const bool targetChanged =
      profile.engine != m_profile.engine ||
      profile.transport != m_profile.transport ||
      profile.host != m_profile.host || profile.port != m_profile.port ||
      profile.user != m_profile.user ||
      profile.container != m_profile.container ||
      profile.filePath != m_profile.filePath ||
      profile.clientPath != m_profile.clientPath ||
      profile.extraArgs != m_profile.extraArgs ||
      profile.readOnly != m_profile.readOnly ||
      profile.trustServerCertificate != m_profile.trustServerCertificate;
  m_profile = profile;
  if (targetChanged && m_state != DbConnectionState::Disconnected) {
    disconnectFromServer();
    m_hasPassword = false;
    m_password.clear();
  }
}

void DbConnection::setState(DbConnectionState state, const QString &error) {
  m_state = state;
  m_lastError = error;
  emit stateChanged(state);
}

void DbConnection::connectToServer(const QString &password) {
  if (m_state == DbConnectionState::Connecting ||
      m_state == DbConnectionState::Connected) {
    return;
  }
  m_password = password;
  m_hasPassword = true;
  m_schema = DbSchema();
  m_databases.clear();
  setState(DbConnectionState::Connecting);
  createSession();
  m_session->open(password);
}

bool DbConnection::reconnect() {
  if (m_state == DbConnectionState::Connecting) {
    return true;
  }
  if (!m_hasPassword && m_profile.engine != DbEngine::Sqlite) {
    return false;
  }
  if (m_state == DbConnectionState::Connected) {
    return true;
  }
  connectToServer(m_password);
  return true;
}

void DbConnection::disconnectFromServer() {
  failPendingRequests(QStringLiteral("The connection was closed."));
  if (m_session) {
    m_session->disconnect(this);
    m_session->close();
    destroySession();
  }
  m_hasActive = false;
  m_userRequestActive = false;
  m_schema = DbSchema();
  m_databases.clear();
  m_schemaLoading = false;
  setState(DbConnectionState::Disconnected);
}

void DbConnection::onOpened() {
  setState(DbConnectionState::Connected);
  if (m_profile.engine != DbEngine::Sqlite) {
    Request dbs;
    dbs.id = g_nextRequestId++;
    dbs.purpose = Purpose::Databases;
    dbs.statements = {DbCatalog::databaseListQuery(m_profile.engine)};
    enqueue(dbs);
  }
  refreshSchema();
  pump();
}

void DbConnection::onOpenFailed(const QString &message) {
  destroySession();

  if (m_profile.engine != DbEngine::Sqlite) {
    m_password.clear();
    m_hasPassword = false;
  }
  failPendingRequests(message);
  setState(DbConnectionState::Failed,
           message.isEmpty() ? QStringLiteral("Could not connect.") : message);
}

void DbConnection::onClosed(const QString &reason) {

  if (m_state == DbConnectionState::Disconnected) {
    return;
  }
  const bool wasConnected = m_state == DbConnectionState::Connected;
  failPendingRequests(
      reason.isEmpty() ? QStringLiteral("The connection was closed.") : reason);
  m_hasActive = false;
  m_userRequestActive = false;
  if (wasConnected) {
    setState(DbConnectionState::Disconnected, reason);
  }
}

void DbConnection::failPendingRequests(const QString &message) {
  QQueue<Request> pending;
  pending.swap(m_queue);
  for (const Request &r : pending) {
    if (r.purpose != Purpose::User) {
      continue;
    }
    DbStatementResult res;
    res.ok = false;
    res.error = message;
    if (!r.statements.isEmpty()) {
      res.sql = r.statements.first();
    }
    emit statementFinished(r.id, 0, res);
    emit requestFinished(r.id, false);
  }
  m_schemaLoading = false;
}

quint64 DbConnection::execute(const QStringList &statements, bool stopOnError) {
  Request r;
  r.id = g_nextRequestId++;
  r.purpose = Purpose::User;
  r.statements = statements;
  r.stopOnError = stopOnError;

  if (m_profile.readOnly) {
    for (int i = 0; i < statements.size(); ++i) {
      const SqlStatementKind kind =
          SqlStatementSplitter::classify(statements[i], m_profile.engine);
      if (!SqlStatementSplitter::isReadOnly(kind)) {
        DbStatementResult res;
        res.sql = statements[i];
        res.ok = false;
        res.error =
            QStringLiteral("\"%1\" is a read-only connection; this statement "
                           "would modify data. Turn off \"Read-only\" in the "
                           "connection settings to run it.")
                .arg(m_profile.name);
        emit requestStarted(r.id);
        emit statementFinished(r.id, i, res);
        emit requestFinished(r.id, false);
        return r.id;
      }
    }
  }

  if (m_state != DbConnectionState::Connected) {
    if (m_state == DbConnectionState::Connecting || reconnect()) {
      emit requestStarted(r.id);
      enqueue(r);
      return r.id;
    }
    DbStatementResult res;
    res.ok = false;
    res.error = QStringLiteral("Not connected to \"%1\".").arg(m_profile.name);
    if (!statements.isEmpty()) {
      res.sql = statements.first();
    }
    emit requestStarted(r.id);
    emit statementFinished(r.id, 0, res);
    emit requestFinished(r.id, false);
    return r.id;
  }
  emit requestStarted(r.id);
  enqueue(r);
  return r.id;
}

void DbConnection::enqueue(const Request &request) {
  m_queue.enqueue(request);
  pump();
}

void DbConnection::pump() {
  if (m_hasActive || m_state != DbConnectionState::Connected || !m_session ||
      m_queue.isEmpty()) {
    return;
  }
  m_active = m_queue.dequeue();
  m_hasActive = true;
  m_userRequestActive = m_active.purpose == Purpose::User;
  m_metaRows = DbResultSet();
  m_metaFailed = false;
  if (m_active.purpose == Purpose::Schema) {
    m_schemaLoading = true;
  }
  m_session->execute(m_active.id, m_active.statements, m_active.stopOnError);
}

void DbConnection::onStatementFinished(quint64 id, int index,
                                       const DbStatementResult &r) {
  if (!m_hasActive || id != m_active.id) {
    return;
  }
  if (m_active.purpose == Purpose::User) {
    if (r.ok) {
      const SqlStatementKind kind =
          SqlStatementSplitter::classify(r.sql, m_profile.engine);
      if (kind == SqlStatementKind::Ddl) {
        m_schemaDirty = true;
      }
    }
    emit statementFinished(id, index, r);
    return;
  }
  if (!r.ok) {
    m_metaFailed = true;
  } else if (!r.resultSets.isEmpty()) {
    m_metaRows = r.resultSets.first();
  }
}

void DbConnection::onRequestFinished(quint64 id, bool cancelled) {
  if (!m_hasActive || id != m_active.id) {
    return;
  }
  const Request done = m_active;
  m_hasActive = false;
  m_userRequestActive = false;

  switch (done.purpose) {
  case Purpose::User:
    emit requestFinished(id, cancelled);
    if (m_schemaDirty) {

      m_schemaDirty = false;
      refreshSchema();
    }
    break;
  case Purpose::Schema:
    m_schemaLoading = false;
    if (!m_metaFailed) {
      m_schema = DbCatalog::buildSchema(m_metaRows);
    }
    emit schemaLoaded();
    break;
  case Purpose::Databases:
    if (!m_metaFailed) {
      m_databases = DbCatalog::parseDatabaseList(m_metaRows);
    }
    emit databasesLoaded();
    break;
  case Purpose::UseDatabase:
    if (!m_metaFailed) {
      m_profile.database = done.argument;
      emit databaseChanged(done.argument);
      refreshSchema();
    }
    break;
  }
  pump();
}

void DbConnection::cancel() {
  if (m_session && m_hasActive) {
    m_session->cancel();
  }

  QQueue<Request> keep;
  while (!m_queue.isEmpty()) {
    const Request r = m_queue.dequeue();
    if (r.purpose == Purpose::User) {
      emit requestFinished(r.id, true);
    } else {
      keep.enqueue(r);
    }
  }
  m_queue = keep;
}

void DbConnection::refreshSchema() {
  if (m_state != DbConnectionState::Connected) {
    return;
  }
  Request r;
  r.id = g_nextRequestId++;
  r.purpose = Purpose::Schema;
  r.statements = {DbCatalog::schemaQuery(m_profile.engine)};
  m_schemaLoading = true;
  enqueue(r);
}

void DbConnection::switchDatabase(const QString &database) {
  const QString sql =
      DbCatalog::useDatabaseStatement(m_profile.engine, database);
  if (sql.isEmpty() || m_state != DbConnectionState::Connected) {
    return;
  }
  Request r;
  r.id = g_nextRequestId++;
  r.purpose = Purpose::UseDatabase;
  r.statements = {sql};
  r.argument = database;
  enqueue(r);
}
