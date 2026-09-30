#include "dbtypes.h"

#include <QFileInfo>
#include <QJsonValue>

namespace DbEngineInfo {

QString id(DbEngine engine) {
  switch (engine) {
  case DbEngine::Sqlite:
    return QStringLiteral("sqlite");
  case DbEngine::PostgreSql:
    return QStringLiteral("postgresql");
  case DbEngine::MySql:
    return QStringLiteral("mysql");
  case DbEngine::SqlServer:
    return QStringLiteral("sqlserver");
  }
  return QStringLiteral("sqlite");
}

DbEngine fromId(const QString &id, bool *ok) {
  const QString key = id.trimmed().toLower();
  if (ok) {
    *ok = true;
  }
  if (key == "sqlite" || key == "sqlite3") {
    return DbEngine::Sqlite;
  }
  if (key == "postgresql" || key == "postgres" || key == "pg") {
    return DbEngine::PostgreSql;
  }
  if (key == "mysql" || key == "mariadb") {
    return DbEngine::MySql;
  }
  if (key == "sqlserver" || key == "mssql") {
    return DbEngine::SqlServer;
  }
  if (ok) {
    *ok = false;
  }
  return DbEngine::Sqlite;
}

QString displayName(DbEngine engine) {
  switch (engine) {
  case DbEngine::Sqlite:
    return QStringLiteral("SQLite");
  case DbEngine::PostgreSql:
    return QStringLiteral("PostgreSQL");
  case DbEngine::MySql:
    return QStringLiteral("MySQL / MariaDB");
  case DbEngine::SqlServer:
    return QStringLiteral("SQL Server");
  }
  return {};
}

QString clientName(DbEngine engine) {
  switch (engine) {
  case DbEngine::Sqlite:
    return QStringLiteral("sqlite");
  case DbEngine::PostgreSql:
    return QStringLiteral("psql");
  case DbEngine::MySql:
    return QStringLiteral("mysql");
  case DbEngine::SqlServer:
    return QStringLiteral("sqlcmd");
  }
  return {};
}

int defaultPort(DbEngine engine) {
  switch (engine) {
  case DbEngine::PostgreSql:
    return 5432;
  case DbEngine::MySql:
    return 3306;
  case DbEngine::SqlServer:
    return 1433;
  case DbEngine::Sqlite:
    break;
  }
  return 0;
}

QString defaultUser(DbEngine engine) {
  switch (engine) {
  case DbEngine::PostgreSql:
    return QStringLiteral("postgres");
  case DbEngine::MySql:
    return QStringLiteral("root");
  case DbEngine::SqlServer:
    return QStringLiteral("sa");
  case DbEngine::Sqlite:
    break;
  }
  return {};
}

QString defaultDatabase(DbEngine engine) {
  switch (engine) {
  case DbEngine::PostgreSql:
    return QStringLiteral("postgres");
  case DbEngine::SqlServer:
    return QStringLiteral("master");
  case DbEngine::MySql:
  case DbEngine::Sqlite:
    break;
  }
  return {};
}

bool usesFile(DbEngine engine) { return engine == DbEngine::Sqlite; }

bool supportsDocker(DbEngine engine) { return engine != DbEngine::Sqlite; }

QVector<DbEngine> allEngines() {
  return {DbEngine::SqlServer, DbEngine::PostgreSql, DbEngine::MySql,
          DbEngine::Sqlite};
}

bool engineForImage(const QString &image, DbEngine *engine) {

  QString name = image.toLower();
  const int at = name.indexOf('@');
  if (at >= 0) {
    name = name.left(at);
  }
  const int slash = name.lastIndexOf('/');
  QString repo = name;
  QString tail = slash >= 0 ? name.mid(slash + 1) : name;
  const int colon = tail.indexOf(':');
  if (colon >= 0) {
    tail = tail.left(colon);
  }
  if (slash >= 0) {
    repo = name.left(slash + 1) + tail;
  } else {
    repo = tail;
  }

  DbEngine found = DbEngine::Sqlite;
  bool matched = true;
  if (repo.contains(QStringLiteral("mssql")) ||
      repo.contains(QStringLiteral("sql-server")) ||
      repo.contains(QStringLiteral("sqlserver"))) {
    found = DbEngine::SqlServer;
  } else if (tail == "postgres" || tail == "postgresql" ||
             tail.startsWith(QStringLiteral("postgres-")) ||
             tail.startsWith(QStringLiteral("postgis")) ||
             tail.startsWith(QStringLiteral("timescaledb"))) {
    found = DbEngine::PostgreSql;
  } else if (tail == "mysql" || tail == "mariadb" || tail == "percona" ||
             tail.startsWith(QStringLiteral("mysql-")) ||
             tail.startsWith(QStringLiteral("mariadb-")) ||
             tail == "mysql-server") {
    found = DbEngine::MySql;
  } else {
    matched = false;
  }
  if (matched && engine) {
    *engine = found;
  }
  return matched;
}

} // namespace DbEngineInfo

QString DbConnectionProfile::summary() const {
  if (engine == DbEngine::Sqlite) {
    return filePath.isEmpty() ? QStringLiteral("SQLite (unsaved)")
                              : QFileInfo(filePath).fileName();
  }
  QString where;
  if (transport == DbTransport::Docker) {
    where = QStringLiteral("docker:") + container;
  } else {
    where = host.isEmpty() ? QStringLiteral("localhost") : host;
    where += QLatin1Char(':') + QString::number(effectivePort());
  }
  QString text = user.isEmpty() ? where : user + QLatin1Char('@') + where;
  if (!database.isEmpty()) {
    text += QLatin1Char('/') + database;
  }
  return text;
}

bool DbConnectionProfile::isValid(QString *reason) const {
  auto fail = [reason](const QString &why) {
    if (reason) {
      *reason = why;
    }
    return false;
  };
  if (name.trimmed().isEmpty()) {
    return fail(QStringLiteral("Give the connection a name."));
  }
  if (engine == DbEngine::Sqlite) {
    if (filePath.trimmed().isEmpty()) {
      return fail(QStringLiteral("Choose a SQLite database file."));
    }
    return true;
  }
  if (transport == DbTransport::Docker) {
    if (container.trimmed().isEmpty()) {
      return fail(QStringLiteral("Choose a Docker container."));
    }
  } else if (host.trimmed().isEmpty()) {
    return fail(QStringLiteral("Enter a host name."));
  }
  if (port < 0 || port > 65535) {
    return fail(QStringLiteral("The port must be between 1 and 65535."));
  }
  return true;
}

QJsonObject DbConnectionProfile::toJson() const {
  QJsonObject obj;
  obj["id"] = id;
  obj["name"] = name;
  obj["engine"] = DbEngineInfo::id(engine);
  obj["transport"] = transport == DbTransport::Docker
                         ? QStringLiteral("docker")
                         : QStringLiteral("direct");
  obj["host"] = host;
  obj["port"] = port;
  obj["user"] = user;
  obj["database"] = database;
  obj["filePath"] = filePath;
  obj["container"] = container;
  obj["clientPath"] = clientPath;
  obj["extraArgs"] = extraArgs;
  obj["trustServerCertificate"] = trustServerCertificate;
  obj["readOnly"] = readOnly;
  obj["confirmDestructive"] = confirmDestructive;
  obj["color"] = color;
  return obj;
}

DbConnectionProfile DbConnectionProfile::fromJson(const QJsonObject &obj) {
  DbConnectionProfile p;
  p.id = obj.value("id").toString();
  p.name = obj.value("name").toString();
  p.engine = DbEngineInfo::fromId(obj.value("engine").toString());
  p.transport = obj.value("transport").toString() == "docker"
                    ? DbTransport::Docker
                    : DbTransport::Direct;
  p.host = obj.value("host").toString(QStringLiteral("localhost"));
  p.port = obj.value("port").toInt(0);
  p.user = obj.value("user").toString();
  p.database = obj.value("database").toString();
  p.filePath = obj.value("filePath").toString();
  p.container = obj.value("container").toString();
  p.clientPath = obj.value("clientPath").toString();
  p.extraArgs = obj.value("extraArgs").toString();
  p.trustServerCertificate = obj.value("trustServerCertificate").toBool(true);
  p.readOnly = obj.value("readOnly").toBool(false);
  p.confirmDestructive = obj.value("confirmDestructive").toBool(true);
  p.color = obj.value("color").toString();
  return p;
}
