

#include "database/dbconnection.h"

#include <QEventLoop>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

namespace {

struct RunOutcome {
  QVector<DbStatementResult> results;
  bool cancelled = false;
  bool finished = false;
};

RunOutcome waitFor(DbConnection &conn, quint64 id, int timeoutMs = 30000) {
  RunOutcome out;
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  auto c1 =
      QObject::connect(&conn, &DbConnection::statementFinished, &loop,
                       [&](quint64 rid, int index, const DbStatementResult &r) {
                         if (rid != id) {
                           return;
                         }
                         if (out.results.size() <= index) {
                           out.results.resize(index + 1);
                         }
                         out.results[index] = r;
                       });
  auto c2 = QObject::connect(&conn, &DbConnection::requestFinished, &loop,
                             [&](quint64 rid, bool cancelled) {
                               if (rid == id) {
                                 out.cancelled = cancelled;
                                 out.finished = true;
                                 loop.quit();
                               }
                             });
  timeout.start(timeoutMs);
  loop.exec();
  QObject::disconnect(c1);
  QObject::disconnect(c2);
  return out;
}

RunOutcome run(DbConnection &conn, const QStringList &sql, bool stop = true) {

  RunOutcome out;
  QEventLoop loop;
  quint64 id = 0;
  bool done = false;
  auto c1 =
      QObject::connect(&conn, &DbConnection::statementFinished, &loop,
                       [&](quint64, int index, const DbStatementResult &r) {
                         if (out.results.size() <= index) {
                           out.results.resize(index + 1);
                         }
                         out.results[index] = r;
                       });
  auto c2 = QObject::connect(&conn, &DbConnection::requestFinished, &loop,
                             [&](quint64, bool cancelled) {
                               out.cancelled = cancelled;
                               out.finished = true;
                               done = true;
                               loop.quit();
                             });
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  timeout.start(30000);
  id = conn.execute(sql, stop);
  Q_UNUSED(id)
  if (!done) {
    loop.exec();
  }
  QObject::disconnect(c1);
  QObject::disconnect(c2);
  return out;
}

bool connectAndWait(DbConnection &conn, const QString &password,
                    QString *error = nullptr, int timeoutMs = 60000) {
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  QObject::connect(&conn, &DbConnection::stateChanged, &loop,
                   [&](DbConnectionState s) {
                     if (s == DbConnectionState::Connected ||
                         s == DbConnectionState::Failed) {
                       loop.quit();
                     }
                   });
  timeout.start(timeoutMs);
  conn.connectToServer(password);
  if (conn.state() == DbConnectionState::Connecting) {
    loop.exec();
  }
  if (error) {
    *error = conn.lastError();
  }
  return conn.isConnected();
}

bool waitForTable(DbConnection &conn, const QString &table, int seconds = 20) {
  for (int i = 0; i < seconds * 10 && !conn.schema().findByName(table); ++i) {
    QSignalSpy spy(&conn, &DbConnection::schemaLoaded);
    spy.wait(100);
  }
  return conn.schema().findByName(table) != nullptr;
}

struct EnvTarget {
  QString container;
  QString user;
  QString password;
  bool valid = false;
};

EnvTarget targetFromEnv(const char *var) {
  EnvTarget t;
  const QString v = QString::fromLocal8Bit(qgetenv(var));
  const QStringList parts = v.split(':');
  if (parts.size() >= 4 && parts[0] == "docker") {
    t.container = parts[1];
    t.user = parts[2];
    t.password = parts.mid(3).join(':');
    t.valid = true;
  }
  return t;
}

QVariant cell(const DbStatementResult &r, int row, int col) {
  return r.resultSets.value(0).rows.value(row).value(col);
}

} // namespace

class TestDatabaseLive : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void sqliteFullWorkflow();
  void sqliteErrorsDoNotStopWhenAsked();
  void sqliteReadOnlyProfileRefusesWrites();
  void sqliteMissingFileFailsToOpen();
  void sqliteRowLimit();
  void sqliteSchemaAndDisconnect();
  void sqliteDisconnectInsideResultHandler();

  void missingClientProgramGivesAHelpfulError();
  void sqlServerWorkflow();
  void postgresWorkflow();
  void mysqlWorkflow();

private:
  void engineWorkflow(DbEngine engine, const EnvTarget &target,
                      const QString &database, const QString &tableSuffix);
  QTemporaryDir m_dir;
  bool m_sqliteAvailable = false;
};

void TestDatabaseLive::initTestCase() {
  m_sqliteAvailable =
      QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"));
}

static DbConnectionProfile sqliteProfile(const QString &path,
                                         bool readOnly = false) {
  DbConnectionProfile p;
  p.id = "sqlite-test";
  p.name = "sqlite test";
  p.engine = DbEngine::Sqlite;
  p.filePath = path;
  p.readOnly = readOnly;
  return p;
}

static QString makeSqliteFile(QTemporaryDir &dir, const QString &name) {
  const QString path = dir.filePath(name);
  QFile f(path);
  f.open(QIODevice::WriteOnly);
  f.close();
  return path;
}

void TestDatabaseLive::sqliteFullWorkflow() {
  if (!m_sqliteAvailable) {
    QSKIP("Qt SQLite driver not installed");
  }
  DbConnection conn(sqliteProfile(makeSqliteFile(m_dir, "a.db")));
  QString err;
  QVERIFY2(connectAndWait(conn, "", &err), qPrintable(err));

  auto out = run(conn, {"CREATE TABLE people (id INTEGER PRIMARY KEY, name "
                        "TEXT NOT NULL, age INT)",
                        "INSERT INTO people (name, age) VALUES ('Ann', 30), "
                        "('Bob', NULL), ('Cy', 41)",
                        "SELECT id, name, age FROM people ORDER BY id"});
  QVERIFY(out.finished);
  QCOMPARE(out.results.size(), 3);
  QVERIFY2(out.results[0].ok, qPrintable(out.results[0].error));
  QVERIFY(!out.results[0].hasRows());
  QCOMPARE(out.results[1].rowsAffected, qint64(3));
  QVERIFY(out.results[2].ok);
  QCOMPARE(out.results[2].resultSets[0].rows.size(), 3);
  QCOMPARE(cell(out.results[2], 0, 1).toString(), QString("Ann"));
  QCOMPARE(cell(out.results[2], 0, 2).toInt(), 30);
  QVERIFY(cell(out.results[2], 1, 2).isNull());
  QCOMPARE(out.results[2].resultSets[0].columns[1].name, QString("name"));
  QVERIFY(out.results[2].elapsedMs >= 0);

  QVERIFY(waitForTable(conn, "people"));
  const DbTableInfo *t = conn.schema().findByName("people");
  QVERIFY(t);
  QCOMPARE(t->columns.size(), 3);
  QVERIFY(t->columns[0].primaryKey);
  QVERIFY(!t->columns[1].nullable);
  QCOMPARE(t->columns[1].type, QString("TEXT"));
}

void TestDatabaseLive::sqliteErrorsDoNotStopWhenAsked() {
  if (!m_sqliteAvailable) {
    QSKIP("Qt SQLite driver not installed");
  }
  DbConnection conn(sqliteProfile(makeSqliteFile(m_dir, "b.db")));
  QVERIFY(connectAndWait(conn, ""));
  auto out = run(conn, {"SELECT * FROM missing", "SELECT 1 AS one"}, true);
  QCOMPARE(out.results.size(), 1);
  QVERIFY(!out.results[0].ok);
  QVERIFY(out.results[0].error.contains("missing"));
  out = run(conn, {"SELECT * FROM missing", "SELECT 1 AS one"}, false);
  QCOMPARE(out.results.size(), 2);
  QVERIFY(!out.results[0].ok);
  QVERIFY(out.results[1].ok);
  QCOMPARE(cell(out.results[1], 0, 0).toInt(), 1);
}

void TestDatabaseLive::sqliteReadOnlyProfileRefusesWrites() {
  if (!m_sqliteAvailable) {
    QSKIP("Qt SQLite driver not installed");
  }
  const QString path = makeSqliteFile(m_dir, "ro.db");
  {
    DbConnection setup(sqliteProfile(path));
    QVERIFY(connectAndWait(setup, ""));
    run(setup, {"CREATE TABLE t (a INT)", "INSERT INTO t VALUES (1)"});
  }
  DbConnection conn(sqliteProfile(path, true));
  QVERIFY(connectAndWait(conn, ""));
  auto out = run(conn, {"DELETE FROM t"});
  QCOMPARE(out.results.size(), 1);
  QVERIFY(!out.results[0].ok);
  QVERIFY(out.results[0].error.contains("read-only"));
  out = run(conn, {"SELECT COUNT(*) FROM t"});
  QCOMPARE(cell(out.results[0], 0, 0).toInt(), 1);
}

void TestDatabaseLive::sqliteMissingFileFailsToOpen() {
  if (!m_sqliteAvailable) {
    QSKIP("Qt SQLite driver not installed");
  }
  DbConnection conn(sqliteProfile(m_dir.filePath("does-not-exist.db")));
  QString err;
  QVERIFY(!connectAndWait(conn, "", &err));
  QCOMPARE(conn.state(), DbConnectionState::Failed);
  QVERIFY(err.contains("does not exist"));

  const QString junk = m_dir.filePath("junk.db");
  QFile f(junk);
  QVERIFY(f.open(QIODevice::WriteOnly));
  f.write("this is definitely not a sqlite database file, just text........");
  f.close();
  DbConnection bad(sqliteProfile(junk));
  QVERIFY(!connectAndWait(bad, "", &err));
  QVERIFY(!err.isEmpty());
}

void TestDatabaseLive::sqliteRowLimit() {
  if (!m_sqliteAvailable) {
    QSKIP("Qt SQLite driver not installed");
  }
  DbConnection conn(sqliteProfile(makeSqliteFile(m_dir, "big.db")));
  conn.setMaxRows(50);
  QVERIFY(connectAndWait(conn, ""));
  auto out = run(conn, {"WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x+1 "
                        "FROM n WHERE x < 500) SELECT x FROM n"});
  QVERIFY2(out.results[0].ok, qPrintable(out.results[0].error));
  const auto &rs = out.results[0].resultSets[0];
  QCOMPARE(rs.rows.size(), 50);
  QCOMPARE(rs.totalRows, qint64(500));
  QVERIFY(rs.truncated);
}

void TestDatabaseLive::sqliteSchemaAndDisconnect() {
  if (!m_sqliteAvailable) {
    QSKIP("Qt SQLite driver not installed");
  }
  DbConnection conn(sqliteProfile(makeSqliteFile(m_dir, "c.db")));
  QVERIFY(connectAndWait(conn, ""));
  run(conn, {"CREATE TABLE a (x INT)", "CREATE VIEW v AS SELECT x FROM a"});
  QVERIFY(waitForTable(conn, "v"));
  QVERIFY(conn.schema().findByName("v"));
  QVERIFY(conn.schema().findByName("v")->isView);
  QVERIFY(!conn.schema().findByName("a")->isView);
  conn.disconnectFromServer();
  QCOMPARE(conn.state(), DbConnectionState::Disconnected);
  QVERIFY(conn.schema().isEmpty());

  auto out = run(conn, {"SELECT 7 AS n"});
  QVERIFY(out.finished);
  QVERIFY(out.results[0].ok);
  QCOMPARE(cell(out.results[0], 0, 0).toInt(), 7);
}

static void disconnectInsideHandler(DbConnection &conn, const QString &sql) {
  QEventLoop loop;
  bool done = false;
  auto c = QObject::connect(&conn, &DbConnection::statementFinished, &conn,
                            [&](quint64, int, const DbStatementResult &) {
                              conn.disconnectFromServer();
                              done = true;
                              loop.quit();
                            });
  conn.execute({sql});
  QTimer::singleShot(15000, &loop, &QEventLoop::quit);
  if (!done) {
    loop.exec();
  }
  QObject::disconnect(c);
  QVERIFY(done);
  QTest::qWait(300);
  QCOMPARE(conn.state(), DbConnectionState::Disconnected);
}

void TestDatabaseLive::sqliteDisconnectInsideResultHandler() {
  if (!m_sqliteAvailable) {
    QSKIP("Qt SQLite driver not installed");
  }
  DbConnection conn(sqliteProfile(makeSqliteFile(m_dir, "d.db")));
  QVERIFY(connectAndWait(conn, ""));
  disconnectInsideHandler(conn, "SELECT 1");

  auto out = run(conn, {"SELECT 2 AS two"});
  QVERIFY(out.results.value(0).ok);
  QCOMPARE(cell(out.results[0], 0, 0).toInt(), 2);
}

void TestDatabaseLive::engineWorkflow(DbEngine engine, const EnvTarget &target,
                                      const QString &database,
                                      const QString &suffix) {
  DbConnectionProfile p;
  p.id = "live-" + suffix;
  p.name = "live " + suffix;
  p.engine = engine;
  p.transport = DbTransport::Docker;
  p.container = target.container;
  p.user = target.user;
  p.database = database;

  if (engine != DbEngine::PostgreSql) {
    DbConnection bad(p);
    QString badErr;
    QVERIFY(!connectAndWait(bad, target.password + "-wrong", &badErr));
    QCOMPARE(bad.state(), DbConnectionState::Failed);
    QVERIFY2(!badErr.trimmed().isEmpty(), "expected a login error message");
    QVERIFY(!bad.hasSessionPassword());
  }

  {
    DbConnectionProfile missing = p;
    missing.container = "lp-no-such-container-xyz";
    DbConnection ghost(missing);
    QString ghostErr;
    QVERIFY(!connectAndWait(ghost, "x", &ghostErr));
    QVERIFY2(ghostErr.contains("No such container", Qt::CaseInsensitive) ||
                 ghostErr.contains("lp-no-such-container-xyz"),
             qPrintable(ghostErr));
  }

  DbConnection conn(p);
  QString err;
  QVERIFY2(connectAndWait(conn, target.password, &err), qPrintable(err));

  const QString table = "lp_live_" + suffix;
  DbTableInfo tmp;
  tmp.name = table;
  const QString q = DbCatalog::quoteIdentifier(engine, table);

  run(conn, {"DROP TABLE IF EXISTS " + q});
  auto out = run(
      conn,
      {QString("CREATE TABLE %1 (id INT PRIMARY KEY, name VARCHAR(50) NOT "
               "NULL, note VARCHAR(100) NULL)")
           .arg(q),
       QString("INSERT INTO %1 (id, name, note) VALUES (1, 'Ann', 'multi word, "
               "with comma'), (2, 'Bob', NULL), (3, 'Cy', 'quote \"x\"')")
           .arg(q),
       QString("SELECT id, name, note FROM %1 ORDER BY id").arg(q),
       QString("UPDATE %1 SET note = 'u' WHERE id > 1").arg(q)});
  QCOMPARE(out.results.size(), 4);
  QVERIFY2(out.results[0].ok, qPrintable(out.results[0].error));
  QVERIFY2(out.results[1].ok, qPrintable(out.results[1].error));
  QCOMPARE(out.results[1].rowsAffected, qint64(3));
  QVERIFY2(out.results[2].ok, qPrintable(out.results[2].error));
  const auto &rs = out.results[2].resultSets.value(0);
  QCOMPARE(rs.rows.size(), 3);
  QCOMPARE(rs.columns.size(), 3);
  QCOMPARE(rs.columns[1].name.toLower(), QString("name"));
  QCOMPARE(rs.rows[0][1].toString(), QString("Ann"));
  QCOMPARE(rs.rows[0][2].toString(), QString("multi word, with comma"));
  QVERIFY(rs.rows[1][2].isNull());
  QCOMPARE(rs.rows[2][2].toString(), QString("quote \"x\""));
  QVERIFY2(out.results[3].ok, qPrintable(out.results[3].error));
  QCOMPARE(out.results[3].rowsAffected, qint64(2));

  out = run(conn, {"SELECT * FROM lp_definitely_missing_table"});
  QVERIFY(!out.results[0].ok);
  QVERIFY2(out.results[0].error.contains("lp_definitely_missing_table"),
           qPrintable(out.results[0].error));
  out = run(conn, {"SELECT 1 AS still_alive"});
  QVERIFY2(out.results[0].ok, qPrintable(out.results[0].error));
  QCOMPARE(cell(out.results[0], 0, 0).toInt(), 1);

  out = run(conn,
            {QString("SELECT N'héllo 世界' AS u")
                 .replace("N'", engine == DbEngine::SqlServer ? "N'" : "'")});
  QVERIFY2(out.results[0].ok, qPrintable(out.results[0].error));
  QCOMPARE(cell(out.results[0], 0, 0).toString(), QString("héllo 世界"));

  QVERIFY2(waitForTable(conn, table), "table missing from schema");
  const DbTableInfo *info = conn.schema().findByName(table);
  QVERIFY2(info, "table missing from schema");
  QCOMPARE(info->columns.size(), 3);
  QVERIFY(info->columns[0].primaryKey);
  QVERIFY(!info->columns[1].nullable);
  QVERIFY(info->columns[2].nullable);
  QVERIFY(!conn.databases().isEmpty());

  if (engine == DbEngine::PostgreSql) {

    out = run(conn, {"\\dt"});
    QVERIFY2(out.results.value(0).ok, qPrintable(out.results.value(0).error));
    QVERIFY(out.results[0].hasRows());
    bool listed = false;
    for (const auto &row : out.results[0].resultSets[0].rows) {
      listed = listed || row.value(1).toString() == table;
    }
    QVERIFY2(listed, "\\dt did not list the test table");
  }

  out = run(conn,
            {"SELECT 1 AS a", "SELECT * FROM lp_definitely_missing_table",
             "SELECT 3 AS c"},
            false);
  QCOMPARE(out.results.size(), 3);
  QVERIFY(out.results[0].ok);
  QVERIFY(!out.results[1].ok);
  QVERIFY(out.results[2].ok);
  QCOMPARE(cell(out.results[2], 0, 0).toInt(), 3);

  QString sleepSql;
  switch (engine) {
  case DbEngine::SqlServer:
    sleepSql = "WAITFOR DELAY '00:00:20'";
    break;
  case DbEngine::PostgreSql:
    sleepSql = "SELECT pg_sleep(20)";
    break;
  default:
    sleepSql = "SELECT SLEEP(20)";
    break;
  }
  {
    QSignalSpy finished(&conn, &DbConnection::requestFinished);
    const quint64 id = conn.execute({sleepSql});
    Q_UNUSED(id)
    QTest::qWait(700);
    QVERIFY(conn.isBusy());
    conn.cancel();
    QVERIFY(finished.count() > 0 || finished.wait(5000));
    QVERIFY(finished.last().at(1).toBool());
  }
  out = run(conn, {"SELECT 42 AS after_cancel"});
  QVERIFY2(out.finished, "no reply after cancel");
  QVERIFY2(out.results.value(0).ok, qPrintable(out.results.value(0).error));
  QCOMPARE(cell(out.results[0], 0, 0).toInt(), 42);

  run(conn, {"DROP TABLE " + q});
  disconnectInsideHandler(conn, "SELECT 1");
  QVERIFY(connectAndWait(conn, target.password));
  out = run(conn, {"SELECT 5 AS five"});
  QVERIFY2(out.results.value(0).ok, qPrintable(out.results.value(0).error));
  conn.disconnectFromServer();
}

void TestDatabaseLive::missingClientProgramGivesAHelpfulError() {

  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", "/nonexistent");
  DbConnectionProfile p;
  p.id = "nopsql";
  p.name = "no psql";
  p.engine = DbEngine::PostgreSql;
  p.transport = DbTransport::Direct;
  {
    DbConnection conn(p);
    QString err;
    QVERIFY(!connectAndWait(conn, "pw", &err));
    QCOMPARE(conn.state(), DbConnectionState::Failed);
    QVERIFY2(err.contains("psql"), qPrintable(err));
    QVERIFY(!conn.hasSessionPassword());
  }

  p.transport = DbTransport::Docker;
  p.container = "whatever";
  {
    DbConnection conn(p);
    QString err;
    QVERIFY(!connectAndWait(conn, "pw", &err));
    QVERIFY2(err.contains("docker"), qPrintable(err));
  }
  qputenv("PATH", oldPath);
}

void TestDatabaseLive::sqlServerWorkflow() {
  const EnvTarget t = targetFromEnv("LIGHTPAD_TEST_MSSQL");
  if (!t.valid) {
    QSKIP("LIGHTPAD_TEST_MSSQL not set");
  }
  engineWorkflow(DbEngine::SqlServer, t, "master", "mssql");
}

void TestDatabaseLive::postgresWorkflow() {
  const EnvTarget t = targetFromEnv("LIGHTPAD_TEST_PG");
  if (!t.valid) {
    QSKIP("LIGHTPAD_TEST_PG not set");
  }
  engineWorkflow(DbEngine::PostgreSql, t, "postgres", "pg");
}

void TestDatabaseLive::mysqlWorkflow() {
  const EnvTarget t = targetFromEnv("LIGHTPAD_TEST_MYSQL");
  if (!t.valid) {
    QSKIP("LIGHTPAD_TEST_MYSQL not set");
  }

  engineWorkflow(DbEngine::MySql, t, "lp_live_db", "mysql");
}

QTEST_MAIN(TestDatabaseLive)
#include "test_databaselive.moc"
