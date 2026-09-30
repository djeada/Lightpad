#include "sqlitesession.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QMetaObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

class SqliteWorker : public QObject {
  Q_OBJECT

public:
  explicit SqliteWorker(int maxRows) : m_maxRows(maxRows) {}
  ~SqliteWorker() override { closeDatabase(); }

  int m_maxRows;

public slots:
  void openDatabase(const QString &path, bool readOnly) {
    closeDatabase();
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
      emit openFailed(QStringLiteral(
          "The Qt SQLite driver (qsqlite) is not installed. On Debian/Ubuntu "
          "install the package libqt6sql6-sqlite."));
      return;
    }
    if (!QFileInfo::exists(path)) {
      emit openFailed(
          QStringLiteral("The database file does not exist: %1").arg(path));
      return;
    }
    m_name = QStringLiteral("lightpad_sqlite_%1")
                 .arg(QUuid::createUuid().toString(QUuid::Id128));
    QString error;
    {

      QSqlDatabase db =
          QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
      db.setDatabaseName(path);
      if (readOnly) {
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
      }
      if (!db.open()) {
        error = db.lastError().text();
      } else {

        QSqlQuery probe(db);
        if (!probe.exec(QStringLiteral("SELECT count(*) FROM sqlite_master"))) {
          error = probe.lastError().text();
        }
      }
      if (!error.isEmpty()) {
        db.close();
      }
    }
    if (!error.isEmpty()) {
      QSqlDatabase::removeDatabase(m_name);
      m_name.clear();
      emit openFailed(error);
      return;
    }
    emit opened();
  }

  void closeDatabase() {
    if (m_name.isEmpty()) {
      return;
    }
    {
      QSqlDatabase db = QSqlDatabase::database(m_name, false);
      if (db.isOpen()) {
        db.close();
      }
    }
    QSqlDatabase::removeDatabase(m_name);
    m_name.clear();
  }

  void run(quint64 generation, quint64 requestId, const QStringList &statements,
           bool stopOnError) {
    for (int i = 0; i < statements.size(); ++i) {
      DbStatementResult res = runOne(statements[i]);
      emit statementDone(generation, requestId, i, res);
      if (!res.ok && stopOnError) {
        break;
      }
    }
    emit jobDone(generation, requestId);
  }

signals:
  void opened();
  void openFailed(const QString &message);
  void statementDone(quint64 generation, quint64 requestId, int index,
                     const DbStatementResult &result);
  void jobDone(quint64 generation, quint64 requestId);

private:
  DbStatementResult runOne(const QString &sql) {
    DbStatementResult res;
    res.sql = sql;
    QElapsedTimer timer;
    timer.start();
    QSqlDatabase db = QSqlDatabase::database(m_name, false);
    if (!db.isOpen()) {
      res.ok = false;
      res.error = QStringLiteral("The database is not open.");
      return res;
    }
    QSqlQuery q(db);
    q.setForwardOnly(true);
    if (!q.exec(sql)) {
      res.ok = false;
      const QSqlError err = q.lastError();
      res.error =
          err.databaseText().isEmpty() ? err.text() : err.databaseText();
      res.elapsedMs = timer.elapsed();
      return res;
    }
    const QSqlRecord rec = q.record();
    if (rec.count() > 0) {
      DbResultSet rs;
      for (int c = 0; c < rec.count(); ++c) {
        DbColumn col;
        col.name = rec.fieldName(c);
        rs.columns.append(col);
      }
      const int cols = rec.count();
      while (q.next()) {
        ++rs.totalRows;
        if (m_maxRows > 0 && rs.rows.size() >= m_maxRows) {
          rs.truncated = true;
          continue;
        }
        QVector<QVariant> row;
        row.reserve(cols);
        for (int c = 0; c < cols; ++c) {
          QVariant v = q.value(c);
          if (v.typeId() == QMetaType::QByteArray) {
            v = QStringLiteral("<BLOB %1 bytes>").arg(v.toByteArray().size());
          }
          row.append(v.isNull() ? QVariant() : v);
        }
        rs.rows.append(row);
      }
      res.resultSets.append(rs);
    } else {
      res.rowsAffected = q.numRowsAffected();
    }
    res.elapsedMs = timer.elapsed();
    return res;
  }

  QString m_name;
};

SqliteSession::SqliteSession(const DbConnectionProfile &profile,
                             QObject *parent)
    : IDbSession(parent), m_profile(profile) {
  qRegisterMetaType<DbStatementResult>("DbStatementResult");
  m_worker = new SqliteWorker(m_maxRows);
  m_worker->moveToThread(&m_thread);
  connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
  connect(m_worker, &SqliteWorker::opened, this, [this]() {
    m_open = true;
    emit opened();
  });
  connect(m_worker, &SqliteWorker::openFailed, this,
          [this](const QString &msg) {
            m_open = false;
            emit openFailed(msg);
          });
  connect(
      m_worker, &SqliteWorker::statementDone, this,
      [this](quint64 gen, quint64 id, int index, const DbStatementResult &res) {
        if (gen != m_generation) {
          return;
        }
        emit statementFinished(id, index, res);
      });
  connect(m_worker, &SqliteWorker::jobDone, this,
          [this](quint64 gen, quint64 id) {
            if (gen != m_generation) {
              return;
            }
            m_busy = false;
            emit requestFinished(id, false);
          });
  m_thread.start();
}

SqliteSession::~SqliteSession() {
  QMetaObject::invokeMethod(m_worker, "closeDatabase",
                            Qt::BlockingQueuedConnection);
  m_thread.quit();
  m_thread.wait(5000);
}

void SqliteSession::open(const QString &) {
  m_worker->m_maxRows = m_maxRows;
  QMetaObject::invokeMethod(m_worker, "openDatabase", Qt::QueuedConnection,
                            Q_ARG(QString, m_profile.filePath),
                            Q_ARG(bool, m_profile.readOnly));
}

void SqliteSession::close() {
  const bool was = m_open;
  m_open = false;
  m_busy = false;
  ++m_generation;
  QMetaObject::invokeMethod(m_worker, "closeDatabase", Qt::QueuedConnection);
  if (was) {
    emit closed(QString());
  }
}

void SqliteSession::execute(quint64 requestId, const QStringList &statements,
                            bool stopOnError) {
  if (!m_open) {
    DbStatementResult res;
    res.ok = false;
    res.error = QStringLiteral("The connection is not open.");
    if (!statements.isEmpty()) {
      res.sql = statements.first();
    }
    emit statementFinished(requestId, 0, res);
    emit requestFinished(requestId, false);
    return;
  }
  m_busy = true;
  m_activeRequest = requestId;
  m_worker->m_maxRows = m_maxRows;
  QMetaObject::invokeMethod(
      m_worker, "run", Qt::QueuedConnection, Q_ARG(quint64, m_generation),
      Q_ARG(quint64, requestId), Q_ARG(QStringList, statements),
      Q_ARG(bool, stopOnError));
}

void SqliteSession::cancel() {
  if (!m_busy) {
    return;
  }

  ++m_generation;
  m_busy = false;
  emit requestFinished(m_activeRequest, true);
}

#include "sqlitesession.moc"
