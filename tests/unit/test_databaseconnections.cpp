#include <QtTest/QtTest>

#include "database/clidialect.h"
#include "database/dbconnection.h"
#include "database/dbconnectiontemplates.h"
#include "database/dbconnectionurl.h"
#include "database/dbprojectdiscovery.h"

#include <QJsonDocument>
#include <QTemporaryDir>

class TestDatabaseConnections : public QObject {
  Q_OBJECT

private slots:
  void testPostgresUrl();
  void testUrlEncodingAndOptions();
  void testMysqlAndDriverSchemes();
  void testSqlServerForms();
  void testSqliteForms();
  void testKeyValueStrings();
  void testRejectsUnknownText();
  void testFormatRoundTrips();
  void testProfileJsonKeepsNewFields();
  void testSslArgumentsPerClient();
  void testPostgresSslEnvironment();
  void testDockerLaunchCarriesSslFlags();
  void testTemplatesAreUsable();
  void testUserTemplatesOverrideAndExtend();
  void testEnvFileParsing();
  void testEnvironmentDetection();
  void testComposeDetection();
  void testProjectScan();
  void testPasswordSources();
  void testDatabaseChangeKeepsSessionAccurate();
};

void TestDatabaseConnections::testPostgresUrl() {
  DbConnectionUrl::Parsed parsed;
  QVERIFY(DbConnectionUrl::parse(
      "postgresql://app:s3cret@db.example.com:6543/shop?sslmode=require",
      &parsed));
  const DbConnectionProfile &p = parsed.profile;
  QCOMPARE(p.engine, DbEngine::PostgreSql);
  QCOMPARE(p.host, QString("db.example.com"));
  QCOMPARE(p.port, 6543);
  QCOMPARE(p.user, QString("app"));
  QCOMPARE(p.database, QString("shop"));
  QCOMPARE(p.sslMode, QString("require"));
  QVERIFY(parsed.hasPassword);
  QCOMPARE(parsed.password, QString("s3cret"));

  QVERIFY(DbConnectionUrl::parse("postgres://localhost", &parsed));
  QCOMPARE(parsed.profile.host, QString("localhost"));
  QCOMPARE(parsed.profile.port, 0);
  QVERIFY(!parsed.hasPassword);

  // A whole line copied from a .env file, quoted.
  QVERIFY(DbConnectionUrl::parse(
      "DATABASE_URL=\"postgres://u@h/d?sslmode=verify-full\"", &parsed));
  QCOMPARE(parsed.profile.database, QString("d"));
  QCOMPARE(parsed.profile.sslMode, QString("verify-full"));

  // libpq allows several hosts; the first one is used.
  QVERIFY(DbConnectionUrl::parse("postgresql://u@h1:5432,h2:5433/d", &parsed));
  QCOMPARE(parsed.profile.host, QString("h1"));
  QCOMPARE(parsed.profile.port, 5432);
}

void TestDatabaseConnections::testUrlEncodingAndOptions() {
  DbConnectionUrl::Parsed parsed;
  QVERIFY(DbConnectionUrl::parse(
      "postgresql://user%40corp:p%40ss%2Fw0rd@[::1]:5432/my%20db", &parsed));
  QCOMPARE(parsed.profile.user, QString("user@corp"));
  QCOMPARE(parsed.password, QString("p@ss/w0rd"));
  QCOMPARE(parsed.profile.host, QString("::1"));
  QCOMPARE(parsed.profile.database, QString("my db"));

  // Unencoded @ in a password still splits at the last @.
  QVERIFY(DbConnectionUrl::parse("mysql://root:a@b@host/db", &parsed));
  QCOMPARE(parsed.password, QString("a@b"));
  QCOMPARE(parsed.profile.host, QString("host"));

  // Unix socket directory as a query parameter.
  QVERIFY(DbConnectionUrl::parse(
      "postgresql:///app?host=/var/run/postgresql&user=me", &parsed));
  QCOMPARE(parsed.profile.host, QString("/var/run/postgresql"));
  QCOMPARE(parsed.profile.user, QString("me"));
  QCOMPARE(parsed.profile.database, QString("app"));
}

void TestDatabaseConnections::testMysqlAndDriverSchemes() {
  DbConnectionUrl::Parsed parsed;
  QVERIFY(DbConnectionUrl::parse(
      "mysql+pymysql://admin:pw@rds.example.com:3307/sales?ssl-mode=REQUIRED",
      &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::MySql);
  QCOMPARE(parsed.profile.port, 3307);
  QCOMPARE(parsed.profile.sslMode, QString("require"));

  QVERIFY(DbConnectionUrl::parse(
      "jdbc:mysql://h/db?useSSL=false&user=bob&password=x", &parsed));
  QCOMPARE(parsed.profile.sslMode, QString("disable"));
  QCOMPARE(parsed.profile.user, QString("bob"));
  QCOMPARE(parsed.password, QString("x"));

  // Prisma's PlanetScale strings.
  QVERIFY(DbConnectionUrl::parse(
      "mysql://u:p@aws.connect.psdb.cloud/app?sslaccept=strict", &parsed));
  QCOMPARE(parsed.profile.sslMode, QString("verify-full"));

  QVERIFY(DbConnectionUrl::parse("mariadb://u@h/db", &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::MySql);
  QVERIFY(DbConnectionUrl::parse("postgresql+asyncpg://u@h/db", &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::PostgreSql);
}

void TestDatabaseConnections::testSqlServerForms() {
  DbConnectionUrl::Parsed parsed;
  QVERIFY(DbConnectionUrl::parse(
      "jdbc:sqlserver://sql.local\\SQLEXPRESS:1434;databaseName=Inventory;"
      "user=sa;password=Pa;ss;encrypt=true;trustServerCertificate=false",
      &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::SqlServer);
  QCOMPARE(parsed.profile.host, QString("sql.local\\SQLEXPRESS"));
  QCOMPARE(parsed.profile.port, 1434);
  QCOMPARE(parsed.profile.database, QString("Inventory"));
  QCOMPARE(parsed.profile.user, QString("sa"));
  QVERIFY(!parsed.profile.trustServerCertificate);

  QVERIFY(DbConnectionUrl::parse(
      "sqlserver://sa:pw@localhost:1433?database=master", &parsed));
  QCOMPARE(parsed.profile.database, QString("master"));
  QCOMPARE(parsed.password, QString("pw"));

  QVERIFY(DbConnectionUrl::parse(
      "Server=tcp:acme.database.windows.net,1433;Initial Catalog=crm;"
      "User ID=admin;Password=\"a;b\";Encrypt=True;",
      &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::SqlServer);
  QCOMPARE(parsed.profile.host, QString("acme.database.windows.net"));
  QCOMPARE(parsed.profile.port, 1433);
  QCOMPARE(parsed.profile.database, QString("crm"));
  QCOMPARE(parsed.profile.user, QString("admin"));
  QCOMPARE(parsed.password, QString("a;b"));
  QVERIFY(!parsed.profile.trustServerCertificate);
}

void TestDatabaseConnections::testSqliteForms() {
  DbConnectionUrl::Parsed parsed;
  QVERIFY(DbConnectionUrl::parse("sqlite:////var/data/app.db", &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::Sqlite);
  QCOMPARE(parsed.profile.filePath, QString("/var/data/app.db"));

  QVERIFY(DbConnectionUrl::parse("sqlite:///db.sqlite3", &parsed, nullptr,
                                 "/srv/site"));
  QCOMPARE(parsed.profile.filePath, QString("/srv/site/db.sqlite3"));

  QVERIFY(DbConnectionUrl::parse("file:./dev.db", &parsed, nullptr, "/w"));
  QCOMPARE(parsed.profile.filePath, QString("/w/dev.db"));

  QVERIFY(DbConnectionUrl::parse("jdbc:sqlite:/tmp/x.sqlite", &parsed));
  QCOMPARE(parsed.profile.filePath, QString("/tmp/x.sqlite"));

  QVERIFY(DbConnectionUrl::parse("sqlite://", &parsed));
  QCOMPARE(parsed.profile.filePath, QString(":memory:"));

  QVERIFY(DbConnectionUrl::parse("/home/me/notes.sqlite", &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::Sqlite);

  QVERIFY(DbConnectionUrl::parse("Data Source=app.db;Cache=Shared", &parsed,
                                 nullptr, "/w"));
  QCOMPARE(parsed.profile.engine, DbEngine::Sqlite);
  QCOMPARE(parsed.profile.filePath, QString("/w/app.db"));
}

void TestDatabaseConnections::testKeyValueStrings() {
  DbConnectionUrl::Parsed parsed;
  QVERIFY(DbConnectionUrl::parse(
      "host=pg.internal port=5433 dbname=metrics user=grafana "
      "password='it''s' sslmode=verify-ca",
      &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::PostgreSql);
  QCOMPARE(parsed.profile.host, QString("pg.internal"));
  QCOMPARE(parsed.profile.port, 5433);
  QCOMPARE(parsed.profile.database, QString("metrics"));
  QCOMPARE(parsed.profile.sslMode, QString("verify-ca"));

  // Npgsql
  QVERIFY(DbConnectionUrl::parse(
      "Host=localhost;Port=5432;Database=app;Username=dev;Password=pw;"
      "SSL Mode=Require",
      &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::PostgreSql);
  QCOMPARE(parsed.profile.user, QString("dev"));
  QCOMPARE(parsed.profile.sslMode, QString("require"));

  // MySQL Connector/NET
  QVERIFY(DbConnectionUrl::parse(
      "Server=mysql.local;Port=3306;Database=shop;Uid=web;Pwd=pw;", &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::MySql);
  QCOMPARE(parsed.profile.user, QString("web"));
  QCOMPARE(parsed.password, QString("pw"));

  // ODBC driver names decide the engine.
  QVERIFY(DbConnectionUrl::parse(
      "Driver={PostgreSQL Unicode};Server=h;Database=d;Uid=u;Pwd=p;", &parsed));
  QCOMPARE(parsed.profile.engine, DbEngine::PostgreSql);
}

void TestDatabaseConnections::testRejectsUnknownText() {
  DbConnectionUrl::Parsed parsed;
  QString error;
  QVERIFY(!DbConnectionUrl::parse("redis://localhost:6379", &parsed, &error));
  QVERIFY(!error.isEmpty());
  QVERIFY(!DbConnectionUrl::parse("hello world", &parsed, &error));
  QVERIFY(!DbConnectionUrl::parse("", &parsed, &error));
  QVERIFY(!DbConnectionUrl::looksLikeConnectionString("just text"));
  QVERIFY(DbConnectionUrl::looksLikeConnectionString("postgres://h/d"));
}

void TestDatabaseConnections::testFormatRoundTrips() {
  DbConnectionProfile p;
  p.engine = DbEngine::PostgreSql;
  p.host = "db.example.com";
  p.port = 6543;
  p.user = "app user";
  p.database = "shop";
  p.sslMode = "require";
  const QString url = DbConnectionUrl::format(p, "p@ss");
  QCOMPARE(url, QString("postgresql://app%20user:p%40ss@db.example.com:6543/"
                        "shop?sslmode=require"));
  DbConnectionUrl::Parsed back;
  QVERIFY(DbConnectionUrl::parse(url, &back));
  QCOMPARE(back.profile.user, p.user);
  QCOMPARE(back.password, QString("p@ss"));
  QCOMPARE(back.profile.sslMode, p.sslMode);
  // Without a password nothing secret is in the URL.
  QVERIFY(!DbConnectionUrl::format(p).contains("p@ss"));

  p.engine = DbEngine::MySql;
  p.sslMode = "verify-full";
  QVERIFY(DbConnectionUrl::format(p).endsWith("?ssl-mode=VERIFY_IDENTITY"));
  QVERIFY(DbConnectionUrl::parse(DbConnectionUrl::format(p), &back));
  QCOMPARE(back.profile.sslMode, QString("verify-full"));

  p.engine = DbEngine::SqlServer;
  p.port = 0;
  QVERIFY(DbConnectionUrl::parse(DbConnectionUrl::format(p), &back));
  QCOMPARE(back.profile.database, QString("shop"));
  QCOMPARE(back.profile.effectivePort(), 1433);

  DbConnectionProfile file;
  file.engine = DbEngine::Sqlite;
  file.filePath = "/data/app.db";
  QCOMPARE(DbConnectionUrl::format(file), QString("sqlite:////data/app.db"));
  QVERIFY(DbConnectionUrl::parse(DbConnectionUrl::format(file), &back));
  QCOMPARE(back.profile.filePath, file.filePath);
}

void TestDatabaseConnections::testProfileJsonKeepsNewFields() {
  DbConnectionProfile p;
  p.name = "x";
  p.engine = DbEngine::PostgreSql;
  p.sslMode = "verify-ca";
  p.passwordSource = DbPasswordSource::Environment;
  p.passwordEnv = "PGPASSWORD";
  const DbConnectionProfile back = DbConnectionProfile::fromJson(p.toJson());
  QCOMPARE(back.sslMode, p.sslMode);
  QCOMPARE(back.passwordSource, DbPasswordSource::Environment);
  QCOMPARE(back.passwordEnv, QString("PGPASSWORD"));
  QVERIFY(back.sameTargetAs(p));

  // Old files without the fields keep the old behaviour.
  const DbConnectionProfile old = DbConnectionProfile::fromJson(
      QJsonDocument::fromJson(R"({"name":"a","engine":"mysql"})").object());
  QVERIFY(old.sslMode.isEmpty());
  QCOMPARE(old.passwordSource, DbPasswordSource::Ask);

  DbConnectionProfile none = p;
  none.passwordSource = DbPasswordSource::None;
  QVERIFY(!none.needsPassword());
  QVERIFY(!none.sameTargetAs(p));
  DbConnectionProfile env = p;
  env.passwordEnv.clear();
  QString why;
  QVERIFY(!env.isValid(&why));
  QVERIFY(why.contains("environment variable"));
}

void TestDatabaseConnections::testSslArgumentsPerClient() {
  const auto mysql = CliDialect::create(DbEngine::MySql);
  DbConnectionProfile p;
  p.engine = DbEngine::MySql;
  QVERIFY(mysql->sslArguments(p, false).isEmpty());
  p.sslMode = "require";
  QCOMPARE(mysql->sslArguments(p, false), QStringList{"--ssl-mode=REQUIRED"});
  QCOMPARE(mysql->sslArguments(p, true),
           QStringList({"--ssl", "--skip-ssl-verify-server-cert"}));
  p.sslMode = "verify-full";
  QCOMPARE(mysql->sslArguments(p, false),
           QStringList{"--ssl-mode=VERIFY_IDENTITY"});
  QCOMPARE(mysql->sslArguments(p, true),
           QStringList({"--ssl", "--ssl-verify-server-cert"}));
  p.sslMode = "disable";
  QCOMPARE(mysql->sslArguments(p, true), QStringList{"--skip-ssl"});
  QVERIFY(CliDialect::isMariaDbClient("/usr/bin/mariadb"));
}

void TestDatabaseConnections::testPostgresSslEnvironment() {
  const auto pg = CliDialect::create(DbEngine::PostgreSql);
  DbConnectionProfile p;
  p.engine = DbEngine::PostgreSql;
  QVERIFY(!pg->clientEnvironment(p).contains("PGSSLMODE"));
  p.sslMode = "verify-full";
  QCOMPARE(pg->clientEnvironment(p).value("PGSSLMODE"), QString("verify-full"));
}

void TestDatabaseConnections::testDockerLaunchCarriesSslFlags() {
  const auto mysql = CliDialect::create(DbEngine::MySql);
  DbConnectionProfile p;
  p.engine = DbEngine::MySql;
  p.transport = DbTransport::Docker;
  p.container = "db";
  p.user = "root";
  p.sslMode = "require";
  const CliLaunch launch = mysql->launch(p, "pw");
  QCOMPARE(launch.program, QString("docker"));
  QVERIFY(launch.arguments.contains("LP_SSL_MYSQL=--ssl-mode=REQUIRED"));
  QVERIFY(launch.arguments.contains(
      "LP_SSL_MARIADB=--ssl --skip-ssl-verify-server-cert"));
  // Without a mode no flavour probe runs inside the container.
  p.sslMode.clear();
  const CliLaunch plain = mysql->launch(p, "pw");
  for (const QString &arg : plain.arguments) {
    QVERIFY(!arg.startsWith("LP_SSL_"));
  }
}

void TestDatabaseConnections::testTemplatesAreUsable() {
  DbConnectionTemplateCatalog &catalog =
      DbConnectionTemplateCatalog::instance();
  catalog.reload();
  QVERIFY(catalog.templates().size() >= 20);
  QVERIFY(catalog.categories().contains("Cloud"));
  QSet<QString> ids;
  for (const DbConnectionTemplate &entry : catalog.templates()) {
    QVERIFY2(!ids.contains(entry.id), qPrintable(entry.id));
    ids.insert(entry.id);
    QVERIFY(!entry.description.isEmpty());
    QVERIFY(catalog.categories().contains(entry.category));
    DbConnectionProfile p = entry.instantiate();
    QCOMPARE(p.name, entry.name);
    if (p.engine == DbEngine::Sqlite) {
      continue;
    }
    // Cloud templates leave the host to the user but say what it looks like.
    if (p.host.isEmpty()) {
      QVERIFY2(!entry.hostPlaceholder.isEmpty() ||
                   p.transport == DbTransport::Docker,
               qPrintable(entry.id));
    }
  }
  const DbConnectionTemplate supabase = catalog.templateById("supabase");
  QCOMPARE(supabase.profile.engine, DbEngine::PostgreSql);
  QCOMPARE(supabase.profile.sslMode, QString("require"));
  QVERIFY(!supabase.help.isEmpty());
  const DbConnectionTemplate memory = catalog.templateById("sqlite.memory");
  QCOMPARE(memory.profile.filePath, QString(":memory:"));
  QVERIFY(memory.profile.isValid() == false); // needs a name only
  QVERIFY(memory.instantiate().isValid());
  QCOMPARE(catalog.templateById("cockroach.local").profile.passwordSource,
           DbPasswordSource::None);
}

void TestDatabaseConnections::testUserTemplatesOverrideAndExtend() {
  const QJsonObject root = QJsonDocument::fromJson(R"({
    "categories": ["Team"],
    "templates": [
      {"id": "team.pg", "name": "Team Postgres", "category": "Team",
       "engine": "postgres", "host": "pg.team", "port": 6000,
       "description": "Shared dev database"},
      {"id": "broken", "name": "No engine"},
      {"name": "No id", "engine": "mysql"}
    ]})")
                               .object();
  QStringList categories;
  const auto entries = DbConnectionTemplateCatalog::parse(root, &categories);
  QCOMPARE(entries.size(), 1);
  QCOMPARE(categories, QStringList{"Team"});
  QCOMPARE(entries.first().profile.host, QString("pg.team"));
  QCOMPARE(entries.first().profile.user, QString("postgres"));
}

void TestDatabaseConnections::testEnvFileParsing() {
  const auto env = DbProjectDiscovery::parseEnvFile(
      "# comment\n"
      "export DB_USER=app\n"
      "DB_PASSWORD=\"quoted # not a comment\"\n"
      "DB_NAME='literal ${DB_USER}'\n"
      "DB_HOST=localhost # trailing comment\n"
      "DATABASE_URL=postgres://${DB_USER}@${DB_HOST}:5432/app\n"
      "broken line\n");
  QCOMPARE(env.value("DB_USER"), QString("app"));
  QCOMPARE(env.value("DB_PASSWORD"), QString("quoted # not a comment"));
  QCOMPARE(env.value("DB_NAME"), QString("literal ${DB_USER}"));
  QCOMPARE(env.value("DB_HOST"), QString("localhost"));
  QCOMPARE(env.value("DATABASE_URL"),
           QString("postgres://app@localhost:5432/app"));
}

void TestDatabaseConnections::testEnvironmentDetection() {
  QMap<QString, QString> env;
  env["DATABASE_URL"] = "postgres://web:pw@localhost:5432/site";
  env["REDIS_URL"] = "redis://localhost:6379";
  env["EXAMPLE_URL"] = "postgres://<user>:<password>@<host>/db";
  env["DB_CONNECTION"] = "mysql";
  env["DB_HOST"] = "127.0.0.1";
  env["DB_PORT"] = "3306";
  env["DB_DATABASE"] = "laravel";
  env["DB_USERNAME"] = "sail";
  env["DB_PASSWORD"] = "password";
  env["POSTGRES_USER"] = "admin";
  env["POSTGRES_PASSWORD"] = "secret";
  env["POSTGRES_DB"] = "analytics";
  const auto found = DbProjectDiscovery::fromEnvironment(env, ".env");
  QCOMPARE(found.size(), 3);
  QCOMPARE(found[0].title, QString("DATABASE_URL"));
  QCOMPARE(found[0].profile.database, QString("site"));
  QCOMPARE(found[0].password, QString("pw"));
  QCOMPARE(found[1].profile.engine, DbEngine::MySql);
  QCOMPARE(found[1].profile.user, QString("sail"));
  QCOMPARE(found[1].profile.database, QString("laravel"));
  QCOMPARE(found[2].profile.engine, DbEngine::PostgreSql);
  QCOMPARE(found[2].profile.user, QString("admin"));
  QCOMPARE(found[2].profile.database, QString("analytics"));
  QCOMPARE(found[2].password, QString("secret"));
  for (const auto &entry : found) {
    QCOMPARE(entry.source, QString(".env"));
  }
}

void TestDatabaseConnections::testComposeDetection() {
  const QString yaml = R"(
version: "3.9"
services:
  web:
    build: .
    ports:
      - "8000:8000"
  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: ${PG_USER:-shop}
      POSTGRES_PASSWORD: "hunter2"
      POSTGRES_DB: shop
    ports:
      - "127.0.0.1:5433:5432"
  cache:
    image: redis:7
  mysql:
    image: "mariadb:11"
    container_name: legacy-mysql
    environment:
      - MARIADB_ROOT_PASSWORD=rootpw
      - MARIADB_DATABASE=legacy
  mssql:
    image: mcr.microsoft.com/mssql/server:2022-latest
    ports: ["14330:1433"]
    environment:
      MSSQL_SA_PASSWORD: Str0ng!Pass # comment
volumes:
  data:
)";
  const auto direct = DbProjectDiscovery::fromCompose(
      yaml, "docker-compose.yml", "myapp", {}, true);
  QCOMPARE(direct.size(), 3);
  const DbDetectedConnection &pg = direct[0];
  QCOMPARE(pg.title, QString("db"));
  QCOMPARE(pg.profile.engine, DbEngine::PostgreSql);
  QCOMPARE(pg.profile.user, QString("shop"));
  QCOMPARE(pg.profile.database, QString("shop"));
  QCOMPARE(pg.password, QString("hunter2"));
  QCOMPARE(pg.profile.port, 5433);
  QCOMPARE(pg.profile.container, QString("myapp-db-1"));

  const DbDetectedConnection &maria = direct[1];
  QCOMPARE(maria.profile.engine, DbEngine::MySql);
  // No published port: reached through docker exec.
  QCOMPARE(maria.profile.transport, DbTransport::Docker);
  QCOMPARE(maria.profile.container, QString("legacy-mysql"));
  QCOMPARE(maria.profile.user, QString("root"));
  QCOMPARE(maria.password, QString("rootpw"));
  QCOMPARE(maria.profile.database, QString("legacy"));

  QCOMPARE(direct[2].profile.engine, DbEngine::SqlServer);
  QCOMPARE(direct[2].profile.port, 14330);
  QCOMPARE(direct[2].password, QString("Str0ng!Pass"));

  // Without the clients installed everything goes through docker exec.
  const auto viaDocker = DbProjectDiscovery::fromCompose(
      yaml, "docker-compose.yml", "myapp", {{"PG_USER", "other"}}, false);
  QCOMPARE(viaDocker[0].profile.transport, DbTransport::Docker);
  QCOMPARE(viaDocker[0].profile.user, QString("other"));
}

void TestDatabaseConnections::testProjectScan() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  auto write = [&](const QString &name, const QByteArray &data) {
    QDir().mkpath(QFileInfo(dir.filePath(name)).absolutePath());
    QFile file(dir.filePath(name));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
  };
  write(".env", "DATABASE_URL=sqlite:///data/app.db\n"
                "PGPASSWORD_FOR_TEST=from-dotenv\n");
  write("data/app.db", QByteArray("SQLite format 3") + QByteArray(1, '\0') +
                           QByteArray(100, 'x'));
  write("notes.db", "not a database");
  write("node_modules/pkg/cache.sqlite",
        QByteArray("SQLite format 3") + QByteArray(1, '\0'));
  write("backend/compose.yaml", "services:\n  pg:\n    image: postgres\n");
  const auto found = DbProjectDiscovery::scan(dir.path());
  QStringList titles;
  for (const auto &entry : found) {
    titles << entry.title + "@" + entry.source;
  }
  QVERIFY2(titles.contains("DATABASE_URL@.env"), qPrintable(titles.join(',')));
  QVERIFY2(titles.contains("pg@backend/compose.yaml"),
           qPrintable(titles.join(',')));
  // data/app.db is the same file as DATABASE_URL and is listed once; the
  // fake .db and files under node_modules are ignored.
  QCOMPARE(found.size(), 2);

  DbProjectDiscovery::setWorkspaceFolder(dir.path());
  bool ok = false;
  QCOMPARE(DbProjectDiscovery::environmentValue("PGPASSWORD_FOR_TEST", &ok),
           QString("from-dotenv"));
  QVERIFY(ok);
  DbProjectDiscovery::environmentValue("NOT_DEFINED_ANYWHERE_42", &ok);
  QVERIFY(!ok);
  DbProjectDiscovery::setWorkspaceFolder(QString());
}

void TestDatabaseConnections::testPasswordSources() {
  QTemporaryDir dir;
  QFile db(dir.filePath("x.db"));
  QVERIFY(db.open(QIODevice::WriteOnly));
  db.close();

  DbConnectionProfile p;
  p.name = "pg";
  p.engine = DbEngine::PostgreSql;
  p.passwordSource = DbPasswordSource::Ask;
  DbConnection ask(p);
  QVERIFY(!ask.resolvePassword());
  QVERIFY(!ask.hasSessionPassword());

  p.passwordSource = DbPasswordSource::None;
  DbConnection none(p);
  QVERIFY(none.resolvePassword());
  QVERIFY(none.hasSessionPassword());

  qputenv("LP_TEST_DB_PASSWORD", "env-secret");
  p.passwordSource = DbPasswordSource::Environment;
  p.passwordEnv = "LP_TEST_DB_PASSWORD";
  DbConnection env(p);
  QVERIFY(env.resolvePassword());
  QVERIFY(env.hasSessionPassword());
  p.passwordEnv = "LP_TEST_DB_PASSWORD_MISSING";
  DbConnection missing(p);
  QVERIFY(!missing.resolvePassword());
  qunsetenv("LP_TEST_DB_PASSWORD");
}

void TestDatabaseConnections::testDatabaseChangeKeepsSessionAccurate() {
  // In-memory SQLite connects without a file and with no password.
  DbConnectionProfile p;
  p.name = "scratch";
  p.engine = DbEngine::Sqlite;
  p.filePath = ":memory:";
  DbConnection conn(p);
  QSignalSpy state(&conn, &DbConnection::stateChanged);
  QVERIFY(conn.reconnect());
  QTRY_VERIFY(conn.isConnected());
  QSignalSpy finished(&conn, &DbConnection::statementFinished);
  conn.execute({"CREATE TABLE t(x)", "INSERT INTO t VALUES (1)",
                "SELECT count(*) FROM t"});
  QTRY_COMPARE(finished.count(), 3);
  const auto result = finished.last().at(2).value<DbStatementResult>();
  QVERIFY(result.ok);
  QCOMPARE(result.resultSets.first().rows.first().first().toInt(), 1);

  // Renaming the connection keeps the session.
  DbConnectionProfile renamed = conn.profile();
  renamed.name = "renamed";
  conn.setProfile(renamed);
  QVERIFY(conn.isConnected());
  // Changing what it points at closes it.
  DbConnectionProfile other = conn.profile();
  other.readOnly = true;
  conn.setProfile(other);
  QVERIFY(!conn.isConnected());
}

QTEST_MAIN(TestDatabaseConnections)
#include "test_databaseconnections.moc"
