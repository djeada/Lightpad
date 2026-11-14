#include "debugconfiguration.h"
#include "../core/logging/logger.h"
#include "../language/languagecatalog.h"
#include "../python/pythonbackendproject.h"
#include "../test_templates/testconfiguration.h"
#include "debugadapterregistry.h"
#include "test_templates/pytestintegration.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QSet>
#include <algorithm>
#include <functional>

DebugConfigurationManager &DebugConfigurationManager::instance() {
  static DebugConfigurationManager instance;
  return instance;
}

DebugConfigurationManager::DebugConfigurationManager() : QObject(nullptr) {}

bool DebugConfigurationManager::loadFromFile(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_WARNING(
        QString("Failed to open debug configuration file: %1").arg(filePath));
    return false;
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    LOG_ERROR(QString("Failed to parse debug configuration: %1")
                  .arg(parseError.errorString()));
    return false;
  }

  QJsonObject root = doc.object();

  m_configurations.clear();
  m_compoundConfigurations.clear();

  QJsonArray cfgArray = root["configurations"].toArray();
  for (const auto &val : cfgArray) {
    DebugConfiguration cfg = DebugConfiguration::fromJson(val.toObject());
    if (!cfg.name.isEmpty()) {
      m_configurations[cfg.name] = cfg;
    }
  }

  QJsonArray compoundArray = root["compounds"].toArray();
  for (const auto &val : compoundArray) {
    CompoundDebugConfiguration cfg =
        CompoundDebugConfiguration::fromJson(val.toObject());
    if (!cfg.name.isEmpty()) {
      m_compoundConfigurations[cfg.name] = cfg;
    }
  }

  m_configFilePath = filePath;

  LOG_INFO(QString("Loaded %1 debug configurations from %2")
               .arg(m_configurations.size())
               .arg(filePath));

  emit configurationsLoaded();
  return true;
}

bool DebugConfigurationManager::saveToFile(const QString &filePath) {
  QJsonObject root;
  root["version"] = "0.2.0";

  QJsonArray cfgArray;
  for (const auto &cfg : m_configurations) {
    cfgArray.append(cfg.toJson());
  }
  root["configurations"] = cfgArray;

  if (!m_compoundConfigurations.isEmpty()) {
    QJsonArray compoundArray;
    for (const auto &cfg : m_compoundConfigurations) {
      compoundArray.append(cfg.toJson());
    }
    root["compounds"] = compoundArray;
  }

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(
        QString("Failed to save debug configuration file: %1").arg(filePath));
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));

  m_configFilePath = filePath;
  LOG_INFO(QString("Saved %1 debug configurations to %2")
               .arg(m_configurations.size())
               .arg(filePath));

  return true;
}

void DebugConfigurationManager::addConfiguration(
    const DebugConfiguration &config) {
  m_configurations[config.name] = config;
  emit configurationAdded(config.name);
}

void DebugConfigurationManager::removeConfiguration(const QString &name) {
  if (m_configurations.remove(name) > 0) {
    emit configurationRemoved(name);
  }
}

bool DebugConfigurationManager::updateConfiguration(
    const QString &name, const DebugConfiguration &config) {
  if (name != config.name && m_configurations.contains(config.name)) {
    LOG_WARNING(QString("Refusing to rename debug configuration '%1' to "
                        "existing name '%2'")
                    .arg(name, config.name));
    return false;
  }
  if (name != config.name && m_configurations.contains(name)) {
    m_configurations.remove(name);
  }
  m_configurations[config.name] = config;
  emit configurationChanged(config.name);
  return true;
}

void DebugConfigurationManager::replaceConfigurations(
    const QList<DebugConfiguration> &configurations) {
  m_configurations.clear();
  for (const DebugConfiguration &cfg : configurations) {
    if (!cfg.name.isEmpty()) {
      m_configurations[cfg.name] = cfg;
    }
  }
  emit configurationsLoaded();
}

QString DebugConfigurationManager::joinCommandLineArguments(
    const QStringList &arguments) {
  QStringList quoted;
  for (const QString &arg : arguments) {
    const bool needsQuotes =
        arg.contains(QLatin1Char('"')) ||
        std::any_of(arg.begin(), arg.end(),
                    [](const QChar &ch) { return ch.isSpace(); });
    if (!needsQuotes) {
      quoted.append(arg);
      continue;
    }
    QString escaped = arg;
    escaped.replace(QLatin1String("\""), QLatin1String("\"\"\""));
    quoted.append(QLatin1Char('"') + escaped + QLatin1Char('"'));
  }
  return quoted.join(QLatin1Char(' '));
}

QStringList DebugConfigurationManager::splitCommandLineArguments(
    const QString &commandLine) {
  return QProcess::splitCommand(commandLine);
}

DebugConfiguration
DebugConfigurationManager::configuration(const QString &name) const {
  return m_configurations.value(name);
}

QList<DebugConfiguration> DebugConfigurationManager::allConfigurations() const {
  return m_configurations.values();
}

QList<DebugConfiguration>
DebugConfigurationManager::configurationsForType(const QString &type) const {
  QList<DebugConfiguration> result;
  for (const auto &cfg : m_configurations) {
    if (cfg.type == type) {
      result.append(cfg);
    }
  }
  return result;
}

void DebugConfigurationManager::addCompoundConfiguration(
    const CompoundDebugConfiguration &config) {
  m_compoundConfigurations[config.name] = config;
}

QList<CompoundDebugConfiguration>
DebugConfigurationManager::allCompoundConfigurations() const {
  return m_compoundConfigurations.values();
}

void DebugConfigurationManager::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
}

DebugConfiguration
DebugConfigurationManager::resolveVariables(const DebugConfiguration &config,
                                            const QString &currentFile) const {
  DebugConfiguration resolved = config;
  const PythonEnvironmentPreference pythonPreference = {
      config.pythonMode, config.pythonInterpreter, config.pythonVenvPath,
      config.pythonRequirementsFile};
  const QString baseWorkingDirectory =
      currentFile.isEmpty() ? m_workspaceFolder
                            : QFileInfo(currentFile).absolutePath();
  const std::function<QJsonValue(const QJsonValue &)> substituteJsonValue =
      [this, &currentFile, &pythonPreference, &baseWorkingDirectory,
       &substituteJsonValue](const QJsonValue &value) -> QJsonValue {
    if (value.isString()) {
      return substituteVariable(value.toString(), currentFile, pythonPreference,
                                baseWorkingDirectory);
    }

    if (value.isArray()) {
      QJsonArray result;
      const QJsonArray input = value.toArray();
      for (const QJsonValue &entry : input) {
        result.append(substituteJsonValue(entry));
      }
      return result;
    }

    if (value.isObject()) {
      QJsonObject result;
      const QJsonObject input = value.toObject();
      for (auto it = input.begin(); it != input.end(); ++it) {
        result[it.key()] = substituteJsonValue(it.value());
      }
      return result;
    }

    return value;
  };

  resolved.cwd = substituteVariable(config.cwd, currentFile, pythonPreference,
                                    baseWorkingDirectory);
  resolved.program = substituteVariable(config.program, currentFile,
                                        pythonPreference, resolved.cwd);

  resolved.args.clear();
  for (const QString &arg : config.args) {
    resolved.args.append(
        substituteVariable(arg, currentFile, pythonPreference, resolved.cwd));
  }

  for (auto it = config.env.begin(); it != config.env.end(); ++it) {
    resolved.env[it.key()] = substituteVariable(it.value(), currentFile,
                                                pythonPreference, resolved.cwd);
  }

  for (auto it = config.adapterConfig.begin(); it != config.adapterConfig.end();
       ++it) {
    resolved.adapterConfig[it.key()] = substituteJsonValue(it.value());
  }

  resolved.pythonInterpreter = substituteVariable(
      config.pythonInterpreter, currentFile, pythonPreference, resolved.cwd);
  resolved.pythonVenvPath = substituteVariable(
      config.pythonVenvPath, currentFile, pythonPreference, resolved.cwd);
  resolved.pythonRequirementsFile =
      substituteVariable(config.pythonRequirementsFile, currentFile,
                         pythonPreference, resolved.cwd);

  return resolved;
}

QString DebugConfigurationManager::substituteVariable(
    const QString &value, const QString &currentFile,
    const PythonEnvironmentPreference &pythonPreference,
    const QString &workingDirectory) const {
  QString result = PythonProjectEnvironment::substituteVariables(
      value, m_workspaceFolder, currentFile, workingDirectory,
      pythonPreference);

  if (!currentFile.isEmpty()) {
    QFileInfo fi(currentFile);
    result.replace("${fileBasenameNoExtension}", fi.completeBaseName());
    result.replace("${fileDirname}", fi.absolutePath());
    result.replace("${fileExtname}",
                   fi.suffix().isEmpty() ? "" : "." + fi.suffix());

    if (!m_workspaceFolder.isEmpty() &&
        currentFile.startsWith(m_workspaceFolder)) {
      QString relative = currentFile.mid(m_workspaceFolder.length());
      if (relative.startsWith('/') || relative.startsWith('\\')) {
        relative = relative.mid(1);
      }
      result.replace("${relativeFile}", relative);
    }
  }

#ifdef Q_OS_WIN
  result.replace("${pathSeparator}", "\\");
#else
  result.replace("${pathSeparator}", "/");
#endif

  return result;
}

DebugConfiguration
DebugConfigurationManager::createQuickConfig(const QString &filePath,
                                             const QString &languageId) const {
  DebugConfiguration config;

  QString canonicalLanguageId = LanguageCatalog::normalize(languageId);
  const QString filename = QFileInfo(filePath).fileName();
  if (canonicalLanguageId == "py" &&
      (filename.startsWith("test_") || filename.endsWith("_test.py"))) {
    TestConfiguration pytest;
    pytest.name = "pytest";
    pytest.command = "${python}";
    pytest.args = {"-m", "pytest", "-v", "--tb=short"};
    pytest.outputFormat = "pytest";
    pytest.workingDirectory = "${workspaceFolder}";
    return createPytestConfig({filePath}, pytest);
  }

  auto adapter =
      DebugAdapterRegistry::instance().preferredAdapterForFile(filePath);
  if (!adapter) {
    adapter =
        canonicalLanguageId.isEmpty()
            ? nullptr
            : DebugAdapterRegistry::instance().preferredAdapterForLanguage(
                  canonicalLanguageId);
  }
  if (!adapter) {
    return config;
  }

  QJsonObject launchConfig =
      adapter->createLaunchConfig(filePath, QFileInfo(filePath).absolutePath());
  config = DebugConfiguration::fromJson(launchConfig);
  config.adapterId = adapter->config().id;

  QFileInfo fi(filePath);
  config.name = QString("Debug %1").arg(fi.fileName());

  const auto backend = PythonBackendProject::discover(m_workspaceFolder);
  if (canonicalLanguageId == "py" && backend.ownsFile(filePath)) {
    config.name = "Debug " + backend.name();
    config.program.clear();
    config.cwd = backend.workingDirectory;
    config.args = backend.serverArguments(true);
    config.env = backend.environment();
    config.adapterConfig["module"] = backend.module();
    config.adapterConfig["jinja"] = true;
    config.adapterConfig["subProcess"] = true;
    const auto preference = backend.pythonPreference();
    config.pythonMode = preference.mode;
    config.pythonInterpreter = preference.customInterpreter;
    config.pythonVenvPath = preference.venvPath;
    config.pythonRequirementsFile = preference.requirementsFile;
  }

  if (config.type == "cppdbg") {
    config.stopOnEntry = false;
    static const QSet<QString> sourceExtensions = {
        ".c",   ".cc", ".cpp", ".cxx", ".h",   ".hpp",
        ".hxx", ".rs", ".go",  ".f",   ".f90", ".s"};
    const QString extension = "." + fi.suffix().toLower();
    if (sourceExtensions.contains(extension)) {
      QString executablePath = fi.absolutePath() + "/" + fi.completeBaseName();
#ifdef Q_OS_WIN
      executablePath += ".exe";
#endif
      config.program = executablePath;
    }
  }

  return config;
}

QString DebugConfigurationManager::lightpadLaunchConfigPath() const {
  if (m_workspaceFolder.isEmpty()) {
    return QString();
  }
  return m_workspaceFolder + "/.lightpad/debug/launch.json";
}

DebugConfiguration DebugConfigurationManager::createPytestConfig(
    const QStringList &targets, const TestConfiguration &testConfig) const {
  DebugConfiguration config;
  config.name =
      targets.isEmpty() ? "Debug pytest" : "Debug " + targets.join(", ");
  config.adapterId = "python-debugpy";
  config.type = "debugpy";
  config.request = "launch";
  const auto backend = PythonBackendProject::discover(m_workspaceFolder);
  const auto preference = backend.pythonPreference();
  config.pythonMode = preference.mode;
  config.pythonInterpreter = preference.customInterpreter;
  config.pythonVenvPath = preference.venvPath;
  config.pythonRequirementsFile = preference.requirementsFile;
  const QString file =
      targets.isEmpty() ? QString() : targets.first().section("::", 0, 0);
  config.cwd = PythonProjectEnvironment::substituteVariables(
      testConfig.workingDirectory, m_workspaceFolder, file, m_workspaceFolder,
      preference);
  if (config.cwd.isEmpty())
    config.cwd = m_workspaceFolder;
  config.args = testConfig.args;
  if (config.args.size() >= 2 && config.args.at(0) == "-m" &&
      config.args.at(1) == "pytest")
    config.args = config.args.mid(2);
  for (QString &arg : config.args)
    arg = PythonProjectEnvironment::substituteVariables(
        arg, m_workspaceFolder, file, config.cwd, preference);
  // Coverage/capture options from pytest.ini and PYTEST_ADDOPTS can hide output
  // or replace the debugger's trace function. Debug runs use explicit options.
  config.args << "-s" << "-o" << "addopts=";
  config.args << targets;
  const auto python = PythonProjectEnvironment::resolve(
      preference, m_workspaceFolder, file, config.cwd);
  config.env = PythonProjectEnvironment::activationEnvironment(python);
  if (backend.isValid())
    config.env["PYTHONPATH"] = backend.environment().value("PYTHONPATH");
  for (auto it = testConfig.env.begin(); it != testConfig.env.end(); ++it)
    config.env[it.key()] = PythonProjectEnvironment::substituteVariables(
        it.value(), m_workspaceFolder, file, config.cwd, preference);
  config.env["PYTEST_ADDOPTS"] = "";
  config.env["PYTHONUNBUFFERED"] = "1";
  if (!targets.isEmpty())
    config.env["LIGHTPAD_PYTEST_SELECTION"] =
        PytestIntegration::selectionValue(targets);
  config.adapterConfig["module"] = "pytest";
  config.adapterConfig["justMyCode"] = false;
  config.adapterConfig["subProcess"] = true;
  return config;
}

bool DebugConfigurationManager::loadFromLightpadDir() {
  QString path = lightpadLaunchConfigPath();
  if (path.isEmpty()) {
    LOG_WARNING("Cannot load launch config: workspace folder not set");
    return false;
  }

  QDir dir(m_workspaceFolder + "/.lightpad/debug");
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  if (!QFile::exists(path)) {
    LOG_INFO("Creating default launch.json in .lightpad/debug/");

    QJsonObject root;
    root["version"] = "1.0.0";
    root["_comment"] = "Debug launch configurations. Edit this file to add "
                       "your own configurations.";
    root["configurations"] = QJsonArray();
    root["compounds"] = QJsonArray();

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
      QJsonDocument doc(root);
      file.write(doc.toJson(QJsonDocument::Indented));
    }
  }

  return loadFromFile(path);
}

bool DebugConfigurationManager::saveToLightpadDir() {
  QString path = lightpadLaunchConfigPath();
  if (path.isEmpty()) {
    LOG_WARNING("Cannot save launch config: workspace folder not set");
    return false;
  }

  QDir dir(m_workspaceFolder + "/.lightpad/debug");
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  return saveToFile(path);
}
