#include "sqlitesession.h"
#include "sqlstatementsplitter.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <atomic>
#include <mutex>
#include <sqlite3.h>

// SQLite permits interrupting queries from another thread. Protect the native
// handle's lifetime without holding this mutex during query execution.
struct SqliteControl {
  std::atomic_bool cancelled{false};
  std::mutex mutex;
  sqlite3 *handle = nullptr;

  void interrupt() {
    cancelled.store(true);
    std::lock_guard<std::mutex> lock(mutex);
    if (handle) {
      sqlite3_interrupt(handle);
    }
  }
};

class SqliteWorker : public QObject {
  Q_OBJECT

public:
  explicit SqliteWorker(std::shared_ptr<SqliteControl> control)
      : m_control(std::move(control)) {}
  ~SqliteWorker() override { closeDatabase(); }

public slots:
  void openDatabase(const QString &path, bool readOnly) {
    closeDatabase();
    if (!QFileInfo::exists(path)) {
      emit openFailed(
          QStringLiteral("The database file does not exist: %1").arg(path));
      return;
    }
    sqlite3 *database = nullptr;
    const int flags =
        (readOnly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE) |
        SQLITE_OPEN_FULLMUTEX;
    int code =
        sqlite3_open_v2(path.toUtf8().constData(), &database, flags, nullptr);
    QString error;
    if (code == SQLITE_OK) {
      sqlite3_busy_timeout(database, 1000);
      code = sqlite3_exec(database, "SELECT count(*) FROM sqlite_master",
                          nullptr, nullptr, nullptr);
    }
    if (code != SQLITE_OK) {
      error = database
                  ? QString::fromUtf8(sqlite3_errmsg(database))
                  : QStringLiteral("Could not allocate a SQLite connection.");
      if (database)
        sqlite3_close(database);
      emit openFailed(error);
      return;
    }
    {
      std::lock_guard<std::mutex> lock(m_control->mutex);
      m_control->handle = database;
      sqlite3_progress_handler(
          database, 1000,
          [](void *ctx) {
            return static_cast<SqliteControl *>(ctx)->cancelled.load() ? 1 : 0;
          },
          m_control.get());
    }
    emit opened();
  }

  void closeDatabase() {
    std::lock_guard<std::mutex> lock(m_control->mutex);
    if (m_control->handle) {
      sqlite3_progress_handler(m_control->handle, 0, nullptr, nullptr);
      sqlite3_close_v2(m_control->handle);
      m_control->handle = nullptr;
    }
  }

  void run(quint64 generation, quint64 requestId, const QStringList &statements,
           bool stopOnError, int maxRows) {
    for (int i = 0; i < statements.size() && !m_control->cancelled.load();
         ++i) {
      const DbStatementResult result = runOne(statements[i], maxRows);
      if (m_control->cancelled.load())
        break;
      emit statementDone(generation, requestId, i, result);
      if (!result.ok && stopOnError)
        break;
    }
    emit jobDone(generation, requestId, m_control->cancelled.load());
  }

signals:
  void opened();
  void openFailed(const QString &message);
  void statementDone(quint64 generation, quint64 requestId, int index,
                     const DbStatementResult &result);
  void jobDone(quint64 generation, quint64 requestId, bool cancelled);

private:
  DbStatementResult runOne(const QString &sql, int maxRows) {
    DbStatementResult result;
    result.sql = sql;
    QElapsedTimer timer;
    timer.start();
    sqlite3 *database = m_control->handle; // Owned exclusively by this worker.
    sqlite3_stmt *raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    const char *tail = nullptr;
    int code = database ? sqlite3_prepare_v2(database, utf8.constData(),
                                             utf8.size(), &raw, &tail)
                        : SQLITE_MISUSE;
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(
        raw, sqlite3_finalize);
    if (code != SQLITE_OK || !raw) {
      result.ok = false;
      result.error = database ? QString::fromUtf8(sqlite3_errmsg(database))
                              : QStringLiteral("The database is not open.");
      result.elapsedMs = timer.elapsed();
      return result;
    }
    if (tail && !SqlStatementSplitter::stripComments(QString::fromUtf8(tail))
                     .trimmed()
                     .isEmpty()) {
      result.ok = false;
      result.error = QStringLiteral("Run multiple statements as a script.");
      return result;
    }
    const int columns = sqlite3_column_count(raw);
    DbResultSet rows;
    for (int c = 0; c < columns; ++c) {
      const char *type = sqlite3_column_decltype(raw, c);
      rows.columns.append({QString::fromUtf8(sqlite3_column_name(raw, c)),
                           type ? QString::fromUtf8(type) : QString()});
    }
    constexpr qint64 byteBudget = 64 * 1024 * 1024;
    qint64 bytes = 0;
    while (!m_control->cancelled.load() &&
           (code = sqlite3_step(raw)) == SQLITE_ROW) {
      if (maxRows > 0 && rows.rows.size() >= maxRows) {
        rows.truncated = true;
        break;
      }
      QVector<QVariant> row;
      qint64 rowBytes = 0;
      for (int c = 0; c < columns; ++c) {
        QVariant value;
        switch (sqlite3_column_type(raw, c)) {
        case SQLITE_INTEGER:
          value = QVariant::fromValue<qlonglong>(sqlite3_column_int64(raw, c));
          break;
        case SQLITE_FLOAT:
          value = sqlite3_column_double(raw, c);
          break;
        case SQLITE_TEXT:
          value = QString::fromUtf8(
              reinterpret_cast<const char *>(sqlite3_column_text(raw, c)),
              sqlite3_column_bytes(raw, c));
          break;
        case SQLITE_BLOB: {
          const char *data =
              static_cast<const char *>(sqlite3_column_blob(raw, c));
          value = QByteArray(data ? data : "", sqlite3_column_bytes(raw, c));
          break;
        }
        default:
          break;
        }
        rowBytes += value.typeId() == QMetaType::QByteArray
                        ? value.toByteArray().size()
                        : value.toString().size() * 2 + 32;
        row.append(value);
      }
      // Keep one row even when a single value exceeds the budget, so it can be
      // saved.
      if (maxRows > 0 && !rows.rows.isEmpty() &&
          bytes + rowBytes > byteBudget) {
        rows.truncated = true;
        result.messages.append(
            QStringLiteral("Result reached the 64 MiB memory limit."));
        break;
      }
      bytes += rowBytes;
      rows.rows.append(row);
    }
    if (!rows.truncated && code != SQLITE_DONE) {
      result.ok = false;
      result.error = QString::fromUtf8(sqlite3_errmsg(database));
    } else if (columns > 0) {
      rows.totalRowsKnown = !rows.truncated;
      rows.totalRows = rows.rows.size() + (rows.truncated ? 1 : 0);
      result.resultSets.append(rows);
    } else {
      const auto kind = SqlStatementSplitter::classify(sql, DbEngine::Sqlite);
      if (kind == SqlStatementKind::Dml)
        result.rowsAffected = sqlite3_changes64(database);
    }
    result.elapsedMs = timer.elapsed();
    return result;
  }

  std::shared_ptr<SqliteControl> m_control;
};

SqliteSession::SqliteSession(const DbConnectionProfile &profile,
                             QObject *parent)
    : IDbSession(parent), m_profile(profile),
      m_control(std::make_shared<SqliteControl>()) {
  qRegisterMetaType<DbStatementResult>("DbStatementResult");
  m_worker = new SqliteWorker(m_control);
  m_worker->moveToThread(&m_thread);
  connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
  connect(m_worker, &SqliteWorker::opened, this, [this]() {
    m_open = true;
    emit opened();
  });
  connect(m_worker, &SqliteWorker::openFailed, this,
          [this](const QString &message) {
            m_open = false;
            emit openFailed(message);
          });
  connect(m_worker, &SqliteWorker::statementDone, this,
          [this](quint64 generation, quint64 id, int index,
                 const DbStatementResult &result) {
            if (generation == m_generation)
              emit statementFinished(id, index, result);
          });
  connect(m_worker, &SqliteWorker::jobDone, this,
          [this](quint64 generation, quint64 id, bool cancelled) {
            if (generation == m_generation) {
              m_busy = false;
              emit requestFinished(id, cancelled);
            }
          });
  m_thread.start();
}

SqliteSession::~SqliteSession() {
  m_control->interrupt();
  QMetaObject::invokeMethod(m_worker, "closeDatabase",
                            Qt::BlockingQueuedConnection);
  m_thread.quit();
  m_thread.wait();
}

void SqliteSession::open(const QString &) {
  m_control->cancelled.store(false);
  QMetaObject::invokeMethod(m_worker, "openDatabase", Qt::QueuedConnection,
                            Q_ARG(QString, m_profile.filePath),
                            Q_ARG(bool, m_profile.readOnly));
}

void SqliteSession::close() {
  const bool wasOpen = m_open;
  m_control->interrupt();
  m_open = false;
  m_busy = false;
  ++m_generation;
  QMetaObject::invokeMethod(m_worker, "closeDatabase", Qt::QueuedConnection);
  if (wasOpen)
    emit closed(QString());
}

void SqliteSession::execute(quint64 requestId, const QStringList &statements,
                            bool stopOnError) {
  if (!m_open) {
    DbStatementResult result;
    result.ok = false;
    result.error = "The connection is not open.";
    result.sql = statements.value(0);
    emit statementFinished(requestId, 0, result);
    emit requestFinished(requestId, false);
    return;
  }
  m_busy = true;
  m_control->cancelled.store(false);
  QMetaObject::invokeMethod(
      m_worker, "run", Qt::QueuedConnection, Q_ARG(quint64, m_generation),
      Q_ARG(quint64, requestId), Q_ARG(QStringList, statements),
      Q_ARG(bool, stopOnError), Q_ARG(int, m_maxRows));
}

void SqliteSession::cancel() {
  if (m_busy)
    m_control->interrupt();
}

#include "sqlitesession.moc"
