#include "launchplanner.h"

#include "../dap/debugadapterregistry.h"
#include "../language/languagecatalog.h"
#include "../test_templates/testconfiguration.h"
#include "runtemplatemanager.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace {

const QLatin1String kCompoundPrefix("compound:");

QString tr(const char *text) {
  return QCoreApplication::translate("LaunchPlanner", text);
}

QString quoteForDisplay(const QString &value) {
  if (value.isEmpty()) {
    return QStringLiteral("''");
  }
  static const QString safe = QStringLiteral(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "_-./=:,+@%");
  for (const QChar ch : value) {
    if (!safe.contains(ch)) {
      QString escaped = value;
      escaped.replace(QLatin1Char('\''), QLatin1String("'\\''"));
      return QLatin1Char('\'') + escaped + QLatin1Char('\'');
    }
  }
  return value;
}

bool isCLike(const QString &languageId) {
  return languageId == QLatin1String("c") || languageId == QLatin1String("cpp");
}

// The build command used when a C or C++ file's run template has no debug
// recipe of its own (custom user templates, for example). C shares the "cpp"
// language id, so the extension decides the compiler.
QStringList defaultCompileCommand(const QString &filePath) {
  if (QFileInfo(filePath).suffix() == QLatin1String("c")) {
    return {"gcc",    "-std=c11", "-g",
            "-O0",    "-o",       "${fileDir}/${fileBasenameNoExt}",
            "${file}"};
  }
  return {"g++",    "-std=c++17", "-g",
          "-O0",    "-o",         "${fileDir}/${fileBasenameNoExt}",
          "${file}"};
}

QString expand(const QString &value, const QString &filePath,
               const FileTemplateAssignment &assignment) {
  return PythonProjectEnvironment::substituteVariables(
      value, DebugConfigurationManager::instance().workspaceFolder(), filePath,
      QFileInfo(filePath).absolutePath(),
      {assignment.pythonMode, assignment.pythonInterpreter,
       assignment.pythonVenvPath, assignment.pythonRequirementsFile});
}

QStringList expand(const QStringList &values, const QString &filePath,
                   const FileTemplateAssignment &assignment) {
  QStringList result;
  result.reserve(values.size());
  for (const QString &value : values) {
    result << expand(value, filePath, assignment);
  }
  return result;
}

LaunchBuildStep compileStep(const QStringList &recipe, const QString &filePath,
                            const FileTemplateAssignment &assignment) {
  LaunchBuildStep step;
  step.title =
      tr("Compiling %1 for debugging…").arg(QFileInfo(filePath).fileName());
  step.workingDirectory = QFileInfo(filePath).absolutePath();
  step.command = expand(recipe, filePath, assignment);
  step.command << expand(assignment.sourceFiles, filePath, assignment);
  step.command << expand(assignment.compilerFlags, filePath, assignment);
  return step;
}

void applyAssignment(DebugConfiguration &config, const RunTarget &target,
                     const FileTemplateAssignment &assignment) {
  config.args = expand(assignment.customArgs, target.filePath, assignment);
  if (!target.workingDirectory.isEmpty()) {
    config.cwd = target.workingDirectory;
  }
  for (auto it = target.environment.constBegin();
       it != target.environment.constEnd(); ++it) {
    config.env.insert(it.key(), it.value());
  }
  if (!assignment.pythonMode.isEmpty()) {
    config.pythonMode = assignment.pythonMode;
    config.pythonInterpreter = assignment.pythonInterpreter;
    config.pythonVenvPath = assignment.pythonVenvPath;
    config.pythonRequirementsFile = assignment.pythonRequirementsFile;
  }
}

DebugConfiguration adapterDefaultFor(const RunTarget &target) {
  return DebugConfigurationManager::instance().createQuickConfig(
      target.filePath, target.languageId);
}

QString noDebuggerMessage(const RunTarget &target) {
  const QString language =
      target.languageId.isEmpty()
          ? QFileInfo(target.filePath).suffix()
          : LanguageCatalog::displayName(target.languageId);
  if (language.isEmpty()) {
    return tr("No debugger is available for this file.");
  }
  return tr("No debugger is available for %1 files.").arg(language);
}

} // namespace

QString LaunchTarget::toKey() const {
  switch (kind) {
  case Kind::CurrentFile:
    return {};
  case Kind::Configuration:
    return name;
  case Kind::Compound:
    return kCompoundPrefix + name;
  }
  return {};
}

LaunchTarget LaunchTarget::fromKey(const QString &key) {
  const QString trimmed = key.trimmed();
  if (trimmed.isEmpty()) {
    return currentFile();
  }
  if (trimmed.startsWith(kCompoundPrefix)) {
    const QString name = trimmed.mid(kCompoundPrefix.size());
    return name.isEmpty() ? currentFile() : compound(name);
  }
  return configuration(trimmed);
}

FileDebugPlan LaunchPlanner::debugPlanForFile(const RunTarget &target) {
  FileDebugPlan plan;
  const QString filePath = target.filePath;
  const QString fileName = QFileInfo(filePath).fileName();
  RunTemplateManager &templates = RunTemplateManager::instance();
  const FileTemplateAssignment assignment =
      filePath.isEmpty() ? FileTemplateAssignment()
                         : templates.getAssignmentForFile(filePath);
  DebugConfigurationManager &configs = DebugConfigurationManager::instance();

  switch (target.kind) {
  case RunTarget::Kind::PythonBackend:
    plan.configuration = adapterDefaultFor(target);
    plan.configuration.args
        << expand(assignment.customArgs, filePath, assignment);
    for (auto it = target.environment.constBegin();
         it != target.environment.constEnd(); ++it) {
      plan.configuration.env.insert(it.key(), it.value());
    }
    break;
  case RunTarget::Kind::Pytest: {
    // pytest under the debugger, on the file or (workspace template) on the
    // whole suite, whatever the file is called.
    TestConfiguration pytest;
    pytest.name = QStringLiteral("pytest");
    pytest.command = QStringLiteral("${python}");
    pytest.args = {"-m", "pytest", "-v", "--tb=short"};
    pytest.outputFormat = QStringLiteral("pytest");
    pytest.workingDirectory = QStringLiteral("${workspaceFolder}");
    const QStringList targets =
        target.templateId == QLatin1String("python_pytest_workspace")
            ? QStringList()
            : QStringList{filePath};
    plan.configuration = configs.createPytestConfig(targets, pytest);
    if (!assignment.pythonMode.isEmpty()) {
      plan.configuration.pythonMode = assignment.pythonMode;
      plan.configuration.pythonInterpreter = assignment.pythonInterpreter;
      plan.configuration.pythonVenvPath = assignment.pythonVenvPath;
      plan.configuration.pythonRequirementsFile =
          assignment.pythonRequirementsFile;
    }
    break;
  }
  case RunTarget::Kind::CTest:
    plan.error = tr("This file runs as a test suite. Debug individual tests "
                    "from the Tests panel.");
    return plan;
  case RunTarget::Kind::CMakeExecutable: {
    auto adapter =
        DebugAdapterRegistry::instance().preferredAdapterForLanguage("cpp");
    if (!adapter) {
      plan.error = tr("No C/C++ debugger (GDB or LLDB) is registered.");
      return plan;
    }
    plan.configuration = DebugConfiguration::fromJson(
        adapter->createLaunchConfig(target.program, target.workingDirectory));
    plan.configuration.adapterId = adapter->config().id;
    plan.configuration.name =
        tr("Debug %1")
            .arg(target.cmakeTargetName.isEmpty() ? fileName
                                                  : target.cmakeTargetName);
    plan.configuration.program = target.program;
    plan.configuration.args = target.arguments;
    plan.configuration.cwd = target.workingDirectory;
    plan.configuration.env = target.environment;
    plan.requiresCMakeBuild = true;
    return plan;
  }
  case RunTarget::Kind::Template: {
    const RunTemplate runTemplate =
        templates.getTemplateById(target.templateId);
    QStringList buildRecipe;
    if (runTemplate.isDebuggable()) {
      QJsonObject recipe = runTemplate.debug;
      for (const QJsonValue &part : recipe.take("build").toArray()) {
        buildRecipe << part.toString();
      }
      plan.configuration = DebugConfiguration::fromJson(recipe);
      plan.configuration.name = tr("Debug %1").arg(fileName);
    } else {
      plan.configuration = adapterDefaultFor(target);
      if (isCLike(target.languageId) &&
          plan.configuration.type == QLatin1String("cppdbg")) {
        buildRecipe = defaultCompileCommand(filePath);
      }
    }
    if (plan.configuration.type.isEmpty()) {
      plan.error = noDebuggerMessage(target);
      return plan;
    }
    applyAssignment(plan.configuration, target, assignment);
    if (!buildRecipe.isEmpty()) {
      plan.buildSteps << compileStep(buildRecipe, filePath, assignment);
    }
    break;
  }
  case RunTarget::Kind::None:
    if (filePath.isEmpty()) {
      plan.error = target.unavailableReason.isEmpty()
                       ? tr("Open and save a file to debug it.")
                       : target.unavailableReason;
      return plan;
    }
    plan.configuration = adapterDefaultFor(target);
    if (plan.configuration.type.isEmpty()) {
      plan.error = target.unavailableReason.isEmpty()
                       ? noDebuggerMessage(target)
                       : target.unavailableReason;
      return plan;
    }
    if (isCLike(target.languageId) &&
        plan.configuration.type == QLatin1String("cppdbg")) {
      plan.buildSteps << compileStep(defaultCompileCommand(filePath), filePath,
                                     assignment);
    }
    break;
  }

  if (plan.configuration.type.isEmpty()) {
    plan.error = noDebuggerMessage(target);
    return plan;
  }
  plan.configuration = configs.resolveVariables(plan.configuration, filePath);
  return plan;
}

RunCommand LaunchPlanner::runCommandFor(const DebugConfiguration &resolved) {
  const auto adapters =
      DebugAdapterRegistry::instance().adaptersForConfiguration(resolved);
  if (!adapters.isEmpty() && adapters.first()) {
    return adapters.first()->runCommand(resolved);
  }

  // Configurations without a debugger ("type": "command", or a type no
  // adapter handles) simply run their program.
  RunCommand command;
  if (resolved.request.compare(QLatin1String("attach"), Qt::CaseInsensitive) ==
      0) {
    command.error = tr("Attach configurations connect to a running program; "
                       "use Debug.");
    return command;
  }
  command.program = resolved.program;
  command.arguments = resolved.args;
  if (command.program.isEmpty()) {
    command.error = tr("Set the program this configuration runs.");
  }
  return command;
}

QString
LaunchPlanner::debuggerNameFor(const DebugConfiguration &configuration) {
  const auto adapters =
      DebugAdapterRegistry::instance().adaptersForConfiguration(configuration);
  return adapters.isEmpty() || !adapters.first()
             ? QString()
             : adapters.first()->config().name;
}

QString LaunchPlanner::fileDebuggerName(const RunTarget &target,
                                        QString *reason) {
  DebugAdapterRegistry &registry = DebugAdapterRegistry::instance();
  auto firstName = [](const QList<std::shared_ptr<IDebugAdapter>> &adapters) {
    return adapters.isEmpty() || !adapters.first()
               ? QString()
               : adapters.first()->config().name;
  };
  QString name;
  switch (target.kind) {
  case RunTarget::Kind::CTest:
    if (reason) {
      *reason = tr("Test suites are debugged from the Tests panel.");
    }
    return {};
  case RunTarget::Kind::CMakeExecutable:
    name = firstName(registry.adaptersForLanguage("cpp"));
    break;
  case RunTarget::Kind::PythonBackend:
  case RunTarget::Kind::Pytest:
    name = firstName(registry.adaptersForType("debugpy"));
    break;
  case RunTarget::Kind::Template: {
    const RunTemplate runTemplate =
        RunTemplateManager::instance().getTemplateById(target.templateId);
    if (runTemplate.isDebuggable()) {
      name = firstName(
          registry.adaptersForType(runTemplate.debug.value("type").toString()));
      break;
    }
    [[fallthrough]];
  }
  case RunTarget::Kind::None:
    if (!target.filePath.isEmpty()) {
      name = firstName(registry.adaptersForFile(target.filePath));
    }
    break;
  }
  if (name.isEmpty() && reason) {
    *reason = target.filePath.isEmpty() ? tr("Open a file to debug it.")
                                        : noDebuggerMessage(target);
  }
  return name;
}

QString LaunchPlanner::commandLine(const QString &program,
                                   const QStringList &arguments) {
  if (program.isEmpty()) {
    return {};
  }
  QStringList parts;
  parts.reserve(arguments.size() + 1);
  parts << quoteForDisplay(program);
  for (const QString &argument : arguments) {
    parts << quoteForDisplay(argument);
  }
  return parts.join(QLatin1Char(' '));
}
