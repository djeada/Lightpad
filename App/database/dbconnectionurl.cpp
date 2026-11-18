#include "dbconnectionurl.h"

#include <QDir>
#include <QHash>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace {

QString decode(const QString &text) {
  return QUrl::fromPercentEncoding(text.toUtf8());
}

QString encode(const QString &text) {
  return QString::fromUtf8(QUrl::toPercentEncoding(text));
}

QString unquote(QString text) {
  text = text.trimmed();
  if (text.size() >= 2 && ((text.startsWith('"') && text.endsWith('"')) ||
                           (text.startsWith('\'') && text.endsWith('\'')))) {
    text = text.mid(1, text.size() - 2).trimmed();
  }
  return text;
}

bool engineForScheme(const QString &scheme, DbEngine *engine) {
  QString base = scheme.toLower();
  const int plus = base.indexOf('+');
  if (plus > 0) {
    base = base.left(plus);
  }
  static const QHash<QString, DbEngine> schemes = {
      {QStringLiteral("postgres"), DbEngine::PostgreSql},
      {QStringLiteral("postgresql"), DbEngine::PostgreSql},
      {QStringLiteral("pg"), DbEngine::PostgreSql},
      {QStringLiteral("pgsql"), DbEngine::PostgreSql},
      {QStringLiteral("cockroachdb"), DbEngine::PostgreSql},
      {QStringLiteral("timescaledb"), DbEngine::PostgreSql},
      {QStringLiteral("redshift"), DbEngine::PostgreSql},
      {QStringLiteral("mysql"), DbEngine::MySql},
      {QStringLiteral("mysql2"), DbEngine::MySql},
      {QStringLiteral("mariadb"), DbEngine::MySql},
      {QStringLiteral("tidb"), DbEngine::MySql},
      {QStringLiteral("sqlserver"), DbEngine::SqlServer},
      {QStringLiteral("mssql"), DbEngine::SqlServer},
      {QStringLiteral("sqlite"), DbEngine::Sqlite},
      {QStringLiteral("sqlite3"), DbEngine::Sqlite},
      {QStringLiteral("file"), DbEngine::Sqlite},
  };
  const auto it = schemes.constFind(base);
  if (it == schemes.constEnd()) {
    return false;
  }
  *engine = it.value();
  return true;
}

QString normalizedKey(const QString &key) {
  QString k = key.trimmed().toLower();
  k.remove(' ');
  k.remove('_');
  k.remove('-');
  return k;
}

bool isTrue(const QString &value) {
  const QString v = value.trimmed().toLower();
  return v == "true" || v == "1" || v == "yes" || v == "on" ||
         v == "mandatory" || v == "strict";
}

// Applies one option shared by URL query strings and key=value strings.
// Returns false when the key is not a connection option.
bool applyOption(const QString &rawKey, const QString &value,
                 DbConnectionUrl::Parsed *parsed) {
  DbConnectionProfile &p = parsed->profile;
  const QString key = normalizedKey(rawKey);
  if (key == "sslmode") {
    p.sslMode = DbEngineInfo::normalizedSslMode(value);
  } else if (key == "ssl" || key == "tls" || key == "usessl") {
    const QString v = value.trimmed().toLower();
    if (v.contains("rejectunauthorized\":true") ||
        v.contains("rejectunauthorized\": true")) {
      p.sslMode = QStringLiteral("verify-full");
    } else if (v == "skip-verify" || v == "preferred") {
      p.sslMode = v == "preferred" ? QStringLiteral("prefer")
                                   : QStringLiteral("require");
    } else {
      p.sslMode = isTrue(value) || v.startsWith('{')
                      ? QStringLiteral("require")
                      : QStringLiteral("disable");
    }
  } else if (key == "sslaccept") {
    p.sslMode = value.trimmed().toLower() == "strict"
                    ? QStringLiteral("verify-full")
                    : QStringLiteral("require");
  } else if (key == "trustservercertificate") {
    p.trustServerCertificate = isTrue(value);
  } else if (key == "encrypt") {
    // SQL Server encrypts by default; only the certificate check is ours.
  } else if (key == "user" || key == "username" || key == "uid" ||
             key == "userid") {
    p.user = value;
  } else if (key == "password" || key == "pwd") {
    parsed->password = value;
    parsed->hasPassword = true;
  } else if (key == "host" || key == "hostname" || key == "server" ||
             key == "datasource" || key == "address" || key == "addr" ||
             key == "networkaddress") {
    p.host = value;
  } else if (key == "port") {
    p.port = value.trimmed().toInt();
  } else if (key == "dbname" || key == "database" || key == "databasename" ||
             key == "initialcatalog" || key == "db") {
    p.database = value;
  } else {
    return false;
  }
  return true;
}

void splitHostPort(const QString &hostPort, QString *host, int *port) {
  QString value = hostPort;
  if (value.contains(',') && !value.startsWith('[')) {
    value = value.section(',', 0, 0);
  }
  *port = 0;
  if (value.startsWith('[')) {
    const int close = value.indexOf(']');
    *host = value.mid(1, close > 0 ? close - 1 : -1);
    const QString after = close > 0 ? value.mid(close + 1) : QString();
    if (after.startsWith(':')) {
      *port = after.mid(1).toInt();
    }
    return;
  }
  const int colon = value.lastIndexOf(':');
  bool ok = false;
  const int parsedPort = colon >= 0 ? value.mid(colon + 1).toInt(&ok) : 0;
  if (ok) {
    *host = value.left(colon);
    *port = parsedPort;
  } else {
    *host = value;
  }
}

bool parseSqlite(const QString &scheme, QString rest, const QString &baseDir,
                 DbConnectionUrl::Parsed *out) {
  const int query = rest.indexOf('?');
  if (query >= 0) {
    rest = rest.left(query);
  }
  QString path = rest;
  if (scheme.compare("file", Qt::CaseInsensitive) == 0) {
    if (path.startsWith(QLatin1String("//localhost/"))) {
      path = path.mid(11);
    } else if (path.startsWith(QLatin1String("//"))) {
      path = path.mid(2);
    }
  } else if (path.startsWith(QLatin1String("///"))) {
    // SQLAlchemy: sqlite:///relative.db and sqlite:////absolute/path.db.
    path = path.mid(3);
  } else if (path.startsWith(QLatin1String("//"))) {
    path = path.mid(2);
  }
  path = decode(path);
  out->profile.engine = DbEngine::Sqlite;
  if (path.isEmpty() || path == QLatin1String(":memory:")) {
    out->profile.filePath = QStringLiteral(":memory:");
    return true;
  }
  if (QDir::isRelativePath(path) && !baseDir.isEmpty()) {
    path = QDir::cleanPath(QDir(baseDir).absoluteFilePath(path));
  }
  out->profile.filePath = path;
  return true;
}

bool parseAuthorityUrl(DbEngine engine, const QString &rest,
                       DbConnectionUrl::Parsed *out, QString *error) {
  if (!rest.startsWith(QLatin1String("//"))) {
    if (error) {
      *error = QStringLiteral("Expected // after the scheme.");
    }
    return false;
  }
  DbConnectionProfile &p = out->profile;
  p.engine = engine;
  QString remainder = rest.mid(2);
  const int fragment = remainder.indexOf('#');
  if (fragment >= 0) {
    remainder = remainder.left(fragment);
  }

  const int lastAt = remainder.lastIndexOf('@');
  // JDBC SQL Server: //host\instance:1433;databaseName=db;user=sa;...
  const int semicolon = remainder.indexOf(';', qMax(lastAt, 0));
  const int slashOrQuery = remainder.indexOf(
      QRegularExpression(QStringLiteral("[/?]")), qMax(lastAt, 0));
  if (engine == DbEngine::SqlServer && semicolon >= 0 &&
      (slashOrQuery < 0 || semicolon < slashOrQuery)) {
    const QStringList parts = remainder.split(';', Qt::SkipEmptyParts);
    QString host;
    int port = 0;
    splitHostPort(parts.value(0), &host, &port);
    p.host = host.isEmpty() ? QStringLiteral("localhost") : host;
    p.port = port;
    for (int i = 1; i < parts.size(); ++i) {
      const int eq = parts[i].indexOf('=');
      if (eq > 0) {
        applyOption(parts[i].left(eq), unquote(parts[i].mid(eq + 1)), out);
      }
    }
    return true;
  }

  QString query;
  const int questionMark = remainder.indexOf('?', qMax(lastAt, 0));
  if (questionMark >= 0) {
    query = remainder.mid(questionMark + 1);
    remainder = remainder.left(questionMark);
  }
  const int at = remainder.lastIndexOf('@');
  const int slash = remainder.indexOf('/', qMax(at, 0));
  const QString authority = slash >= 0 ? remainder.left(slash) : remainder;
  const QString path = slash >= 0 ? remainder.mid(slash + 1) : QString();

  QString hostPort = authority;
  const int authorityAt = authority.lastIndexOf('@');
  if (authorityAt >= 0) {
    const QString userInfo = authority.left(authorityAt);
    hostPort = authority.mid(authorityAt + 1);
    const int colon = userInfo.indexOf(':');
    if (colon >= 0) {
      p.user = decode(userInfo.left(colon));
      out->password = decode(userInfo.mid(colon + 1));
      out->hasPassword = true;
    } else {
      p.user = decode(userInfo);
    }
  }
  QString host;
  int port = 0;
  splitHostPort(hostPort, &host, &port);
  p.host = host.isEmpty() ? QStringLiteral("localhost") : decode(host);
  p.port = port;
  QString database = decode(path);
  while (database.endsWith('/')) {
    database.chop(1);
  }
  p.database = database;

  const QUrlQuery items(query);
  for (const auto &item : items.queryItems(QUrl::FullyDecoded)) {
    applyOption(item.first, item.second, out);
  }
  if (engine == DbEngine::SqlServer &&
      p.host.endsWith(QLatin1String(".database.windows.net"),
                      Qt::CaseInsensitive) &&
      !items.hasQueryItem(QStringLiteral("trustServerCertificate")) &&
      !items.hasQueryItem(QStringLiteral("TrustServerCertificate"))) {
    p.trustServerCertificate = false;
  }
  return true;
}

QList<QPair<QString, QString>> splitKeyValues(const QString &text,
                                              bool semicolons) {
  QList<QPair<QString, QString>> pairs;
  if (semicolons) {
    // Values may be quoted to hold a ';' (Password="a;b").
    QString key;
    QString value;
    bool inValue = false;
    QChar quote;
    auto flush = [&]() {
      if (!key.trimmed().isEmpty()) {
        pairs.append({key.trimmed(), unquote(value)});
      }
      key.clear();
      value.clear();
      inValue = false;
    };
    for (const QChar c : text) {
      if (!inValue) {
        if (c == '=') {
          inValue = true;
        } else if (c == ';') {
          flush();
        } else {
          key += c;
        }
        continue;
      }
      if (!quote.isNull()) {
        value += c;
        if (c == quote) {
          quote = QChar();
        }
      } else if ((c == '"' || c == '\'') && value.trimmed().isEmpty()) {
        quote = c;
        value += c;
      } else if (c == ';') {
        flush();
      } else {
        value += c;
      }
    }
    flush();
    return pairs;
  }
  // libpq: key=value separated by spaces, values optionally single-quoted.
  static const QRegularExpression token(
      QStringLiteral("([A-Za-z_]+)\\s*=\\s*('(?:[^'\\\\]|\\\\.)*'|\\S*)"));
  auto it = token.globalMatch(text);
  while (it.hasNext()) {
    const auto m = it.next();
    QString value = m.captured(2);
    if (value.startsWith('\'')) {
      value = value.mid(1, value.size() - 2)
                  .replace(QLatin1String("\\'"), QLatin1String("'"));
    }
    pairs.append({m.captured(1), value});
  }
  return pairs;
}

bool isSqliteFileName(const QString &path) {
  static const QRegularExpression suffix(
      QStringLiteral("\\.(db|sqlite|sqlite3|db3|s3db)$"),
      QRegularExpression::CaseInsensitiveOption);
  return suffix.match(path.trimmed()).hasMatch();
}

bool parseKeyValues(const QString &text, const QString &baseDir,
                    DbConnectionUrl::Parsed *out, QString *error) {
  const bool semicolons = text.contains(';');
  const auto pairs = splitKeyValues(text, semicolons);
  if (pairs.isEmpty()) {
    return false;
  }
  QHash<QString, QString> keys;
  for (const auto &pair : pairs) {
    keys.insert(normalizedKey(pair.first), pair.second);
  }
  DbConnectionProfile &p = out->profile;
  QString driver = keys.value("driver") + keys.value("provider");
  driver = driver.toLower();
  const QString server = keys.value(
      "server",
      keys.value("datasource", keys.value("address", keys.value("addr"))));
  const int port = keys.value("port").toInt();

  DbEngine engine = DbEngine::SqlServer;
  bool known = true;
  if (driver.contains("sqlite") ||
      (!server.isEmpty() && isSqliteFileName(server))) {
    out->profile.engine = DbEngine::Sqlite;
    QString path = server;
    if (QDir::isRelativePath(path) && !baseDir.isEmpty()) {
      path = QDir::cleanPath(QDir(baseDir).absoluteFilePath(path));
    }
    p.filePath = path;
    return !path.isEmpty();
  } else if (driver.contains("postgres") || driver.contains("psql")) {
    engine = DbEngine::PostgreSql;
  } else if (driver.contains("mysql") || driver.contains("maria")) {
    engine = DbEngine::MySql;
  } else if (driver.contains("sql server") || driver.contains("sqlncli") ||
             driver.contains("msoledbsql")) {
    engine = DbEngine::SqlServer;
  } else if (!semicolons) {
    engine = DbEngine::PostgreSql;
  } else if (keys.contains("initialcatalog") ||
             keys.contains("trustservercertificate") ||
             keys.contains("integratedsecurity") ||
             keys.contains("multipleactiveresultsets") ||
             server.startsWith(QLatin1String("tcp:"), Qt::CaseInsensitive) ||
             server.contains('\\') || server.contains(',')) {
    engine = DbEngine::SqlServer;
  } else if (port == 5432 || keys.contains("host")) {
    engine = DbEngine::PostgreSql;
  } else if (port == 3306 || keys.contains("uid") ||
             keys.value("sslmode").toLower() == "required" ||
             keys.value("sslmode").toLower() == "none") {
    engine = DbEngine::MySql;
  } else if (port == 1433 || !server.isEmpty()) {
    engine = DbEngine::SqlServer;
  } else {
    known = false;
  }
  if (!known) {
    if (error) {
      *error = QStringLiteral("Could not tell which database this is for.");
    }
    return false;
  }
  p.engine = engine;
  p.host.clear();
  for (const auto &pair : pairs) {
    applyOption(pair.first, pair.second, out);
  }
  QString host = p.host;
  if (host.startsWith(QLatin1String("tcp:"), Qt::CaseInsensitive)) {
    host = host.mid(4);
  }
  if (engine == DbEngine::SqlServer && host.contains(',')) {
    p.port = host.section(',', 1).trimmed().toInt();
    host = host.section(',', 0, 0);
  }
  if (host.isEmpty()) {
    if (error) {
      *error = QStringLiteral("The connection string names no server.");
    }
    return false;
  }
  p.host = host.trimmed();
  if (engine == DbEngine::SqlServer &&
      p.host.endsWith(QLatin1String(".database.windows.net"),
                      Qt::CaseInsensitive) &&
      !keys.contains("trustservercertificate")) {
    p.trustServerCertificate = false;
  }
  return true;
}

} // namespace

namespace DbConnectionUrl {

bool looksLikeConnectionString(const QString &text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty() || trimmed.contains('\n')) {
    return false;
  }
  Parsed parsed;
  return parse(trimmed, &parsed);
}

bool parse(const QString &input, Parsed *out, QString *error,
           const QString &baseDir) {
  Parsed parsed;
  QString text = unquote(input);
  // A whole line from a .env file: DATABASE_URL=postgres://...
  static const QRegularExpression envLine(QStringLiteral(
      "^(?:export\\s+)?[A-Za-z_][A-Za-z0-9_]*\\s*=\\s*(.*://.*|jdbc:.*)$"));
  const auto envMatch = envLine.match(text);
  if (envMatch.hasMatch()) {
    text = unquote(envMatch.captured(1));
  }
  if (text.isEmpty()) {
    if (error) {
      *error = QStringLiteral("Paste a connection URL.");
    }
    return false;
  }
  if (text.startsWith(QLatin1String("jdbc:"), Qt::CaseInsensitive)) {
    text = text.mid(5);
  }
  static const QRegularExpression schemeRe(
      QStringLiteral("^([A-Za-z][A-Za-z0-9+.\\-]+):(.*)$"),
      QRegularExpression::DotMatchesEverythingOption);
  const auto scheme = schemeRe.match(text);
  if (scheme.hasMatch()) {
    DbEngine engine = DbEngine::Sqlite;
    if (engineForScheme(scheme.captured(1), &engine)) {
      const bool ok =
          engine == DbEngine::Sqlite
              ? parseSqlite(scheme.captured(1), scheme.captured(2), baseDir,
                            &parsed)
              : parseAuthorityUrl(engine, scheme.captured(2), &parsed, error);
      if (ok && out) {
        *out = parsed;
      }
      return ok;
    }
  }
  if (text.contains('=')) {
    QString why;
    if (parseKeyValues(text, baseDir, &parsed, &why)) {
      if (out) {
        *out = parsed;
      }
      return true;
    }
    if (!why.isEmpty()) {
      if (error) {
        *error = why;
      }
      return false;
    }
  } else if (isSqliteFileName(text)) {
    parseSqlite(QStringLiteral("sqlite"), QStringLiteral("//") + text, baseDir,
                &parsed);
    if (out) {
      *out = parsed;
    }
    return true;
  }
  if (error) {
    *error = QStringLiteral(
        "Not a connection string Lightpad understands. Paste a URL such as "
        "postgresql://user@host:5432/db or mysql://user@host/db.");
  }
  return false;
}

QString format(const DbConnectionProfile &p, const QString &password) {
  if (p.engine == DbEngine::Sqlite) {
    if (p.filePath.isEmpty()) {
      return {};
    }
    if (p.filePath == QLatin1String(":memory:")) {
      return QStringLiteral("sqlite:///:memory:");
    }
    return QStringLiteral("sqlite:///") +
           QDir::fromNativeSeparators(p.filePath);
  }
  QString scheme = QStringLiteral("postgresql");
  if (p.engine == DbEngine::MySql) {
    scheme = QStringLiteral("mysql");
  } else if (p.engine == DbEngine::SqlServer) {
    scheme = QStringLiteral("sqlserver");
  }
  QString auth;
  if (!p.user.isEmpty()) {
    auth = encode(p.user);
    if (!password.isEmpty()) {
      auth += QLatin1Char(':') + encode(password);
    }
    auth += QLatin1Char('@');
  }
  QString host = p.transport == DbTransport::Docker || p.host.isEmpty()
                     ? QStringLiteral("localhost")
                     : p.host;
  if (host.startsWith('/')) {
    host = encode(host);
  } else if (host.contains(':')) {
    host = QLatin1Char('[') + host + QLatin1Char(']');
  }
  QString url = scheme + QStringLiteral("://") + auth + host +
                QLatin1Char(':') + QString::number(p.effectivePort());
  QUrlQuery query;
  if (p.engine == DbEngine::SqlServer) {
    if (!p.database.isEmpty()) {
      query.addQueryItem(QStringLiteral("database"), p.database);
    }
    query.addQueryItem(QStringLiteral("TrustServerCertificate"),
                       p.trustServerCertificate ? QStringLiteral("true")
                                                : QStringLiteral("false"));
  } else {
    if (!p.database.isEmpty()) {
      url += QLatin1Char('/') + encode(p.database);
    }
    if (!p.sslMode.isEmpty()) {
      if (p.engine == DbEngine::PostgreSql) {
        query.addQueryItem(QStringLiteral("sslmode"), p.sslMode);
      } else {
        static const QHash<QString, QString> mysqlModes = {
            {QStringLiteral("disable"), QStringLiteral("DISABLED")},
            {QStringLiteral("prefer"), QStringLiteral("PREFERRED")},
            {QStringLiteral("require"), QStringLiteral("REQUIRED")},
            {QStringLiteral("verify-ca"), QStringLiteral("VERIFY_CA")},
            {QStringLiteral("verify-full"), QStringLiteral("VERIFY_IDENTITY")}};
        query.addQueryItem(QStringLiteral("ssl-mode"),
                           mysqlModes.value(p.sslMode));
      }
    }
  }
  if (!query.isEmpty()) {
    url += QLatin1Char('?') + query.toString(QUrl::FullyEncoded);
  }
  return url;
}

} // namespace DbConnectionUrl
