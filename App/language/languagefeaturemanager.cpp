#include "languagefeaturemanager.h"
#include "../core/logging/logger.h"
#include "../diagnostics/diagnosticutils.h"
#include "../settings/settingsmanager.h"
#include "projectroot.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
QString protocolLanguageId(const QString &languageId, const QString &filePath) {
  const QString suffix = QFileInfo(filePath).suffix().toLower();
  if (languageId == "py") {
    return "python";
  }
  if (languageId == "cpp" && suffix == "c") {
    return "c";
  }
  if (languageId == "js") {
    return suffix == "jsx" ? "javascriptreact" : "javascript";
  }
  if (languageId == "ts") {
    return suffix == "tsx" ? "typescriptreact" : "typescript";
  }
  return languageId;
}
} // namespace

LanguageFeatureManager::LanguageFeatureManager(
    DiagnosticsManager *diagnosticsManager, QObject *parent)
    : QObject(parent), m_diagnosticsManager(diagnosticsManager),
      m_serverConfigs(defaultServerConfigs()) {
  loadSettingsOverrides();
}

LanguageFeatureManager::~LanguageFeatureManager() {
  for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
    LspClient *client = it.value();
    if (client) {
      client->disconnect(this);
      client->stop();
    }
  }
  qDeleteAll(m_clients);
  m_clients.clear();
}

QList<DiagnosticsServerConfig> LanguageFeatureManager::defaultServerConfigs() {
  QList<DiagnosticsServerConfig> configs{
      {"cpp", "clangd", {"--background-index"}},
      {"c", "clangd", {"--background-index"}},
      {"py", "basedpyright-langserver", {"--stdio"}},
      {"rust", "rust-analyzer", {}},
      {"go", "gopls", {"serve"}},
      {"js", "typescript-language-server", {"--stdio"}},
      {"ts", "typescript-language-server", {"--stdio"}},
      {"dockerfile", "docker-langserver", {"--stdio"}},
      {"java", "jdtls", {}},
  };
  configs.last().initializationOptions =
      QJsonObject{{"extendedClientCapabilities",
                   QJsonObject{{"classFileContentsSupport", true}}}};
  return configs;
}

void LanguageFeatureManager::openDocument(const QString &filePath,
                                          const QString &languageId,
                                          const QString &text) {
  QString effectiveLang = languageId.isEmpty()
                              ? resolveLanguageId(filePath)
                              : LanguageCatalog::normalize(languageId);
  if (effectiveLang.isEmpty() && !languageId.isEmpty()) {
    effectiveLang = languageId;
  }

  if (effectiveLang.isEmpty()) {
    LOG_DEBUG(
        QString("No language resolved for %1, skipping LSP").arg(filePath));
    return;
  }

  if (!isLanguageSupported(effectiveLang)) {
    const QString message =
        QString("No language server configured for '%1'.").arg(effectiveLang);
    LOG_DEBUG(message);
    emit serverUnavailable(effectiveLang, message);
    return;
  }

  if (m_fileToLanguage.contains(filePath)) {
    detachDocument(filePath);
  }

  LspClient *client = ensureClient(effectiveLang, filePath);
  if (!client) {
    return;
  }

  m_fileToLanguage[filePath] = effectiveLang;
  m_fileVersions[filePath] = 1;
  m_fileTexts[filePath] = text;

  QString uri = documentUriForFile(filePath);

  if (m_diagnosticsManager) {
    m_diagnosticsManager->trackDocumentVersion(uri, 1);
  }

  if (client->isReady()) {
    client->didOpen(uri, protocolLanguageId(effectiveLang, filePath), 1, text);
    LOG_DEBUG(QString("didOpen sent for %1 [%2]").arg(filePath, effectiveLang));
  } else {
    queuePendingDocument(m_fileToClient.value(filePath), filePath);
    LOG_DEBUG(QString("Client for '%1' not ready, didOpen queued for %2")
                  .arg(effectiveLang, filePath));
  }
}

void LanguageFeatureManager::changeDocument(const QString &filePath,
                                            int version, const QString &text) {
  if (!m_fileToLanguage.contains(filePath)) {
    return;
  }

  QString languageId = m_fileToLanguage.value(filePath);
  LspClient *client = clientForFile(filePath);

  m_fileVersions[filePath] = version;
  m_fileTexts[filePath] = text;
  QString uri = documentUriForFile(filePath);

  if (m_diagnosticsManager) {
    m_diagnosticsManager->trackDocumentVersion(uri, version);
  }

  if (!client || !client->isReady() ||
      isPendingDocument(languageId, filePath)) {
    return;
  }

  client->didChange(uri, version, text);
  LOG_DEBUG(
      QString("didChange sent for %1 (version %2)").arg(filePath).arg(version));
}

void LanguageFeatureManager::saveDocument(const QString &filePath) {
  if (!m_fileToLanguage.contains(filePath)) {
    return;
  }

  QString languageId = m_fileToLanguage.value(filePath);
  LspClient *client = clientForFile(filePath);
  if (!client || !client->isReady() ||
      isPendingDocument(languageId, filePath)) {
    return;
  }

  QString uri = documentUriForFile(filePath);
  client->didSave(uri);
  LOG_DEBUG(QString("didSave sent for %1").arg(filePath));
}

void LanguageFeatureManager::closeDocument(const QString &filePath) {
  if (!m_fileToLanguage.contains(filePath)) {
    return;
  }

  QString uri = documentUriForFile(filePath);

  detachDocument(filePath);

  if (m_diagnosticsManager) {
    m_diagnosticsManager->clearDiagnostics(uri);
  }

  m_fileToLanguage.remove(filePath);
  m_fileToClient.remove(filePath);
  m_documentUris.remove(filePath);
  m_externalSources.remove(filePath);
  m_fileVersions.remove(filePath);
  m_fileTexts.remove(filePath);
}

QString
LanguageFeatureManager::documentUriForFile(const QString &filePath) const {
  return m_documentUris.value(filePath,
                              DiagnosticUtils::filePathToUri(filePath));
}

bool LanguageFeatureManager::isExternalDocument(const QString &filePath) const {
  return m_documentUris.contains(filePath);
}

void LanguageFeatureManager::openExternalDocument(const QString &filePath,
                                                  const QString &uri,
                                                  const QString &sourceFile,
                                                  const QString &text) {
  const QString origin = m_externalSources.value(sourceFile, sourceFile);
  const QString language =
      m_fileToLanguage.value(sourceFile, resolveLanguageId(origin));
  m_externalSources[filePath] = origin;
  m_documentUris[filePath] = uri;
  openDocument(filePath, language, text);
}

void LanguageFeatureManager::detachDocument(const QString &filePath) {
  const QString clientKey = m_fileToClient.value(filePath);
  if (m_pendingDocuments.contains(clientKey) &&
      m_pendingDocuments[clientKey].removeAll(filePath) > 0) {
    return;
  }

  LspClient *client = clientForFile(filePath);
  if (client && client->isReady()) {
    client->didClose(documentUriForFile(filePath));
    LOG_DEBUG(QString("didClose sent for %1").arg(filePath));
  }
}

LspClient *
LanguageFeatureManager::clientForFile(const QString &filePath) const {
  if (!m_fileToLanguage.contains(filePath)) {
    return nullptr;
  }
  return m_clients.value(m_fileToClient.value(filePath), nullptr);
}

QString
LanguageFeatureManager::resolveLanguageId(const QString &filePath,
                                          const QString &override) const {
  if (!override.isEmpty()) {
    QString normalized = LanguageCatalog::normalize(override);
    if (!normalized.isEmpty()) {
      return normalized;
    }
  }

  int dotIndex = filePath.lastIndexOf('.');
  if (dotIndex >= 0) {
    QString ext = filePath.mid(dotIndex + 1);
    QString lang = LanguageCatalog::languageForExtension(ext);
    if (!lang.isEmpty()) {
      return lang;
    }
  }

  return {};
}

bool LanguageFeatureManager::isLanguageSupported(
    const QString &languageId) const {
  for (const DiagnosticsServerConfig &cfg : m_serverConfigs) {
    if (cfg.languageId == languageId) {
      return true;
    }
  }
  return false;
}

QStringList LanguageFeatureManager::supportedLanguages() const {
  QStringList result;
  for (const DiagnosticsServerConfig &cfg : m_serverConfigs) {
    result.append(cfg.languageId);
  }
  return result;
}

LspClient *LanguageFeatureManager::ensureClient(const QString &languageId,
                                                const QString &filePath) {
  const DiagnosticsServerConfig config = configForLanguage(languageId);
  if (config.command.isEmpty() || !config.enabled) {
    const QString reason =
        config.command.isEmpty()
            ? tr("No language server configured for '%1'.").arg(languageId)
            : tr("Language server for '%1' is disabled in settings.")
                  .arg(languageId);
    m_lastServerErrors[languageId] = reason;
    m_serverHealth[languageId] = ServerHealthStatus::Stopped;
    emit serverError(languageId, reason);
    emit serverHealthChanged(languageId, ServerHealthStatus::Stopped);
    return nullptr;
  }

  const QString contextFile = m_externalSources.value(filePath, filePath);
  const QString root = detectProjectRoot(contextFile, m_workspaceFolder);
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  QJsonObject configuration = config.settings;
  PythonEnvironmentInfo python;
  if (languageId == "py") {
    const auto preference = m_pythonEnvironmentResolver
                                ? m_pythonEnvironmentResolver(contextFile)
                                : PythonEnvironmentPreference{};
    python = PythonProjectEnvironment::resolve(
        preference, m_workspaceFolder.isEmpty() ? root : m_workspaceFolder,
        contextFile, root);
    if (python.found) {
      const auto activation =
          PythonProjectEnvironment::activationEnvironment(python);
      for (auto it = activation.constBegin(); it != activation.constEnd();
           ++it) {
        environment.insert(it.key(), it.value());
      }
      QJsonObject pylsp = configuration.value("pylsp").toObject();
      QJsonObject plugins = pylsp.value("plugins").toObject();
      QJsonObject jedi = plugins.value("jedi").toObject();
      if (!jedi.contains("environment")) {
        jedi["environment"] = python.interpreter;
      }
      plugins["jedi"] = jedi;
      pylsp["plugins"] = plugins;
      configuration["pylsp"] = pylsp;
      QJsonObject pythonSettings = configuration.value("python").toObject();
      if (!pythonSettings.contains("pythonPath")) {
        pythonSettings["pythonPath"] = python.interpreter;
      }
      configuration["python"] = pythonSettings;
    }
  }

  const QString clientKey =
      languageId + '\n' + root + '\n' + python.interpreter;
  m_fileToClient[filePath] = clientKey;
  if (m_clients.contains(clientKey)) {
    return m_clients.value(clientKey);
  }

  const auto expand = [&](QString value) {
    value.replace("${workspaceFolder}", root);
    value.replace("${fileDir}", QFileInfo(contextFile).absolutePath());
    value.replace("${file}", contextFile);
    value.replace("${python}", python.interpreter);
    return value;
  };
  for (const QString &entry : config.environmentVariables) {
    const int separator = entry.indexOf('=');
    if (separator > 0) {
      environment.insert(entry.left(separator),
                         expand(entry.mid(separator + 1)));
    }
  }
  const QString nodeBin = QDir(root).filePath("node_modules/.bin");
  if ((languageId == "js" || languageId == "ts") && QDir(nodeBin).exists()) {
    environment.insert("PATH", nodeBin + QDir::listSeparator() +
                                   environment.value("PATH"));
  }
  QString command = expand(config.command);
  if (!QFileInfo(command).isAbsolute() &&
      (command.contains('/') || command.contains('\\'))) {
    command = QDir(root).absoluteFilePath(command);
  }
  QString resolvedCommand = QStandardPaths::findExecutable(
      command, environment.value("PATH").split(QDir::listSeparator(),
                                               Qt::SkipEmptyParts));
  bool usePylspFallback = false;
  if (languageId == "py" && resolvedCommand.isEmpty() &&
      config.command == "basedpyright-langserver" &&
      config.arguments == QStringList{"--stdio"}) {
    resolvedCommand = QStandardPaths::findExecutable(
        "pylsp", environment.value("PATH").split(QDir::listSeparator(),
                                                 Qt::SkipEmptyParts));
    usePylspFallback = !resolvedCommand.isEmpty();
  }
  if (!resolvedCommand.isEmpty()) {
    command = resolvedCommand;
  }
  QStringList arguments;
  for (const QString &argument : config.arguments) {
    arguments.append(expand(argument));
  }
  if (usePylspFallback) {
    arguments.clear();
  }
  if (languageId == "java" && QFileInfo(config.command).baseName() == "jdtls" &&
      !arguments.contains("-data")) {
    const QString cache =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(root.toUtf8(), QCryptographicHash::Sha256)
            .toHex());
    const QString data = QDir(cache).filePath("jdtls-workspaces/" + hash);
    if (!cache.isEmpty() && QDir().mkpath(data)) {
      arguments << "-data" << data;
    }
  }

  m_serverHealth[languageId] = ServerHealthStatus::Starting;
  emit serverHealthChanged(languageId, ServerHealthStatus::Starting);
  auto *client = new LspClient(this);
  client->setRootUri(DiagnosticUtils::filePathToUri(root));
  client->setConfiguration(configuration);
  client->setInitializationOptions(config.initializationOptions);
  connect(client, &LspClient::diagnosticsReceived, this,
          [this, languageId](const QString &uri,
                             const QList<LspDiagnostic> &diagnostics,
                             int version) {
            onDiagnosticsReceived(languageId, uri, diagnostics, version);
          });
  connect(client, &LspClient::stateChanged, this,
          [this, clientKey, client](LspClient::State state) {
            if (state == LspClient::State::Disconnected ||
                state == LspClient::State::Error) {
              onClientLost(clientKey, client);
            }
          });
  connect(client, &LspClient::initialized, this,
          [this, languageId, clientKey]() {
            m_serverHealth[languageId] = ServerHealthStatus::Running;
            m_lastServerErrors.remove(languageId);
            emit serverHealthChanged(languageId, ServerHealthStatus::Running);
            emit serverStarted(languageId);
            flushPendingDocuments(clientKey);
          });
  connect(client, &LspClient::error, this,
          [this, languageId](const QString &message) {
            m_serverHealth[languageId] = ServerHealthStatus::Error;
            m_lastServerErrors[languageId] = message;
            emit serverHealthChanged(languageId, ServerHealthStatus::Error);
            emit serverError(languageId, message);
          });
  if (!client->start(command, arguments, root, environment)) {
    delete client;
    return nullptr;
  }
  m_clients[clientKey] = client;
  m_clientLanguages[clientKey] = languageId;
  return client;
}

DiagnosticsServerConfig
LanguageFeatureManager::configForLanguage(const QString &languageId) const {
  for (const DiagnosticsServerConfig &cfg : m_serverConfigs) {
    if (cfg.languageId == languageId) {
      return cfg;
    }
  }
  return {};
}

void LanguageFeatureManager::onDiagnosticsReceived(
    const QString &languageId, const QString &uri,
    const QList<LspDiagnostic> &diagnostics, int version) {
  if (!m_diagnosticsManager)
    return;

  QString sourceId = QString("lsp:%1").arg(languageId);
  m_diagnosticsManager->upsertDiagnostics(DiagnosticUtils::normalizeUri(uri),
                                          diagnostics, sourceId, version);
}

void LanguageFeatureManager::onClientLost(const QString &clientKey,
                                          LspClient *client) {
  if (m_clients.value(clientKey) != client) {
    return;
  }
  const QString languageId = m_clientLanguages.value(clientKey);
  m_clients.remove(clientKey);
  client->disconnect(this);
  client->deleteLater();
  if (m_serverHealth.value(languageId) != ServerHealthStatus::Error) {
    const QString message =
        tr("Language server for '%1' exited unexpectedly.").arg(languageId);
    m_lastServerErrors[languageId] = message;
    m_serverHealth[languageId] = ServerHealthStatus::Error;
    emit serverHealthChanged(languageId, ServerHealthStatus::Error);
    emit serverError(languageId, message);
  }
  for (auto it = m_fileToClient.constBegin(); it != m_fileToClient.constEnd();
       ++it) {
    if (it.value() == clientKey) {
      queuePendingDocument(clientKey, it.key());
    }
  }
}

QString
LanguageFeatureManager::detectProjectRoot(const QString &filePath,
                                          const QString &workspaceFolder) {
  static const QStringList projectMarkers = {
      "Cargo.toml",       "go.mod",           "go.work",
      "package.json",     "tsconfig.json",    "jsconfig.json",
      "pyproject.toml",   "requirements.txt", "setup.py",
      "setup.cfg",        "Pipfile",          ".venv/pyvenv.cfg",
      "venv/pyvenv.cfg",  "pom.xml",          "build.gradle",
      "build.gradle.kts", "CMakeLists.txt",   "compile_commands.json",
      "latexmkrc",        ".latexmkrc"};

  const QString root =
      ProjectRoot::find(filePath, projectMarkers, workspaceFolder);
  if (!root.isEmpty()) {
    return root;
  }
  const QString relative = QDir(workspaceFolder).relativeFilePath(filePath);
  if (!workspaceFolder.isEmpty() && relative != ".." &&
      !relative.startsWith("../") && !QDir::isAbsolutePath(relative)) {
    return QDir(workspaceFolder).absolutePath();
  }
  return QFileInfo(filePath).absolutePath();
}

ServerHealthStatus
LanguageFeatureManager::serverHealth(const QString &languageId) const {
  return m_serverHealth.value(languageId, ServerHealthStatus::Unknown);
}

DiagnosticsServerConfig
LanguageFeatureManager::serverConfig(const QString &languageId) const {
  return configForLanguage(languageId);
}

QString
LanguageFeatureManager::lastServerError(const QString &languageId) const {
  return m_lastServerErrors.value(languageId);
}

void LanguageFeatureManager::flushPendingDocuments(const QString &clientKey) {
  if (!m_pendingDocuments.contains(clientKey)) {
    return;
  }

  LspClient *client = m_clients.value(clientKey);
  if (!client || !client->isReady()) {
    return;
  }

  const QStringList pending = m_pendingDocuments.take(clientKey);
  for (const QString &filePath : pending) {
    if (m_fileToClient.value(filePath) != clientKey) {
      continue;
    }
    QString uri = documentUriForFile(filePath);
    int version = m_fileVersions.value(filePath, 1);
    client->didOpen(
        uri, protocolLanguageId(m_fileToLanguage.value(filePath), filePath),
        version, m_fileTexts.value(filePath));
    LOG_DEBUG(
        QString("Flushed queued didOpen for %1 [%2]").arg(filePath, clientKey));
  }
}

void LanguageFeatureManager::queuePendingDocument(const QString &languageId,
                                                  const QString &filePath) {
  QStringList &pending = m_pendingDocuments[languageId];
  if (!pending.contains(filePath)) {
    pending.append(filePath);
  }
}

bool LanguageFeatureManager::isPendingDocument(const QString &languageId,
                                               const QString &filePath) const {
  Q_UNUSED(languageId);
  return m_pendingDocuments.value(m_fileToClient.value(filePath))
      .contains(filePath);
}

QStringList LanguageFeatureManager::trackedFilesForLanguage(
    const QString &languageId) const {
  QStringList files;
  for (auto it = m_fileToLanguage.constBegin();
       it != m_fileToLanguage.constEnd(); ++it) {
    if (it.value() == languageId) {
      files.append(it.key());
    }
  }
  return files;
}

void LanguageFeatureManager::restartServer(const QString &languageId) {
  for (const QString &key : m_clients.keys()) {
    if (m_clientLanguages.value(key) != languageId) {
      continue;
    }
    auto *client = m_clients.take(key);
    client->disconnect(this);
    client->stop();
    client->deleteLater();
    m_pendingDocuments.remove(key);
  }
  m_serverHealth[languageId] = ServerHealthStatus::Unknown;
  emit serverHealthChanged(languageId, ServerHealthStatus::Unknown);
  for (const QString &filePath : trackedFilesForLanguage(languageId)) {
    if (auto *client = ensureClient(languageId, filePath)) {
      const QString key = m_fileToClient.value(filePath);
      queuePendingDocument(key, filePath);
      if (client->isReady()) {
        flushPendingDocuments(key);
      }
    }
  }
}

void LanguageFeatureManager::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
}

void LanguageFeatureManager::setPythonEnvironmentResolver(
    std::function<PythonEnvironmentPreference(const QString &)> resolver) {
  m_pythonEnvironmentResolver = std::move(resolver);
}

void LanguageFeatureManager::setServerConfig(
    const DiagnosticsServerConfig &config) {
  for (DiagnosticsServerConfig &cfg : m_serverConfigs) {
    if (cfg.languageId == config.languageId) {
      cfg = config;
      return;
    }
  }
  m_serverConfigs.append(config);
}

void LanguageFeatureManager::loadSettingsOverrides() {
  m_serverConfigs = defaultServerConfigs();
  SettingsManager &settings = SettingsManager::instance();

  const QJsonObject servers =
      settings.getValue("languageServers").toJsonObject();
  for (const QString &key : servers.keys()) {
    const QString language = LanguageCatalog::normalize(key);
    const QString id = language.isEmpty() ? key : language;
    if (!isLanguageSupported(id)) {
      DiagnosticsServerConfig config;
      config.languageId = id;
      m_serverConfigs.append(config);
    }
  }
  for (DiagnosticsServerConfig &cfg : m_serverConfigs) {
    QString prefix = QString("languageServers.%1").arg(cfg.languageId);

    if (cfg.languageId == "c" && !servers.contains("c")) {
      prefix = "languageServers.cpp";
    }
    cfg.settings = settings.getValue(prefix + ".settings").toJsonObject();
    const auto initializationOptions =
        settings.getValue(prefix + ".initializationOptions");
    if (initializationOptions.isValid()) {
      cfg.initializationOptions = initializationOptions.toJsonObject();
    }
    QVariant cmdVal = settings.getValue(prefix + ".command");
    if (cmdVal.isValid() && !cmdVal.toString().isEmpty()) {
      cfg.command = cmdVal.toString();
    }

    QVariant argsVal = settings.getValue(prefix + ".arguments");
    if (argsVal.isValid()) {
      cfg.arguments = argsVal.toStringList();
    }

    QVariant envVal = settings.getValue(prefix + ".environment");
    if (envVal.isValid()) {
      cfg.environmentVariables = envVal.toStringList();
    }

    QVariant enabledVal = settings.getValue(prefix + ".enabled");
    if (enabledVal.isValid()) {
      cfg.enabled = enabledVal.toBool();
    }
  }
}
