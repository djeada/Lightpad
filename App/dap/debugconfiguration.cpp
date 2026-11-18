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
#include <QSaveFile>
#include <QSet>
#include <algorithm>
#include <functional>

DebugConfigurationManager &DebugConfigurationManager::instance() {
  static DebugConfigurationManager instance;
  return instance;
}

DebugConfigurationManager::DebugConfigurationManager() : QObject(nullptr) {}

namespace {
// The exact entries DebugSettings used to write into every new project.
QJsonObject legacySampleDocument() {
  static const QJsonObject document = QJsonDocument::fromJson(R"json({
    "configurations": [
      {"name": "Python: Current File", "adapterId": "python-debugpy",
       "type": "debugpy", "request": "launch", "program": "${file}",
       "console": "integratedTerminal", "cwd": "${workspaceFolder}",
       "_comment": "Example Python debug configuration"},
      {"name": "C++: GDB Launch", "adapterId": "cppdbg-gdb", "type": "cppdbg",
       "request": "launch", "program": "${workspaceFolder}/a.out", "args": [],
       "stopAtEntry": false, "cwd": "${workspaceFolder}", "environment": [],
       "externalConsole": false, "MIMode": "gdb", "miDebuggerPath": "gdb",
       "setupCommands": [{"description": "Enable pretty-printing for gdb",
                          "text": "-enable-pretty-printing",
                          "ignoreFailures": true}],
       "_comment": "Example C++ GDB debug configuration"},
      {"name": "Node.js: Current File", "adapterId": "node-debug",
       "type": "node", "request": "launch", "program": "${file}",
       "cwd": "${workspaceFolder}", "console": "integratedTerminal",
       "_comment": "Example Node.js debug configuration"},
      {"name": "Go: Current File", "adapterId": "go-delve", "type": "go",
       "request": "launch", "mode": "debug", "program": "${file}",
       "cwd": "${workspaceFolder}",
       "_comment": "Example Go debug configuration using Delve"},
      {"name": "Rust: Debug Binary", "adapterId": "rust-codelldb",
       "type": "lldb", "request": "launch",
       "program": "${workspaceFolder}/target/debug/${workspaceFolderBasename}",
       "cwd": "${workspaceFolder}", "args": [],
       "_comment": "Example Rust debug configuration using CodeLLDB"}
    ],
    "compounds": [
      {"name": "Server + Client", "configurations": ["Server", "Client"],
       "stopAll": true,
       "_comment": "Example compound configuration for debugging multiple targets"}
    ]
  })json")
                                          .object();
  return document;
}

// Compares through fromJson/toJson so samples that a previous version of the
// configuration dialog re-saved (dropping empty fields) still match.
bool containsSample(const QJsonArray &samples, const QJsonObject &object) {
  const QJsonObject normalized = DebugConfiguration::fromJson(object).toJson();
  for (const QJsonValue &sample : samples) {
    if (DebugConfiguration::fromJson(sample.toObject()).toJson() ==
        normalized) {
      return true;
    }
  }
  return false;
}
} // namespace

bool DebugConfigurationManager::isLegacySampleConfiguration(
    const QJsonObject &object) {
  return object.value("_comment").toString().startsWith("Example ") &&
         containsSample(
             legacySampleDocument().value("configurations").toArray(), object);
}

bool DebugConfigurationManager::isLegacySampleCompound(
    const QJsonObject &object) {
  const CompoundDebugConfiguration compound =
      CompoundDebugConfiguration::fromJson(object);
  const CompoundDebugConfiguration sample =
      CompoundDebugConfiguration::fromJson(legacySampleDocument()
                                               .value("compounds")
                                               .toArray()
                                               .first()
                                               .toObject());
  return compound.toJson() == sample.toJson();
}

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

  for (const auto &val : root["configurations"].toArray()) {
    const QJsonObject object = val.toObject();
    if (isLegacySampleConfiguration(object)) {
      continue;
    }
    DebugConfiguration cfg = DebugConfiguration::fromJson(object);
    if (!cfg.name.isEmpty() && indexOf(cfg.name) < 0) {
      m_configurations.append(cfg);
    }
  }

  for (const auto &val : root["compounds"].toArray()) {
    const QJsonObject object = val.toObject();
    if (isLegacySampleCompound(object) && indexOf("Server") < 0 &&
        indexOf("Client") < 0) {
      continue;
    }
    CompoundDebugConfiguration cfg =
        CompoundDebugConfiguration::fromJson(object);
    if (!cfg.name.isEmpty() && compoundIndexOf(cfg.name) < 0) {
      m_compoundConfigurations.append(cfg);
    }
  }

  m_configFilePath = filePath;

  LOG_INFO(QString("Loaded %1 debug configurations from %2")
               .arg(m_configurations.size())
               .arg(filePath));

  emit configurationsLoaded();
  emit configurationsChanged();
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

  const QFileInfo info(filePath);
  if (!QDir().mkpath(info.absolutePath())) {
    LOG_ERROR(QString("Failed to create %1").arg(info.absolutePath()));
    return false;
  }

  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(
        QString("Failed to save debug configuration file: %1").arg(filePath));
    return false;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  if (!file.commit()) {
    LOG_ERROR(
        QString("Failed to save debug configuration file: %1").arg(filePath));
    return false;
  }

  m_configFilePath = filePath;
  LOG_INFO(QString("Saved %1 debug configurations to %2")
               .arg(m_configurations.size())
               .arg(filePath));

  return true;
}

int DebugConfigurationManager::indexOf(const QString &name) const {
  for (int i = 0; i < m_configurations.size(); ++i) {
    if (m_configurations.at(i).name == name) {
      return i;
    }
  }
  return -1;
}

int DebugConfigurationManager::compoundIndexOf(const QString &name) const {
  for (int i = 0; i < m_compoundConfigurations.size(); ++i) {
    if (m_compoundConfigurations.at(i).name == name) {
      return i;
    }
  }
  return -1;
}

void DebugConfigurationManager::addConfiguration(
    const DebugConfiguration &config) {
  const int index = indexOf(config.name);
  if (index >= 0) {
    m_configurations[index] = config;
  } else {
    m_configurations.append(config);
  }
  emit configurationAdded(config.name);
  emit configurationsChanged();
}

void DebugConfigurationManager::removeConfiguration(const QString &name) {
  const int index = indexOf(name);
  if (index < 0) {
    return;
  }
  m_configurations.removeAt(index);
  emit configurationRemoved(name);
  emit configurationsChanged();
}

bool DebugConfigurationManager::updateConfiguration(
    const QString &name, const DebugConfiguration &config) {
  if (name != config.name && indexOf(config.name) >= 0) {
    LOG_WARNING(QString("Refusing to rename debug configuration '%1' to "
                        "existing name '%2'")
                    .arg(name, config.name));
    return false;
  }
  const int index = indexOf(name);
  if (index >= 0) {
    m_configurations[index] = config;
  } else {
    m_configurations.append(config);
  }
  emit configurationChanged(config.name);
  emit configurationsChanged();
  return true;
}

void DebugConfigurationManager::replaceConfigurations(
    const QList<DebugConfiguration> &configurations) {
  m_configurations.clear();
  for (const DebugConfiguration &cfg : configurations) {
    if (!cfg.name.isEmpty() && indexOf(cfg.name) < 0) {
      m_configurations.append(cfg);
    }
  }
  emit configurationsLoaded();
  emit configurationsChanged();
}

QString DebugConfigurationManager::joinCommandLineArguments(
    const QStringList &arguments) {
  QStringList quoted;
  for (const QString &arg : arguments) {
    const bool needsQuotes =
        arg.isEmpty() || arg.contains(QLatin1Char('"')) ||
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
  const int index = indexOf(name);
  return index >= 0 ? m_configurations.at(index) : DebugConfiguration();
}

bool DebugConfigurationManager::hasConfiguration(const QString &name) const {
  return indexOf(name) >= 0;
}

QList<DebugConfiguration> DebugConfigurationManager::allConfigurations() const {
  return m_configurations;
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
  const int index = compoundIndexOf(config.name);
  if (index >= 0) {
    m_compoundConfigurations[index] = config;
  } else {
    m_compoundConfigurations.append(config);
  }
  emit configurationsChanged();
}

void DebugConfigurationManager::replaceCompoundConfigurations(
    const QList<CompoundDebugConfiguration> &compounds) {
  m_compoundConfigurations.clear();
  for (const CompoundDebugConfiguration &compound : compounds) {
    if (!compound.name.isEmpty() && compoundIndexOf(compound.name) < 0) {
      m_compoundConfigurations.append(compound);
    }
  }
  emit configurationsChanged();
}

CompoundDebugConfiguration
DebugConfigurationManager::compoundConfiguration(const QString &name) const {
  const int index = compoundIndexOf(name);
  return index >= 0 ? m_compoundConfigurations.at(index)
                    : CompoundDebugConfiguration();
}

bool DebugConfigurationManager::hasCompoundConfiguration(
    const QString &name) const {
  return compoundIndexOf(name) >= 0;
}

QList<CompoundDebugConfiguration>
DebugConfigurationManager::allCompoundConfigurations() const {
  return m_compoundConfigurations;
}

QString DebugConfigurationManager::uniqueName(const QString &baseName) const {
  const QString base =
      baseName.trimmed().isEmpty() ? tr("New Configuration") : baseName;
  QString candidate = base;
  int suffix = 2;
  while (indexOf(candidate) >= 0 || compoundIndexOf(candidate) >= 0) {
    candidate = QString("%1 (%2)").arg(base).arg(suffix++);
  }
  return candidate;
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

  resolved.preLaunchTask = substituteVariable(config.preLaunchTask, currentFile,
                                              pythonPreference, resolved.cwd);
  resolved.postDebugTask = substituteVariable(config.postDebugTask, currentFile,
                                              pythonPreference, resolved.cwd);
  resolved.host = substituteVariable(config.host, currentFile, pythonPreference,
                                     resolved.cwd);
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
  return PythonProjectEnvironment::substituteVariables(
      value, m_workspaceFolder, currentFile, workingDirectory,
      pythonPreference);
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
  const QString path = lightpadLaunchConfigPath();
  if (path.isEmpty()) {
    LOG_WARNING("Cannot load launch config: workspace folder not set");
    return false;
  }

  if (!QFile::exists(path)) {
    m_configurations.clear();
    m_compoundConfigurations.clear();
    m_configFilePath = path;
    emit configurationsLoaded();
    emit configurationsChanged();
    return true;
  }

  return loadFromFile(path);
}

bool DebugConfigurationManager::saveToLightpadDir() {
  const QString path = lightpadLaunchConfigPath();
  if (path.isEmpty()) {
    LOG_WARNING("Cannot save launch config: workspace folder not set");
    return false;
  }
  return saveToFile(path);
}
