#include "dbprojectdiscovery.h"

#include "clidialect.h"
#include "dbconnectionurl.h"
#include "dockerdiscovery.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <functional>

namespace {

QString g_workspaceFolder;

QString readText(const QString &path, qint64 limit = 512 * 1024) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > limit) {
    return {};
  }
  return QString::fromUtf8(file.readAll());
}

// Expands ${NAME}, ${NAME:-default}, ${NAME-default} and $NAME.
QString expandVariables(const QString &value,
                        const QMap<QString, QString> &variables) {
  if (!value.contains('$')) {
    return value;
  }
  static const QRegularExpression ref(
      QStringLiteral("\\$\\{([A-Za-z_][A-Za-z0-9_]*)(?::?-([^}]*))?\\}|\\$([A-"
                     "Za-z_][A-Za-z0-9_]*)"));
  const QProcessEnvironment system = QProcessEnvironment::systemEnvironment();
  QString out;
  int last = 0;
  auto it = ref.globalMatch(value);
  while (it.hasNext()) {
    const auto m = it.next();
    out += value.mid(last, m.capturedStart() - last);
    const QString name =
        m.captured(1).isEmpty() ? m.captured(3) : m.captured(1);
    QString replacement;
    if (variables.contains(name)) {
      replacement = variables.value(name);
    } else if (system.contains(name)) {
      replacement = system.value(name);
    } else {
      replacement = m.captured(2);
    }
    out += replacement;
    last = m.capturedEnd();
  }
  out += value.mid(last);
  return out;
}

QString unquote(QString value) {
  value = value.trimmed();
  if (value.size() >= 2 && ((value.startsWith('"') && value.endsWith('"')) ||
                            (value.startsWith('\'') && value.endsWith('\'')))) {
    value = value.mid(1, value.size() - 2);
  }
  return value;
}

bool looksLikePlaceholder(const QString &value) {
  return value.contains('<') || value.contains(QLatin1String("${")) ||
         value.contains(QLatin1String("..."));
}

QString identity(const DbConnectionProfile &p) {
  if (p.engine == DbEngine::Sqlite) {
    return QStringLiteral("sqlite|") + QDir::cleanPath(p.filePath);
  }
  return QStringList{DbEngineInfo::id(p.engine),
                     p.transport == DbTransport::Docker ? p.container : p.host,
                     QString::number(p.effectivePort()),
                     p.user,
                     p.database,
                     QDir::cleanPath(p.filePath)}
      .join('|');
}

bool hasLocalClient(DbEngine engine) {
  const auto dialect = CliDialect::create(engine);
  if (!dialect) {
    return true;
  }
  for (const QString &candidate : dialect->clientCandidates()) {
    if (!QStandardPaths::findExecutable(candidate).isEmpty() ||
        QFileInfo(candidate).isExecutable()) {
      return true;
    }
  }
  return false;
}

bool engineForName(const QString &value, DbEngine *engine) {
  const QString v = value.trimmed().toLower();
  if (v.isEmpty()) {
    return false;
  }
  if (v.contains("postgres") || v == "pgsql" || v == "pg" ||
      v.contains("psycopg")) {
    *engine = DbEngine::PostgreSql;
  } else if (v.contains("mysql") || v.contains("maria")) {
    *engine = DbEngine::MySql;
  } else if (v.contains("sqlsrv") || v.contains("mssql") ||
             v.contains("sqlserver") || v.contains("sql_server")) {
    *engine = DbEngine::SqlServer;
  } else if (v.contains("sqlite")) {
    *engine = DbEngine::Sqlite;
  } else {
    return false;
  }
  return true;
}

// Groups like DB_HOST/DB_PORT/DB_USERNAME/DB_PASSWORD/DB_DATABASE.
struct VariableGroup {
  QString prefix;
  bool fixedEngine = false;
  DbEngine engine = DbEngine::PostgreSql;
};

QString firstValue(const QMap<QString, QString> &env, const QString &prefix,
                   const QStringList &names) {
  for (const QString &name : names) {
    const QString value = env.value(prefix + name).trimmed();
    if (!value.isEmpty()) {
      return value;
    }
  }
  return {};
}

} // namespace

namespace DbProjectDiscovery {

QMap<QString, QString> parseEnvFile(const QString &text) {
  QMap<QString, QString> env;
  static const QRegularExpression line(QStringLiteral(
      "^\\s*(?:export\\s+)?([A-Za-z_][A-Za-z0-9_.]*)\\s*[=:]\\s*(.*)$"));
  for (const QString &raw : text.split('\n')) {
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith('#')) {
      continue;
    }
    const auto m = line.match(raw);
    if (!m.hasMatch()) {
      continue;
    }
    QString value = m.captured(2).trimmed();
    if (value.startsWith('"')) {
      int end = 1;
      QString unescaped;
      while (end < value.size() && value[end] != '"') {
        if (value[end] == '\\' && end + 1 < value.size()) {
          const QChar next = value[end + 1];
          unescaped += next == 'n' ? QChar('\n') : next;
          end += 2;
          continue;
        }
        unescaped += value[end++];
      }
      value = expandVariables(unescaped, env);
    } else if (value.startsWith('\'')) {
      const int end = value.indexOf('\'', 1);
      value = value.mid(1, end > 0 ? end - 1 : -1);
    } else {
      const int comment = value.indexOf(QLatin1String(" #"));
      if (comment >= 0) {
        value = value.left(comment).trimmed();
      }
      value = expandVariables(value, env);
    }
    env.insert(m.captured(1), value);
  }
  return env;
}

QVector<DbDetectedConnection> fromEnvironment(const QMap<QString, QString> &env,
                                              const QString &source,
                                              const QString &baseDir) {
  QVector<DbDetectedConnection> out;
  // Whole connection strings: DATABASE_URL, POSTGRES_URL, DB_DSN, ...
  for (auto it = env.constBegin(); it != env.constEnd(); ++it) {
    const QString value = it.value().trimmed();
    const QString key = it.key().toUpper();
    const bool urlKey = key.contains("URL") || key.contains("URI") ||
                        key.contains("DSN") || key.contains("CONNECTION");
    if (value.isEmpty() || looksLikePlaceholder(value) ||
        (!value.contains(QLatin1String("://")) &&
         !value.startsWith(QLatin1String("jdbc:"), Qt::CaseInsensitive) &&
         !(urlKey && value.contains('=') && value.contains(';')))) {
      continue;
    }
    DbConnectionUrl::Parsed parsed;
    if (!DbConnectionUrl::parse(value, &parsed, nullptr, baseDir)) {
      continue;
    }
    if (parsed.profile.engine == DbEngine::Sqlite &&
        parsed.profile.filePath != QLatin1String(":memory:") &&
        !QFileInfo::exists(parsed.profile.filePath)) {
      continue;
    }
    DbDetectedConnection found;
    found.title = it.key();
    found.source = source;
    found.profile = parsed.profile;
    found.password = parsed.password;
    found.hasPassword = parsed.hasPassword;
    out.append(found);
  }

  static const QList<VariableGroup> groups = {
      {QStringLiteral("DB_")},
      {QStringLiteral("DATABASE_")},
      {QStringLiteral("PG"), true, DbEngine::PostgreSql},
      {QStringLiteral("POSTGRES_"), true, DbEngine::PostgreSql},
      {QStringLiteral("POSTGRESQL_"), true, DbEngine::PostgreSql},
      {QStringLiteral("MYSQL_"), true, DbEngine::MySql},
      {QStringLiteral("MARIADB_"), true, DbEngine::MySql},
      {QStringLiteral("MSSQL_"), true, DbEngine::SqlServer},
      {QStringLiteral("SQLSERVER_"), true, DbEngine::SqlServer},
  };
  for (const VariableGroup &group : groups) {
    const QString &prefix = group.prefix;
    const QString host = firstValue(env, prefix, {"HOST", "SERVER"});
    const QString port = firstValue(env, prefix, {"PORT"});
    QString user = firstValue(env, prefix, {"USER", "USERNAME"});
    QString password = firstValue(env, prefix, {"PASSWORD", "PASS"});
    QString database = firstValue(env, prefix, {"DATABASE", "NAME", "DB"});
    if (group.engine == DbEngine::MySql && group.fixedEngine &&
        user.isEmpty()) {
      const QString root = firstValue(env, prefix, {"ROOT_PASSWORD"});
      if (!root.isEmpty()) {
        user = QStringLiteral("root");
        password = root;
      }
    }
    if (group.engine == DbEngine::SqlServer && group.fixedEngine) {
      const QString sa = firstValue(env, prefix, {"SA_PASSWORD"});
      if (!sa.isEmpty() && password.isEmpty()) {
        password = sa;
        user = user.isEmpty() ? QStringLiteral("sa") : user;
      }
    }
    const int fields = int(!host.isEmpty()) + int(!port.isEmpty()) +
                       int(!user.isEmpty()) + int(!password.isEmpty()) +
                       int(!database.isEmpty());
    if (fields < 2 || looksLikePlaceholder(host)) {
      continue;
    }
    DbEngine engine = group.engine;
    if (!group.fixedEngine) {
      const QString kind = firstValue(
          env, prefix,
          {"CONNECTION", "ENGINE", "DRIVER", "TYPE", "DIALECT", "CLIENT"});
      if (!engineForName(kind, &engine)) {
        const int number = port.toInt();
        if (number == 5432) {
          engine = DbEngine::PostgreSql;
        } else if (number == 3306) {
          engine = DbEngine::MySql;
        } else if (number == 1433) {
          engine = DbEngine::SqlServer;
        } else {
          continue;
        }
      }
    }
    DbDetectedConnection found;
    found.title = prefix + QStringLiteral("* variables");
    found.source = source;
    DbConnectionProfile &p = found.profile;
    p.engine = engine;
    if (engine == DbEngine::Sqlite) {
      if (database.isEmpty()) {
        continue;
      }
      p.filePath = QDir::isRelativePath(database) && !baseDir.isEmpty()
                       ? QDir(baseDir).absoluteFilePath(database)
                       : database;
      if (!QFileInfo::exists(p.filePath)) {
        continue;
      }
    } else {
      p.host = host.isEmpty() ? QStringLiteral("localhost") : host;
      p.port = port.toInt();
      p.user = user.isEmpty() ? DbEngineInfo::defaultUser(engine) : user;
      p.database = database;
      found.password = password;
      found.hasPassword = !password.isEmpty();
    }
    out.append(found);
  }
  return out;
}

QVector<DbDetectedConnection>
fromCompose(const QString &yaml, const QString &source,
            const QString &projectName, const QMap<QString, QString> &variables,
            bool preferDirect) {
  struct Service {
    QString name;
    QString image;
    QString containerName;
    QStringList ports;
    QStringList env;
    QString longTarget;
    QString longPublished;
  };
  QVector<Service> services;

  int servicesIndent = -1;
  int serviceIndent = -1;
  int propertyIndent = -1;
  QString section;
  Service *current = nullptr;
  auto indentOf = [](const QString &line) {
    int i = 0;
    while (i < line.size() && line[i] == ' ') {
      ++i;
    }
    return i;
  };
  auto stripComment = [](QString text) {
    const int hash = text.indexOf(QLatin1String(" #"));
    return (hash >= 0 ? text.left(hash) : text).trimmed();
  };
  auto addListValues = [](QString text, QStringList *into) {
    text = text.trimmed();
    if (text.startsWith('[') && text.endsWith(']')) {
      for (const QString &item : text.mid(1, text.size() - 2).split(',')) {
        if (!item.trimmed().isEmpty()) {
          into->append(unquote(item));
        }
      }
    }
  };

  for (const QString &raw : yaml.split('\n')) {
    if (raw.trimmed().isEmpty() || raw.trimmed().startsWith('#')) {
      continue;
    }
    const int indent = indentOf(raw);
    const QString text = stripComment(raw.mid(indent));
    if (servicesIndent < 0) {
      if (text == QLatin1String("services:")) {
        servicesIndent = indent;
      }
      continue;
    }
    if (indent <= servicesIndent) {
      break;
    }
    if (serviceIndent < 0) {
      serviceIndent = indent;
    }
    if (indent == serviceIndent) {
      if (text.endsWith(':')) {
        services.append(Service{});
        current = &services.last();
        current->name = unquote(text.left(text.size() - 1));
        propertyIndent = -1;
        section.clear();
      }
      continue;
    }
    if (!current) {
      continue;
    }
    if (propertyIndent < 0) {
      propertyIndent = indent;
    }
    if (indent == propertyIndent) {
      const int colon = text.indexOf(':');
      const QString key = colon > 0 ? text.left(colon).trimmed() : text;
      const QString value =
          colon > 0 ? text.mid(colon + 1).trimmed() : QString();
      section = key;
      if (key == QLatin1String("image")) {
        current->image = expandVariables(unquote(value), variables);
      } else if (key == QLatin1String("container_name")) {
        current->containerName = expandVariables(unquote(value), variables);
      } else if (key == QLatin1String("ports")) {
        addListValues(value, &current->ports);
      } else if (key == QLatin1String("environment")) {
        addListValues(value, &current->env);
      }
      continue;
    }
    // Lines below a property: list items or mapping entries.
    QString item = text;
    const bool listItem = item.startsWith(QLatin1String("- ")) || item == "-";
    if (listItem) {
      item = item.mid(1).trimmed();
    }
    if (section == QLatin1String("ports")) {
      if (item.startsWith(QLatin1String("target:"))) {
        current->longTarget = unquote(item.mid(7));
      } else if (item.startsWith(QLatin1String("published:"))) {
        current->longPublished = unquote(item.mid(10));
      } else if (listItem && !item.contains(QLatin1String(": "))) {
        current->ports << expandVariables(unquote(item), variables);
      }
      if (!current->longTarget.isEmpty() && !current->longPublished.isEmpty()) {
        current->ports << current->longPublished + QLatin1Char(':') +
                              current->longTarget;
        current->longTarget.clear();
        current->longPublished.clear();
      }
    } else if (section == QLatin1String("environment")) {
      if (listItem) {
        current->env << expandVariables(unquote(item), variables);
      } else {
        const int colon = item.indexOf(':');
        if (colon > 0) {
          current->env << item.left(colon).trimmed() + QLatin1Char('=') +
                              expandVariables(unquote(item.mid(colon + 1)),
                                              variables);
        }
      }
    }
  }

  QVector<DbDetectedConnection> out;
  for (const Service &service : services) {
    DbEngine engine = DbEngine::Sqlite;
    if (!DbEngineInfo::engineForImage(service.image, &engine)) {
      continue;
    }
    const int containerPort = DbEngineInfo::defaultPort(engine);
    int hostPort = 0;
    QString hostAddress;
    for (const QString &mapping : service.ports) {
      QString value = mapping.section('/', 0, 0);
      const QStringList parts = value.split(':');
      if (parts.size() < 2 || parts.last().toInt() != containerPort) {
        continue;
      }
      hostPort = parts.at(parts.size() - 2).section('-', 0, 0).toInt();
      if (parts.size() >= 3) {
        hostAddress = parts.first();
      }
    }
    const DockerCredentialHints hints =
        DockerDiscoveryParsing::credentialsFromEnv(engine, service.env);
    DbDetectedConnection found;
    found.title = service.name;
    found.source = source;
    DbConnectionProfile &p = found.profile;
    p.engine = engine;
    p.user =
        hints.user.isEmpty() ? DbEngineInfo::defaultUser(engine) : hints.user;
    p.database = hints.database.isEmpty()
                     ? DbEngineInfo::defaultDatabase(engine)
                     : hints.database;
    p.port = hostPort;
    p.container = service.containerName.isEmpty()
                      ? QStringLiteral("%1-%2-1").arg(projectName, service.name)
                      : service.containerName;
    if (hostPort > 0 && preferDirect && hasLocalClient(engine)) {
      p.transport = DbTransport::Direct;
      p.host = hostAddress.isEmpty() || hostAddress == QLatin1String("0.0.0.0")
                   ? QStringLiteral("localhost")
                   : hostAddress;
    } else {
      p.transport = DbTransport::Docker;
      p.host = QStringLiteral("localhost");
    }
    found.password = hints.password;
    found.hasPassword = hints.hasPassword();
    out.append(found);
  }
  return out;
}

QVector<DbDetectedConnection> scan(const QString &workspaceFolder) {
  QVector<DbDetectedConnection> out;
  if (workspaceFolder.isEmpty() || !QFileInfo(workspaceFolder).isDir()) {
    return out;
  }
  const QDir root(workspaceFolder);
  static const QStringList skipped = {
      ".git",   "node_modules", ".venv",  "venv",        "env",
      "build",  "dist",         "target", "__pycache__", ".lightpad",
      ".cache", ".idea",        ".vscode"};
  QStringList folders = {root.absolutePath()};
  for (const QFileInfo &child :
       root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
    if (!skipped.contains(child.fileName())) {
      folders << child.absoluteFilePath();
    }
  }

  QSet<QString> seen;
  auto add = [&](const QVector<DbDetectedConnection> &found) {
    for (const DbDetectedConnection &entry : found) {
      const QString key = identity(entry.profile);
      if (!seen.contains(key)) {
        seen.insert(key);
        out.append(entry);
      }
    }
  };

  static const QStringList envNames = {
      ".env",     ".env.local", ".env.development", ".env.development.local",
      ".env.dev", ".env.test",  ".env.example",     ".env.sample"};
  static const QStringList composeNames = {"compose.yaml",
                                           "compose.yml",
                                           "docker-compose.yaml",
                                           "docker-compose.yml",
                                           "docker-compose.override.yml",
                                           "docker-compose.dev.yml"};

  for (const QString &folder : folders) {
    const QDir dir(folder);
    QMap<QString, QString> folderEnv;
    for (const QString &name : envNames) {
      const QString path = dir.filePath(name);
      if (!QFileInfo::exists(path)) {
        continue;
      }
      const QMap<QString, QString> env = parseEnvFile(readText(path));
      if (name == QLatin1String(".env")) {
        folderEnv = env;
      }
      add(fromEnvironment(env, root.relativeFilePath(path), folder));
    }
    for (const QString &name : composeNames) {
      const QString path = dir.filePath(name);
      if (!QFileInfo::exists(path)) {
        continue;
      }
      QString project = folderEnv.value(QStringLiteral("COMPOSE_PROJECT_NAME"));
      if (project.isEmpty()) {
        project = dir.dirName().toLower();
        project.remove(QRegularExpression(QStringLiteral("[^a-z0-9_-]")));
      }
      add(fromCompose(readText(path), root.relativeFilePath(path), project,
                      folderEnv));
    }
  }

  // SQLite files a few levels down (Django's db.sqlite3, app.db, ...). The
  // walk is bounded so large trees (node_modules, build output) stay cheap.
  static const QStringList patterns = {"*.db", "*.sqlite", "*.sqlite3",
                                       "*.db3"};
  QVector<DbDetectedConnection> files;
  std::function<void(const QDir &, int)> walk = [&](const QDir &dir,
                                                    int depth) {
    if (files.size() >= 12) {
      return;
    }
    for (const QFileInfo &info :
         dir.entryInfoList(patterns, QDir::Files | QDir::NoSymLinks)) {
      QFile file(info.absoluteFilePath());
      if (files.size() >= 12 || !file.open(QIODevice::ReadOnly) ||
          !file.read(16).startsWith("SQLite format 3")) {
        continue;
      }
      DbDetectedConnection found;
      found.title = info.fileName();
      found.source = root.relativeFilePath(info.absoluteFilePath());
      found.profile.engine = DbEngine::Sqlite;
      found.profile.filePath = info.absoluteFilePath();
      found.profile.host.clear();
      files.append(found);
    }
    if (depth >= 3) {
      return;
    }
    for (const QFileInfo &child :
         dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot |
                           QDir::NoSymLinks | QDir::Hidden)) {
      if (!skipped.contains(child.fileName())) {
        walk(QDir(child.absoluteFilePath()), depth + 1);
      }
    }
  };
  walk(root, 0);
  add(files);
  return out;
}

void setWorkspaceFolder(const QString &folder) { g_workspaceFolder = folder; }

QString workspaceFolder() { return g_workspaceFolder; }

QString environmentValue(const QString &name, bool *found) {
  if (found) {
    *found = false;
  }
  if (name.trimmed().isEmpty()) {
    return {};
  }
  const QProcessEnvironment system = QProcessEnvironment::systemEnvironment();
  if (system.contains(name)) {
    if (found) {
      *found = true;
    }
    return system.value(name);
  }
  if (g_workspaceFolder.isEmpty()) {
    return {};
  }
  for (const QString &file :
       {QStringLiteral(".env.local"), QStringLiteral(".env")}) {
    const QMap<QString, QString> env =
        parseEnvFile(readText(QDir(g_workspaceFolder).filePath(file)));
    if (env.contains(name)) {
      if (found) {
        *found = true;
      }
      return env.value(name);
    }
  }
  return {};
}

} // namespace DbProjectDiscovery
