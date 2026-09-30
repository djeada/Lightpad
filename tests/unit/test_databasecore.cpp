#include "database/clidialect.h"
#include "database/columnprofiler.h"
#include "database/connectionstore.h"
#include "database/databasemanager.h"
#include "database/dbcatalog.h"
#include "database/dockerdiscovery.h"
#include "database/queryhistory.h"
#include "database/resultexporter.h"
#include "database/sqlcompletion.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using K = SqlStatementKind;

class TestDatabaseCore : public QObject {
  Q_OBJECT

private slots:

  void profileJsonRoundTrip();
  void profileNeverSerialisesPasswords();
  void profileValidation();
  void engineFromImage_data();
  void engineFromImage();
  void profileSummary();

  void csvParserHandlesQuotesAndNewlines();
  void batchTsvUnescapes();
  void postgresParsesRowsAndNulls();
  void postgresParsesCommandTags();
  void postgresParsesErrors();
  void postgresConnectMessageIsNotAResult();
  void mysqlParsesRowsAndRowCount();
  void mysqlParsesErrorsAndIgnoresEcho();
  void sqlServerParsesGrid();
  void sqlServerParsesMultipleGrids();
  void sqlServerParsesErrorsAndMessages();
  void sqlServerMultilineValues();
  void sqlServerUnnamedColumn();
  void resultRowCapIsReported();
  void dockerLaunchDoesNotLeakPassword();
  void directLaunchUsesEnvironmentForPassword();
  void launchReportsMissingClient();

  void catalogBuildsSchema();
  void catalogQuotingAndTemplates();
  void schemaLookupByName();

  void exportCsv();
  void exportJsonMarkdownInsert();
  void profilerNumbers();
  void profilerText();
  void profilerTemporalAndBoolean();

  void dockerPsParsing();
  void dockerPortParsing();
  void dockerCredentialHints();
  void dockerProfileSuggestion();

  void completionAliasMap();
  void completionColumnsAfterAlias();
  void completionTablesAfterFrom();
  void completionColumnsInSelectFromReferencedTables();
  void completionKeywordCasing();
  void completionQuotesAwkwardNames();
  void completionWithoutSchemaStillOffersKeywords();

  void connectionStoreRoundTrip();
  void connectionStoreUniqueNames();
  void historyDeduplicatesAndSearches();
  void managerPersistsProfilesAndActive();
};

static DbResultSet makeResult(const QStringList &cols,
                              const QVector<QVector<QVariant>> &rows) {
  DbResultSet rs;
  for (const QString &c : cols) {
    rs.columns.append({c, QString()});
  }
  rs.rows = rows;
  rs.totalRows = rows.size();
  return rs;
}

void TestDatabaseCore::profileJsonRoundTrip() {
  DbConnectionProfile p;
  p.id = "abc";
  p.name = "Local";
  p.engine = DbEngine::SqlServer;
  p.transport = DbTransport::Docker;
  p.container = "sqlserver-demo";
  p.user = "sa";
  p.database = "master";
  p.port = 14330;
  p.readOnly = true;
  p.confirmDestructive = false;
  p.extraArgs = "-N";
  p.color = "#ff0000";
  const DbConnectionProfile q = DbConnectionProfile::fromJson(p.toJson());
  QCOMPARE(q.id, p.id);
  QCOMPARE(q.name, p.name);
  QCOMPARE(q.engine, p.engine);
  QCOMPARE(q.transport, p.transport);
  QCOMPARE(q.container, p.container);
  QCOMPARE(q.port, 14330);
  QVERIFY(q.readOnly);
  QVERIFY(!q.confirmDestructive);
  QCOMPARE(q.extraArgs, QString("-N"));
  QCOMPARE(q.color, QString("#ff0000"));
}

void TestDatabaseCore::profileNeverSerialisesPasswords() {
  DbConnectionProfile p;
  p.name = "x";
  const QByteArray json = QJsonDocument(p.toJson()).toJson();
  QVERIFY(!json.toLower().contains("password"));
}

void TestDatabaseCore::profileValidation() {
  DbConnectionProfile p;
  QString why;
  QVERIFY(!p.isValid(&why));
  p.name = "n";
  p.engine = DbEngine::Sqlite;
  QVERIFY(!p.isValid(&why));
  p.filePath = "/tmp/x.db";
  QVERIFY(p.isValid());
  p.engine = DbEngine::PostgreSql;
  p.transport = DbTransport::Docker;
  QVERIFY(!p.isValid(&why));
  p.container = "pg";
  QVERIFY(p.isValid());
  p.transport = DbTransport::Direct;
  p.host = "";
  QVERIFY(!p.isValid());
  p.host = "db";
  p.port = 70000;
  QVERIFY(!p.isValid());
}

void TestDatabaseCore::engineFromImage_data() {
  QTest::addColumn<QString>("image");
  QTest::addColumn<bool>("matches");
  QTest::addColumn<int>("engine");
  auto E = [](DbEngine e) { return int(e); };
  QTest::newRow("mssql") << "mcr.microsoft.com/mssql/server:2022-latest" << true
                         << E(DbEngine::SqlServer);
  QTest::newRow("azure sql edge")
      << "mcr.microsoft.com/azure-sql-edge" << false << 0;
  QTest::newRow("postgres")
      << "postgres:16-alpine" << true << E(DbEngine::PostgreSql);
  QTest::newRow("postgis") << "postgis/postgis:15-3.4" << true
                           << E(DbEngine::PostgreSql);
  QTest::newRow("registry port") << "myreg:5000/library/postgres@sha256:abc"
                                 << true << E(DbEngine::PostgreSql);
  QTest::newRow("mariadb") << "mariadb:11" << true << E(DbEngine::MySql);
  QTest::newRow("mysql") << "docker.io/library/mysql:8" << true
                         << E(DbEngine::MySql);
  QTest::newRow("redis") << "redis:7" << false << 0;
  QTest::newRow("kibana") << "docker.elastic.co/kibana/kibana:8.6.0" << false
                          << 0;
}

void TestDatabaseCore::engineFromImage() {
  QFETCH(QString, image);
  QFETCH(bool, matches);
  QFETCH(int, engine);
  DbEngine found = DbEngine::Sqlite;
  QCOMPARE(DbEngineInfo::engineForImage(image, &found), matches);
  if (matches) {
    QCOMPARE(int(found), engine);
  }
}

void TestDatabaseCore::profileSummary() {
  DbConnectionProfile p;
  p.engine = DbEngine::PostgreSql;
  p.user = "u";
  p.host = "h";
  p.database = "d";
  QCOMPARE(p.summary(), QString("u@h:5432/d"));
  p.transport = DbTransport::Docker;
  p.container = "pg1";
  QCOMPARE(p.summary(), QString("u@docker:pg1/d"));
}

void TestDatabaseCore::csvParserHandlesQuotesAndNewlines() {
  const auto rec =
      CliParsing::parseCsv("a,b,c\n1,\"x,\"\"y\"\"\nz\",\n\"\",2,3\r\n");
  QCOMPARE(rec.size(), 3);
  QCOMPARE(rec[1], (QStringList{"1", "x,\"y\"\nz", ""}));
  QCOMPARE(rec[2], (QStringList{"", "2", "3"}));
}

void TestDatabaseCore::batchTsvUnescapes() {
  const auto rec = CliParsing::parseBatchTsv("a\tb\n1\tx\\ty\\nz\\\\w\n");
  QCOMPARE(rec.size(), 2);
  QCOMPARE(rec[1], (QStringList{"1", "x\ty\nz\\w"}));
}

void TestDatabaseCore::postgresParsesRowsAndNulls() {
  auto d = CliDialect::create(DbEngine::PostgreSql);
  const QString out = "a,b,c\n1,\"x,\"\"y\"\"\nz\",__LP_NULL__\n2,plain,3\n";
  const auto r = d->parse("SELECT 1", K::Select, out, "", 100);
  QVERIFY(r.ok);
  QCOMPARE(r.resultSets.size(), 1);
  const auto &rs = r.resultSets[0];
  QCOMPARE(rs.columns.size(), 3);
  QCOMPARE(rs.rows.size(), 2);
  QCOMPARE(rs.rows[0][1].toString(), QString("x,\"y\"\nz"));
  QVERIFY(rs.rows[0][2].isNull());
  QCOMPARE(rs.rows[1][2].toString(), QString("3"));
}

void TestDatabaseCore::postgresParsesCommandTags() {
  auto d = CliDialect::create(DbEngine::PostgreSql);
  auto r =
      d->parse("INSERT INTO t VALUES (1),(2)", K::Dml, "INSERT 0 2\n", "", 100);
  QVERIFY(r.ok);
  QVERIFY(!r.hasRows());
  QCOMPARE(r.rowsAffected, qint64(2));
  r = d->parse("UPDATE t SET a=3", K::Dml, "UPDATE 5\n", "", 100);
  QCOMPARE(r.rowsAffected, qint64(5));
  r = d->parse("CREATE TABLE t(a int)", K::Ddl, "CREATE TABLE\n", "", 100);
  QVERIFY(!r.hasRows());
  QVERIFY(r.ok);

  r = d->parse("INSERT INTO t VALUES (1) RETURNING a", K::Dml,
               "a\n1\nINSERT 0 1\n", "", 100);
  QCOMPARE(r.rowsAffected, qint64(1));
  QCOMPARE(r.resultSets.size(), 1);
  QCOMPARE(r.resultSets[0].rows.size(), 1);

  r = d->parse("SELECT 'UPDATE 1' AS x", K::Select, "x\nUPDATE 1\n", "", 100);
  QCOMPARE(r.resultSets.size(), 1);
  QCOMPARE(r.resultSets[0].rows.size(), 1);
}

void TestDatabaseCore::postgresParsesErrors() {
  auto d = CliDialect::create(DbEngine::PostgreSql);
  const auto r = d->parse(
      "SELECT * FROM nosuch", K::Select, "",
      "ERROR:  relation \"nosuch\" does not exist\nLINE 1: SELECT * FROM "
      "nosuch;\n                      ^\nNOTICE:  something\n",
      100);
  QVERIFY(!r.ok);
  QVERIFY(r.error.startsWith("relation \"nosuch\" does not exist"));
  QVERIFY(r.error.contains("LINE 1"));
  QVERIFY(r.messages.contains("NOTICE:  something"));
}

void TestDatabaseCore::postgresConnectMessageIsNotAResult() {
  auto d = CliDialect::create(DbEngine::PostgreSql);
  const auto r = d->parse("\\c \"other\"", K::Other,
                          "You are now connected to database \"other\" as user "
                          "\"postgres\".\n",
                          "", 100);
  QVERIFY(r.ok);
  QVERIFY(!r.hasRows());
  QVERIFY(!r.messages.isEmpty());
}

void TestDatabaseCore::mysqlParsesRowsAndRowCount() {
  auto d = CliDialect::create(DbEngine::MySql);
  auto r =
      d->parse("SELECT 1", K::Select, "a\tb\tc\n1\tx\\ty\\nz\tNULL\n", "", 100);
  QVERIFY(r.ok);
  QCOMPARE(r.resultSets[0].rows.size(), 1);
  QCOMPARE(r.resultSets[0].rows[0][1].toString(), QString("x\ty\nz"));
  QVERIFY(r.resultSets[0].rows[0][2].isNull());
  r = d->parse("INSERT INTO t VALUES (1),(2)", K::Dml, "lp_rows\n2\n", "", 100);
  QVERIFY(!r.hasRows());
  QCOMPARE(r.rowsAffected, qint64(2));
  r = d->parse("CREATE TABLE t (a int)", K::Ddl, "lp_rows\n0\n", "", 100);
  QCOMPARE(r.rowsAffected, qint64(0));
}

void TestDatabaseCore::mysqlParsesErrorsAndIgnoresEcho() {
  auto d = CliDialect::create(DbEngine::MySql);
  const auto r = d->parse(
      "SELECT * FROM nosuch", K::Select, "",
      "--------------\nSELECT * FROM nosuch\n--------------\n\nERROR 1146 "
      "(42S02) at line 8: Table 'd1.nosuch' doesn't exist\n",
      100);
  QVERIFY(!r.ok);
  QCOMPARE(r.error, QString("Table 'd1.nosuch' doesn't exist (1146)"));
  QVERIFY(r.messages.isEmpty());
}

void TestDatabaseCore::sqlServerParsesGrid() {
  auto d = CliDialect::create(DbEngine::SqlServer);
  const QChar s(0x1f);
  const QString out =
      QString("a%1b%1c\n-%1-%1-\n1%1x y%1NULL\n2%1z%1w\n\n(2 rows affected)\n")
          .arg(s);
  const auto r = d->parse("SELECT ...", K::Select, out, "", 100);
  QVERIFY(r.ok);
  QCOMPARE(r.resultSets.size(), 1);
  QCOMPARE(r.resultSets[0].columns[1].name, QString("b"));
  QCOMPARE(r.resultSets[0].rows.size(), 2);
  QVERIFY(r.resultSets[0].rows[0][2].isNull());
  QCOMPARE(r.resultSets[0].rows[0][1].toString(), QString("x y"));
  QCOMPARE(r.rowsAffected, qint64(2));
}

void TestDatabaseCore::sqlServerParsesMultipleGrids() {
  auto d = CliDialect::create(DbEngine::SqlServer);
  const QChar s(0x1f);
  const QString out = QString("a\n-\n1\n\nb%1c\n-%1-\nx%1y\n\n").arg(s);
  const auto r = d->parse("EXEC p", K::Call, out, "", 100);
  QCOMPARE(r.resultSets.size(), 2);
  QCOMPARE(r.resultSets[1].columns.size(), 2);
}

void TestDatabaseCore::sqlServerParsesErrorsAndMessages() {
  auto d = CliDialect::create(DbEngine::SqlServer);
  auto r = d->parse(
      "SELECT * FROM nosuch", K::Select, "",
      "hello\nMsg 208, Level 16, State 1, Server 449ce4b7daa0, Line 1\nInvalid "
      "object name 'nosuch'.\n",
      100);
  QVERIFY(!r.ok);
  QVERIFY(r.error.contains("Invalid object name 'nosuch'."));
  QVERIFY(r.error.contains("Msg 208"));
  QVERIFY(r.messages.contains("hello"));

  r = d->parse("USE x", K::Session, "",
               "Msg 5701, Level 0, State 1, Server s, Line 1\nChanged database "
               "context to 'x'.\n",
               100);
  QVERIFY(r.ok);
  QVERIFY(r.messages.contains("Changed database context to 'x'."));
  r = d->parse("x", K::Other, "",
               "Sqlcmd: Error: Login failed for user 'sa'.\n", 100);
  QVERIFY(!r.ok);
  QVERIFY(r.error.contains("Login failed"));
}

void TestDatabaseCore::sqlServerMultilineValues() {
  auto d = CliDialect::create(DbEngine::SqlServer);
  const QChar s(0x1f);
  const QString out =
      QString("a%1b\n-%1-\n1%1line one\nline two\n2%1single\n\n").arg(s);
  const auto r = d->parse("SELECT", K::Select, out, "", 100);
  QCOMPARE(r.resultSets[0].rows.size(), 2);
  QCOMPARE(r.resultSets[0].rows[0][1].toString(),
           QString("line one\nline two"));
}

void TestDatabaseCore::sqlServerUnnamedColumn() {
  auto d = CliDialect::create(DbEngine::SqlServer);
  const QChar s(0x1f);
  const QString out =
      QString("\n-------------------\nMicrosoft SQL Server 2025\n\n"
              "a%1\n-%1-\n")
          .arg(s);
  const auto r = d->parse("SELECT @@VERSION", K::Select, out, "", 100);
  QVERIFY(r.ok);
  QVERIFY(!r.resultSets.isEmpty());
  QCOMPARE(r.resultSets[0].columns[0].name, QString("(No column name)"));
  QCOMPARE(r.resultSets[0].rows[0][0].toString(),
           QString("Microsoft SQL Server 2025"));
}

void TestDatabaseCore::resultRowCapIsReported() {
  auto d = CliDialect::create(DbEngine::PostgreSql);
  QString out = "n\n";
  for (int i = 0; i < 50; ++i) {
    out += QString::number(i) + "\n";
  }
  const auto r = d->parse("SELECT n", K::Select, out, "", 10);
  QCOMPARE(r.resultSets[0].rows.size(), 10);
  QCOMPARE(r.resultSets[0].totalRows, qint64(50));
  QVERIFY(r.resultSets[0].truncated);
}

void TestDatabaseCore::dockerLaunchDoesNotLeakPassword() {
  DbConnectionProfile p;
  p.engine = DbEngine::SqlServer;
  p.transport = DbTransport::Docker;
  p.container = "sqlserver-demo";
  p.user = "sa";
  auto d = CliDialect::create(p.engine);
  const CliLaunch l = d->launch(p, "S3cret!pw");
  QVERIFY(l.error.isEmpty());
  QCOMPARE(l.program, QString("docker"));
  QVERIFY(!l.arguments.join(' ').contains("S3cret!pw"));
  QVERIFY(l.environment.isEmpty());
  QCOMPARE(l.stdinPreamble, QByteArray("S3cret!pw\n"));
  QVERIFY(l.arguments.contains("sqlserver-demo"));
  QVERIFY(l.arguments.join(' ').contains("SQLCMDPASSWORD"));
  QVERIFY(l.arguments.join(' ').contains("/opt/mssql-tools18/bin/sqlcmd"));
}

void TestDatabaseCore::directLaunchUsesEnvironmentForPassword() {
  DbConnectionProfile p;
  p.engine = DbEngine::PostgreSql;
  p.host = "db.example";
  p.port = 5433;
  p.user = "app";
  p.database = "shop";
  p.clientPath = "/usr/bin/psql";
  auto d = CliDialect::create(p.engine);
  const CliLaunch l = d->launch(p, "pw");
  QVERIFY(l.error.isEmpty());
  QCOMPARE(l.program, QString("/usr/bin/psql"));
  QVERIFY(!l.arguments.join(' ').contains("pw "));
  QCOMPARE(l.environment.value("PGPASSWORD"), QString("pw"));
  QVERIFY(l.arguments.contains("db.example"));
  QVERIFY(l.arguments.contains("5433"));
  QVERIFY(l.arguments.contains("shop"));
  QVERIFY(l.arguments.contains("-w"));
  QVERIFY(l.stdinPreamble.isEmpty());
}

void TestDatabaseCore::launchReportsMissingClient() {
  DbConnectionProfile p;
  p.engine = DbEngine::SqlServer;
  p.clientPath.clear();
  auto d = CliDialect::create(p.engine);
  qputenv("PATH", "/nonexistent");
  const CliLaunch l = d->launch(p, "x");
  QVERIFY(!l.error.isEmpty());
  QVERIFY(l.error.contains("sqlcmd"));
}

static DbResultSet catalogRows() {
  return makeResult(
      {"s", "t", "tt", "c", "ty", "n", "pk", "o"},
      {{"public", "users", "BASE TABLE", "id", "integer", "NO", "YES", 1},
       {"public", "users", "BASE TABLE", "email", "text", "YES", "NO", 2},
       {"public", "active_users", "VIEW", "id", "integer", "YES", "NO", 1},
       {"audit", "log", "BASE TABLE", "at", "timestamp", "NO", "NO", 1}});
}

void TestDatabaseCore::catalogBuildsSchema() {
  const DbSchema s = DbCatalog::buildSchema(catalogRows());
  QCOMPARE(s.tables.size(), 3);
  QCOMPARE(s.columnCount(), 4);
  QCOMPARE(s.schemaNames(), (QStringList{"public", "audit"}));
  const DbTableInfo *users = s.find("public", "users");
  QVERIFY(users);
  QCOMPARE(users->columns.size(), 2);
  QVERIFY(users->columns[0].primaryKey);
  QVERIFY(!users->columns[0].nullable);
  QVERIFY(users->columns[1].nullable);
  QCOMPARE(users->primaryKey(), QStringList{"id"});
  QVERIFY(s.find("public", "active_users")->isView);
  QCOMPARE(s.tablesIn("audit").size(), 1);
}

void TestDatabaseCore::catalogQuotingAndTemplates() {
  QCOMPARE(DbCatalog::quoteIdentifier(DbEngine::MySql, "a`b"),
           QString("`a``b`"));
  QCOMPARE(DbCatalog::quoteIdentifier(DbEngine::SqlServer, "a]b"),
           QString("[a]]b]"));
  QCOMPARE(DbCatalog::quoteIdentifier(DbEngine::PostgreSql, "a\"b"),
           QString("\"a\"\"b\""));
  DbTableInfo t;
  t.schema = "dbo";
  t.name = "Orders";
  t.columns = {{"Id", "int", false, true}, {"Total", "money", true, false}};
  QCOMPARE(DbCatalog::selectTopSql(DbEngine::SqlServer, t, 100),
           QString("SELECT TOP 100 * FROM [dbo].[Orders];"));
  QCOMPARE(DbCatalog::selectTopSql(DbEngine::PostgreSql, t, 5),
           QString("SELECT * FROM \"dbo\".\"Orders\" LIMIT 5;"));
  QVERIFY(DbCatalog::countSql(DbEngine::MySql, t).contains("COUNT(*)"));
  QVERIFY(DbCatalog::updateTemplateSql(DbEngine::SqlServer, t)
              .contains("WHERE [Id]"));
  QVERIFY(DbCatalog::insertTemplateSql(DbEngine::SqlServer, t)
              .contains("[Id], [Total]"));
  QVERIFY(DbCatalog::createTableSql(DbEngine::SqlServer, t)
              .contains("PRIMARY KEY ([Id])"));
  QVERIFY(DbCatalog::createTableSql(DbEngine::SqlServer, t)
              .contains("[Id] int NOT NULL"));

  DbTableInfo lite;
  lite.schema = "main";
  lite.name = "t";
  QCOMPARE(DbCatalog::selectTopSql(DbEngine::Sqlite, lite, 3),
           QString("SELECT * FROM \"t\" LIMIT 3;"));
  QVERIFY(DbCatalog::useDatabaseStatement(DbEngine::Sqlite, "x").isEmpty());
  QCOMPARE(DbCatalog::useDatabaseStatement(DbEngine::SqlServer, "my db"),
           QString("USE [my db]"));
  QCOMPARE(DbCatalog::useDatabaseStatement(DbEngine::PostgreSql, "shop"),
           QString("\\c \"shop\""));
}

void TestDatabaseCore::schemaLookupByName() {
  const DbSchema s = DbCatalog::buildSchema(catalogRows());
  QVERIFY(s.findByName("USERS"));
  QVERIFY(s.findByName("public.users"));
  QVERIFY(s.findByName("\"audit\".\"log\""));
  QVERIFY(!s.findByName("audit.users"));
  QVERIFY(!s.findByName("nothing"));
}

void TestDatabaseCore::exportCsv() {
  const DbResultSet rs =
      makeResult({"id", "note"}, {{1, "plain"},
                                  {2, "with,comma"},
                                  {3, "quote \" and\nnewline"},
                                  {4, QVariant()}});
  const QString csv =
      ResultExporter::exportResult(rs, ResultExporter::Format::Csv);
  QCOMPARE(csv, QString("id,note\n1,plain\n2,\"with,comma\"\n3,\"quote \"\" "
                        "and\nnewline\"\n4,\n"));
  const QString sub = ResultExporter::exportResult(
      rs, ResultExporter::Format::Csv, {1}, {1}, false);
  QCOMPARE(sub, QString("\"with,comma\"\n"));

  const auto back = CliParsing::parseCsv(csv);
  QCOMPARE(back.size(), 5);
  QCOMPARE(back[3][1], QString("quote \" and\nnewline"));
}

void TestDatabaseCore::exportJsonMarkdownInsert() {
  const DbResultSet rs = makeResult(
      {"id", "name", "ok"}, {{1, "O'Brien", true}, {2, QVariant(), false}});
  const QString json =
      ResultExporter::exportResult(rs, ResultExporter::Format::Json);
  const QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();
  QCOMPARE(arr.size(), 2);
  QCOMPARE(arr[0].toObject().value("id").toInt(), 1);
  QVERIFY(arr[1].toObject().value("name").isNull());
  QCOMPARE(arr[0].toObject().value("ok").toBool(), true);

  const QString md =
      ResultExporter::exportResult(rs, ResultExporter::Format::Markdown);
  QVERIFY(md.startsWith("| id | name | ok |\n| --- | --- | --- |\n"));

  const QString ins = ResultExporter::exportResult(
      rs, ResultExporter::Format::SqlInsert, {}, {}, true, "\"people\"",
      DbEngine::PostgreSql);
  QVERIFY(
      ins.contains("INSERT INTO \"people\" (\"id\", \"name\", \"ok\") VALUES "
                   "(1, 'O''Brien', 1);"));
  QVERIFY(ins.contains("VALUES (2, NULL, 0);"));
  QCOMPARE(ResultExporter::sqlLiteral(QString("007")), QString("'007'"));
  QCOMPARE(ResultExporter::sqlLiteral(QString("42")), QString("42"));
}

void TestDatabaseCore::profilerNumbers() {
  DbResultSet rs =
      makeResult({"v"}, {{"1"}, {"2"}, {"3"}, {"4"}, {QVariant()}, {"4"}});
  const ColumnProfile p = ColumnProfiler::profileColumn(rs, 0);
  QCOMPARE(p.kind, ColumnKind::Number);
  QCOMPARE(p.rows, qint64(6));
  QCOMPARE(p.nulls, qint64(1));
  QCOMPARE(p.distinct, qint64(4));
  QCOMPARE(p.min, QString("1"));
  QCOMPARE(p.max, QString("4"));
  QVERIFY(qAbs(p.mean - 2.8) < 1e-9);
  QVERIFY(qAbs(p.median - 3.0) < 1e-9);
  QVERIFY(qAbs(p.sum - 14.0) < 1e-9);
  QVERIFY(qAbs(p.stddev - 1.3038404810405297) < 1e-9);
  QVERIFY(!p.isUnique());
  QCOMPARE(p.top.first().value, QString("4"));
  QCOMPARE(p.top.first().count, qint64(2));
  QVERIFY(qAbs(p.nullPercent() - 100.0 / 6) < 1e-9);
}

void TestDatabaseCore::profilerText() {
  DbResultSet rs = makeResult({"v"}, {{"pear"}, {"apple"}, {"fig"}, {"apple"}});
  const ColumnProfile p = ColumnProfiler::profileColumn(rs, 0);
  QCOMPARE(p.kind, ColumnKind::Text);
  QCOMPARE(p.min, QString("apple"));
  QCOMPARE(p.max, QString("pear"));
  QCOMPARE(p.minLength, 3);
  QCOMPARE(p.maxLength, 5);
  QVERIFY(!p.hasNumbers);

  rs = makeResult({"v"}, {{"1"}, {"x"}});
  QCOMPARE(ColumnProfiler::profileColumn(rs, 0).kind, ColumnKind::Text);
  rs = makeResult({"v"}, {{QVariant()}, {QVariant()}});
  QCOMPARE(ColumnProfiler::profileColumn(rs, 0).kind, ColumnKind::Empty);
  rs = makeResult({"v"}, {{"a"}, {"b"}});
  QVERIFY(ColumnProfiler::profileColumn(rs, 0).isUnique());
}

void TestDatabaseCore::profilerTemporalAndBoolean() {
  DbResultSet rs = makeResult(
      {"d"},
      {{"2024-01-02"}, {"2024-01-03 10:20:30"}, {"2023-12-31T01:02:03Z"}});
  QCOMPARE(ColumnProfiler::profileColumn(rs, 0).kind, ColumnKind::Temporal);
  rs = makeResult({"b"}, {{"true"}, {"false"}, {"true"}});
  QCOMPARE(ColumnProfiler::profileColumn(rs, 0).kind, ColumnKind::Boolean);
  double d = 0;
  QVERIFY(ColumnProfiler::parseNumber(QString(" -3.5e2 "), &d));
  QCOMPARE(d, -350.0);
  QVERIFY(!ColumnProfiler::parseNumber(QString("abc"), &d));
  QVERIFY(!ColumnProfiler::parseNumber(QString("12abc"), &d));
  QVERIFY(ColumnProfiler::parseNumber(QVariant(12), &d));
}

void TestDatabaseCore::dockerPsParsing() {
  const QString out = "449ce4b7daa0\tlp-mssql\tmcr.microsoft.com/mssql/"
                      "server:2025-latest\tUp 2 "
                      "minutes\t0.0.0.0:14330->1433/tcp, :::14330->1433/tcp\n"
                      "aaaa\tcache\tredis:7\tUp 1 hour\t6379/tcp\n"
                      "bbbb\tpg\tpostgres:16-alpine\tExited (0) 3 days ago\t\n"
                      "\n";
  const auto list = DockerDiscoveryParsing::parsePs(out);
  QCOMPARE(list.size(), 3);
  QCOMPARE(list[0].name, QString("lp-mssql"));
  QVERIFY(list[0].isDatabase);
  QCOMPARE(list[0].engine, DbEngine::SqlServer);
  QCOMPARE(list[0].hostPort, 14330);
  QVERIFY(list[0].running);
  QCOMPARE(list[1].name, QString("pg"));
  QVERIFY(!list[1].running);
  QVERIFY(!list[2].isDatabase);
}

void TestDatabaseCore::dockerPortParsing() {
  QCOMPARE(DockerDiscoveryParsing::publishedHostPort(
               "0.0.0.0:15432->5432/tcp, [::]:15432->5432/tcp", 5432),
           15432);
  QCOMPARE(DockerDiscoveryParsing::publishedHostPort("5432/tcp", 5432), 0);
  QCOMPARE(
      DockerDiscoveryParsing::publishedHostPort("0.0.0.0:80->80/tcp", 5432), 0);
}

void TestDatabaseCore::dockerCredentialHints() {
  auto h = DockerDiscoveryParsing::credentialsFromEnv(
      DbEngine::SqlServer, {"ACCEPT_EULA=Y", "MSSQL_SA_PASSWORD=Pw#1", "A=b"});
  QCOMPARE(h.user, QString("sa"));
  QCOMPARE(h.password, QString("Pw#1"));
  h = DockerDiscoveryParsing::credentialsFromEnv(
      DbEngine::PostgreSql, {"POSTGRES_PASSWORD=x=y", "POSTGRES_USER=app"});
  QCOMPARE(h.user, QString("app"));
  QCOMPARE(h.database, QString("app"));
  QCOMPARE(h.password, QString("x=y"));
  h = DockerDiscoveryParsing::credentialsFromEnv(DbEngine::MySql,
                                                 {"MARIADB_ROOT_PASSWORD=r"});
  QCOMPARE(h.user, QString("root"));
  QCOMPARE(h.password, QString("r"));
  h = DockerDiscoveryParsing::credentialsFromEnv(
      DbEngine::MySql,
      {"MYSQL_USER=bob", "MYSQL_PASSWORD=p", "MYSQL_DATABASE=d"});
  QCOMPARE(h.user, QString("bob"));
  QCOMPARE(h.database, QString("d"));
  QVERIFY(!DockerDiscoveryParsing::credentialsFromEnv(DbEngine::PostgreSql, {})
               .hasPassword());
}

void TestDatabaseCore::dockerProfileSuggestion() {
  DockerContainer c;
  c.name = "shop-db";
  c.engine = DbEngine::PostgreSql;
  c.isDatabase = true;
  c.hostPort = 15432;
  DockerCredentialHints hints;
  hints.user = "shop";
  hints.database = "shopdb";
  const DbConnectionProfile p = DockerDiscoveryParsing::profileFor(c, hints);
  QCOMPARE(p.transport, DbTransport::Docker);
  QCOMPARE(p.container, QString("shop-db"));
  QCOMPARE(p.user, QString("shop"));
  QCOMPARE(p.database, QString("shopdb"));
  QCOMPARE(p.name, QString("shop-db"));
  const DbConnectionProfile d = DockerDiscoveryParsing::profileFor(c);
  QCOMPARE(d.user, QString("postgres"));
  QCOMPARE(d.database, QString("postgres"));
}

static DbSchema shopSchema() {
  DbResultSet rs = makeResult(
      {"s", "t", "tt", "c", "ty", "n", "pk", "o"},
      {{"public", "users", "BASE TABLE", "id", "integer", "NO", "YES", 1},
       {"public", "users", "BASE TABLE", "email", "text", "YES", "NO", 2},
       {"public", "orders", "BASE TABLE", "id", "integer", "NO", "YES", 1},
       {"public", "orders", "BASE TABLE", "user_id", "integer", "NO", "NO", 2},
       {"public", "orders", "BASE TABLE", "Total Amount", "numeric", "YES",
        "NO", 3}});
  return DbCatalog::buildSchema(rs);
}

static QStringList labels(const QVector<SqlSuggestion> &v) {
  QStringList out;
  for (const auto &s : v) {
    out << s.label;
  }
  return out;
}

void TestDatabaseCore::completionAliasMap() {
  const auto m = SqlCompletion::aliasMap(
      "SELECT * FROM public.users AS u JOIN orders o ON o.user_id = u.id "
      "LEFT JOIN \"Items\" WHERE 1=1");
  QCOMPARE(m.value("u"), QString("public.users"));
  QCOMPARE(m.value("o"), QString("orders"));
  QCOMPARE(m.value("users"), QString("public.users"));
  QVERIFY(m.contains("items"));
  QVERIFY(!m.contains("where"));
  QVERIFY(!m.contains("join"));
  QCOMPARE(SqlCompletion::referencedTables("UPDATE users SET a=1").size(), 1);
}

void TestDatabaseCore::completionColumnsAfterAlias() {
  const DbSchema s = shopSchema();
  const QString full = "SELECT u. FROM users u";
  const auto r =
      SqlCompletion::suggest(&s, DbEngine::PostgreSql, "SELECT u.", full, "");
  QCOMPARE(labels(r), (QStringList{"id", "email"}));
  const auto p = SqlCompletion::suggest(&s, DbEngine::PostgreSql, "SELECT u.em",
                                        full, "em");
  QCOMPARE(labels(p), QStringList{"email"});

  QVERIFY(SqlCompletion::suggest(&s, DbEngine::PostgreSql, "SELECT z.",
                                 "SELECT z.", "")
              .isEmpty());

  const auto t =
      SqlCompletion::suggest(&s, DbEngine::PostgreSql, "SELECT * FROM public.",
                             "SELECT * FROM public.", "");
  QCOMPARE(t.size(), 2);
}

void TestDatabaseCore::completionTablesAfterFrom() {
  const DbSchema s = shopSchema();
  const auto r = SqlCompletion::suggest(
      &s, DbEngine::PostgreSql, "SELECT * FROM us", "SELECT * FROM us", "us");
  QVERIFY(!r.isEmpty());
  QCOMPARE(r.first().label, QString("users"));
  QCOMPARE(int(r.first().kind), int(SqlSuggestion::Kind::Table));

  const auto all = SqlCompletion::suggest(
      &s, DbEngine::PostgreSql, "SELECT * FROM ", "SELECT * FROM ", "");
  QVERIFY(labels(all).indexOf("orders") < labels(all).indexOf("WHERE"));
}

void TestDatabaseCore::completionColumnsInSelectFromReferencedTables() {
  const DbSchema s = shopSchema();
  const QString full = "SELECT  FROM orders WHERE user_id = 1";
  const auto r =
      SqlCompletion::suggest(&s, DbEngine::PostgreSql, "SELECT us", full, "us");
  QVERIFY(labels(r).contains("user_id"));
  QCOMPARE(labels(r).first(), QString("user_id"));
  QCOMPARE(int(r.first().kind), int(SqlSuggestion::Kind::Column));
  QVERIFY(r.first().detail.contains("orders"));
}

void TestDatabaseCore::completionKeywordCasing() {
  const auto lower = SqlCompletion::suggest(nullptr, DbEngine::PostgreSql,
                                            "sel", "sel", "sel");
  QVERIFY(labels(lower).contains("select"));
  const auto upper = SqlCompletion::suggest(nullptr, DbEngine::PostgreSql,
                                            "SEL", "SEL", "SEL");
  QVERIFY(labels(upper).contains("SELECT"));
  const auto fn = SqlCompletion::suggest(nullptr, DbEngine::PostgreSql,
                                         "SELECT cou", "SELECT cou", "cou");
  bool found = false;
  for (const auto &x : fn) {
    if (x.label == "count") {
      found = true;
      QCOMPARE(x.insertText, QString("count("));
    }
  }
  QVERIFY(found);
}

void TestDatabaseCore::completionQuotesAwkwardNames() {
  QCOMPARE(SqlCompletion::identifierForInsert(DbEngine::PostgreSql, "users"),
           QString("users"));
  QCOMPARE(
      SqlCompletion::identifierForInsert(DbEngine::PostgreSql, "Total Amount"),
      QString("\"Total Amount\""));
  QCOMPARE(
      SqlCompletion::identifierForInsert(DbEngine::PostgreSql, "MixedCase"),
      QString("\"MixedCase\""));
  QCOMPARE(SqlCompletion::identifierForInsert(DbEngine::SqlServer, "MixedCase"),
           QString("MixedCase"));
  QCOMPARE(SqlCompletion::identifierForInsert(DbEngine::SqlServer, "order"),
           QString("[order]"));
  QCOMPARE(SqlCompletion::identifierForInsert(DbEngine::MySql, "my col"),
           QString("`my col`"));
  const DbSchema s = shopSchema();
  const auto r = SqlCompletion::suggest(&s, DbEngine::PostgreSql, "SELECT o.",
                                        "SELECT o. FROM orders o", "");
  bool quoted = false;
  for (const auto &x : r) {
    if (x.label == "Total Amount") {
      quoted = x.insertText == "\"Total Amount\"";
    }
  }
  QVERIFY(quoted);
}

void TestDatabaseCore::completionWithoutSchemaStillOffersKeywords() {
  const auto r = SqlCompletion::suggest(nullptr, DbEngine::SqlServer, "SELECT ",
                                        "SELECT ", "");
  QVERIFY(!r.isEmpty());
  QVERIFY(SqlCompletion::suggest(nullptr, DbEngine::SqlServer, "SELECT u.",
                                 "SELECT u.", "")
              .isEmpty());
}

void TestDatabaseCore::connectionStoreRoundTrip() {
  QTemporaryDir dir;
  const QString file = dir.filePath("sub/connections.json");
  {
    ConnectionStore store(file);
    DbConnectionProfile a;
    a.name = "A";
    a.engine = DbEngine::Sqlite;
    a.filePath = "/tmp/a.db";
    const QString id = store.upsert(a);
    QVERIFY(!id.isEmpty());
    store.setActiveId(id);
    QVERIFY(store.save());
  }
  ConnectionStore again(file);
  QVERIFY(again.load());
  QCOMPARE(again.profiles().size(), 1);
  QCOMPARE(again.profiles()[0].name, QString("A"));
  QCOMPARE(again.activeId(), again.profiles()[0].id);
  QVERIFY(again.findByName("a"));

  QFile f(file);
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  f.write("{not json");
  f.close();
  ConnectionStore bad(file);
  QVERIFY(!bad.load());
  QVERIFY(bad.profiles().isEmpty());
}

void TestDatabaseCore::connectionStoreUniqueNames() {
  QTemporaryDir dir;
  ConnectionStore store(dir.filePath("c.json"));
  DbConnectionProfile p;
  p.name = "Dev";
  const QString id = store.upsert(p);
  QCOMPARE(store.uniqueName("Dev"), QString("Dev 2"));
  QCOMPARE(store.uniqueName("Dev", id), QString("Dev"));
  QCOMPARE(store.uniqueName("Prod"), QString("Prod"));
  p.id = id;
  p.name = "Renamed";
  store.upsert(p);
  QCOMPARE(store.profiles().size(), 1);
  QVERIFY(store.remove(id));
  QVERIFY(!store.remove(id));
}

void TestDatabaseCore::historyDeduplicatesAndSearches() {
  QTemporaryDir dir;
  const QString file = dir.filePath("h.json");
  QueryHistory h(file, 3);
  QueryHistoryEntry e;
  e.connection = "A";
  e.sql = "SELECT 1";
  h.add(e);
  h.add(e);
  QCOMPARE(h.entries().size(), 1);
  e.sql = "SELECT 2";
  h.add(e);
  e.sql = "SELECT 3 FROM users";
  e.connection = "B";
  e.ok = false;
  e.error = "boom";
  h.add(e);
  e.sql = "SELECT 4";
  h.add(e);
  QCOMPARE(h.entries().size(), 3);
  QCOMPARE(h.entries().first().sql, QString("SELECT 4"));
  QCOMPARE(h.search("users").size(), 1);
  QCOMPARE(h.search("BOOM").size(), 2);
  QCOMPARE(h.search("", "A").size(), 1);
  QVERIFY(h.save());
  QueryHistory loaded(file, 3);
  QVERIFY(loaded.load());
  QCOMPARE(loaded.entries().size(), 3);
  QCOMPARE(loaded.entries().first().sql, QString("SELECT 4"));
  QVERIFY(!loaded.entries().first().ok);
  loaded.clear();
  QVERIFY(loaded.entries().isEmpty());
}

void TestDatabaseCore::managerPersistsProfilesAndActive() {
  QTemporaryDir dir;
  const QString cf = dir.filePath("c.json");
  const QString hf = dir.filePath("h.json");
  QString idA;
  {
    DatabaseManager m(cf, hf);
    QVERIFY(m.profiles().isEmpty());
    QSignalSpy profilesSpy(&m, &DatabaseManager::profilesChanged);
    DbConnectionProfile a;
    a.name = "A";
    a.engine = DbEngine::Sqlite;
    a.filePath = "/tmp/a.db";
    idA = m.saveProfile(a);
    DbConnectionProfile b = a;
    b.name = "B";
    const QString idB = m.saveProfile(b);
    QCOMPARE(profilesSpy.count(), 2);
    QCOMPARE(m.activeId(), idA);
    m.setActive(idB);
    QCOMPARE(m.connections().size(), 2);
    QVERIFY(m.connectionByName("b"));
    QCOMPARE(m.activeConnection()->profile().name, QString("B"));
    m.removeProfile(idB);
    QCOMPARE(m.activeId(), idA);
    QCOMPARE(m.connections().size(), 1);
  }
  DatabaseManager again(cf, hf);
  QCOMPARE(again.profiles().size(), 1);
  QCOMPARE(again.activeId(), idA);
}

QTEST_MAIN(TestDatabaseCore)
#include "test_databasecore.moc"
