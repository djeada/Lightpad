#include "cmakeproject.h"

#include "../core/logging/logger.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QThread>

namespace {

constexpr int CMAKE_PROCESS_TIMEOUT_MS = 600000;

// (line-comment stripping is applied inline in parseTargetsInFile)

QList<CMakeTargetInfo> parseTargetsInFile(const QString &filePath,
                                          bool allowLibraries,
                                          const QString &relativeDir = {}) {
  QList<CMakeTargetInfo> targets;

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return targets;
  }

  // Join continued lines so multi-line add_executable() calls parse fully,
  // and strip line comments so disabled commands are ignored.
  QString contents = QString::fromUtf8(file.readAll());
  QRegularExpression continuationRegex("\\\\\n");
  contents.replace(continuationRegex, QStringLiteral(" "));

  QStringList activeLines;
  const QStringList rawLines = contents.split(QLatin1Char('\n'));
  activeLines.reserve(rawLines.size());
  for (const QString &rawLine : rawLines) {
    const int hashAt = rawLine.indexOf(QLatin1Char('#'));
    activeLines.append(hashAt >= 0 ? rawLine.left(hashAt) : rawLine);
  }
  contents = activeLines.join(QLatin1Char('\n'));

  static const QRegularExpression commandRegex(
      QStringLiteral(
          "\\b(add_executable|add_library)\\s*\\(\\s*([^\\s)]+)([^)]*)\\)"),
      QRegularExpression::DotMatchesEverythingOption);

  static const QStringList ignoredKeywords = {
      "WIN32",     "MACOSX_BUNDLE", "EXCLUDE_FROM_ALL", "SHARED",
      "STATIC",    "MODULE",        "OBJECT",           "UNKNOWN",
      "INTERFACE", "IMPORTED",      "GLOBAL",           "ALIAS"};

  auto it = commandRegex.globalMatch(contents);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    const bool isExecutable = match.captured(1) == "add_executable";
    if (!isExecutable && !allowLibraries) {
      continue;
    }

    // Skip imported/alias declarations: they carry no buildable source list.
    const QString tail = match.captured(3);
    const QStringList tokens =
        tail.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (tokens.contains("IMPORTED") || tokens.contains("ALIAS")) {
      continue;
    }

    CMakeTargetInfo target;
    target.name = match.captured(2);
    target.isExecutable = isExecutable;
    target.relativeDir = relativeDir;
    for (const QString &token : tokens) {
      const QString trimmed = token.trimmed();
      if (trimmed.isEmpty() || ignoredKeywords.contains(trimmed)) {
        continue;
      }
      target.sources.append(trimmed);
    }
    if (target.name.isEmpty()) {
      continue;
    }

    targets.append(target);
  }

  return targets;
}

} // namespace

CMakeProject::CMakeProject(QObject *parent) : QObject(parent) {}

bool CMakeProject::isCMakeProject(const QString &projectRoot) {
  if (projectRoot.isEmpty()) {
    return false;
  }
  return QFileInfo::exists(projectRoot + "/CMakeLists.txt");
}

QString CMakeProject::defaultBinaryDir(const QString &projectRoot) {
  return projectRoot + "/build";
}

QString CMakeProject::findProjectRoot(const QString &startPath) {
  QDir dir(startPath);
  while (true) {
    if (isCMakeProject(dir.absolutePath())) {
      return dir.absolutePath();
    }
    if (!dir.cdUp()) {
      return QString();
    }
  }
}

QList<CMakeTargetInfo>
CMakeProject::parseTargets(const QString &projectRoot) const {
  QList<CMakeTargetInfo> targets =
      parseTargetsInFile(projectRoot + "/CMakeLists.txt", true);

  // Follow simple `add_subdirectory(dir)` references one level deep so the
  // common src/-style layout resolves without a full CMake language parse.
  QFile topFile(projectRoot + "/CMakeLists.txt");
  if (topFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString topContents =
        QString::fromUtf8(topFile.readAll()).replace("\\\n", " ");
    static const QRegularExpression subdirectoryRegex(
        QStringLiteral("\\badd_subdirectory\\s*\\(\\s*([^\\s)]+)"),
        QRegularExpression::CaseInsensitiveOption);
    auto it = subdirectoryRegex.globalMatch(topContents);
    QSet<QString> visited;
    while (it.hasNext()) {
      const QRegularExpressionMatch match = it.next();
      const QString ref = match.captured(1).trimmed();
      if (ref.startsWith("${") || ref.isEmpty()) {
        continue;
      }
      QDir subDir(ref);
      if (subDir.isRelative()) {
        subDir = QDir(projectRoot + "/" + ref);
      }
      const QString listsPath = subDir.absoluteFilePath("CMakeLists.txt");
      if (visited.contains(listsPath) || !QFileInfo::exists(listsPath)) {
        continue;
      }
      visited.insert(listsPath);
      targets.append(parseTargetsInFile(listsPath, true, ref));
    }
  }

  return targets;
}

QList<CMakeTargetInfo>
CMakeProject::parseExecutableTargets(const QString &projectRoot) const {
  QList<CMakeTargetInfo> executables;
  for (const CMakeTargetInfo &target : parseTargets(projectRoot)) {
    if (target.isExecutable) {
      executables.append(target);
    }
  }
  return executables;
}

QString CMakeProject::executablePathFor(const CMakeTargetInfo &target,
                                        const QString &binaryDir) {
  if (target.name.isEmpty() || binaryDir.isEmpty()) {
    return QString();
  }
#ifdef Q_OS_WIN
  const QString fileName = target.name + ".exe";
#else
  const QString fileName = target.name;
#endif
  if (target.relativeDir.isEmpty()) {
    return binaryDir + "/" + fileName;
  }
  return binaryDir + "/" + target.relativeDir + "/" + fileName;
}

QString CMakeProject::targetForSource(const QList<CMakeTargetInfo> &targets,
                                      const QString &projectRoot,
                                      const QString &sourcePath) {
  if (sourcePath.isEmpty() || projectRoot.isEmpty()) {
    return QString();
  }
  const QString wanted = QFileInfo(sourcePath).absoluteFilePath();

  for (const CMakeTargetInfo &target : targets) {
    const QDir targetDir(target.relativeDir.isEmpty()
                             ? projectRoot
                             : projectRoot + "/" + target.relativeDir);
    for (const QString &source : target.sources) {
      // Unexpanded CMake variables cannot be resolved without running CMake.
      if (source.contains(QLatin1String("${"))) {
        continue;
      }
      if (QDir::cleanPath(targetDir.absoluteFilePath(source)) == wanted) {
        return target.name;
      }
    }
  }
  return QString();
}

bool CMakeProject::needsConfigure(const QString &binaryDir) {
  return !QFileInfo::exists(binaryDir + "/CMakeCache.txt");
}

QStringList CMakeProject::configureCommand(const QString &projectRoot,
                                           const QString &binaryDir) {
  // The debug launch flow is the only caller, so configure for debugging:
  // without a build type single-config generators compile without -g and the
  // adapter has no line information to stop on.
  return {"cmake",
          "-S",
          projectRoot,
          "-B",
          binaryDir,
          "-DCMAKE_BUILD_TYPE=Debug",
          "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"};
}

QStringList CMakeProject::buildCommand(const QString &binaryDir, int jobs) {
  QStringList args = {"cmake", "--build", binaryDir};
  if (jobs > 0) {
    args << "-j" << QString::number(jobs);
  }
  return args;
}

bool CMakeProject::configure(const QString &projectRoot,
                             const QString &binaryDir, QString *output,
                             QString *errorMessage) const {
  if (projectRoot.isEmpty() || binaryDir.isEmpty()) {
    if (errorMessage) {
      *errorMessage = QObject::tr("CMake configure: invalid paths");
    }
    return false;
  }

  // Already configured: the subsequent build re-checks the generator.
  if (!needsConfigure(binaryDir)) {
    return true;
  }

  QProcess process;
  process.setWorkingDirectory(projectRoot);
  const QStringList args = configureCommand(projectRoot, binaryDir);
  process.start(args.first(), args.mid(1));

  if (!process.waitForStarted(10000)) {
    if (errorMessage) {
      *errorMessage =
          QObject::tr("Failed to start cmake: %1").arg(process.errorString());
    }
    return false;
  }
  if (!process.waitForFinished(CMAKE_PROCESS_TIMEOUT_MS)) {
    process.kill();
    if (errorMessage) {
      *errorMessage = QObject::tr("CMake configure timed out");
    }
    return false;
  }

  const QByteArray combined =
      process.readAllStandardOutput() + process.readAllStandardError();
  if (output) {
    *output = QString::fromUtf8(combined);
  }

  if (process.exitCode() != 0 || process.exitStatus() != QProcess::NormalExit) {
    if (errorMessage) {
      *errorMessage =
          QObject::tr("CMake configure failed:\n%1")
              .arg(QString::fromUtf8(combined).trimmed().left(4000));
    }
    return false;
  }

  LOG_INFO("CMake project configured in " + binaryDir);
  return true;
}

bool CMakeProject::build(const QString &binaryDir, int jobs, QString *output,
                         QString *errorMessage) const {
  if (binaryDir.isEmpty()) {
    if (errorMessage) {
      *errorMessage = QObject::tr("CMake build: invalid binary directory");
    }
    return false;
  }

  QStringList args = buildCommand(binaryDir, jobs);

  QProcess process;
  process.setWorkingDirectory(binaryDir);
  process.start(args.first(), args.mid(1));

  if (!process.waitForStarted(10000)) {
    if (errorMessage) {
      *errorMessage =
          QObject::tr("Failed to start cmake: %1").arg(process.errorString());
    }
    return false;
  }
  if (!process.waitForFinished(CMAKE_PROCESS_TIMEOUT_MS)) {
    process.kill();
    if (errorMessage) {
      *errorMessage = QObject::tr("CMake build timed out");
    }
    return false;
  }

  const QByteArray combined =
      process.readAllStandardOutput() + process.readAllStandardError();
  if (output) {
    *output = QString::fromUtf8(combined);
  }

  if (process.exitCode() != 0 || process.exitStatus() != QProcess::NormalExit) {
    if (errorMessage) {
      *errorMessage =
          QObject::tr("Build failed:\n%1")
              .arg(QString::fromUtf8(combined).trimmed().left(4000));
    }
    return false;
  }

  return true;
}

QString CMakeProject::resolveBinaryDirectory(const QString &projectRoot,
                                             const QString &configuredPath) {
  if (!configuredPath.trimmed().isEmpty()) {
    QDir dir(configuredPath.trimmed());
    if (dir.isRelative()) {
      return QDir(projectRoot).absoluteFilePath(configuredPath.trimmed());
    }
    return dir.absolutePath();
  }
  return defaultBinaryDir(projectRoot);
}
