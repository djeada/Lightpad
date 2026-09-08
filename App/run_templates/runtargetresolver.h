#ifndef RUNTARGETRESOLVER_H
#define RUNTARGETRESOLVER_H

#include <QMap>
#include <QString>
#include <QStringList>

struct RunTarget {
  enum class Kind {
    None,
    Template,
    CMakeExecutable,
    CTest,
  };

  Kind kind = Kind::None;

  QString filePath;
  QString languageId;

  QString program;
  QStringList arguments;
  QString workingDirectory;
  QMap<QString, QString> environment;

  QString templateId;
  QString templateName;

  QString cmakeRoot;
  QString cmakeBinaryDir;
  QString cmakeTargetName;

  bool ownedByCMakeTarget = false;

  bool needsTargetChoice = false;

  bool requiresBuild = false;

  QString unavailableReason;

  bool isValid() const { return kind != Kind::None; }

  QString displayName() const;

  QString targetPath() const;

  QString commandLine() const;

  QString rationale() const;
};

struct RunTargetContext {
  QString filePath;
  QString languageId;
  QString projectRoot;

  QString cmakeBinaryDir;

  QString preferredCMakeTarget;
};

class RunTargetResolver {
public:
  static RunTarget resolve(const RunTargetContext &context);

  static bool isOverridableTemplate(const QString &templateId);

  static QString cmakeRootFor(const QString &filePath,
                              const QString &projectRoot);
};

#endif
