#include "languagefeaturemanager.h"
#include "../core/logging/logger.h"
#include "../diagnostics/diagnosticutils.h"

LanguageFeatureManager::LanguageFeatureManager(
    DiagnosticsManager *diagnosticsManager, QObject *parent)
    : QObject(parent), m_diagnosticsManager(diagnosticsManager),
      m_serverConfigs(defaultServerConfigs()) {}

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
      {"cpp", "clangd", {}},
      {"py", "pylsp", {}},
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
    LOG_DEBUG(QString("Language '%1' has no configured server, skipping LSP")
                  .arg(effectiveLang));
    return;
  }

  LspClient *client = ensureClient(effectiveLang);
  if (!client) {
    return;
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
                                            int version,
                                            const QString &text) {
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

LspClient *LanguageFeatureManager::clientForFile(const QString &filePath) const {
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
  if (config.command.isEmpty()) {
    LOG_WARNING(
        QString("No server command configured for language '%1'")
            .arg(languageId));
    emit serverError(languageId,
                     QString("No language server configured for '%1'. "
                             "Install the appropriate server and update "
                             "settings.")
                         .arg(languageId));
    return nullptr;
  }

  auto *client = new LspClient(this);

  connect(client, &LspClient::diagnosticsReceived, this,
          [this, languageId](const QString &uri,
                             const QList<LspDiagnostic> &diagnostics) {
            onDiagnosticsReceived(languageId, uri, diagnostics);
          });

  connect(client, &LspClient::initialized, this,
          [this, languageId]() {
            LOG_INFO(QString("Language server for '%1' initialized")
                         .arg(languageId));
            emit serverStarted(languageId);
          });

  connect(client, &LspClient::error, this,
          [this, languageId](const QString &message) {
            LOG_ERROR(QString("Language server error for '%1': %2")
                          .arg(languageId, message));
            emit serverError(languageId, message);
          });

  bool started = client->start(config.command, config.arguments);
  if (!started) {
    LOG_WARNING(
        QString("Failed to start language server '%1' (%2). "
                "Make sure '%3' is installed and available on PATH.")
            .arg(languageId, config.command, config.command));
    emit serverError(
        languageId,
        QString("Failed to start '%1'. Is it installed?").arg(config.command));
    delete client;
    return nullptr;
  }

  m_clients[languageId] = client;
  LOG_INFO(
      QString("Started language server for '%1' (%2)")
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
