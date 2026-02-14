#include "completionengine.h"
#include "../core/logging/logger.h"
#include "../language/languagecatalog.h"
#include <algorithm>

CompletionEngine::CompletionEngine(QObject *parent)
    : QObject(parent), m_debounceTimer(new QTimer(this)) {
  m_debounceTimer->setSingleShot(true);
  connect(m_debounceTimer, &QTimer::timeout, this,
          &CompletionEngine::onDebounceTimeout);
}

CompletionEngine::~CompletionEngine() { cancelPendingRequests(); }

void CompletionEngine::setLanguage(const QString &languageId) {
  m_languageId = LanguageCatalog::normalize(languageId);
}

void CompletionEngine::requestCompletions(const CompletionContext &context) {

  m_currentContext = context;

  if (context.isAutoComplete && context.prefix.length() < m_minPrefixLength) {
    cancelPendingRequests();
    emit completionsReady({});
    return;
  }

  if (context.isAutoComplete) {

    m_debounceTimer->start(m_autoTriggerDelay);
  } else {

    cancelPendingRequests();
    executeCompletionRequest();
  }
}

void CompletionEngine::executeCompletionRequest() {
  m_pendingItems.clear();
  m_currentRequestId++;

  QString langId =
      m_currentContext.languageId.isEmpty()
          ? m_languageId
          : LanguageCatalog::normalize(m_currentContext.languageId);
  if (langId.isEmpty()) {
    langId = m_currentContext.languageId.trimmed().toLower();
  }
  auto providers =
      CompletionProviderRegistry::instance().providersForLanguage(langId);

  if (providers.isEmpty()) {
    Logger::instance().warning(
        QString("No completion providers for language '%1'").arg(langId));
    emit completionsReady({});
    return;
  }

  m_pendingProviders = providers.size();

  int requestId = m_currentRequestId;

  for (auto &provider : providers) {
    provider->requestCompletions(
        m_currentContext,
        [this, requestId](const QList<CompletionItem> &items) {
          collectProviderResults(requestId, items);
        });
  }
}

void CompletionEngine::cancelPendingRequests() {
  m_debounceTimer->stop();
  m_pendingProviders = 0;
  m_currentRequestId++;

  auto providers =
      CompletionProviderRegistry::instance().providersForLanguage(m_languageId);
  for (auto &provider : providers) {
    provider->cancelPendingRequests();
  }
}

void CompletionEngine::collectProviderResults(
    int requestId, const QList<CompletionItem> &items) {

  if (requestId != m_currentRequestId) {
    return;
  }

  m_pendingItems.append(items);
  m_pendingProviders--;

  if (m_pendingProviders <= 0) {
    mergeAndSortResults();
    notifyResults();
  }
}

void CompletionEngine::mergeAndSortResults() {

  QMap<QString, CompletionItem> uniqueItems;

  for (const CompletionItem &item : m_pendingItems) {
    QString key = item.label.toLower();
    if (!uniqueItems.contains(key) ||
        item.priority < uniqueItems[key].priority) {
      uniqueItems[key] = item;
    }
  }

  m_lastResults = uniqueItems.values();

  std::sort(m_lastResults.begin(), m_lastResults.end());

  if (m_lastResults.size() > m_maxResults) {
    m_lastResults = m_lastResults.mid(0, m_maxResults);
  }
}

void CompletionEngine::notifyResults() { emit completionsReady(m_lastResults); }

QList<CompletionItem>
CompletionEngine::filterResults(const QString &prefix) const {
  if (prefix.isEmpty()) {
    return m_lastResults;
  }

  QList<CompletionItem> filtered;
  for (const CompletionItem &item : m_lastResults) {
    if (item.effectiveFilterText().startsWith(prefix, Qt::CaseInsensitive)) {
      filtered.append(item);
    }
  }

  return filtered;
}

void CompletionEngine::onDebounceTimeout() {

  cancelPendingRequests();
  executeCompletionRequest();
}
