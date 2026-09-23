#include "lspdefinitionprovider.h"
#include "../core/logging/logger.h"
#include "../diagnostics/diagnosticutils.h"
#include <QFileInfo>
#include <QUrl>

LspDefinitionProvider::LspDefinitionProvider(LspClient *client, QObject *parent)
    : IDefinitionProvider(parent), m_client(client), m_nextRequestId(1),
      m_activeProviderRequestId(0) {
  if (m_client) {
    connect(m_client, &LspClient::definitionReceived, this,
            [this](int lspRequestId, const QList<LspLocation> &locations) {
              if (m_activeProviderRequestId == 0 ||
                  lspRequestId != m_activeLspRequestId) {
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
            });
    connect(m_client, &LspClient::requestFailed, this,
            [this](int lspRequestId, const QString &, const QString &message) {
              if (m_activeProviderRequestId == 0 ||
                  lspRequestId != m_activeLspRequestId) {
                return;
              }
              int providerRequestId = m_activeProviderRequestId;
              m_activeProviderRequestId = 0;
              m_activeLspRequestId = 0;
              emit definitionFailed(providerRequestId, message);
            });
  }
}

QString LspDefinitionProvider::id() const { return "lsp"; }

bool LspDefinitionProvider::supports(const QString &languageId) const {
  if (!m_client || !m_client->isReady()) {
    return false;
  }
  Q_UNUSED(languageId);
  return true;
}

int LspDefinitionProvider::requestDefinition(const DefinitionRequest &req) {
  int providerRequestId = m_nextRequestId++;

  if (!m_client || !m_client->isReady()) {
    QMetaObject::invokeMethod(
        this,
        [this, providerRequestId]() {
          emit definitionFailed(providerRequestId,
                                tr("LSP server is not ready"));
        },
        Qt::QueuedConnection);
    return providerRequestId;
  }

  m_activeProviderRequestId = providerRequestId;

  QString uri = filePathToUri(req.filePath);
  LspPosition position;
  position.line = req.line - 1;
  position.character = req.column;

  m_activeLspRequestId = m_client->requestDefinition(uri, position);

  return providerRequestId;
}

QString LspDefinitionProvider::filePathToUri(const QString &filePath) {
  return DiagnosticUtils::filePathToUri(filePath);
}

QString LspDefinitionProvider::uriToFilePath(const QString &uri) {
  if (uri.startsWith("file://")) {
    return DiagnosticUtils::uriToFilePath(uri);
  }
  return uri;
}
