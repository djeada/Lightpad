#include "database/csvimport.h"
#include "database/resultedit.h"
#include "database/schemadiff.h"
#include "database/sqllint.h"
#include "database/sqlparams.h"

#include <QtTest/QtTest>

namespace {

DbTableInfo table(const QString &name,
                  std::initializer_list<DbColumnInfo> cols,
                  const QString &schema = QStringLiteral("public")) {
  DbTableInfo t;
  t.schema = schema;
  t.name = name;
  for (const DbColumnInfo &c : cols) {
    t.columns.append(c);
  }
  return t;
}

DbSchema sampleSchema() {
  DbSchema s;
  s.tables << table("users", {{"id", "integer", false, true},
                              {"name", "text", false, false},
                              {"email", "text", true, false}});
  s.tables << table("orders", {{"id", "integer", false, true},
                               {"user_id", "integer", false, false},
                               {"total", "numeric", true, false}});
  return s;
}

bool hasIssue(const QVector<SqlIssue> &v, const QString &needle) {
  for (const SqlIssue &i : v) {
    if (i.message.contains(needle, Qt::CaseInsensitive)) {
      return true;
    }
  }
  return false;
}

} // namespace

class TestDatabaseTools : public QObject {
  Q_OBJECT
private slots:
  void paramsFindSkipsStringsCastsComments();
  void paramsBindEscapes();
  void paramsBindReportsMissing();

  void lintCleanQueryHasNoIssues();
  void lintUnknownTableSuggests();
  void lintUnknownColumn();
  void lintAliasesAndCtes();
  void lintStructural();
  void lintNullComparisonAndMissingWhere();
  void lintInsertColumnCount();
  void lintWithoutCatalogOnlyStructural();

  void diffDetectsChanges();
  void diffMigrationSqlPerEngine();
  void diffSqliteCannotAlter();
  void diffNoDropsOption();

  void csvParseQuotesAndDelimiters();
  void csvInferTypes();
  void csvCreateAndInsert();
  void csvRowWidthWarning();

  void editBuildsUpdateWithKey();
  void editRejectsWithoutKey();
  void editRejectsBadNumberAndNull();
  void pagingPerEngine();
};

void TestDatabaseTools::paramsFindSkipsStringsCastsComments() {
  const QString sql =
      "SELECT a::int, ':fake', \":q\", x -- :c\n FROM t /* :d */ WHERE id = "
      ":id AND n = :name AND m = :id";
  QCOMPARE(SqlParams::findParameters(sql), QStringList({"id", "name"}));
  QVERIFY(SqlParams::findParameters("SELECT 1 WHERE t = '10:30'").isEmpty());
}

void TestDatabaseTools::paramsBindEscapes() {
  QHash<QString, QString> in;
  in["id"] = "42";
  in["name"] = "O'Brien";
  in["n"] = "null";
  const QString out = SqlParams::bind(
      "SELECT * FROM t WHERE id = :id AND name = :name AND z IS :n", in,
      DbEngine::PostgreSql);
  QCOMPARE(out, QString("SELECT * FROM t WHERE id = 42 AND name = 'O''Brien' "
                        "AND z IS NULL"));
  QHash<QString, QString> my;
  my["p"] = "a\\b";
  QCOMPARE(SqlParams::bind(":p", my, DbEngine::MySql), QString("'a\\\\b'"));
  QCOMPARE(SqlParams::literalFromInput("true", DbEngine::SqlServer), QString("1"));
  QCOMPARE(SqlParams::literalFromInput("true", DbEngine::PostgreSql),
           QString("TRUE"));
}

void TestDatabaseTools::paramsBindReportsMissing() {
  QStringList missing;
  const QString out =
      SqlParams::bind("SELECT :a, :b", {{"a", "1"}}, DbEngine::Sqlite, &missing);
  QCOMPARE(out, QString("SELECT 1, :b"));
  QCOMPARE(missing, QStringList({"b"}));
}

void TestDatabaseTools::lintCleanQueryHasNoIssues() {
  const DbSchema s = sampleSchema();
  const auto issues = SqlLint::lint(
      "SELECT u.id, u.name, o.total FROM users u JOIN orders o ON o.user_id = "
      "u.id WHERE u.email IS NOT NULL;",
      DbEngine::PostgreSql, &s);
  QVERIFY2(issues.isEmpty(), qPrintable(issues.isEmpty() ? "" : issues[0].message));
}

void TestDatabaseTools::lintUnknownTableSuggests() {
  const DbSchema s = sampleSchema();
  const auto issues =
      SqlLint::lint("SELECT * FROM usres", DbEngine::PostgreSql, &s);
  QCOMPARE(issues.size(), 1);
  QVERIFY(issues[0].message.contains("did you mean 'users'"));
  QCOMPARE(issues[0].start, 14);
  QCOMPARE(issues[0].length, 5);
  // system schemas and temp tables are not flagged
  QVERIFY(SqlLint::lint("SELECT * FROM information_schema.tables",
                        DbEngine::PostgreSql, &s)
              .isEmpty());
  QVERIFY(SqlLint::lint("SELECT * FROM #tmp", DbEngine::SqlServer, &s).isEmpty());
}

void TestDatabaseTools::lintUnknownColumn() {
  const DbSchema s = sampleSchema();
  const auto issues = SqlLint::lint("SELECT u.nmae FROM users u",
                                    DbEngine::PostgreSql, &s);
  QCOMPARE(issues.size(), 1);
  QVERIFY(issues[0].message.contains("no column 'nmae'"));
  QVERIFY(issues[0].message.contains("did you mean 'name'"));
  QVERIFY(SqlLint::lint("SELECT users.name, u.* FROM users u",
                        DbEngine::PostgreSql, &s)
              .isEmpty());
}

void TestDatabaseTools::lintAliasesAndCtes() {
  const DbSchema s = sampleSchema();
  QVERIFY(SqlLint::lint("WITH recent AS (SELECT id FROM users) SELECT r.id FROM "
                        "recent r",
                        DbEngine::PostgreSql, &s)
              .isEmpty());
  const auto dup = SqlLint::lint("SELECT 1 FROM users a, orders a",
                                 DbEngine::PostgreSql, &s);
  QVERIFY(hasIssue(dup, "used more than once"));
  QVERIFY(SqlLint::lint("SELECT * FROM public.users", DbEngine::PostgreSql, &s)
              .isEmpty());
}

void TestDatabaseTools::lintStructural() {
  QVERIFY(hasIssue(SqlLint::lint("SELECT 'abc", DbEngine::Sqlite, nullptr),
                   "Unterminated string"));
  QVERIFY(hasIssue(SqlLint::lint("SELECT (1 + 2", DbEngine::Sqlite, nullptr),
                   "closing parenthesis"));
  QVERIFY(hasIssue(SqlLint::lint("SELECT 1)", DbEngine::Sqlite, nullptr),
                   "Unmatched"));
  QVERIFY(hasIssue(SqlLint::lint("SELECT 1 /* x", DbEngine::Sqlite, nullptr),
                   "block comment"));
  // quotes and comments hide parentheses
  QVERIFY(SqlLint::lint("SELECT '(' -- )\n", DbEngine::Sqlite, nullptr).isEmpty());
}

void TestDatabaseTools::lintNullComparisonAndMissingWhere() {
  const DbSchema s = sampleSchema();
  QVERIFY(hasIssue(SqlLint::lint("SELECT * FROM users WHERE email = NULL",
                                 DbEngine::PostgreSql, &s),
                   "IS NULL"));
  QVERIFY(hasIssue(SqlLint::lint("DELETE FROM users", DbEngine::PostgreSql, &s),
                   "without WHERE"));
  QVERIFY(!hasIssue(SqlLint::lint("DELETE FROM users WHERE id = 1",
                                  DbEngine::PostgreSql, &s),
                    "without WHERE"));
  // assigning NULL in SET is fine
  QVERIFY(SqlLint::lint("UPDATE users SET email = NULL WHERE id = 1",
                        DbEngine::PostgreSql, &s)
              .isEmpty());
}

void TestDatabaseTools::lintInsertColumnCount() {
  const DbSchema s = sampleSchema();
  QVERIFY(hasIssue(SqlLint::lint("INSERT INTO users (id, name) VALUES (1)",
                                 DbEngine::PostgreSql, &s),
                   "2 column(s) listed but 1 value(s)"));
  QVERIFY(SqlLint::lint("INSERT INTO users (id, name) VALUES (1, 'a'), (2, 'b')",
                        DbEngine::PostgreSql, &s)
              .isEmpty());
  QVERIFY(hasIssue(SqlLint::lint("INSERT INTO users (id, nope) VALUES (1, 2)",
                                 DbEngine::PostgreSql, &s),
                   "no column 'nope'"));
  // function calls with commas count as one value
  QVERIFY(SqlLint::lint("INSERT INTO users (id, name) VALUES (1, coalesce('a','b'))",
                        DbEngine::PostgreSql, &s)
              .isEmpty());
}

void TestDatabaseTools::lintWithoutCatalogOnlyStructural() {
  QVERIFY(SqlLint::lint("SELECT * FROM whatever", DbEngine::Sqlite, nullptr).isEmpty());
  DbSchema empty;
  QVERIFY(SqlLint::lint("SELECT * FROM whatever", DbEngine::Sqlite, &empty).isEmpty());
}

void TestDatabaseTools::diffDetectsChanges() {
  DbSchema a = sampleSchema();
  DbSchema b = sampleSchema();
  b.tables[0].columns[1].type = "varchar(80)";       // users.name type
  b.tables[0].columns[2].nullable = false;           // users.email NOT NULL
  b.tables[0].columns.append({"age", "integer", true, false});
  b.tables[1].columns.removeLast();                  // orders.total dropped
  b.tables << table("tags", {{"id", "integer", false, true}});
  a.tables << table("legacy", {{"id", "integer", false, true}});

  const DbSchemaDiff d = SchemaDiff::diff(a, b);
  QCOMPARE(d.addedTables.size(), 1);
  QCOMPARE(d.addedTables[0].name, QString("tags"));
  QCOMPARE(d.droppedTables.size(), 1);
  QCOMPARE(d.droppedTables[0].name, QString("legacy"));
  QCOMPARE(d.changedTables.size(), 2);
  const DbTableDiff &users = d.changedTables[0];
  QCOMPARE(users.name, QString("users"));
  QCOMPARE(users.addedColumns.size(), 1);
  QCOMPARE(users.changedColumns.size(), 2);
  QVERIFY(users.changedColumns[0].typeChanged);
  QVERIFY(users.changedColumns[1].nullabilityChanged);
  QCOMPARE(d.changedTables[1].droppedColumns.size(), 1);
  QVERIFY(!SchemaDiff::summary(d).isEmpty());
  QVERIFY(SchemaDiff::diff(a, a).isEmpty());
}

void TestDatabaseTools::diffMigrationSqlPerEngine() {
  DbSchema a;
  a.tables << table("t", {{"id", "integer", false, true}, {"v", "text", true, false}});
  DbSchema b = a;
  b.tables[0].columns[1].type = "varchar(10)";
  b.tables[0].columns[1].nullable = false;
  b.tables[0].columns.append({"extra", "integer", true, false});
  const DbSchemaDiff d = SchemaDiff::diff(a, b);

  const QString pg = SchemaDiff::migrationSql(DbEngine::PostgreSql, d);
  QVERIFY(pg.contains("ALTER TABLE \"public\".\"t\" ADD COLUMN \"extra\" integer;"));
  QVERIFY(pg.contains("ALTER COLUMN \"v\" TYPE varchar(10);"));
  QVERIFY(pg.contains("ALTER COLUMN \"v\" SET NOT NULL;"));

  const QString my = SchemaDiff::migrationSql(DbEngine::MySql, d);
  QVERIFY(my.contains("MODIFY COLUMN `v` varchar(10) NOT NULL;"));

  const QString ms = SchemaDiff::migrationSql(DbEngine::SqlServer, d);
  QVERIFY(ms.contains("ADD [extra] integer;"));
  QVERIFY(ms.contains("ALTER COLUMN [v] varchar(10) NOT NULL;"));
}

void TestDatabaseTools::diffSqliteCannotAlter() {
  DbSchema a;
  a.tables << table("t", {{"v", "text", true, false}}, "");
  DbSchema b = a;
  b.tables[0].columns[0].type = "integer";
  const QString sql =
      SchemaDiff::migrationSql(DbEngine::Sqlite, SchemaDiff::diff(a, b));
  QVERIFY(sql.startsWith("-- SQLite cannot alter column"));
}

void TestDatabaseTools::diffNoDropsOption() {
  DbSchema a;
  a.tables << table("gone", {{"id", "integer", false, true}});
  const DbSchemaDiff d = SchemaDiff::diff(a, DbSchema());
  QVERIFY(SchemaDiff::migrationSql(DbEngine::PostgreSql, d).contains("DROP TABLE"));
  const QString safe = SchemaDiff::migrationSql(DbEngine::PostgreSql, d, false);
  QVERIFY(!safe.contains("DROP TABLE"));
  QVERIFY(safe.contains("omitted"));
}

void TestDatabaseTools::csvParseQuotesAndDelimiters() {
  const auto t = CsvImport::parse(
      "id;name;note\r\n1;\"Smith; J\";\"line1\nline2\"\r\n2;\"say \"\"hi\"\"\";\r\n");
  QVERIFY(t.ok);
  QCOMPARE(t.delimiter, QChar(';'));
  QCOMPARE(t.header, QStringList({"id", "name", "note"}));
  QCOMPARE(t.rows.size(), 2);
  QCOMPARE(t.rows[0][1], QString("Smith; J"));
  QCOMPARE(t.rows[0][2], QString("line1\nline2"));
  QCOMPARE(t.rows[1][1], QString("say \"hi\""));

  const auto tsv = CsvImport::parse("a\tb\n1\t2\n");
  QCOMPARE(tsv.delimiter, QChar('\t'));
  const auto dup = CsvImport::parse("x,x,\n1,2,3\n");
  QCOMPARE(dup.header, QStringList({"x", "x_2", "col_3"}));
  QVERIFY(!CsvImport::parse("a,b\n\"oops").ok);
  QVERIFY(!CsvImport::parse("").ok);
  const auto nohdr = CsvImport::parse("1,2\n3,4\n", ',', false);
  QCOMPARE(nohdr.rows.size(), 2);
  QCOMPARE(nohdr.header, QStringList({"col_1", "col_2"}));
}

void TestDatabaseTools::csvInferTypes() {
  const auto t = CsvImport::parse(
      "i,big,r,b,d,ts,zip,mixed,empty\n"
      "1,5000000000,1.5,true,2024-01-02,2024-01-02 10:11:12,01234,1,\n"
      "2,6,2,false,2024-02-03,2024-01-03T10:11:12,98765,x,\n"
      ",7,,TRUE,2024-03-04,2024-01-04 00:00:00,11111,3,\n");
  const auto cols = CsvImport::inferColumns(t);
  using K = CsvImport::ColumnKind;
  QCOMPARE(cols[0].kind, K::Integer);
  QVERIFY(cols[0].nullable);
  QCOMPARE(cols[1].kind, K::BigInt);
  QVERIFY(!cols[1].nullable);
  QCOMPARE(cols[2].kind, K::Real);
  QCOMPARE(cols[3].kind, K::Boolean);
  QCOMPARE(cols[4].kind, K::Date);
  QCOMPARE(cols[5].kind, K::Timestamp);
  QCOMPARE(cols[6].kind, K::Text); // leading zero must survive
  QCOMPARE(cols[7].kind, K::Text);
  QCOMPARE(cols[8].kind, K::Text);
}

void TestDatabaseTools::csvCreateAndInsert() {
  const auto t = CsvImport::parse("id,name,ok\n1,O'Neil,true\n2,,false\n");
  const auto cols = CsvImport::inferColumns(t);
  const QString ddl =
      CsvImport::createTableSql(DbEngine::PostgreSql, "people", cols);
  QVERIFY(ddl.contains("\"id\" INTEGER NOT NULL"));
  QVERIFY(ddl.contains("\"ok\" BOOLEAN NOT NULL"));
  const QStringList ins =
      CsvImport::insertSql(DbEngine::PostgreSql, "people", cols, t);
  QCOMPARE(ins.size(), 1);
  QVERIFY(ins[0].contains("(1, 'O''Neil', TRUE)"));
  QVERIFY(ins[0].contains("(2, '', FALSE)"));

  const QStringList ms = CsvImport::insertSql(DbEngine::SqlServer, "p", cols, t);
  QVERIFY(ms[0].contains("N'O''Neil'"));
  QVERIFY(ms[0].contains(", 1)"));

  QStringList batches = CsvImport::insertSql(DbEngine::Sqlite, "p", cols, t, 1);
  QCOMPARE(batches.size(), 2);
}

void TestDatabaseTools::csvRowWidthWarning() {
  const auto t = CsvImport::parse("a,b\n1,2\n3\n");
  const auto cols = CsvImport::inferColumns(t);
  QStringList warnings;
  const QStringList ins =
      CsvImport::insertSql(DbEngine::Sqlite, "t", cols, t, 500, &warnings);
  QCOMPARE(warnings.size(), 1);
  QVERIFY(warnings[0].contains("Row 3"));
  QVERIFY(ins[0].contains("(3, NULL)"));
}

static DbResultSet userResult() {
  DbResultSet r;
  r.columns = {{"id", ""}, {"name", ""}, {"email", ""}};
  r.rows.append({QVariant("7"), QVariant("Ann"), QVariant()});
  r.rows.append({QVariant("8"), QVariant("Bob"), QVariant("b@x")});
  return r;
}

void TestDatabaseTools::editBuildsUpdateWithKey() {
  const DbSchema s = sampleSchema();
  const DbTableInfo &users = s.tables[0];
  ResultEdit::CellEdit e1;
  e1.row = 0;
  e1.column = 1;
  e1.text = "Anne's";
  ResultEdit::CellEdit e2;
  e2.row = 0;
  e2.column = 2;
  e2.setNull = true;
  ResultEdit::CellEdit e3;
  e3.row = 1;
  e3.column = 1;
  e3.text = "Rob";
  const auto p = ResultEdit::buildUpdates(DbEngine::PostgreSql, users,
                                          userResult(), {e1, e2, e3});
  QVERIFY(p.ok());
  QCOMPARE(p.statements.size(), 2);
  QCOMPARE(p.statements[0],
           QString("UPDATE \"public\".\"users\"\nSET \"name\" = 'Anne''s', "
                   "\"email\" = NULL\nWHERE \"id\" = 7;"));
  QVERIFY(p.statements[1].contains("WHERE \"id\" = 8;"));
}

void TestDatabaseTools::editRejectsWithoutKey() {
  DbTableInfo t = table("nokey", {{"a", "text", true, false}});
  DbResultSet r;
  r.columns = {{"a", ""}};
  r.rows.append({QVariant("x")});
  ResultEdit::CellEdit e;
  e.text = "y";
  auto p = ResultEdit::buildUpdates(DbEngine::Sqlite, t, r, {e});
  QVERIFY(!p.ok());
  QVERIFY(p.errors[0].contains("no primary key"));

  // key not selected
  const DbSchema s = sampleSchema();
  DbResultSet noId;
  noId.columns = {{"name", ""}};
  noId.rows.append({QVariant("Ann")});
  p = ResultEdit::buildUpdates(DbEngine::Sqlite, s.tables[0], noId, {e});
  QVERIFY(p.errors[0].contains("not part of the result"));

  DbTableInfo v = s.tables[0];
  v.isView = true;
  QVERIFY(ResultEdit::buildUpdates(DbEngine::Sqlite, v, userResult(), {e})
              .errors[0]
              .contains("view"));
}

void TestDatabaseTools::editRejectsBadNumberAndNull() {
  const DbSchema s = sampleSchema();
  ResultEdit::CellEdit bad;
  bad.row = 0;
  bad.column = 0;
  bad.text = "abc";
  auto p = ResultEdit::buildUpdates(DbEngine::PostgreSql, s.tables[0],
                                    userResult(), {bad});
  QVERIFY(!p.ok());
  QVERIFY(p.errors[0].contains("not a number"));

  ResultEdit::CellEdit nul;
  nul.row = 0;
  nul.column = 1; // name is NOT NULL
  nul.setNull = true;
  p = ResultEdit::buildUpdates(DbEngine::PostgreSql, s.tables[0], userResult(),
                               {nul});
  QVERIFY(p.errors[0].contains("does not allow NULL"));

  QString lit;
  QVERIFY(ResultEdit::literalFor(DbEngine::MySql, {"f", "tinyint(1)"}, "5", &lit));
  QCOMPARE(lit, QString("5"));
  QVERIFY(ResultEdit::literalFor(DbEngine::PostgreSql, {"f", "boolean"}, "yes", &lit));
  QCOMPARE(lit, QString("TRUE"));
  QVERIFY(!ResultEdit::literalFor(DbEngine::PostgreSql, {"f", "boolean"}, "maybe", &lit));
  QVERIFY(ResultEdit::literalFor(DbEngine::SqlServer, {"f", "nvarchar"}, "a", &lit));
  QCOMPARE(lit, QString("N'a'"));
}

void TestDatabaseTools::pagingPerEngine() {
  QCOMPARE(ResultEdit::pagedSql(DbEngine::PostgreSql, "SELECT * FROM t;", 50, 2),
           QString("SELECT * FROM t\nLIMIT 50 OFFSET 100;"));
  QCOMPARE(ResultEdit::pagedSql(DbEngine::Sqlite, "-- c\nSELECT 1", 10, 0),
           QString("SELECT 1\nLIMIT 10 OFFSET 0;"));
  QVERIFY(ResultEdit::pagedSql(DbEngine::MySql, "SELECT * FROM t LIMIT 5", 10, 1).isEmpty());
  QVERIFY(ResultEdit::pagedSql(DbEngine::PostgreSql, "DELETE FROM t", 10, 1).isEmpty());
  QVERIFY(ResultEdit::pagedSql(DbEngine::SqlServer, "SELECT * FROM t", 10, 1).isEmpty());
  QCOMPARE(ResultEdit::pagedSql(DbEngine::SqlServer,
                                "SELECT * FROM t ORDER BY id", 10, 3),
           QString("SELECT * FROM t ORDER BY id\nOFFSET 30 ROWS FETCH NEXT 10 "
                   "ROWS ONLY;"));
  QVERIFY(ResultEdit::pagedSql(DbEngine::Sqlite, "SELECT 1", 0, 0).isEmpty());
}

QTEST_MAIN(TestDatabaseTools)
#include "test_databasetools.moc"
