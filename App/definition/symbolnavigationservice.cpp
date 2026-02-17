#include "symbolnavigationservice.h"
#include "../core/logging/logger.h"

SymbolNavigationService::SymbolNavigationService(QObject *parent)
    : QObject(parent), m_activeRequestId(0) {
  m_timeoutTimer.setSingleShot(true);
  connect(&m_timeoutTimer, &QTimer::timeout, this, [this]() {
    if (m_activeRequestId != 0) {
      m_activeRequestId = 0;
      emit noDefinitionFound(tr("Definition request timed out"));
      emit definitionRequestFinished();
    }
  });
}

SymbolNavigationService::~SymbolNavigationService() { cancelPendingRequest(); }

void SymbolNavigationService::registerProvider(IDefinitionProvider *provider) {
  if (provider && !m_providers.contains(provider)) {
    m_providers.append(provider);

    connect(provider, &IDefinitionProvider::definitionReady, this,
            [this](int requestId, QList<DefinitionTarget> targets) {
              if (requestId != m_activeRequestId) {
                return;
              }
              m_activeRequestId = 0;
              m_timeoutTimer.stop();
              emit definitionRequestFinished();

              if (targets.isEmpty()) {
                emit noDefinitionFound(tr("No definition found"));
              } else {
                emit definitionFound(targets);
              }
            });

    connect(provider, &IDefinitionProvider::definitionFailed, this,
            [this](int requestId, QString error) {
              if (requestId != m_activeRequestId) {
                return;
              }
              m_activeRequestId = 0;
              m_timeoutTimer.stop();
              emit noDefinitionFound(error);
              emit definitionRequestFinished();
            });
  }
}

void SymbolNavigationService::goToDefinition(const DefinitionRequest &req) {
  if (m_activeRequestId != 0) {
    return;
  }

  IDefinitionProvider *provider = findProvider(req.languageId);
  if (!provider) {
    emit noDefinitionFound(
        tr("No definition provider available for language: %1")
            .arg(req.languageId));
    return;
  }

  emit definitionRequestStarted();

  m_activeRequestId = provider->requestDefinition(req);
  m_timeoutTimer.start(REQUEST_TIMEOUT_MS);
}

bool SymbolNavigationService::isRequestInFlight() const {
  return m_activeRequestId != 0;
}

void SymbolNavigationService::cancelPendingRequest() {
  if (m_activeRequestId != 0) {
    m_activeRequestId = 0;
    m_timeoutTimer.stop();
    emit definitionRequestFinished();
  }
}

IDefinitionProvider *
SymbolNavigationService::findProvider(const QString &languageId) const {
  for (IDefinitionProvider *provider : m_providers) {
    if (provider->supports(languageId)) {
      return provider;
    }
  }
  return nullptr;
}
