#ifndef CMAKEPROJECT_H
#define CMAKEPROJECT_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

struct CMakeTargetInfo {
  QString name;
  bool isExecutable = false;
  // Relative source directory of the CMakeLists that declared this target
  // (empty for the top-level file). Binaries land under the matching
  // subdirectory of the build tree.
  QString relativeDir;
  QStringList sources;
};

// Lightweight CMake project support for the debug launch flow:
// detection, executable-target discovery and configure/build execution.
class CMakeProject : public QObject {
  Q_OBJECT

public:
  explicit CMakeProject(QObject *parent = nullptr);

  static bool isCMakeProject(const QString &projectRoot);

  static QString defaultBinaryDir(const QString &projectRoot);

  static QString findProjectRoot(const QString &startPath);

  // Parses add_executable()/add_library() declarations from the top-level
  // CMakeLists.txt plus direct add_subdirectory() children.
  QList<CMakeTargetInfo> parseTargets(const QString &projectRoot) const;

  QList<CMakeTargetInfo>
  parseExecutableTargets(const QString &projectRoot) const;

  static QString executablePathFor(const CMakeTargetInfo &target,
                                   const QString &binaryDir);

  // Name of the target that lists `sourcePath` among its sources, so a launch
  // started from an open file picks the executable that file belongs to.
  // Empty when no target claims the file.
  static QString targetForSource(const QList<CMakeTargetInfo> &targets,
                                 const QString &projectRoot,
                                 const QString &sourcePath);

  // True when any target lists sources this parser cannot expand (a CMake
  // variable or a glob). Ownership answers are only trustworthy as a negative
  // - "no target claims this file" - when every source list was resolvable.
  static bool hasUnresolvedSources(const QList<CMakeTargetInfo> &targets);

  // Runs `cmake -S <root> -B <binaryDir>` when the binary dir is not yet
  // configured; returns true when no configure step is needed.
  bool configure(const QString &projectRoot, const QString &binaryDir,
                 QString *output, QString *errorMessage) const;

  bool build(const QString &binaryDir, int jobs, QString *output,
             QString *errorMessage) const;

  static QString resolveBinaryDirectory(const QString &projectRoot,
                                        const QString &configuredPath);

  // Low-level helpers so the UI can drive the same steps with its own
  // progress/cancel handling.
  static bool needsConfigure(const QString &binaryDir);
  static QStringList configureCommand(const QString &projectRoot,
                                      const QString &binaryDir);
  static QStringList buildCommand(const QString &binaryDir, int jobs);

  // Builds a single target instead of everything the project declares, so
  // running one file does not wait on every unrelated test binary.
  static QStringList buildCommand(const QString &binaryDir, int jobs,
                                  const QString &targetName);
};

#endif
