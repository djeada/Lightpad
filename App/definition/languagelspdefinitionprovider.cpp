#include "languagelspdefinitionprovider.h"
#include "../core/logging/logger.h"
#include "../diagnostics/diagnosticutils.h"
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

LanguageLspDefinitionProvider::LanguageLspDefinitionProvider(
    const LanguageServerConfig &config, QObject *parent)
    : IDefinitionProvider(parent), m_config(config), m_nextRequestId(1),
      m_activeProviderRequestId(0), m_activeLspRequestId(0) {}

LanguageLspDefinitionProvider::~LanguageLspDefinitionProvider() = default;

QString LanguageLspDefinitionProvider::id() const {
  return m_config.providerId;
}

bool LanguageLspDefinitionProvider::supports(const QString &languageId) const {
  return m_config.supportedLanguages.contains(languageId, Qt::CaseInsensitive);
}

void LanguageLspDefinitionProvider::setClientResolver(ClientResolver resolver) {
  m_clientResolver = std::move(resolver);
}

int LanguageLspDefinitionProvider::requestDefinition(
    const DefinitionRequest &req) {
  int providerRequestId = m_nextRequestId++;

  LspClient *client =
      m_clientResolver ? m_clientResolver(req.filePath) : nullptr;

  if (!client) {
    failLater(providerRequestId,
              tr("Language server '%1' is not available. Install '%2' to "
                 "enable Go to Definition for %3.")
                  .arg(m_config.displayName)
                  .arg(m_config.serverCommand)
                  .arg(m_config.supportedLanguages.join(", ")));
    return providerRequestId;
  }

  if (!client->isReady()) {
    failLater(providerRequestId,
              tr("Language server '%1' is still starting. Try again in a "
                 "moment.")
                  .arg(m_config.displayName));
    return providerRequestId;
  }

  attachClient(client);

  LspPosition position;
  position.line = req.line - 1;
  position.character = req.column;

  m_activeProviderRequestId = providerRequestId;
  m_activeLspRequestId =
      client->requestDefinition(filePathToUri(req.filePath), position);

  return providerRequestId;
}

void LanguageLspDefinitionProvider::attachClient(LspClient *client) {
  if (m_activeClient == client) {
    return;
  }

  if (m_activeClient) {
    disconnect(m_activeClient, nullptr, this, nullptr);
  }

  m_activeClient = client;
  connect(client, &LspClient::definitionReceived, this,
          &LanguageLspDefinitionProvider::onDefinitionReceived);
  connect(client, &LspClient::requestFailed, this,
          &LanguageLspDefinitionProvider::onRequestFailed);
}

void LanguageLspDefinitionProvider::onDefinitionReceived(
    int lspRequestId, const QList<LspLocation> &locations) {
  if (m_activeProviderRequestId == 0 || lspRequestId != m_activeLspRequestId ||
      sender() != m_activeClient.data()) {
    return;
  }

  int providerRequestId = m_activeProviderRequestId;
  m_activeProviderRequestId = 0;
  m_activeLspRequestId = 0;

  QList<DefinitionTarget> targets;
  for (const LspLocation &loc : locations) {
    DefinitionTarget target;
    target.filePath = uriToFilePath(loc.uri);
    target.line = loc.range.start.line + 1;
    target.column = loc.range.start.character;
    targets.append(target);
  }
  emit definitionReady(providerRequestId, targets);
}

void LanguageLspDefinitionProvider::onRequestFailed(int lspRequestId,
                                                    const QString &method,
                                                    const QString &message) {
  Q_UNUSED(method);
  if (m_activeProviderRequestId == 0 || lspRequestId != m_activeLspRequestId ||
      sender() != m_activeClient.data()) {
    return;
  }

  int providerRequestId = m_activeProviderRequestId;
  m_activeProviderRequestId = 0;
  m_activeLspRequestId = 0;
  emit definitionFailed(providerRequestId, message);
}

void LanguageLspDefinitionProvider::failLater(int providerRequestId,
                                              const QString &message) {
  QMetaObject::invokeMethod(
      this,
      [this, providerRequestId, message]() {
        emit definitionFailed(providerRequestId, message);
      },
      Qt::QueuedConnection);
}

bool LanguageLspDefinitionProvider::isServerAvailable() const {
  QString path = QStandardPaths::findExecutable(m_config.serverCommand);
  return !path.isEmpty();
}

QString LanguageLspDefinitionProvider::serverCommand() const {
  return m_config.serverCommand;
}

QStringList LanguageLspDefinitionProvider::supportedLanguages() const {
  return m_config.supportedLanguages;
}

QString LanguageLspDefinitionProvider::filePathToUri(const QString &filePath) {
  return DiagnosticUtils::filePathToUri(filePath);
}

QString LanguageLspDefinitionProvider::uriToFilePath(const QString &uri) {
  if (uri.startsWith("file://")) {
    return DiagnosticUtils::uriToFilePath(uri);
  }
  return uri;
}

QList<LanguageServerConfig> LanguageLspDefinitionProvider::defaultConfigs() {
  return {
      {"clangd",
       "clangd (C/C++)",
       {"cpp", "c"},
       "clangd",
       {"--background-index"}},

      {"pylsp", "Python Language Server", {"py"}, "pylsp", {}},

      {"rust-analyzer", "rust-analyzer", {"rust"}, "rust-analyzer", {}},

      {"gopls", "gopls (Go)", {"go"}, "gopls", {"serve"}},

      {"typescript-language-server",
       "TypeScript Language Server",
       {"ts", "js"},
       "typescript-language-server",
       {"--stdio"}},

      {"jdtls", "Eclipse JDT Language Server (Java)", {"java"}, "jdtls", {}},
  };
}
