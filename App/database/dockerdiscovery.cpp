#include "dockerdiscovery.h"

#include <QMap>
#include <QRegularExpression>
#include <algorithm>

namespace DockerDiscoveryParsing {

QString psFormat() {
  return QStringLiteral(
      "{{.ID}}\t{{.Names}}\t{{.Image}}\t{{.Status}}\t{{.Ports}}");
}

int publishedHostPort(const QString &ports, int containerPort) {

  static const QRegularExpression re(
      QStringLiteral("(?:[\\w.\\[\\]:]*?:)?(\\d+)->(\\d+)(?:-(\\d+))?/"));
  auto it = re.globalMatch(ports);
  while (it.hasNext()) {
    const auto m = it.next();
    const int host = m.captured(1).toInt();
    const int cont = m.captured(2).toInt();
    if (cont == containerPort) {
      return host;
    }
  }
  return 0;
}

QVector<DockerContainer> parsePs(const QString &output) {
  QVector<DockerContainer> out;
  const QStringList lines = output.split('\n');
  for (const QString &raw : lines) {
    const QString line = raw.trimmed();
    if (line.isEmpty()) {
      continue;
    }
    const QStringList f = line.split('\t');
    if (f.size() < 3) {
      continue;
    }
    DockerContainer c;
    c.id = f[0];
    c.name = f[1];
    c.image = f[2];
    c.status = f.value(3);
    c.ports = f.value(4);
    c.running = !c.status.startsWith(QLatin1String("Exited")) &&
                !c.status.startsWith(QLatin1String("Created"));
    DbEngine engine = DbEngine::Sqlite;
    if (DbEngineInfo::engineForImage(c.image, &engine)) {
      c.isDatabase = true;
      c.engine = engine;
      c.hostPort =
          publishedHostPort(c.ports, DbEngineInfo::defaultPort(engine));
    }
    out.append(c);
  }

  std::stable_sort(out.begin(), out.end(),
                   [](const DockerContainer &a, const DockerContainer &b) {
                     if (a.isDatabase != b.isDatabase) {
                       return a.isDatabase;
                     }
                     return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
                   });
  return out;
}

DockerCredentialHints credentialsFromEnv(DbEngine engine,
                                         const QStringList &envLines) {
  QMap<QString, QString> env;
  for (const QString &line : envLines) {
    const int eq = line.indexOf('=');
    if (eq > 0) {
      env.insert(line.left(eq), line.mid(eq + 1));
    }
  }
  DockerCredentialHints hints;
  switch (engine) {
  case DbEngine::SqlServer:
    hints.user = QStringLiteral("sa");
    hints.password = env.value("MSSQL_SA_PASSWORD", env.value("SA_PASSWORD"));
    break;
  case DbEngine::PostgreSql:
    hints.user = env.value("POSTGRES_USER", QStringLiteral("postgres"));
    hints.database = env.value("POSTGRES_DB", hints.user);
    hints.password = env.value("POSTGRES_PASSWORD");
    break;
  case DbEngine::MySql: {
    const QString root =
        env.value("MYSQL_ROOT_PASSWORD", env.value("MARIADB_ROOT_PASSWORD"));
    const QString user = env.value("MYSQL_USER", env.value("MARIADB_USER"));
    const QString userPw =
        env.value("MYSQL_PASSWORD", env.value("MARIADB_PASSWORD"));
    hints.database = env.value("MYSQL_DATABASE", env.value("MARIADB_DATABASE"));
    if (!root.isEmpty() || user.isEmpty()) {
      hints.user = QStringLiteral("root");
      hints.password = root;
    } else {
      hints.user = user;
      hints.password = userPw;
    }
    break;
  }
  case DbEngine::Sqlite:
    break;
  }
  return hints;
}

DbConnectionProfile profileFor(const DockerContainer &c,
                               const DockerCredentialHints &hints) {
  DbConnectionProfile p;
  p.engine = c.engine;
  p.transport = DbTransport::Docker;
  p.container = c.name;
  p.name = c.name;
  p.user =
      hints.user.isEmpty() ? DbEngineInfo::defaultUser(c.engine) : hints.user;
  p.database = hints.database.isEmpty()
                   ? DbEngineInfo::defaultDatabase(c.engine)
                   : hints.database;
  p.host = QStringLiteral("localhost");
  p.port = c.hostPort;
  return p;
}

} // namespace DockerDiscoveryParsing

DockerDiscovery::DockerDiscovery(QObject *parent) : QObject(parent) {}

DockerDiscovery::~DockerDiscovery() {
  if (m_psProcess) {
    m_psProcess->disconnect(this);
    m_psProcess->kill();
    m_psProcess->waitForFinished(500);
  }
}

void DockerDiscovery::refresh() {
  if (m_psProcess) {
    return;
  }
  m_psProcess = new QProcess(this);
  connect(m_psProcess, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart && m_psProcess) {
              m_psProcess->deleteLater();
              m_psProcess = nullptr;
              emit failed(QStringLiteral(
                  "Docker was not found. Install Docker or add it to PATH."));
            }
          });
  connect(
      m_psProcess, &QProcess::finished, this,
      [this](int code, QProcess::ExitStatus) {
        if (!m_psProcess) {
          return;
        }
        const QString out =
            QString::fromUtf8(m_psProcess->readAllStandardOutput());
        const QString err =
            QString::fromUtf8(m_psProcess->readAllStandardError()).trimmed();
        m_psProcess->deleteLater();
        m_psProcess = nullptr;
        if (code != 0) {
          emit failed(err.isEmpty() ? QStringLiteral("docker ps failed.")
                                    : err);
          return;
        }
        emit containersFound(DockerDiscoveryParsing::parsePs(out));
      });
  m_psProcess->start(QStringLiteral("docker"),
                     {QStringLiteral("ps"), QStringLiteral("--format"),
                      DockerDiscoveryParsing::psFormat()});
}

void DockerDiscovery::inspect(const DockerContainer &container) {
  auto *proc = new QProcess(this);
  const QString name = container.name;
  const DbEngine engine = container.engine;
  connect(proc, &QProcess::finished, this,
          [this, proc, name, engine](int code, QProcess::ExitStatus) {
            const QString out =
                QString::fromUtf8(proc->readAllStandardOutput());
            proc->deleteLater();
            if (code != 0) {
              return;
            }
            emit credentialsFound(
                name, DockerDiscoveryParsing::credentialsFromEnv(
                          engine, out.split('\n', Qt::SkipEmptyParts)));
          });
  connect(proc, &QProcess::errorOccurred, this,
          [proc](QProcess::ProcessError) { proc->deleteLater(); });
  proc->start(QStringLiteral("docker"),
              {QStringLiteral("inspect"), QStringLiteral("--format"),
               QStringLiteral("{{range .Config.Env}}{{println .}}{{end}}"),
               container.name});
}
