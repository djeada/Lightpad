#ifndef DBTYPES_H
#define DBTYPES_H

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

enum class DbEngine { Sqlite, PostgreSql, MySql, SqlServer };

enum class DbTransport { Direct, Docker };

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
  bool trustServerCertificate = true;
  bool readOnly = false;
  bool confirmDestructive = true;
  QString color;

  int effectivePort() const {
    return port > 0 ? port : DbEngineInfo::defaultPort(engine);
  }
  QString summary() const;
  bool isValid(QString *reason = nullptr) const;

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
