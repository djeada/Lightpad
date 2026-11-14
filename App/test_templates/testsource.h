#ifndef TESTSOURCE_H
#define TESTSOURCE_H

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#ifndef Q_MOC_RUN
namespace TestSource {
inline QStringList sourceFiles(const QString &workspace,
                               const QStringList &filters) {
  QStringList pending{workspace}, files;
  const QStringList excluded{".git",   ".lightpad",    ".cache", "build",
                             "target", "node_modules", ".venv",  "venv"};
  while (!pending.isEmpty() && files.size() < 5000) {
    QDir directory(pending.takeLast());
    for (const auto &entry : directory.entryInfoList(
             QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
      if (!excluded.contains(entry.fileName()) &&
          !entry.fileName().startsWith("build-"))
        pending << entry.absoluteFilePath();
    for (const auto &entry : directory.entryInfoList(filters, QDir::Files))
      files << entry.absoluteFilePath();
  }
  return files;
}

inline QString sourceCode(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024)
    return {};
  QString code = QString::fromUtf8(file.readAll());
  const QRegularExpression trivia(
      R"trivia(/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*")trivia");
  auto matches = trivia.globalMatch(code);
  while (matches.hasNext()) {
    const auto match = matches.next();
    for (int i = match.capturedStart(); i < match.capturedEnd(); ++i)
      if (code[i] != '\n')
        code[i] = ' ';
  }
  return code;
}

inline bool namedCDefinition(const QString &workspace, const QString &name,
                             QString *path, int *line) {
  if (!QRegularExpression("^[A-Za-z_]\\w*$").match(name).hasMatch())
    return false;
  const QRegularExpression function("(?:^|\\n)[ \\t]*(?:[\\w:*&<>]+[ \\t]+)+" +
                                    QRegularExpression::escape(name) +
                                    R"(\s*\([^;{}]*\)\s*(?:const\s*)?\{)");
  QString found;
  int foundLine = 0;
  for (const QString &file : sourceFiles(
           workspace, {"*.c", "*.cpp", "*.cc", "*.cxx", "*.h", "*.hpp"})) {
    const QString code = sourceCode(file);
    const auto match = function.match(code);
    if (!match.hasMatch())
      continue;
    if (!found.isEmpty())
      return false; // An ambiguous name is not a safe navigation target.
    found = file;
    foundLine = code.left(match.capturedStart() +
                          (match.captured().startsWith('\n') ? 1 : 0))
                    .count('\n') +
                1;
  }
  if (found.isEmpty())
    return false;
  *path = found;
  *line = foundLine;
  return true;
}

inline bool rustDefinition(const QString &workspace,
                           const QString &qualifiedName, QString *path,
                           int *line) {
  const QStringList names = qualifiedName.split("::");
  if (names.isEmpty())
    return false;
  const QRegularExpression tokens(
      R"(\bmod\s+(\w+)\s*\{|\b(?:async\s+)?fn\s+(\w+)\s*\(|[{}])");
  QString found;
  int foundLine = 0;
  for (const QString &file : sourceFiles(workspace, {"*.rs"})) {
    QString relative = QDir(workspace).relativeFilePath(file);
    QStringList prefix;
    if (relative.startsWith("src/")) {
      relative = relative.mid(4);
      prefix = relative.split('/');
      const QString stem = QFileInfo(prefix.takeLast()).completeBaseName();
      if (stem != "lib" && stem != "main" && stem != "mod")
        prefix << stem;
    }
    const QString code = sourceCode(file);
    QList<QPair<int, QString>> modules;
    int depth = 0;
    auto matches = tokens.globalMatch(code);
    while (matches.hasNext()) {
      const auto match = matches.next();
      if (!match.captured(1).isEmpty())
        modules.append({depth++, match.captured(1)});
      else if (match.captured() == "{")
        ++depth;
      else if (match.captured() == "}") {
        --depth;
        while (!modules.isEmpty() && modules.last().first >= depth)
          modules.removeLast();
      } else if (match.captured(2) == names.last()) {
        QStringList owner = prefix;
        for (const auto &module : modules)
          owner << module.second;
        if (owner != names.mid(0, names.size() - 1))
          continue;
        if (!found.isEmpty())
          return false;
        found = file;
        foundLine = code.left(match.capturedStart()).count('\n') + 1;
      }
    }
  }
  if (found.isEmpty())
    return false;
  *path = found;
  *line = foundLine;
  return true;
}

// Fallback for runners without definition metadata. Never executes the source.
inline int pythonDefinitionLine(const QString &path, QString qualifiedName) {
  qualifiedName = qualifiedName.section('[', 0, 0);
  const auto names = qualifiedName.split("::", Qt::SkipEmptyParts);
  if (names.isEmpty())
    return 0;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024)
    return 0;
  const QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
  const QRegularExpression declaration(
      R"(^(\s*)(class|(?:async\s+)?def)\s+([A-Za-z_]\w*)\s*[(:])");
  QList<QPair<int, QString>> classes;
  for (int i = 0; i < lines.size(); ++i) {
    const auto match = declaration.match(lines.at(i));
    if (!match.hasMatch())
      continue;
    const int indentation = match.captured(1).size();
    while (!classes.isEmpty() && classes.last().first >= indentation)
      classes.removeLast();
    if (match.captured(2) == "class") {
      classes.append({indentation, match.captured(3)});
    } else if (match.captured(3) == names.last()) {
      QStringList owner;
      for (const auto &entry : classes)
        owner << entry.second;
      if (owner == names.mid(0, names.size() - 1))
        return i + 1;
    }
  }
  return 0;
}

inline QString existingFile(const QString &candidate, const QString &workspace,
                            const QString &workingDirectory = {}) {
  if (candidate.isEmpty())
    return {};
  QStringList paths;
  if (QFileInfo(candidate).isAbsolute())
    paths << candidate;
  else {
    if (!workingDirectory.isEmpty())
      paths << QDir(workingDirectory).absoluteFilePath(candidate);
    if (!workspace.isEmpty())
      paths << QDir(workspace).absoluteFilePath(candidate);
  }
  for (const QString &path : paths)
    if (QFileInfo(path).isFile())
      return QFileInfo(path).absoluteFilePath();
  return {};
}
} // namespace TestSource
#endif
#endif
