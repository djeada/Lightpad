#ifndef COMPLETIONENGINE_H
#define COMPLETIONENGINE_H

#include "completionitem.h"
#include "completioncontext.h"
#include "completionproviderregistry.h"
#include <QObject>
#include <QTimer>
#include <QList>
#include <memory>

/**
 * @brief Central completion orchestrator
 * 
 * The CompletionEngine coordinates completion requests across multiple
 * providers, merges and sorts results, and manages the completion lifecycle.
 * 
 * Usage:
 * @code
 * CompletionEngine* engine = new CompletionEngine(this);
 * engine->setLanguage("cpp");
 * 
 * connect(engine, &CompletionEngine::completionsReady, 
 *         this, &MyWidget::showCompletions);
 * 
 * engine->requestCompletions(context);
 * @endcode
 */
class CompletionEngine : public QObject {
    Q_OBJECT

public:
    explicit CompletionEngine(QObject* parent = nullptr);
    ~CompletionEngine();
    
    /**
     * @brief Set the current language for completions
     * @param languageId Language identifier
     */
    void setLanguage(const QString& languageId);
    
    /**
     * @brief Get the current language
     */
    QString language() const { return m_languageId; }
    
    /**
     * @brief Request completions for the given context
     * @param context Completion context
     * 
     * Results are delivered via completionsReady signal.
     */
    void requestCompletions(const CompletionContext& context);
    
    /**
     * @brief Cancel any pending completion requests
     */
    void cancelPendingRequests();
    
    /**
     * @brief Check if a completion request is in progress
     */
    bool isRequestPending() const { return m_pendingProviders > 0; }
    
    // Configuration
    void setMinimumPrefixLength(int length) { m_minPrefixLength = length; }
    int minimumPrefixLength() const { return m_minPrefixLength; }
    
    void setAutoTriggerDelay(int ms) { m_autoTriggerDelay = ms; }
    int autoTriggerDelay() const { return m_autoTriggerDelay; }
    
    void setMaxResults(int count) { m_maxResults = count; }
    int maxResults() const { return m_maxResults; }
    
    /**
     * @brief Filter existing results with new prefix
     * @param prefix New prefix to filter with
     * @return Filtered items
     * 
     * Use this for incremental filtering as user types.
     */
    QList<CompletionItem> filterResults(const QString& prefix) const;
    
    /**
     * @brief Get the last completion results
     */
    QList<CompletionItem> lastResults() const { return m_lastResults; }

signals:
    /**
     * @brief Emitted when completion results are ready
     * @param items Sorted and merged completion items
     */
    void completionsReady(const QList<CompletionItem>& items);
    
    /**
     * @brief Emitted when completion request fails
     * @param error Error message
     */
    void completionsFailed(const QString& error);

private slots:
    void onDebounceTimeout();

private:
    void collectProviderResults(const QList<CompletionItem>& items);
    void mergeAndSortResults();
    void notifyResults();
    void executeCompletionRequest();
    
    QString m_languageId;
    CompletionContext m_currentContext;
    QList<CompletionItem> m_pendingItems;
    QList<CompletionItem> m_lastResults;
    int m_pendingProviders = 0;
    
    // Configuration
    int m_minPrefixLength = 1;
    int m_autoTriggerDelay = 300;
    int m_maxResults = 100;
    
    QTimer* m_debounceTimer;
};

#endif // COMPLETIONENGINE_H
