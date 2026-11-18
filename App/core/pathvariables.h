#ifndef PATHVARIABLES_H
#define PATHVARIABLES_H

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QMap>
#include <QPair>
#include <QProcessEnvironment>
#include <QString>

// The ${name} variables shared by run templates, launch configurations, test
// configurations and format templates. Both Lightpad's original spellings
// (${fileDir}, ${fileBasenameNoExt}) and VS Code's (${fileDirname},
// ${fileBasenameNoExtension}) are accepted, so a launch.json copied from VS
// Code resolves the same way as one written for Lightpad.
namespace PathVariables {

inline QString relativeTo(const QString &root, const QString &path) {
  if (root.isEmpty() || path.isEmpty()) {
    return path;
  }
  const QString relative = QDir(root).relativeFilePath(path);
  return relative.startsWith(QLatin1String("..")) ? path : relative;
}

// Variables describing a file and the workspace it belongs to. Values are
// empty when there is no file, so callers can tell "no file" from "unknown
// variable" (which is left untouched).
inline QMap<QString, QString> forContext(const QString &workspaceFolder,
                                         const QString &filePath) {
  QMap<QString, QString> vars;
  const QFileInfo fileInfo(filePath);
  const bool hasFile = !filePath.trimmed().isEmpty();
  const QString dir = hasFile ? fileInfo.absoluteDir().path() : QString();
  const QString suffix = hasFile ? fileInfo.suffix() : QString();

  vars.insert(QStringLiteral("file"), filePath);
  vars.insert(QStringLiteral("fileDir"), dir);
  vars.insert(QStringLiteral("fileDirname"), dir);
  vars.insert(QStringLiteral("fileBasename"),
              hasFile ? fileInfo.fileName() : QString());
  vars.insert(QStringLiteral("fileBasenameNoExt"),
              hasFile ? fileInfo.completeBaseName() : QString());
  vars.insert(QStringLiteral("fileBasenameNoExtension"),
              hasFile ? fileInfo.completeBaseName() : QString());
  vars.insert(QStringLiteral("fileExt"), suffix);
  vars.insert(QStringLiteral("fileExtname"),
              suffix.isEmpty() ? QString() : QLatin1Char('.') + suffix);
  vars.insert(QStringLiteral("relativeFile"),
              hasFile ? relativeTo(workspaceFolder, filePath) : QString());
  vars.insert(QStringLiteral("relativeFileDirname"),
              hasFile ? relativeTo(workspaceFolder, dir) : QString());
  vars.insert(QStringLiteral("workspaceFolder"), workspaceFolder);
  vars.insert(QStringLiteral("workspaceFolderBasename"),
              workspaceFolder.isEmpty() ? QString()
                                        : QDir(workspaceFolder).dirName());
  vars.insert(QStringLiteral("userHome"), QDir::homePath());
  vars.insert(QStringLiteral("pathSeparator"), QString(QDir::separator()));
  return vars;
}

// Replaces ${env:NAME} with the value of NAME in Lightpad's environment.
inline QString expandEnvironment(const QString &input) {
  static const QLatin1String prefix("${env:");
  if (!input.contains(prefix)) {
    return input;
  }
  const QProcessEnvironment environment =
      QProcessEnvironment::systemEnvironment();
  QString result;
  result.reserve(input.size());
  int position = 0;
  while (true) {
    const int start = input.indexOf(prefix, position);
    const int end =
        start < 0 ? -1 : input.indexOf(QLatin1Char('}'), start + prefix.size());
    if (start < 0 || end < 0) {
      result += input.mid(position);
      break;
    }
    result += input.mid(position, start - position);
    const QString name =
        input.mid(start + prefix.size(), end - start - int(prefix.size()));
    result += environment.value(name);
    position = end + 1;
  }
  return result;
}

inline QString expand(const QString &input,
                      const QMap<QString, QString> &variables) {
  if (!input.contains(QLatin1String("${"))) {
    return input;
  }
  QString result = input;
  for (auto it = variables.constBegin(); it != variables.constEnd(); ++it) {
    result.replace(QLatin1String("${") + it.key() + QLatin1Char('}'),
                   it.value());
  }
  return expandEnvironment(result);
}

// True when the value refers to the file open in the editor, which a launch
// started without an open file cannot provide.
inline bool needsFile(const QString &value) {
  static const char *const names[] = {
      "${file}",        "${fileDir}", "${fileDirname}",
      "${fileBasename", "${fileExt",  "${relativeFile",
  };
  for (const char *name : names) {
    if (value.contains(QLatin1String(name))) {
      return true;
    }
  }
  return false;
}

// Name/description pairs for the variable reference shown in the UI.
inline QList<QPair<QString, QString>> documentation() {
  return {
      {QStringLiteral("${file}"), QStringLiteral("Path of the current file")},
      {QStringLiteral("${fileDir}"),
       QStringLiteral("Folder of the current file (${fileDirname})")},
      {QStringLiteral("${fileBasename}"),
       QStringLiteral("File name with extension")},
      {QStringLiteral("${fileBasenameNoExt}"),
       QStringLiteral("File name without extension "
                      "(${fileBasenameNoExtension})")},
      {QStringLiteral("${fileExt}"),
       QStringLiteral("Extension without the dot (${fileExtname} has it)")},
      {QStringLiteral("${relativeFile}"),
       QStringLiteral("Current file relative to the workspace")},
      {QStringLiteral("${workspaceFolder}"),
       QStringLiteral("Root folder of the project")},
      {QStringLiteral("${workspaceFolderBasename}"),
       QStringLiteral("Name of the project folder")},
      {QStringLiteral("${projectRoot}"),
       QStringLiteral("Nearest folder with the template's project marker")},
      {QStringLiteral("${python}"),
       QStringLiteral("Interpreter of the selected Python environment")},
      {QStringLiteral("${venv}"),
       QStringLiteral("Selected virtual environment folder")},
      {QStringLiteral("${env:NAME}"),
       QStringLiteral("Value of environment variable NAME")},
      {QStringLiteral("${userHome}"), QStringLiteral("Your home folder")},
  };
}

} // namespace PathVariables

#endif
