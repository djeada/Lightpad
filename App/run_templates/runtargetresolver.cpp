#include "runtargetresolver.h"

#include "../build/cmakeproject.h"
#include "../language/languagecatalog.h"
#include "runtemplatemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace {

QString quoteIfNeeded(const QString &value) {
  if (value.isEmpty()) {
    return QStringLiteral("\"\"");
  }
  if (!value.contains(QLatin1Char(' ')) && !value.contains(QLatin1Char('\t'))) {
    return value;
  }
  return QLatin1Char('"') + value + QLatin1Char('"');
}

bool isCMakeListsFile(const QString &filePath) {
  return QFileInfo(filePath).fileName().compare(
             QStringLiteral("CMakeLists.txt"), Qt::CaseInsensitive) == 0;
}

bool isHeaderFile(const QString &filePath) {
  static const QSet<QString> headerSuffixes = {
      QStringLiteral("h"),   QStringLiteral("hpp"), QStringLiteral("hh"),
      QStringLiteral("hxx"), QStringLiteral("h++"), QStringLiteral("inl"),
      QStringLiteral("ipp")};
  return headerSuffixes.contains(QFileInfo(filePath).suffix().toLower());
}

const CMakeTargetInfo *findTarget(const QList<CMakeTargetInfo> &targets,
                                  const QString &name) {
  if (name.isEmpty()) {
    return nullptr;
  }
  for (const CMakeTargetInfo &target : targets) {
    if (target.name == name) {
      return &target;
    }
  }
  return nullptr;
}

bool looksLikeStructuredTestRun(const QString &templateId,
                                const QString &commandLine) {
  if (templateId == QLatin1String("cpp_cmake_ctest") ||
      templateId == QLatin1String("cpp_make_test")) {
    return true;
  }
  return commandLine.contains(QLatin1String("ctest"), Qt::CaseInsensitive);
}

} // namespace

QString RunTarget::displayName() const {
  switch (kind) {
  case Kind::CMakeExecutable:
    if (!cmakeTargetName.isEmpty()) {
      return cmakeTargetName;
    }
    break;
  case Kind::None:
  case Kind::Template:
  case Kind::CTest:
    break;
  }

  if (!filePath.isEmpty()) {
    return QFileInfo(filePath).fileName();
  }
  return QString();
}

QString RunTarget::targetPath() const {
  if (kind == Kind::CMakeExecutable && !program.isEmpty()) {
    return program;
  }
  return filePath;
}

QString RunTarget::commandLine() const {
  if (program.isEmpty()) {
    return QString();
  }
  QStringList parts;
  parts.reserve(arguments.size() + 1);
  parts << quoteIfNeeded(program);
  for (const QString &argument : arguments) {
    parts << quoteIfNeeded(argument);
  }
  return parts.join(QLatin1Char(' '));
}

QString RunTarget::rationale() const {
  switch (kind) {
  case Kind::None:
    return unavailableReason;
  case Kind::Template:
    return templateName.isEmpty()
               ? QCoreApplication::translate("RunTarget",
                                             "Runs the open file directly.")
               : QCoreApplication::translate("RunTarget",
                                             "Run template \"%1\" for %2.")
                     .arg(templateName, QFileInfo(filePath).fileName());
  case Kind::CMakeExecutable:
    if (needsTargetChoice) {
      return QCoreApplication::translate(
                 "RunTarget",
                 "%1 declares several executables and none of them lists %2, "
                 "so Run asks which target to build.")
          .arg(QDir(cmakeRoot).dirName(), QFileInfo(filePath).fileName());
    }
    if (ownedByCMakeTarget) {
      return QCoreApplication::translate(
                 "RunTarget",
                 "%1 belongs to the CMake target \"%2\", so Run builds and "
                 "launches that binary instead of compiling the file alone.")
          .arg(QFileInfo(filePath).fileName(), cmakeTargetName);
    }
    return QCoreApplication::translate(
               "RunTarget",
               "No CMake target lists %1, so Run falls back to the chosen "
               "target \"%2\". Pick another one, or a run template, from the "
               "Run menu.")
        .arg(QFileInfo(filePath).fileName(), cmakeTargetName);
  case Kind::CTest:
    return QCoreApplication::translate(
               "RunTarget",
               "Run template \"%1\" is a test run, so results open in the "
               "Tests panel.")
        .arg(templateName.isEmpty() ? templateId : templateName);
  }
  return QString();
}

bool RunTargetResolver::isOverridableTemplate(const QString &templateId) {
  static const QSet<QString> overridable = {QString(),
                                            QStringLiteral("cpp_gcc"),
                                            QStringLiteral("cpp_clang"),
                                            QStringLiteral("cpp_debug"),
                                            QStringLiteral("c_gcc"),
                                            QStringLiteral("c_clang"),
                                            QStringLiteral("cmake_build")};
  return overridable.contains(templateId.trimmed());
}

QString RunTargetResolver::cmakeRootFor(const QString &filePath,
                                        const QString &projectRoot) {
  const QFileInfo fileInfo(filePath);

  if (isCMakeListsFile(filePath)) {
    return fileInfo.absolutePath();
  }

  const QString searchRoot =
      projectRoot.isEmpty() ? fileInfo.absolutePath() : projectRoot;
  QString outerRoot = CMakeProject::findProjectRoot(searchRoot);
  if (outerRoot.isEmpty()) {

    if (filePath.isEmpty()) {
      return QString();
    }
    outerRoot = CMakeProject::findProjectRoot(fileInfo.absolutePath());
  }
  if (outerRoot.isEmpty() || filePath.isEmpty()) {
    return outerRoot;
  }

  CMakeProject project;
  QDir dir = fileInfo.absoluteDir();
  while (true) {
    const QString candidate = dir.absolutePath();
    if (CMakeProject::isCMakeProject(candidate)) {
      const QList<CMakeTargetInfo> executables =
          project.parseExecutableTargets(candidate);
      if (!executables.isEmpty() &&
          !CMakeProject::targetForSource(executables, candidate, filePath)
               .isEmpty()) {
        return candidate;
      }
    }
    if (candidate == outerRoot || !dir.cdUp()) {
      break;
    }
  }

  return outerRoot;
}

RunTarget RunTargetResolver::resolve(const RunTargetContext &context) {
  RunTarget target;
  target.filePath = context.filePath.trimmed();
  target.languageId = LanguageCatalog::normalize(context.languageId);

  if (target.filePath.isEmpty()) {
    target.unavailableReason = QCoreApplication::translate(
        "RunTarget", "Open and save a file to run it.");
    return target;
  }

  RunTemplateManager &templates = RunTemplateManager::instance();
  if (templates.getAllTemplates().isEmpty()) {
    templates.loadTemplates();
  }

  target.templateId =
      templates.effectiveTemplateIdForFile(target.filePath, target.languageId);
  const RunTemplate runTemplate = templates.getTemplateById(target.templateId);
  target.templateName = runTemplate.name;

  const QPair<QString, QStringList> command =
      templates.buildCommand(target.filePath, target.languageId);
  target.program = command.first;
  target.arguments = command.second;
  target.workingDirectory =
      templates.getWorkingDirectory(target.filePath, target.languageId);
  target.environment =
      templates.getEnvironment(target.filePath, target.languageId);

  if (looksLikeStructuredTestRun(target.templateId, target.commandLine())) {
    target.kind = RunTarget::Kind::CTest;
    return target;
  }

  const bool cppLike = target.languageId == QLatin1String("cpp") ||
                       target.languageId == QLatin1String("c");
  const bool isCMakeLists = isCMakeListsFile(target.filePath);
  const bool isHeader = isHeaderFile(target.filePath);

  if ((cppLike || isCMakeLists) && isOverridableTemplate(target.templateId)) {
    const QString cmakeRoot =
        cmakeRootFor(target.filePath, context.projectRoot);
    if (!cmakeRoot.isEmpty()) {
      CMakeProject project;
      const QList<CMakeTargetInfo> executables =
          project.parseExecutableTargets(cmakeRoot);
      if (!executables.isEmpty()) {
        const CMakeTargetInfo *chosen = nullptr;

        const QString owner = CMakeProject::targetForSource(
            executables, cmakeRoot, target.filePath);
        if (!owner.isEmpty()) {
          chosen = findTarget(executables, owner);
        }

        if (!chosen) {
          chosen =
              findTarget(executables, context.preferredCMakeTarget.trimmed());
        }

        if (!chosen && executables.size() == 1 &&
            (isCMakeLists || CMakeProject::hasUnresolvedSources(executables))) {
          chosen = &executables.first();
        }

        if (chosen) {
          target.kind = RunTarget::Kind::CMakeExecutable;
          target.requiresBuild = true;
          target.cmakeRoot = cmakeRoot;
          target.cmakeBinaryDir = CMakeProject::resolveBinaryDirectory(
              cmakeRoot, context.cmakeBinaryDir);
          target.cmakeTargetName = chosen->name;
          target.ownedByCMakeTarget = !owner.isEmpty();
          target.program =
              CMakeProject::executablePathFor(*chosen, target.cmakeBinaryDir);
          target.arguments.clear();

          target.workingDirectory = QFileInfo(target.program).absolutePath();
          return target;
        }

        if (isCMakeLists || isHeader) {
          target.kind = RunTarget::Kind::CMakeExecutable;
          target.requiresBuild = true;
          target.needsTargetChoice = true;
          target.cmakeRoot = cmakeRoot;
          target.cmakeBinaryDir = CMakeProject::resolveBinaryDirectory(
              cmakeRoot, context.cmakeBinaryDir);
          target.program.clear();
          target.arguments.clear();
          target.workingDirectory = cmakeRoot;
          return target;
        }
      }
    }
  }

  if (isHeader && target.program.isEmpty()) {
    target.kind = RunTarget::Kind::None;
    target.unavailableReason = QCoreApplication::translate(
        "RunTarget", "A header cannot be run on its own.");
    return target;
  }

  if (target.program.isEmpty()) {
    const QString suffix = QFileInfo(target.filePath).suffix();
    target.kind = RunTarget::Kind::None;
    target.unavailableReason =
        suffix.isEmpty()
            ? QCoreApplication::translate(
                  "RunTarget", "No run template is configured for this file.")
            : QCoreApplication::translate(
                  "RunTarget", "No run template is configured for .%1 files.")
                  .arg(suffix);
    return target;
  }

  target.kind = RunTarget::Kind::Template;
  return target;
}
