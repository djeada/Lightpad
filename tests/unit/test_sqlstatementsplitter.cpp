#include "database/sqlstatementsplitter.h"

#include <QtTest/QtTest>

class TestSqlStatementSplitter : public QObject {
  Q_OBJECT

private slots:
  void splitsOnSemicolons();
  void keepsSemicolonsInStringsAndComments();
  void dropsCommentOnlyFragments();
  void postgresDollarQuoting();
  void postgresNestedBlockComments();
  void mysqlBackslashEscapesAndHashComments();
  void sqlServerGoSeparator();
  void sqlServerBeginEndBlocksStayTogether();
  void sqlServerTransactionBeginIsNotABlock();
  void sqliteTriggerBody();
  void bracketQuotedIdentifiers();
  void recordsPositionsAndLines();
  void statementAtCaret();
  void classification();
  void selectIntoIsDdl();
  void cteWithWritesIsDml();
  void readOnlyPolicy();
  void destructiveWarnings();
  void connectionDirective();
  void stripComments();
};

using K = SqlStatementKind;

static QStringList texts(const QVector<SqlStatement> &v) {
  QStringList out;
  for (const auto &s : v) {
    out << s.text;
  }
  return out;
}

void TestSqlStatementSplitter::splitsOnSemicolons() {
  const auto r = SqlStatementSplitter::split("SELECT 1; SELECT 2;\nSELECT 3",
                                             DbEngine::PostgreSql);
  QCOMPARE(texts(r), (QStringList{"SELECT 1", "SELECT 2", "SELECT 3"}));
}

void TestSqlStatementSplitter::keepsSemicolonsInStringsAndComments() {
  const auto r = SqlStatementSplitter::split(
      "SELECT 'a;b', \"c;d\" -- trailing; comment\n;SELECT /* x; y */ 2;",
      DbEngine::PostgreSql);
  QCOMPARE(r.size(), 2);
  QVERIFY(r[0].text.startsWith("SELECT 'a;b'"));
  QVERIFY(r[1].text.contains("/* x; y */"));
}

void TestSqlStatementSplitter::dropsCommentOnlyFragments() {
  const auto r = SqlStatementSplitter::split(
      "-- just a comment\n/* another */\n;;  ; SELECT 1;\n-- tail",
      DbEngine::Sqlite);
  QCOMPARE(r.size(), 1);
  QVERIFY(r[0].text.contains("SELECT 1"));
}

void TestSqlStatementSplitter::postgresDollarQuoting() {
  const QString sql =
      "CREATE FUNCTION f() RETURNS int AS $$ BEGIN RETURN 1; END; $$ LANGUAGE "
      "plpgsql;\nSELECT $tag$ a ; b $tag$;\nSELECT $1;";
  const auto r = SqlStatementSplitter::split(sql, DbEngine::PostgreSql);
  QCOMPARE(r.size(), 3);
  QVERIFY(r[0].text.contains("RETURN 1; END;"));
  QVERIFY(r[1].text.contains("a ; b"));
  QCOMPARE(r[2].text, QString("SELECT $1"));
}

void TestSqlStatementSplitter::postgresNestedBlockComments() {
  const auto r = SqlStatementSplitter::split(
      "SELECT 1 /* a /* b ; */ c ; */ ; SELECT 2", DbEngine::PostgreSql);
  QCOMPARE(r.size(), 2);
}

void TestSqlStatementSplitter::mysqlBackslashEscapesAndHashComments() {
  const auto r = SqlStatementSplitter::split(
      "SELECT 'it\\'s; fine'; # comment; here\nSELECT 2;", DbEngine::MySql);
  QCOMPARE(r.size(), 2);
  QVERIFY(r[0].text.contains("it\\'s; fine"));
}

void TestSqlStatementSplitter::sqlServerGoSeparator() {
  const auto r = SqlStatementSplitter::split(
      "CREATE TABLE t (a int)\nGO\nINSERT t VALUES (1)\n go \nSELECT * FROM t\n"
      "GO 3\n",
      DbEngine::SqlServer);
  QCOMPARE(texts(r), (QStringList{"CREATE TABLE t (a int)",
                                  "INSERT t VALUES (1)", "SELECT * FROM t"}));
}

void TestSqlStatementSplitter::sqlServerBeginEndBlocksStayTogether() {
  const QString sql =
      "CREATE PROCEDURE p AS\nBEGIN\n  SELECT 1;\n  IF 1=1 BEGIN SELECT 2; "
      "END\n"
      "  SELECT CASE WHEN 1=1 THEN 'a' ELSE 'b' END;\nEND;\nSELECT 3;";
  const auto r = SqlStatementSplitter::split(sql, DbEngine::SqlServer);
  QCOMPARE(r.size(), 2);
  QVERIFY(r[0].text.startsWith("CREATE PROCEDURE"));
  QVERIFY(r[0].text.endsWith("END"));
  QCOMPARE(r[1].text, QString("SELECT 3"));
}

void TestSqlStatementSplitter::sqlServerTransactionBeginIsNotABlock() {
  const auto r = SqlStatementSplitter::split(
      "BEGIN TRANSACTION; UPDATE t SET a = 1; COMMIT;", DbEngine::SqlServer);
  QCOMPARE(r.size(), 3);
}

void TestSqlStatementSplitter::sqliteTriggerBody() {
  const QString sql =
      "CREATE TRIGGER trg AFTER INSERT ON t BEGIN INSERT INTO log VALUES "
      "(new.a); UPDATE c SET n = n + 1; END;\nSELECT 1;\nBEGIN; COMMIT;";
  const auto r = SqlStatementSplitter::split(sql, DbEngine::Sqlite);
  QCOMPARE(r.size(), 4);
  QVERIFY(r[0].text.contains("UPDATE c SET"));
  QCOMPARE(r[2].text, QString("BEGIN"));
}

void TestSqlStatementSplitter::bracketQuotedIdentifiers() {
  const auto r = SqlStatementSplitter::split(
      "SELECT [a;b] FROM [t]]x]; SELECT 2", DbEngine::SqlServer);
  QCOMPARE(r.size(), 2);
}

void TestSqlStatementSplitter::recordsPositionsAndLines() {
  const QString sql = "SELECT 1;\n\n  SELECT 2;\nSELECT 3";
  const auto r = SqlStatementSplitter::split(sql, DbEngine::Sqlite);
  QCOMPARE(r.size(), 3);
  QCOMPARE(r[0].start, 0);
  QCOMPARE(r[0].end, 9);
  QCOMPARE(r[1].line, 2);
  QCOMPARE(sql.mid(r[1].start, 8), QString("SELECT 2"));
  QCOMPARE(r[2].line, 3);
}

void TestSqlStatementSplitter::statementAtCaret() {
  const QString sql = "SELECT 1;\n\nSELECT 2;\n\nSELECT 3;";
  auto at = [&](int off) {
    return SqlStatementSplitter::statementAt(sql, off, DbEngine::Sqlite).text;
  };
  QCOMPARE(at(3), QString("SELECT 1"));
  QCOMPARE(at(sql.indexOf("SELECT 2") + 4), QString("SELECT 2"));

  QCOMPARE(at(sql.indexOf("SELECT 2") - 1), QString("SELECT 1"));
  QCOMPARE(at(sql.size()), QString("SELECT 3"));
  QVERIFY(SqlStatementSplitter::statementAt("   ", 1, DbEngine::Sqlite)
              .text.isEmpty());
}

void TestSqlStatementSplitter::classification() {
  auto k = [](const QString &s) {
    return SqlStatementSplitter::classify(s, DbEngine::PostgreSql);
  };
  QCOMPARE(k("select 1"), K::Select);
  QCOMPARE(k("  (select 1) union (select 2)"), K::Select);
  QCOMPARE(k("-- c\nSELECT 1"), K::Select);
  QCOMPARE(k("insert into t values (1)"), K::Dml);
  QCOMPARE(k("UPDATE t SET a=1"), K::Dml);
  QCOMPARE(k("create table t(a int)"), K::Ddl);
  QCOMPARE(k("drop table t"), K::Ddl);
  QCOMPARE(k("BEGIN"), K::Transaction);
  QCOMPARE(k("commit"), K::Transaction);
  QCOMPARE(k("USE master"), K::Session);
  QCOMPARE(k("SET search_path = x"), K::Session);
  QCOMPARE(k("SHOW TABLES"), K::Show);
  QCOMPARE(k("EXPLAIN SELECT 1"), K::Explain);
  QCOMPARE(k("EXPLAIN ANALYZE DELETE FROM t"), K::Call);
  QCOMPARE(k("EXEC sp_who"), K::Call);
  QCOMPARE(k("PRAGMA table_info(t)"), K::Show);
  QCOMPARE(k("PRAGMA journal_mode = WAL"), K::Other);
  QCOMPARE(k("   -- nothing"), K::Empty);
}

void TestSqlStatementSplitter::selectIntoIsDdl() {
  QCOMPARE(SqlStatementSplitter::classify("SELECT * INTO backup FROM t",
                                          DbEngine::SqlServer),
           K::Ddl);
  QCOMPARE(SqlStatementSplitter::classify("SELECT a INTO @v FROM t",
                                          DbEngine::SqlServer),
           K::Select);
}

void TestSqlStatementSplitter::cteWithWritesIsDml() {
  QCOMPARE(SqlStatementSplitter::classify(
               "WITH x AS (SELECT 1) SELECT * FROM x", DbEngine::PostgreSql),
           K::Select);
  QCOMPARE(SqlStatementSplitter::classify(
               "WITH x AS (DELETE FROM t RETURNING *) SELECT * FROM x",
               DbEngine::PostgreSql),
           K::Dml);
  QCOMPARE(SqlStatementSplitter::classify(
               "WITH x AS (SELECT 'delete me') SELECT * FROM x",
               DbEngine::PostgreSql),
           K::Select);
}

void TestSqlStatementSplitter::readOnlyPolicy() {
  QVERIFY(SqlStatementSplitter::isReadOnly(K::Select));
  QVERIFY(SqlStatementSplitter::isReadOnly(K::Show));
  QVERIFY(SqlStatementSplitter::isReadOnly(K::Explain));
  QVERIFY(SqlStatementSplitter::isReadOnly(K::Session));
  QVERIFY(!SqlStatementSplitter::isReadOnly(K::Dml));
  QVERIFY(!SqlStatementSplitter::isReadOnly(K::Ddl));
  QVERIFY(!SqlStatementSplitter::isReadOnly(K::Call));
  QVERIFY(!SqlStatementSplitter::isReadOnly(K::Other));
}

void TestSqlStatementSplitter::destructiveWarnings() {
  auto w = [](const QString &s) {
    return SqlStatementSplitter::destructiveWarning(s, DbEngine::PostgreSql);
  };
  QVERIFY(w("DELETE FROM t WHERE id = 1").isEmpty());
  QVERIFY(!w("DELETE FROM t").isEmpty());
  QVERIFY(!w("UPDATE t SET a = 1").isEmpty());
  QVERIFY(w("UPDATE t SET a = 1 WHERE b = 2").isEmpty());
  QVERIFY(!w("DROP TABLE t").isEmpty());
  QVERIFY(!w("truncate table t").isEmpty());
  QVERIFY(w("SELECT * FROM t").isEmpty());

  QVERIFY(!w("UPDATE t SET note = 'where'").isEmpty());
}

void TestSqlStatementSplitter::connectionDirective() {
  QCOMPARE(SqlStatementSplitter::connectionDirective(
               "-- connection: Local SQL Server\nSELECT 1;"),
           QString("Local SQL Server"));
  QCOMPARE(
      SqlStatementSplitter::connectionDirective("SELECT 1;\n-- connection: x"),
      QString());
  QCOMPARE(SqlStatementSplitter::connectionDirective(
               "\n-- note\n--Connection : Prod"),
           QString("Prod"));
}

void TestSqlStatementSplitter::stripComments() {
  QCOMPARE(SqlStatementSplitter::stripComments("a -- x\nb /* y */ c 'z -- w'"),
           QString("a \nb   c 'z -- w'"));
}

QTEST_APPLESS_MAIN(TestSqlStatementSplitter)
#include "test_sqlstatementsplitter.moc"
