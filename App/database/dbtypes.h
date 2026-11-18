#ifndef DBTYPES_H
#define DBTYPES_H

#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

enum class DbEngine { Sqlite, PostgreSql, MySql, SqlServer };

enum class DbTransport { Direct, Docker };

// Where the password for a server connection comes from. Passwords typed by
// the user are never written to disk.
enum class DbPasswordSource { Ask, None, Environment };

namespace DbEngineInfo {

QString id(DbEngine engine);
DbEngine fromId(const QString &id, bool *ok = nullptr);
QString displayName(DbEngine engine);
QString clientName(DbEngine engine);
int defaultPort(DbEngine engine);
QString defaultUser(DbEngine engine);
QString defaultDatabase(DbEngine engine);
bool usesFile(DbEngine engine);
bool supportsDocker(DbEngine engine);
QVector<DbEngine> allEngines();

bool engineForImage(const QString &image, DbEngine *engine);

// Encryption choices for the engine as (id, label) pairs. The ids are libpq's
// sslmode names; an empty id means "use the client's default".
QList<QPair<QString, QString>> sslModes(DbEngine engine);
bool supportsSslMode(DbEngine engine);
QString sslModeLabel(const QString &mode);
QString normalizedSslMode(const QString &mode);

} // namespace DbEngineInfo

struct DbConnectionProfile {
  QString id;
  QString name;
  DbEngine engine = DbEngine::Sqlite;
  DbTransport transport = DbTransport::Direct;
  QString host = QStringLiteral("localhost");
  int port = 0;
  QString user;
  QString database;
  QString filePath;
  QString container;
  QString clientPath;
  QString extraArgs;
  // libpq sslmode names (disable, prefer, require, verify-ca, verify-full),
  // translated for psql and mysql/mariadb. Empty uses the client's default.
  QString sslMode;
  DbPasswordSource passwordSource = DbPasswordSource::Ask;
  // Environment variable holding the password when passwordSource is
  // Environment; Lightpad's environment wins over the project's .env files.
  QString passwordEnv;
  bool trustServerCertificate = true;
  bool readOnly = false;
  bool confirmDestructive = true;
  QString color;

  int effectivePort() const {
    return port > 0 ? port : DbEngineInfo::defaultPort(engine);
  }
  QString summary() const;
  bool isValid(QString *reason = nullptr) const;
  bool needsPassword() const {
    return engine != DbEngine::Sqlite &&
           passwordSource != DbPasswordSource::None;
  }
  // True when changing from `other` to this profile needs a new session.
  bool sameTargetAs(const DbConnectionProfile &other) const;

  QJsonObject toJson() const;
  static DbConnectionProfile fromJson(const QJsonObject &obj);
};

struct DbColumn {
  QString name;
  QString type;
};

struct DbResultSet {
  QVector<DbColumn> columns;
  QVector<QVector<QVariant>> rows;
  bool truncated = false;
  qint64 totalRows = 0;
  // False when fetching stopped at a limit; totalRows is then a lower bound.
  bool totalRowsKnown = true;

  bool isEmpty() const { return columns.isEmpty(); }
};

struct DbStatementResult {
  QString sql;
  bool ok = true;
  QString error;
  QStringList messages;

  QVector<DbResultSet> resultSets;
  qint64 rowsAffected = -1;
  qint64 elapsedMs = 0;

  bool hasRows() const { return !resultSets.isEmpty(); }
};

Q_DECLARE_METATYPE(DbStatementResult)

#endif
