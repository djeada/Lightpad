#include "clidialect.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

const QString kPgNull = QStringLiteral("__LP_NULL__");
const QChar kUnitSep(0x1f);

QStringList splitArgs(const QString &text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }
  return QProcess::splitCommand(trimmed);
}

const char *kDockerShellScript =
    "IFS= read -r LP_PW || exit 2; export \"$LP_PWVAR=$LP_PW\"; unset LP_PW; "
    "C=; for p in $LP_CLIENTS; do if command -v \"$p\" >/dev/null 2>&1; then "
    "C=$p; break; fi; done; "
    "if [ -z \"$C\" ]; then echo \"lightpad: no SQL client found in "
    "container (tried: $LP_CLIENTS)\" >&2; exit 127; fi; exec \"$C\" \"$@\"";

} // namespace

QString CliDialect::startupError(const QString &err, const QString &out) const {
  const QString text = (err.trimmed().isEmpty() ? out : err).trimmed();
  return text;
}

CliLaunch CliDialect::launch(const DbConnectionProfile &profile,
                             const QString &password) const {
  CliLaunch spec;
  const QStringList extra = splitArgs(profile.extraArgs);

  if (profile.transport == DbTransport::Docker) {
    QStringList clients = clientCandidates();
    if (!profile.clientPath.trimmed().isEmpty()) {
      clients.prepend(profile.clientPath.trimmed());
    }
    spec.program = QStringLiteral("docker");
    spec.arguments = {QStringLiteral("exec"),
                      QStringLiteral("-i"),
                      QStringLiteral("-e"),
                      QStringLiteral("LP_PWVAR=") + passwordVariable(),
                      QStringLiteral("-e"),
                      QStringLiteral("LP_CLIENTS=") + clients.join(' ')};
    const QMap<QString, QString> env = clientEnvironment(profile);
    for (auto it = env.begin(); it != env.end(); ++it) {
      spec.arguments << QStringLiteral("-e")
                     << it.key() + QLatin1Char('=') + it.value();
    }
    spec.arguments << profile.container << QStringLiteral("sh")
                   << QStringLiteral("-c")
                   << QString::fromLatin1(kDockerShellScript)
                   << QStringLiteral("sh");
    spec.arguments << clientArguments(profile, true) << extra;
    spec.stdinPreamble = (password + QLatin1Char('\n')).toUtf8();
    return spec;
  }

  QString program = profile.clientPath.trimmed();
  if (program.isEmpty()) {
    for (const QString &candidate : clientCandidates()) {
      if (!QStandardPaths::findExecutable(candidate).isEmpty()) {
        program = candidate;
        break;
      }
    }
  }
  if (program.isEmpty()) {
    spec.error =
        QStringLiteral("Could not find the \"%1\" command line client on this "
                       "machine. Install it, set its path in the connection "
                       "settings, or connect through a Docker container.")
            .arg(clientCandidates().value(0));
    return spec;
  }
  spec.program = program;
  spec.arguments = clientArguments(profile, false) + extra;
  spec.environment = clientEnvironment(profile);
  if (!password.isEmpty()) {
    spec.environment.insert(passwordVariable(), password);
  }
  return spec;
}

QStringList CliParsing::splitLines(const QString &text) {
  QStringList lines = text.split('\n');
  for (QString &l : lines) {
    if (l.endsWith('\r')) {
      l.chop(1);
    }
  }
  if (!lines.isEmpty() && lines.last().isEmpty()) {
    lines.removeLast();
  }
  return lines;
}

QVector<QStringList> CliParsing::parseCsv(const QString &text) {
  QVector<QStringList> records;
  QStringList record;
  QString field;
  bool inQuotes = false;
  bool fieldStarted = false;
  const int n = text.size();
  for (int i = 0; i < n; ++i) {
    const QChar c = text[i];
    if (inQuotes) {
      if (c == '"') {
        if (i + 1 < n && text[i + 1] == '"') {
          field += '"';
          ++i;
        } else {
          inQuotes = false;
        }
      } else {
        field += c;
      }
      continue;
    }
    if (c == '"') {
      inQuotes = true;
      fieldStarted = true;
    } else if (c == ',') {
      record << field;
      field.clear();
      fieldStarted = true;
    } else if (c == '\n' || c == '\r') {
      if (c == '\r' && i + 1 < n && text[i + 1] == '\n') {
        ++i;
      }
      if (fieldStarted || !field.isEmpty() || !record.isEmpty()) {
        record << field;
        records.append(record);
      }
      record.clear();
      field.clear();
      fieldStarted = false;
    } else {
      field += c;
      fieldStarted = true;
    }
  }
  if (fieldStarted || !field.isEmpty() || !record.isEmpty()) {
    record << field;
    records.append(record);
  }
  return records;
}

QVector<QStringList> CliParsing::parseBatchTsv(const QString &text) {
  QVector<QStringList> records;
  for (const QString &line : splitLines(text)) {
    QStringList fields;
    QString field;
    for (int i = 0; i < line.size(); ++i) {
      const QChar c = line[i];
      if (c == '\t') {
        fields << field;
        field.clear();
      } else if (c == '\\' && i + 1 < line.size()) {
        const QChar next = line[++i];
        switch (next.unicode()) {
        case 'n':
          field += '\n';
          break;
        case 't':
          field += '\t';
          break;
        case 'r':
          field += '\r';
          break;
        case '0':
          field += QChar(0);
          break;
        case '\\':
          field += '\\';
          break;
        default:
          field += '\\';
          field += next;
          break;
        }
      } else {
        field += c;
      }
    }
    fields << field;
    records.append(fields);
  }
  return records;
}

DbResultSet CliParsing::buildResultSet(const QVector<QStringList> &records,
                                       const QString &nullToken, int maxRows) {
  DbResultSet rs;
  if (records.isEmpty()) {
    return rs;
  }
  const QStringList &header = records.first();
  for (const QString &name : header) {
    DbColumn col;
    col.name = name;
    rs.columns.append(col);
  }
  const int cols = header.size();
  for (int i = 1; i < records.size(); ++i) {
    ++rs.totalRows;
    if (maxRows > 0 && rs.rows.size() >= maxRows) {
      rs.truncated = true;
      continue;
    }
    const QStringList &rec = records[i];
    QVector<QVariant> row;
    row.reserve(cols);
    for (int c = 0; c < cols; ++c) {
      if (c >= rec.size() || rec[c] == nullToken) {
        row.append(QVariant());
      } else {
        row.append(rec[c]);
      }
    }
    rs.rows.append(row);
  }
  return rs;
}

QString CliParsing::joinNonEmpty(const QStringList &lines) {
  QStringList kept;
  for (const QString &l : lines) {
    if (!l.trimmed().isEmpty()) {
      kept << l;
    }
  }
  return kept.join('\n');
}

namespace {

class PostgresDialect : public CliDialect {
public:
  DbEngine engine() const override { return DbEngine::PostgreSql; }
  QStringList clientCandidates() const override { return {"psql"}; }
  QString passwordVariable() const override { return "PGPASSWORD"; }

  QStringList clientArguments(const DbConnectionProfile &p,
                              bool inContainer) const override {
    QStringList args = {
        "-X",        "-w", "--csv",          "-P", "null=" + kPgNull, "-P",
        "pager=off", "-v", "ON_ERROR_STOP=0"};
    if (!inContainer) {
      args << "-h" << (p.host.isEmpty() ? QStringLiteral("localhost") : p.host)
           << "-p" << QString::number(p.effectivePort());
    }
    args << "-U"
         << (p.user.isEmpty() ? DbEngineInfo::defaultUser(p.engine) : p.user);
    args << "-d"
         << (p.database.isEmpty() ? DbEngineInfo::defaultDatabase(p.engine)
                                  : p.database);
    return args;
  }

  QMap<QString, QString>
  clientEnvironment(const DbConnectionProfile &) const override {
    return {{"PGCONNECT_TIMEOUT", "15"},
            {"PGCLIENTENCODING", "UTF8"},
            {"PGAPPNAME", "Lightpad"}};
  }

  QByteArray frame(const QString &statement, SqlStatementKind,
                   const QString &marker) const override {
    QString text = statement.trimmed();
    QString out;
    if (text.startsWith('\\')) {
      out = text + QLatin1Char('\n');
    } else {
      out = text + QStringLiteral("\n;\n");
    }
    out += QStringLiteral("\\echo ") + marker + QLatin1Char('\n');
    out += QStringLiteral("\\warn ") + marker + QLatin1Char('\n');
    return out.toUtf8();
  }

  DbStatementResult parse(const QString &sql, SqlStatementKind kind,
                          const QString &out, const QString &err,
                          int maxRows) const override {
    DbStatementResult res;
    res.sql = sql;

    const QStringList errLines = CliParsing::splitLines(err);
    QStringList errorBlock;
    bool inError = false;
    for (const QString &line : errLines) {
      if (line.startsWith(QLatin1String("ERROR:")) ||
          line.startsWith(QLatin1String("FATAL:")) ||
          line.startsWith(QLatin1String("PANIC:"))) {
        inError = true;
        errorBlock << line;
        continue;
      }
      if (inError && (line.startsWith(QLatin1String("LINE ")) ||
                      line.startsWith(QLatin1String("DETAIL:")) ||
                      line.startsWith(QLatin1String("HINT:")) ||
                      line.startsWith(QLatin1String("CONTEXT:")) ||
                      line.startsWith(QLatin1String("QUERY:")) ||
                      line.startsWith(QLatin1String("STATEMENT:")) ||
                      line.startsWith(' '))) {
        errorBlock << line;
        continue;
      }
      inError = false;
      if (!line.trimmed().isEmpty()) {
        res.messages << line;
      }
    }
    if (!errorBlock.isEmpty()) {
      res.ok = false;
      QString first = errorBlock.first();
      first.remove(
          QRegularExpression(QStringLiteral("^(ERROR|FATAL|PANIC):\\s+")));
      QStringList rest = errorBlock.mid(1);
      res.error = first;
      if (!rest.isEmpty()) {
        res.error += QLatin1Char('\n') + rest.join('\n');
      }
    }

    QString body = out;
    static const QRegularExpression tag(QStringLiteral(
        "^(INSERT \\d+ (\\d+)|(UPDATE|DELETE|MERGE|COPY|MOVE|FETCH|SELECT) "
        "(\\d+)|(CREATE|ALTER|DROP|TRUNCATE|GRANT|REVOKE|COMMENT|BEGIN|COMMIT|"
        "ROLLBACK|SAVEPOINT|RELEASE|SET|RESET|VACUUM|ANALYZE|CLUSTER|REINDEX|"
        "REFRESH|DISCARD|LOCK|DO|CALL|PREPARE|DEALLOCATE|DECLARE|CLOSE|LISTEN|"
        "NOTIFY|UNLISTEN|START|END|ABORT|EXPLAIN|IMPORT|SECURITY|CHECKPOINT)"
        "( [A-Z ]+)?)$"));
    QStringList lines = CliParsing::splitLines(body);
    static const QRegularExpression connectCmd(
        QStringLiteral("^\\\\(c|connect)\\b"));
    if (connectCmd.match(sql.trimmed()).hasMatch()) {

      res.messages = lines + res.messages;
      return res;
    }

    const bool tabular = kind == SqlStatementKind::Select ||
                         kind == SqlStatementKind::Explain ||
                         kind == SqlStatementKind::Show;
    if (!lines.isEmpty() && !tabular) {
      const auto m = tag.match(lines.last());
      if (m.hasMatch()) {
        if (!m.captured(2).isEmpty()) {
          res.rowsAffected = m.captured(2).toLongLong();
        } else if (!m.captured(4).isEmpty()) {
          res.rowsAffected = m.captured(4).toLongLong();
        }
        res.messages.prepend(lines.last());
        lines.removeLast();
      }
    }
    if (!lines.isEmpty() && res.ok) {
      const QString csv = lines.join('\n');
      DbResultSet rs = CliParsing::buildResultSet(CliParsing::parseCsv(csv),
                                                  kPgNull, maxRows);
      if (!rs.columns.isEmpty()) {
        res.resultSets.append(rs);
      }
    }
    return res;
  }
};

class MySqlDialect : public CliDialect {
public:
  DbEngine engine() const override { return DbEngine::MySql; }
  QStringList clientCandidates() const override { return {"mysql", "mariadb"}; }
  QString passwordVariable() const override { return "MYSQL_PWD"; }

  QStringList clientArguments(const DbConnectionProfile &p,
                              bool inContainer) const override {
    QStringList args = {"--batch", "--force", "--unbuffered",
                        "--default-character-set=utf8mb4",
                        "--connect-timeout=15"};
    if (!inContainer) {
      args << "--protocol=TCP" << "-h"
           << (p.host.isEmpty() ? QStringLiteral("localhost") : p.host) << "-P"
           << QString::number(p.effectivePort());
    }
    args << "-u"
         << (p.user.isEmpty() ? DbEngineInfo::defaultUser(p.engine) : p.user);
    if (!p.database.isEmpty()) {
      args << "-D" << p.database;
    }
    return args;
  }

  QByteArray frame(const QString &statement, SqlStatementKind kind,
                   const QString &marker) const override {
    QString out = statement.trimmed() + QStringLiteral(";\n");
    if (kind == SqlStatementKind::Dml || kind == SqlStatementKind::Ddl) {
      out += QStringLiteral("SELECT ROW_COUNT() AS lp_rows;\n");
    }
    out += QStringLiteral("SELECT '%1' AS `%1H`;\n").arg(marker);
    out += QStringLiteral("\\! echo %1 1>&2\n").arg(marker);
    return out.toUtf8();
  }

  DbStatementResult parse(const QString &sql, SqlStatementKind kind,
                          const QString &out, const QString &err,
                          int maxRows) const override {
    DbStatementResult res;
    res.sql = sql;

    QStringList errLines = CliParsing::splitLines(err);
    QStringList kept;
    bool inEcho = false;
    static const QRegularExpression dashes(QStringLiteral("^-{6,}$"));
    static const QRegularExpression errRe(QStringLiteral(
        "^ERROR (\\d+)(?: \\(([0-9A-Z]+)\\))?(?: at line \\d+)?: (.*)$"));
    for (const QString &line : errLines) {
      if (dashes.match(line).hasMatch()) {
        inEcho = !inEcho;
        continue;
      }
      if (inEcho) {
        continue;
      }
      kept << line;
    }
    QStringList errors;
    for (const QString &line : kept) {
      const auto m = errRe.match(line);
      if (m.hasMatch()) {
        errors << QStringLiteral("%1 (%2)").arg(m.captured(3), m.captured(1));
      } else if (!line.trimmed().isEmpty()) {
        res.messages << line;
      }
    }
    if (!errors.isEmpty()) {
      res.ok = false;
      res.error = errors.join('\n');
    }

    QString body = out;
    QStringList lines = CliParsing::splitLines(body);
    if (kind == SqlStatementKind::Dml || kind == SqlStatementKind::Ddl) {

      const int n = lines.size();
      if (n >= 2 && lines[n - 2] == QLatin1String("lp_rows")) {
        bool ok = false;
        const qint64 v = lines[n - 1].toLongLong(&ok);
        if (ok && v >= 0) {
          res.rowsAffected = v;
        }
        lines = lines.mid(0, n - 2);
      }
    }
    if (!lines.isEmpty() && res.ok) {
      const QString text = lines.join('\n');
      DbResultSet rs = CliParsing::buildResultSet(
          CliParsing::parseBatchTsv(text), QStringLiteral("NULL"), maxRows);
      if (!rs.columns.isEmpty()) {
        res.resultSets.append(rs);
      }
    }
    return res;
  }
};

class SqlServerDialect : public CliDialect {
public:
  DbEngine engine() const override { return DbEngine::SqlServer; }
  QStringList clientCandidates() const override {
    return {"sqlcmd", "/opt/mssql-tools18/bin/sqlcmd",
            "/opt/mssql-tools/bin/sqlcmd"};
  }
  QString passwordVariable() const override { return "SQLCMDPASSWORD"; }

  QStringList clientArguments(const DbConnectionProfile &p,
                              bool inContainer) const override {
    QStringList args;
    QString server = QStringLiteral("localhost");
    if (!inContainer) {
      server = p.host.isEmpty() ? QStringLiteral("localhost") : p.host;
      if (p.port > 0 && p.port != 1433) {
        server += QLatin1Char(',') + QString::number(p.port);
      }
    }
    args << "-S" << server << "-U"
         << (p.user.isEmpty() ? DbEngineInfo::defaultUser(p.engine) : p.user);
    if (p.trustServerCertificate) {
      args << "-C";
    }
    if (!p.database.isEmpty()) {
      args << "-d" << p.database;
    }
    args << "-r" << "1" << "-W" << "-s" << QString(kUnitSep) << "-l" << "30"
         << "-w" << "65535";
    return args;
  }

  QByteArray frame(const QString &statement, SqlStatementKind,
                   const QString &marker) const override {
    QString out = statement.trimmed() + QStringLiteral("\nGO\n");
    out += QStringLiteral(
               "DECLARE @lp_quiet bit = CASE WHEN @@OPTIONS & 512 = 512 THEN 1 "
               "ELSE 0 END;\nSET NOCOUNT ON;\nSELECT '%1' AS [%1H];\n"
               "IF @lp_quiet = 0 SET NOCOUNT OFF;\nPRINT '%1';\nGO\n")
               .arg(marker);
    return out.toUtf8();
  }

  DbStatementResult parse(const QString &sql, SqlStatementKind kind,
                          const QString &out, const QString &err,
                          int maxRows) const override {
    Q_UNUSED(kind)
    DbStatementResult res;
    res.sql = sql;

    static const QRegularExpression header(QStringLiteral(
        "^Msg (\\d+), Level (\\d+), State (\\d+)(?:, Server [^,]*)?(?:, "
        "Procedure [^,]*)?, Line (\\d+)$"));
    const QStringList errLines = CliParsing::splitLines(err);
    QStringList errors;
    int i = 0;
    while (i < errLines.size()) {
      const QString &line = errLines[i];
      const auto m = header.match(line);
      if (m.hasMatch()) {
        const int level = m.captured(2).toInt();
        QStringList text;
        ++i;
        while (i < errLines.size() && !header.match(errLines[i]).hasMatch()) {
          text << errLines[i];
          ++i;
        }
        const QString message = text.join('\n').trimmed();
        if (level >= 11) {
          errors << QStringLiteral("%1 (Msg %2, Line %3)")
                        .arg(message, m.captured(1), m.captured(4));
        } else if (!message.isEmpty()) {
          res.messages << message;
        }
        continue;
      }
      if (line.startsWith(QLatin1String("Sqlcmd: Error:"))) {
        errors << line.mid(QStringLiteral("Sqlcmd: Error:").size()).trimmed();
      } else if (!line.trimmed().isEmpty()) {
        res.messages << line;
      }
      ++i;
    }
    if (!errors.isEmpty()) {
      res.ok = false;
      res.error = errors.join('\n');
    }

    const QStringList lines = CliParsing::splitLines(out);
    static const QRegularExpression dashLine(QStringLiteral("^-+(\\x1f-+)*$"));
    static const QRegularExpression affected(
        QStringLiteral("^\\((\\d+) rows? affected\\)$"));
    int idx = 0;
    while (idx < lines.size()) {
      const QString &line = lines[idx];
      const auto am = affected.match(line.trimmed());
      if (am.hasMatch()) {
        res.rowsAffected = am.captured(1).toLongLong();
        ++idx;
        continue;
      }

      if (idx + 1 < lines.size() && dashLine.match(lines[idx + 1]).hasMatch()) {
        QVector<QStringList> records;
        QStringList headerFields = line.split(kUnitSep);
        for (QString &name : headerFields) {
          if (name.isEmpty()) {
            name = QStringLiteral("(No column name)");
          }
        }
        records.append(headerFields);
        const int cols = headerFields.size();
        idx += 2;
        while (idx < lines.size() && !lines[idx].isEmpty()) {
          QString row = lines[idx];
          ++idx;

          while (idx < lines.size() && !lines[idx].isEmpty() &&
                 (row.count(kUnitSep) < cols - 1 ||
                  (cols > 1 && lines[idx].count(kUnitSep) < cols - 1))) {
            row += QLatin1Char('\n') + lines[idx];
            ++idx;
          }
          records.append(row.split(kUnitSep));
        }
        DbResultSet rs = CliParsing::buildResultSet(
            records, QStringLiteral("NULL"), maxRows);
        res.resultSets.append(rs);
        continue;
      }
      if (!line.trimmed().isEmpty()) {
        res.messages << line;
      }
      ++idx;
    }
    return res;
  }
};

} // namespace

std::unique_ptr<CliDialect> CliDialect::create(DbEngine engine) {
  switch (engine) {
  case DbEngine::PostgreSql:
    return std::make_unique<PostgresDialect>();
  case DbEngine::MySql:
    return std::make_unique<MySqlDialect>();
  case DbEngine::SqlServer:
    return std::make_unique<SqlServerDialect>();
  case DbEngine::Sqlite:
    break;
  }
  return nullptr;
}
