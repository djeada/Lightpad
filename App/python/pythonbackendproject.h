#ifndef PYTHONBACKENDPROJECT_H
#define PYTHONBACKENDPROJECT_H

#include "pythonprojectenvironment.h"
#include <QJsonObject>
#include <QMap>
#include <QStringList>

// A static description: discovering a workspace never imports project code,
// starts a server, creates a database, or installs dependencies.
struct PythonBackendProject {
  QString root;
  QString framework;
  QString application;
  QString entryFile;
  QString sourceDirectory;
  QString workingDirectory;
  QString envFile;
  QString host = QStringLiteral("127.0.0.1");
  int port = 0;
  QString error;
  QJsonObject settings;

  bool isValid() const { return error.isEmpty() && !application.isEmpty(); }
  bool ownsFile(const QString &path) const;
  QString name() const;
  QString module() const;
  QStringList serverArguments(bool debug = false) const;
  QString baseUrl() const;
  QString requirementsFile() const;
  QString composeFile() const;
  PythonEnvironmentPreference pythonPreference() const;
  QMap<QString, QString> environment() const;
  QStringList databaseFiles() const;

  static PythonBackendProject discover(const QString &workspaceFolder);
  static QMap<QString, QString> readEnvFile(const QString &path);
  static QString sqlitePath(const QString &url, const QString &cwd);
};

#endif
