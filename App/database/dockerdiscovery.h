#ifndef DOCKERDISCOVERY_H
#define DOCKERDISCOVERY_H

#include "dbtypes.h"

#include <QObject>
#include <QProcess>
#include <QVector>

struct DockerContainer {
  QString id;
  QString name;
  QString image;
  QString status;
  QString ports;
  bool running = true;
  bool isDatabase = false;
  DbEngine engine = DbEngine::SqlServer;
  int hostPort = 0;
};

struct DockerCredentialHints {
  QString user;
  QString database;
  QString password;
  bool hasPassword() const { return !password.isEmpty(); }
};

namespace DockerDiscoveryParsing {

QString psFormat();
QVector<DockerContainer> parsePs(const QString &output);

int publishedHostPort(const QString &ports, int containerPort);
DockerCredentialHints credentialsFromEnv(DbEngine engine,
                                         const QStringList &envLines);
DbConnectionProfile profileFor(const DockerContainer &container,
                               const DockerCredentialHints &hints = {});

} // namespace DockerDiscoveryParsing

class DockerDiscovery : public QObject {
  Q_OBJECT

public:
  explicit DockerDiscovery(QObject *parent = nullptr);
  ~DockerDiscovery() override;

  void refresh();

  void inspect(const DockerContainer &container);

signals:
  void containersFound(const QVector<DockerContainer> &containers);
  void failed(const QString &message);
  void credentialsFound(const QString &containerName,
                        const DockerCredentialHints &hints);

private:
  QProcess *m_psProcess = nullptr;
};

#endif
