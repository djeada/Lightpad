#include "languagelspdefinitionprovider.h"
#include "../core/logging/logger.h"
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

LanguageLspDefinitionProvider::LanguageLspDefinitionProvider(
    const LanguageServerConfig &config, QObject *parent)
    : IDefinitionProvider(parent), m_config(config), m_client(nullptr),
      m_nextRequestId(1), m_activeProviderRequestId(0),
      m_serverStartAttempted(false) {}

LanguageLspDefinitionProvider::~LanguageLspDefinitionProvider() {
  if (m_client) {
    m_client->stop();
  }
}

QString LanguageLspDefinitionProvider::id() const {
  return m_config.providerId;
}

bool LanguageLspDefinitionProvider::supports(const QString &languageId) const {
  return m_config.supportedLanguages.contains(languageId, Qt::CaseInsensitive);
}

int LanguageLspDefinitionProvider::requestDefinition(
    const DefinitionRequest &req) {
  int providerRequestId = m_nextRequestId++;

  if (!ensureServerStarted() || !m_client || !m_client->isReady()) {
    QMetaObject::invokeMethod(
        this,
        [this, providerRequestId]() {
          emit definitionFailed(
              providerRequestId,
              tr("Language server '%1' is not available. Install '%2' to "
                 "enable Go to Definition for %3.")
                  .arg(m_config.displayName)
                  .arg(m_config.serverCommand)
                  .arg(m_config.supportedLanguages.join(", ")));
        },
        Qt::QueuedConnection);
    return providerRequestId;
  }

  m_activeProviderRequestId = providerRequestId;

  QString uri = filePathToUri(req.filePath);
  LspPosition position;
  position.line = req.line - 1;
  position.character = req.column;

  m_client->requestDefinition(uri, position);

  return providerRequestId;
}

bool LanguageLspDefinitionProvider::isServerAvailable() const {
  QString path =
      QStandardPaths::findExecutable(m_config.serverCommand);
  return !path.isEmpty();
}

QString LanguageLspDefinitionProvider::serverCommand() const {
  return m_config.serverCommand;
}

QStringList LanguageLspDefinitionProvider::supportedLanguages() const {
  return m_config.supportedLanguages;
}

bool LanguageLspDefinitionProvider::ensureServerStarted() {
  if (m_client && m_client->isReady()) {
    return true;
  }

  if (m_serverStartAttempted) {
    return m_client && m_client->isReady();
  }

  m_serverStartAttempted = true;

  if (!isServerAvailable()) {
    LOG_INFO(QString("Language server '%1' (%2) not found in PATH")
                 .arg(m_config.displayName)
                 .arg(m_config.serverCommand));
    return false;
  }

  m_client = new LspClient(this);

  connect(m_client, &LspClient::definitionReceived, this,
          [this](int lspRequestId, const QList<LspLocation> &locations) {
            Q_UNUSED(lspRequestId);
            if (m_activeProviderRequestId == 0) {
              return;
            }

            int providerRequestId = m_activeProviderRequestId;
            m_activeProviderRequestId = 0;

            QList<DefinitionTarget> targets;
            for (const LspLocation &loc : locations) {
              DefinitionTarget target;
              target.filePath = uriToFilePath(loc.uri);
              target.line = loc.range.start.line + 1;
              target.column = loc.range.start.character;
              targets.append(target);
            }
            emit definitionReady(providerRequestId, targets);
          });

  connect(m_client, &LspClient::error, this,
          [this](const QString &message) {
            if (m_activeProviderRequestId != 0) {
              int providerRequestId = m_activeProviderRequestId;
              m_activeProviderRequestId = 0;
              emit definitionFailed(providerRequestId, message);
            }
          });

  bool started =
      m_client->start(m_config.serverCommand, m_config.serverArguments);
  if (!started) {
    LOG_WARNING(QString("Failed to start language server '%1' (%2)")
                    .arg(m_config.displayName)
                    .arg(m_config.serverCommand));
    return false;
  }

  LOG_INFO(QString("Started language server '%1' (%2) for languages: %3")
               .arg(m_config.displayName)
               .arg(m_config.serverCommand)
               .arg(m_config.supportedLanguages.join(", ")));

  return true;
}

QString LanguageLspDefinitionProvider::filePathToUri(const QString &filePath) {
  return QUrl::fromLocalFile(filePath).toString();
}

QString LanguageLspDefinitionProvider::uriToFilePath(const QString &uri) {
  if (uri.startsWith("file://")) {
    return QUrl(uri).toLocalFile();
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

      {"pylsp",
       "Python Language Server",
       {"py"},
       "pylsp",
       {}},

      {"rust-analyzer",
       "rust-analyzer",
       {"rust"},
       "rust-analyzer",
       {}},

      {"gopls",
       "gopls (Go)",
       {"go"},
       "gopls",
       {"serve"}},

      {"typescript-language-server",
       "TypeScript Language Server",
       {"ts", "js"},
       "typescript-language-server",
       {"--stdio"}},

      {"jdtls",
       "Eclipse JDT Language Server (Java)",
       {"java"},
       "jdtls",
       {}},
  };
}
