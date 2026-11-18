#ifndef DBPROJECTDISCOVERY_H
#define DBPROJECTDISCOVERY_H

#include "dbtypes.h"

#include <QMap>
#include <QString>
#include <QVector>

// A database the open project already describes: a DATABASE_URL (or
// DB_HOST/DB_USER/... variables) in a .env file, a database service in a
// docker compose file, or a SQLite file in the tree.
struct DbDetectedConnection {
  QString title;
  // Project-relative file the connection was read from.
  QString source;
  DbConnectionProfile profile;
  // A password found next to the connection; used for the session only.
  QString password;
  bool hasPassword = false;
};

namespace DbProjectDiscovery {

// KEY=VALUE lines of a .env file: comments, export, quotes and ${VAR}
// references (to earlier keys or Lightpad's environment) are handled.
QMap<QString, QString> parseEnvFile(const QString &text);

QVector<DbDetectedConnection>
fromEnvironment(const QMap<QString, QString> &env, const QString &source,
                const QString &baseDir = QString());

// Database services of a docker compose file. Services publish a port to
// connect directly when the engine's client is installed here; otherwise
// they are reached with docker exec. `projectName` names the containers
// (<project>-<service>-1) when the service sets no container_name.
QVector<DbDetectedConnection> fromCompose(
    const QString &yaml, const QString &source, const QString &projectName,
    const QMap<QString, QString> &variables = {}, bool preferDirect = true);

QVector<DbDetectedConnection> scan(const QString &workspaceFolder);

// The workspace whose .env files back password variables.
void setWorkspaceFolder(const QString &folder);
QString workspaceFolder();
// A variable from Lightpad's environment, else from the workspace's .env
// files.
QString environmentValue(const QString &name, bool *found = nullptr);

} // namespace DbProjectDiscovery

#endif
