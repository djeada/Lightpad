#include "completionengine.h"
#include "../core/logging/logger.h"
#include <algorithm>

CompletionEngine::CompletionEngine(QObject *parent)
    : QObject(parent), m_debounceTimer(new QTimer(this)) {
  m_debounceTimer->setSingleShot(true);
  connect(m_debounceTimer, &QTimer::timeout, this,
          &CompletionEngine::onDebounceTimeout);
}

CompletionEngine::~CompletionEngine() { cancelPendingRequests(); }

void CompletionEngine::setLanguage(const QString &languageId) {
  m_languageId = languageId;
}

void CompletionEngine::requestCompletions(const CompletionContext &context) {
  // Store context for debounced execution
  m_currentContext = context;

  // Check minimum prefix length for auto-triggered completions
  if (context.isAutoComplete && context.prefix.length() < m_minPrefixLength) {
    cancelPendingRequests();
    emit completionsReady({});
    return;
  }

  // For auto-complete, use debounce to avoid spamming on rapid typing
  // For explicit invocation (Ctrl+Space), execute immediately
  if (context.isAutoComplete) {
    // Debounce: restart timer on each keystroke
    m_debounceTimer->start(m_autoTriggerDelay);
  } else {
    // Explicit invocation - execute immediately
    cancelPendingRequests();
    executeCompletionRequest();
  }
}

void CompletionEngine::executeCompletionRequest() {
  m_pendingItems.clear();
  m_currentRequestId++;

  // Get providers for this language
  QString langId = m_currentContext.languageId.isEmpty()
                       ? m_languageId
                       : m_currentContext.languageId;
  auto providers =
      CompletionProviderRegistry::instance().providersForLanguage(langId);

  if (providers.isEmpty()) {
    Logger::instance().warning(
        QString("No completion providers for language '%1'").arg(langId));
    emit completionsReady({});
    return;
  }

  m_pendingProviders = providers.size();

  // Capture requestId so stale callbacks are ignored
  int requestId = m_currentRequestId;

  // Request completions from all providers
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

  // Notify providers to cancel
  auto providers =
      CompletionProviderRegistry::instance().providersForLanguage(m_languageId);
  for (auto &provider : providers) {
    provider->cancelPendingRequests();
  }
}

void CompletionEngine::collectProviderResults(
    int requestId, const QList<CompletionItem> &items) {
  // Ignore callbacks from stale requests
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
  // Remove duplicates (same label from different providers)
  // Keep the item with lower priority value (higher precedence)
  // Lower priority number = higher precedence (e.g., LSP=10 > keywords=100)
  QMap<QString, CompletionItem> uniqueItems;

  for (const CompletionItem &item : m_pendingItems) {
    QString key = item.label.toLower();
    if (!uniqueItems.contains(key) ||
        item.priority < uniqueItems[key].priority) {
      uniqueItems[key] = item;
    }
  }

  m_lastResults = uniqueItems.values();

  // Sort by priority then alphabetically
  std::sort(m_lastResults.begin(), m_lastResults.end());

  // Limit results
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
  // This is called after debounce delay - now safe to request completions
  cancelPendingRequests();
  executeCompletionRequest();
}
