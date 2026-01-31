#include "completionengine.h"
#include "../core/logging/logger.h"
#include <algorithm>

CompletionEngine::CompletionEngine(QObject* parent)
    : QObject(parent)
    , m_debounceTimer(new QTimer(this))
{
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this, &CompletionEngine::onDebounceTimeout);
}

CompletionEngine::~CompletionEngine()
{
    cancelPendingRequests();
}

void CompletionEngine::setLanguage(const QString& languageId)
{
    m_languageId = languageId;
}

void CompletionEngine::requestCompletions(const CompletionContext& context)
{
    // Cancel any pending requests
    cancelPendingRequests();
    
    m_currentContext = context;
    m_pendingItems.clear();
    
    // Check minimum prefix length for auto-triggered completions
    if (context.isAutoComplete && context.prefix.length() < m_minPrefixLength) {
        emit completionsReady({});
        return;
    }
    
    // Get providers for this language
    QString langId = context.languageId.isEmpty() ? m_languageId : context.languageId;
    auto providers = CompletionProviderRegistry::instance().providersForLanguage(langId);
    
    if (providers.isEmpty()) {
        Logger::instance().warning(
            QString("No completion providers for language '%1'").arg(langId)
        );
        emit completionsReady({});
        return;
    }
    
    m_pendingProviders = providers.size();
    
    // Request completions from all providers
    for (auto& provider : providers) {
        provider->requestCompletions(context, 
            [this](const QList<CompletionItem>& items) {
                collectProviderResults(items);
            }
        );
    }
}

void CompletionEngine::cancelPendingRequests()
{
    m_debounceTimer->stop();
    m_pendingProviders = 0;
    
    // Notify providers to cancel
    auto providers = CompletionProviderRegistry::instance().providersForLanguage(m_languageId);
    for (auto& provider : providers) {
        provider->cancelPendingRequests();
    }
}

void CompletionEngine::collectProviderResults(const QList<CompletionItem>& items)
{
    m_pendingItems.append(items);
    m_pendingProviders--;
    
    if (m_pendingProviders <= 0) {
        mergeAndSortResults();
        notifyResults();
    }
}

void CompletionEngine::mergeAndSortResults()
{
    // Remove duplicates (same label from different providers)
    // Keep the item with lower priority value (higher precedence)
    // Lower priority number = higher precedence (e.g., LSP=10 > keywords=100)
    QMap<QString, CompletionItem> uniqueItems;
    
    for (const CompletionItem& item : m_pendingItems) {
        QString key = item.label.toLower();
        if (!uniqueItems.contains(key) || item.priority < uniqueItems[key].priority) {
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

void CompletionEngine::notifyResults()
{
    emit completionsReady(m_lastResults);
}

QList<CompletionItem> CompletionEngine::filterResults(const QString& prefix) const
{
    if (prefix.isEmpty()) {
        return m_lastResults;
    }
    
    QList<CompletionItem> filtered;
    for (const CompletionItem& item : m_lastResults) {
        if (item.effectiveFilterText().startsWith(prefix, Qt::CaseInsensitive)) {
            filtered.append(item);
        }
    }
    
    return filtered;
}

void CompletionEngine::onDebounceTimeout()
{
    // This is called after debounce delay for auto-triggered completions
    requestCompletions(m_currentContext);
}
