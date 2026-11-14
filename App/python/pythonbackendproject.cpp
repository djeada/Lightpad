#include "pythonbackendproject.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace {
QString readText(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly)
             ? QString::fromUtf8(file.read(1024 * 1024))
             : QString();
}

QString absolutePath(const QString &root, const QString &path) {
  QString expanded = path;
  expanded.replace("${workspaceFolder}", root);
  return QDir::cleanPath(QDir(root).absoluteFilePath(expanded));
}

bool inside(const QString &root, const QString &path) {
  const QString relative = QDir(root).relativeFilePath(path);
  return relative != ".." && !relative.startsWith("../") &&
         !QDir::isAbsolutePath(relative);
}

// Keep discovery bounded and skip environments, caches, tests and symlinks.
QStringList pythonFiles(const QString &root, int depth = 0) {
  QStringList files;
  const auto entries = QDir(root).entryInfoList(
      QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
      QDir::Name);
  for (const QFileInfo &entry : entries) {
    if (entry.isFile() && entry.suffix() == "py" &&
        !entry.fileName().startsWith("test_") &&
        !entry.fileName().endsWith("_test.py") &&
        entry.fileName() != "conftest.py") {
      files << entry.absoluteFilePath();
    } else if (entry.isDir() && depth < 3 &&
               !entry.fileName().startsWith('.') &&
               !QStringList{"venv", "env", "__pycache__", "node_modules",
                            "tests", "test", "build", "dist", "scripts"}
                    .contains(entry.fileName())) {
      files << pythonFiles(entry.absoluteFilePath(), depth + 1);
    }
    if (files.size() >= 200)
      break;
  }
  return files.mid(0, 200);
}

struct EntryPoint {
  QString file;
  QString framework;
  QString attribute;
};

EntryPoint entryPoint(const QString &file) {
  const QString text = readText(file);
  // Require the actual import as well as a top-level constructor assignment.
  for (const QString &framework : {QString("FastAPI"), QString("Flask")}) {
    const QString package = framework == "FastAPI" ? "fastapi" : "flask";
    const QRegularExpression import("^\\s*(?:from\\s+" + package +
                                        "\\s+import\\s+[^\\n]*\\b" + framework +
                                        "\\b|import\\s+" + package + "\\b)",
                                    QRegularExpression::MultilineOption);
    const QRegularExpression assignment(
        "^([A-Za-z_]\\w*)\\s*(?::[^=\\n]+)?=\\s*(?:" + package + "\\.)?" +
            framework + "\\s*\\(",
        QRegularExpression::MultilineOption);
    const auto match = assignment.match(text);
    if (import.match(text).hasMatch() && match.hasMatch())
      return {file, package, match.captured(1)};
    if (package == "flask" && import.match(text).hasMatch() &&
        QRegularExpression("^def\\s+create_app\\s*\\(",
                           QRegularExpression::MultilineOption)
            .match(text)
            .hasMatch())
      return {file, package, "create_app"};
  }
  return {};
}

QString sourceRoot(const QString &root, const QString &file) {
  // src is an import root, while directories with __init__.py are packages.
  QDir dir(QFileInfo(file).absolutePath());
  while (dir.absolutePath() != root &&
         QFileInfo(dir.filePath("__init__.py")).isFile()) {
    dir.cdUp();
  }
  return dir.absolutePath();
}
} // namespace

PythonBackendProject
PythonBackendProject::discover(const QString &workspaceFolder) {
  PythonBackendProject project;
  if (workspaceFolder.isEmpty() || !QFileInfo(workspaceFolder).isDir())
    return project;
  project.root = QFileInfo(workspaceFolder).absoluteFilePath();
  const QString configPath =
      QDir(project.root).filePath(".lightpad/backend.json");
  if (QFileInfo::exists(configPath)) {
    QJsonParseError parseError;
    const auto document =
        QJsonDocument::fromJson(readText(configPath).toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
      project.error = "Invalid .lightpad/backend.json: " +
                      (document.isObject() ? parseError.errorString()
                                           : QString("expected an object"));
      return project;
    }
    project.settings = document.object();
    if (!project.settings.value("enabled").toBool(true))
      return project;
  }

  const auto &settings = project.settings;
  const QString configuredEntry = settings.value("entryFile").toString();
  if (!configuredEntry.isEmpty()) {
    project.entryFile = absolutePath(project.root, configuredEntry);
    if (!QFileInfo(project.entryFile).isFile()) {
      project.error = "Backend entryFile does not exist: " + configuredEntry;
      return project;
    }
  }
  project.application = settings.value("application").toString();
  project.framework = settings.value("framework").toString().toLower();

  // Resolve an explicitly named module before considering unrelated apps.
  if (project.entryFile.isEmpty() && !project.application.isEmpty()) {
    QString modulePath = project.application.section(':', 0, 0);
    modulePath.replace('.', '/');
    const QStringList roots =
        settings.contains("sourceDirectory")
            ? QStringList{absolutePath(
                  project.root, settings.value("sourceDirectory").toString())}
            : QStringList{project.root, QDir(project.root).filePath("src")};
    for (const QString &importRoot : roots) {
      for (const QString &suffix : {QString(".py"), QString("/__init__.py")}) {
        const QString path = QDir(importRoot).filePath(modulePath + suffix);
        if (QFileInfo(path).isFile()) {
          project.entryFile = path;
          project.sourceDirectory = importRoot;
          break;
        }
      }
      if (!project.entryFile.isEmpty())
        break;
    }
  }

  QList<EntryPoint> candidates;
  const QStringList files = project.entryFile.isEmpty()
                                ? pythonFiles(project.root)
                                : QStringList{project.entryFile};
  for (const QString &file : files) {
    const auto candidate = entryPoint(file);
    if (!candidate.file.isEmpty())
      candidates << candidate;
  }
  if (project.entryFile.isEmpty() && candidates.size() > 1 &&
      project.application.isEmpty()) {
    project.error = "Several Python web apps found. Choose entryFile in "
                    ".lightpad/backend.json.";
    return project;
  }
  if (!candidates.isEmpty()) {
    const auto &candidate = candidates.first();
    project.entryFile = candidate.file;
    if (project.framework.isEmpty())
      project.framework = candidate.framework;
    project.sourceDirectory = sourceRoot(project.root, candidate.file);
    if (project.application.isEmpty()) {
      QString module =
          QDir(project.sourceDirectory).relativeFilePath(candidate.file);
      module.chop(3);
      module.replace('/', '.');
      if (module.endsWith(".__init__"))
        module.chop(9);
      project.application = module + ':' + candidate.attribute;
    }
  }
  if (project.application.isEmpty())
    return project;
  if (project.framework != "fastapi" && project.framework != "flask") {
    project.error = "Set backend framework to fastapi or flask.";
    return project;
  }
  if (!QRegularExpression("^[A-Za-z_]\\w*(?:\\.[A-Za-z_]\\w*)*:[A-Za-z_]"
                          "\\w*(?:\\.[A-Za-z_]\\w*)*(?:\\(\\))?$")
           .match(project.application)
           .hasMatch()) {
    project.error = "Backend application must be module:app or module:factory.";
    return project;
  }
  project.sourceDirectory =
      absolutePath(project.root, settings.value("sourceDirectory")
                                     .toString(project.sourceDirectory.isEmpty()
                                                   ? project.root
                                                   : project.sourceDirectory));
  project.workingDirectory = absolutePath(
      project.root, settings.value("workingDirectory").toString(project.root));
  project.envFile =
      absolutePath(project.root, settings.value("envFile").toString(".env"));
  project.host = settings.value("host").toString("127.0.0.1");
  project.port =
      settings.value("port").toInt(project.framework == "flask" ? 5000 : 8000);
  if (project.host.isEmpty() || project.host.startsWith('-') ||
      project.port < 1 || project.port > 65535 ||
      !QFileInfo(project.sourceDirectory).isDir() ||
      !QFileInfo(project.workingDirectory).isDir())
    project.error = "Check backend host, port, sourceDirectory and "
                    "workingDirectory in .lightpad/backend.json.";
  return project;
}

bool PythonBackendProject::ownsFile(const QString &path) const {
  if (!isValid())
    return false;
  if (path.isEmpty())
    return true;
  const QString relative = QDir(root).relativeFilePath(path);
  if (!inside(root, path))
    return false;
  const QStringList parts = relative.split('/');
  for (const QString &part : parts)
    if (part.startsWith('.') ||
        QStringList{"tests", "test", "scripts", "venv", "env"}.contains(part))
      return false;
  const QString filename = QFileInfo(path).fileName();
  if (filename.startsWith("test_") || filename.endsWith("_test.py") ||
      filename == "conftest.py")
    return false;
  return QStringList{"py", "pyw", "html", "jinja", "jinja2", "j2", "css"}
      .contains(QFileInfo(path).suffix().toLower());
}

QString PythonBackendProject::name() const {
  return (framework == "flask" ? "Flask: " : "FastAPI: ") + application;
}

QString PythonBackendProject::module() const {
  return framework == "flask" ? "flask" : "uvicorn";
}

QStringList PythonBackendProject::serverArguments(bool debug) const {
  QStringList args;
  if (framework == "flask") {
    args = {"--app",  application,          "run", "--host", host,
            "--port", QString::number(port)};
    if (!debug)
      args << "--debug";
    else
      args << "--no-reload" << "--no-debugger";
  } else {
    args = {application, "--host", host, "--port", QString::number(port)};
    if (!debug)
      args << "--reload" << "--reload-dir" << sourceDirectory;
  }
  return args;
}

QString PythonBackendProject::baseUrl() const {
  const QString configured = settings.value("baseUrl").toString();
  if (!configured.isEmpty())
    return configured;
  QUrl url;
  url.setScheme("http");
  url.setHost(host == "0.0.0.0" || host == "::" ? "localhost" : host);
  url.setPort(port);
  return url.toString();
}

QString PythonBackendProject::requirementsFile() const {
  for (const QString &name :
       {QString("requirements-dev.txt"), QString("requirements.txt"),
        QString("pyproject.toml"), QString("setup.py")}) {
    const QString path = QDir(root).filePath(name);
    if (QFileInfo(path).isFile())
      return path;
  }
  return {};
}

QString PythonBackendProject::composeFile() const {
  for (const QString &name :
       {QString("compose.yaml"), QString("compose.yml"),
        QString("docker-compose.yml"), QString("docker-compose.yaml")}) {
    const QString path = QDir(root).filePath(name);
    if (QFileInfo(path).isFile())
      return path;
  }
  return {};
}

PythonEnvironmentPreference PythonBackendProject::pythonPreference() const {
  QJsonObject preference;
  const auto assignments =
      QJsonDocument::fromJson(
          readText(QDir(root).filePath(".lightpad/run_config.json")).toUtf8())
          .object()
          .value("assignments")
          .toArray();
  for (const auto &value : assignments) {
    const auto assignment = value.toObject();
    if (absolutePath(root, assignment.value("file").toString()) == entryFile) {
      preference = assignment;
      break;
    }
  }
  // Explicit workspace settings take precedence over entry-file preferences.
  for (const QString &key :
       {QString("pythonMode"), QString("pythonInterpreter"),
        QString("pythonVenvPath"), QString("pythonRequirementsFile")})
    if (settings.contains(key))
      preference[key] = settings.value(key);
  return {preference.value("pythonMode").toString(),
          preference.value("pythonInterpreter").toString(),
          preference.value("pythonVenvPath").toString(),
          preference.value("pythonRequirementsFile").toString()};
}

QMap<QString, QString> PythonBackendProject::readEnvFile(const QString &path) {
  QMap<QString, QString> values;
  const QString text = readText(path);
  // Quoted values may span lines. Values are data, never shell commands.
  const QRegularExpression assignment(
      R"env(^[\t ]*(?:export[\t ]+)?([A-Za-z_]\w*)[\t ]*=[\t ]*(?:"((?:\\.|[^"\\])*)"|'([^']*)'|([^\r\n]*))[\t ]*(?:#[^\r\n]*)?\r?$)env",
      QRegularExpression::MultilineOption);
  auto matches = assignment.globalMatch(text);
  while (matches.hasNext()) {
    const auto match = matches.next();
    QString value;
    if (!match.captured(2).isNull()) {
      value = match.captured(2);
      value.replace("\\n", "\n");
      value.replace("\\r", "\r");
      value.replace("\\t", "\t");
      value.replace("\\\"", "\"");
      value.replace("\\\\", "\\");
    } else if (!match.captured(3).isNull()) {
      value = match.captured(3);
    } else {
      value = match.captured(4);
      value.remove(QRegularExpression("\\s+#.*$"));
      value = value.trimmed();
    }
    const QRegularExpression variable(R"(\$\{([A-Za-z_]\w*)\})");
    auto variables = variable.globalMatch(value);
    QString expanded;
    qsizetype position = 0;
    while (variables.hasNext()) {
      const auto reference = variables.next();
      expanded += value.mid(position, reference.capturedStart() - position);
      expanded += QProcessEnvironment::systemEnvironment().value(
          reference.captured(1), values.value(reference.captured(1)));
      position = reference.capturedEnd();
    }
    expanded += value.mid(position);
    values.insert(match.captured(1), expanded);
  }
  return values;
}

QMap<QString, QString> PythonBackendProject::environment() const {
  QMap<QString, QString> env = readEnvFile(envFile);
  // Match dotenv's usual precedence: the launching process wins over .env.
  const auto system = QProcessEnvironment::systemEnvironment();
  for (auto it = env.begin(); it != env.end(); ++it)
    if (system.contains(it.key()))
      it.value() = system.value(it.key());
  const auto overrides = settings.value("env").toObject();
  for (auto it = overrides.begin(); it != overrides.end(); ++it)
    env[it.key()] = it.value().toString();
#ifdef Q_OS_WIN
  const QString separator = ";";
#else
  const QString separator = ":";
#endif
  const QString inherited = env.value("PYTHONPATH", system.value("PYTHONPATH"));
  env["PYTHONPATH"] = sourceDirectory +
                      (inherited.isEmpty() ? QString() : separator + inherited);
  env["PYTHONUNBUFFERED"] = "1";
  // Flask should not reload .env after the shared environment is resolved.
  if (framework == "flask")
    env["FLASK_SKIP_DOTENV"] = "1";
  return env;
}

QString PythonBackendProject::sqlitePath(const QString &url,
                                         const QString &cwd) {
  static const QRegularExpression prefix("^sqlite(?:\\+[\\w]+)?:///");
  const auto match = prefix.match(url);
  if (!match.hasMatch())
    return {};
  QString path = url.mid(match.capturedLength()).section('?', 0, 0);
  if (path.isEmpty() || path == ":memory:" || path.startsWith("file:"))
    return {};
  path = QUrl::fromPercentEncoding(path.toUtf8());
  return absolutePath(cwd, path);
}

QStringList PythonBackendProject::databaseFiles() const {
  if (!isValid())
    return {};
  QStringList candidates;
  const auto env = environment();
  const auto system = QProcessEnvironment::systemEnvironment();
  for (const QString &key :
       {QString("BLOG_DATABASE_URL"), QString("SQLALCHEMY_DATABASE_URI"),
        QString("DATABASE_URL")}) {
    const QString path =
        sqlitePath(env.value(key, system.value(key)), workingDirectory);
    if (!path.isEmpty())
      candidates << path;
  }
  for (const auto &path : settings.value("databaseFiles").toArray())
    candidates << absolutePath(workingDirectory, path.toString());
  const QRegularExpression literal(
      R"(["'](sqlite(?:\+\w+)?:///[^"'\r\n]+)["'])");
  for (const QString &source : pythonFiles(root)) {
    auto urls = literal.globalMatch(readText(source));
    while (urls.hasNext()) {
      const QString path =
          sqlitePath(urls.next().captured(1), workingDirectory);
      if (!path.isEmpty())
        candidates << path;
    }
  }
  // Also find databases created by an earlier launch from src/ or instance/.
  QStringList directories{root, sourceDirectory};
  for (const QString &name :
       {QString("src"), QString("instance"), QString("data"), QString("db")})
    directories << QDir(root).filePath(name);
  for (const QString &directory : directories) {
    for (const QFileInfo &file : QDir(directory).entryInfoList(
             {"*.db", "*.sqlite", "*.sqlite3"}, QDir::Files, QDir::Name))
      candidates << file.absoluteFilePath();
  }
  QSet<QString> seen;
  QStringList result;
  for (const QString &candidate : candidates) {
    const QString canonical = QFileInfo(candidate).canonicalFilePath();
    QFile file(canonical);
    if (!canonical.isEmpty() && !seen.contains(canonical) &&
        file.open(QIODevice::ReadOnly) &&
        file.read(16) == QByteArray("SQLite format 3\0", 16)) {
      seen.insert(canonical);
      result << canonical;
    }
  }
  return result;
}
