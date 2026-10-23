#ifndef PROJECTROOT_H
#define PROJECTROOT_H

#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace ProjectRoot {

// Find the nearest matching project without escaping the open workspace or a
// repository boundary. Nested packages must use their own tools and settings.
inline QString find(const QString &filePath, const QStringList &markers,
                    const QString &workspaceFolder = {}) {
  if (filePath.isEmpty() || markers.isEmpty()) {
    return {};
  }
  QDir dir = QFileInfo(filePath).absoluteDir();
  const QString workspace = workspaceFolder.isEmpty()
                                ? QString()
                                : QDir(workspaceFolder).absolutePath();
  const QString relative = QDir(workspace).relativeFilePath(dir.absolutePath());
  const bool insideWorkspace = !workspace.isEmpty() &&
                               relative != QLatin1String("..") &&
                               !relative.startsWith(QLatin1String("../")) &&
                               !QDir::isAbsolutePath(relative);

  do {
    for (const QString &marker : markers) {
      if (QFileInfo(dir.filePath(marker)).isFile()) {
        return dir.absolutePath();
      }
    }
    if ((insideWorkspace && dir.absolutePath() == workspace) ||
        QFileInfo::exists(dir.filePath(".git"))) {
      break;
    }
  } while (dir.cdUp());

  return {};
}

} // namespace ProjectRoot

#endif
