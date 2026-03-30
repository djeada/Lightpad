#include "languagefeaturemanager.h"
#include "../core/logging/logger.h"
#include "../diagnostics/diagnosticutils.h"
#include "../settings/settingsmanager.h"

#include <QDir>
#include <QFileInfo>

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
      client->stop();
    }
  }
  qDeleteAll(m_clients);
  m_clients.clear();
}

QList<DiagnosticsServerConfig> LanguageFeatureManager::defaultServerConfigs() {
  return {
      {"cpp", "clangd", {"--background-index"}},
      {"py", "pylsp", {}},
      {"rust", "rust-analyzer", {}},
      {"go", "gopls", {"serve"}},
  };
}

void LanguageFeatureManager::openDocument(const QString &filePath,
                                          const QString &languageId,
                                          const QString &text) {
  QString effectiveLang =
      languageId.isEmpty() ? resolveLanguageId(filePath) : languageId;

  if (effectiveLang.isEmpty()) {
    LOG_DEBUG(
        QString("No language resolved for %1, skipping LSP").arg(filePath));
    return;
  }

  if (!isLanguageSupported(effectiveLang)) {
    const QString message =
        QString("No language server configured for '%1'.").arg(effectiveLang);
    LOG_WARNING(message);
    m_lastServerErrors[effectiveLang] = message;
    emit serverError(effectiveLang, message);
    return;
  }

  LspClient *client = ensureClient(effectiveLang);
  if (!client) {
    return;
  }

  QString projectRoot = detectProjectRoot(filePath);
  if (!projectRoot.isEmpty()) {
    QString rootUri = DiagnosticUtils::filePathToUri(projectRoot);
    client->setRootUri(rootUri);
  }

  m_fileToLanguage[filePath] = effectiveLang;
  m_fileVersions[filePath] = 1;

  QString uri = DiagnosticUtils::filePathToUri(filePath);

  if (m_diagnosticsManager) {
    m_diagnosticsManager->trackDocumentVersion(uri, 1);
  }

  if (client->isReady()) {
    client->didOpen(uri, effectiveLang, 1, text);
    LOG_DEBUG(QString("didOpen sent for %1 [%2]").arg(filePath, effectiveLang));
  } else {
    LOG_DEBUG(QString("Client for '%1' not ready, didOpen deferred for %2")
                  .arg(effectiveLang, filePath));
  }
}

void LanguageFeatureManager::changeDocument(const QString &filePath,
                                            int version, const QString &text) {
  if (!m_fileToLanguage.contains(filePath)) {
    return;
  }

  QString languageId = m_fileToLanguage.value(filePath);
  LspClient *client = m_clients.value(languageId);
  if (!client || !client->isReady()) {
    return;
  }

  m_fileVersions[filePath] = version;
  QString uri = DiagnosticUtils::filePathToUri(filePath);

  if (m_diagnosticsManager) {
    m_diagnosticsManager->trackDocumentVersion(uri, version);
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
  LspClient *client = m_clients.value(languageId);
  if (!client || !client->isReady()) {
    return;
  }

  QString uri = DiagnosticUtils::filePathToUri(filePath);
  client->didSave(uri);
  LOG_DEBUG(QString("didSave sent for %1").arg(filePath));
}

void LanguageFeatureManager::closeDocument(const QString &filePath) {
  if (!m_fileToLanguage.contains(filePath)) {
    return;
  }

  QString languageId = m_fileToLanguage.value(filePath);
  LspClient *client = m_clients.value(languageId);

  QString uri = DiagnosticUtils::filePathToUri(filePath);

  if (client && client->isReady()) {
    client->didClose(uri);
    LOG_DEBUG(QString("didClose sent for %1").arg(filePath));
  }

  if (m_diagnosticsManager) {
    m_diagnosticsManager->clearDiagnostics(uri);
  }

  m_fileToLanguage.remove(filePath);
  m_fileVersions.remove(filePath);
}

LspClient *
LanguageFeatureManager::clientForFile(const QString &filePath) const {
  if (!m_fileToLanguage.contains(filePath)) {
    return nullptr;
  }
  QString languageId = m_fileToLanguage.value(filePath);
  return m_clients.value(languageId, nullptr);
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

LspClient *LanguageFeatureManager::ensureClient(const QString &languageId) {
  if (m_clients.contains(languageId)) {
    return m_clients.value(languageId);
  }

  DiagnosticsServerConfig config = configForLanguage(languageId);
  if (config.command.isEmpty() || !config.enabled) {
    QString reason = config.command.isEmpty()
                         ? QString("No language server configured for '%1'. "
                                   "Install the appropriate server and update "
                                   "settings.")
                               .arg(languageId)
                         : QString("Language server for '%1' is disabled in "
                                   "settings.")
                               .arg(languageId);
    LOG_WARNING(reason);
    m_lastServerErrors[languageId] = reason;
    emit serverError(languageId, reason);
    m_serverHealth[languageId] = ServerHealthStatus::Stopped;
    emit serverHealthChanged(languageId, ServerHealthStatus::Stopped);
    return nullptr;
  }

  m_serverHealth[languageId] = ServerHealthStatus::Starting;
  emit serverHealthChanged(languageId, ServerHealthStatus::Starting);

  auto *client = new LspClient(this);

  connect(client, &LspClient::diagnosticsReceived, this,
          [this, languageId](const QString &uri,
                             const QList<LspDiagnostic> &diagnostics) {
            onDiagnosticsReceived(languageId, uri, diagnostics);
          });

  connect(client, &LspClient::initialized, this, [this, languageId]() {
    LOG_INFO(QString("Language server for '%1' initialized").arg(languageId));
    m_serverHealth[languageId] = ServerHealthStatus::Running;
    m_lastServerErrors.remove(languageId);
    emit serverHealthChanged(languageId, ServerHealthStatus::Running);
    emit serverStarted(languageId);
  });

  connect(client, &LspClient::error, this,
          [this, languageId](const QString &message) {
            LOG_ERROR(QString("Language server error for '%1': %2")
                          .arg(languageId, message));
            m_serverHealth[languageId] = ServerHealthStatus::Error;
            m_lastServerErrors[languageId] = message;
            emit serverHealthChanged(languageId, ServerHealthStatus::Error);
            emit serverError(languageId, message);
          });

  bool started = client->start(config.command, config.arguments);
  if (!started) {
    const QString message =
        QString("Failed to start '%1'. Is it installed and on PATH?")
            .arg(config.command);
    LOG_WARNING(QString("Failed to start language server '%1' (%2). "
                        "Make sure '%3' is installed and available on PATH.")
                    .arg(languageId, config.command, config.command));
    m_lastServerErrors[languageId] = message;
    emit serverError(languageId, message);
    m_serverHealth[languageId] = ServerHealthStatus::Error;
    emit serverHealthChanged(languageId, ServerHealthStatus::Error);
    delete client;
    return nullptr;
  }

  m_clients[languageId] = client;
  LOG_INFO(QString("Started language server for '%1' (%2)")
               .arg(languageId, config.command));
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
    const QList<LspDiagnostic> &diagnostics) {
  if (!m_diagnosticsManager)
    return;

  QString sourceId = QString("lsp:%1").arg(languageId);
  int version = m_diagnosticsManager->documentVersion(uri);
  m_diagnosticsManager->upsertDiagnostics(uri, diagnostics, sourceId, version);
}

QString LanguageFeatureManager::detectProjectRoot(const QString &filePath) {
  static const QStringList projectMarkers = {
      "Cargo.toml",           // Rust
      "go.mod",               // Go
      "pyproject.toml",       // Python
      "requirements.txt",     // Python
      "setup.py",             // Python
      "CMakeLists.txt",       // C/C++
      "compile_commands.json" // C/C++
  };

  QDir dir = QFileInfo(filePath).absoluteDir();
  QString prevPath;

  while (dir.absolutePath() != prevPath) {
    for (const QString &marker : projectMarkers) {
      if (QFileInfo::exists(dir.filePath(marker))) {
        return dir.absolutePath();
      }
    }
    prevPath = dir.absolutePath();
    dir.cdUp();
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

QString LanguageFeatureManager::lastServerError(const QString &languageId) const {
  return m_lastServerErrors.value(languageId);
}

void LanguageFeatureManager::restartServer(const QString &languageId) {
  LspClient *client = m_clients.take(languageId);
  if (client) {
    client->stop();
    client->deleteLater();
  }
  m_serverHealth[languageId] = ServerHealthStatus::Unknown;
  emit serverHealthChanged(languageId, ServerHealthStatus::Unknown);
}

void LanguageFeatureManager::loadSettingsOverrides() {
  m_serverConfigs = defaultServerConfigs();
  SettingsManager &settings = SettingsManager::instance();

  for (DiagnosticsServerConfig &cfg : m_serverConfigs) {
    QString prefix = QString("languageServers.%1").arg(cfg.languageId);

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
