#ifndef ICOMPLETIONPROVIDER_H
#define ICOMPLETIONPROVIDER_H

#include "completionitem.h"
#include "completioncontext.h"
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

/**
 * @brief Interface for completion providers
 * 
 * Completion providers are responsible for generating completion suggestions
 * based on the current editing context. Multiple providers can be registered
 * to provide different types of completions (keywords, LSP, snippets, etc.).
 * 
 * Providers should be stateless and thread-safe, as they may be called
 * concurrently from different contexts.
 * 
 * Example implementation:
 * @code
 * class KeywordProvider : public ICompletionProvider {
 * public:
 *     QString id() const override { return "keywords"; }
 *     QString displayName() const override { return "Keywords"; }
 *     int basePriority() const override { return 100; }
 *     QStringList supportedLanguages() const override { return {"*"}; }
 *     
 *     void requestCompletions(
 *         const CompletionContext& ctx,
 *         std::function<void(const QList<CompletionItem>&)> cb
 *     ) override {
 *         QList<CompletionItem> items;
 *         // ... populate items ...
 *         cb(items);
 *     }
 * };
 * @endcode
 */
class ICompletionProvider {
public:
    virtual ~ICompletionProvider() = default;
    
    // =========================================================================
    // Provider Metadata
    // =========================================================================
    
    /**
     * @brief Unique identifier for this provider
     * 
     * Used for registration, logging, and debugging.
     * Should be lowercase alphanumeric with underscores.
     * Examples: "keywords", "lsp_cpp", "user_snippets"
     */
    virtual QString id() const = 0;
    
    /**
     * @brief Human-readable name for this provider
     * 
     * Displayed in settings and debugging UI.
     */
    virtual QString displayName() const = 0;
    
    /**
     * @brief Base priority for items from this provider
     * 
     * Lower values = higher priority (appear first in list).
     * Suggested ranges:
     * - 0-20: LSP/context-aware (most relevant)
     * - 20-50: Snippets
     * - 50-80: Plugin keywords
     * - 80-100: Generic keywords
     * - 100+: Low priority suggestions
     * 
     * Individual items can override this via CompletionItem::priority.
     */
    virtual int basePriority() const = 0;
    
    // =========================================================================
    // Language Support
    // =========================================================================
    
    /**
     * @brief List of supported language IDs
     * 
     * Use "*" to indicate support for all languages.
     * Language IDs should match syntax plugin IDs (e.g., "cpp", "python", "js").
     * 
     * @return List of supported language IDs, or {"*"} for all languages
     */
    virtual QStringList supportedLanguages() const = 0;
    
    /**
     * @brief Check if this provider supports a language
     * @param languageId The language to check
     * @return true if provider can provide completions for this language
     * 
     * Default implementation checks against supportedLanguages().
     */
    virtual bool supportsLanguage(const QString& languageId) const {
        QStringList langs = supportedLanguages();
        if (langs.contains("*")) {
            return true;
        }
        return langs.contains(languageId, Qt::CaseInsensitive);
    }
    
    // =========================================================================
    // Trigger Configuration
    // =========================================================================
    
    /**
     * @brief Characters that trigger automatic completion
     * 
     * When the user types one of these characters, completion is automatically
     * triggered with TriggerCharacter kind.
     * 
     * Examples:
     * - C++: [".", "::", "->"]
     * - Python: ["."]
     * - JavaScript: ["."]
     * 
     * @return List of trigger characters, empty for no automatic triggers
     */
    virtual QStringList triggerCharacters() const { return {}; }
    
    /**
     * @brief Minimum prefix length before auto-triggering
     * 
     * For providers that trigger based on typing (not just trigger characters),
     * this specifies the minimum number of characters needed before
     * completion is automatically shown.
     * 
     * @return Minimum prefix length, 0 to disable length-based triggering
     */
    virtual int minimumPrefixLength() const { return 1; }
    
    // =========================================================================
    // Completion Requests
    // =========================================================================
    
    /**
     * @brief Request completion items for the given context
     * 
     * This is the main entry point for completion. Providers should generate
     * relevant completion items and call the callback with the results.
     * 
     * The callback may be called synchronously (for simple providers like
     * keywords) or asynchronously (for LSP or remote providers).
     * 
     * Items returned should already be filtered by the prefix in the context,
     * but the completion engine may apply additional filtering.
     * 
     * @param context The completion context with position and prefix
     * @param callback Function to call with results (can be async)
     */
    virtual void requestCompletions(
        const CompletionContext& context,
        std::function<void(const QList<CompletionItem>&)> callback
    ) = 0;
    
    /**
     * @brief Resolve additional details for a completion item
     * 
     * Called when the user selects (hovers over) an item in the popup.
     * Use this to lazily load documentation or other expensive details.
     * 
     * Default implementation returns the item unchanged.
     * 
     * @param item The item to resolve
     * @param callback Function to call with resolved item
     */
    virtual void resolveItem(
        const CompletionItem& item,
        std::function<void(const CompletionItem&)> callback
    ) {
        callback(item);
    }
    
    /**
     * @brief Cancel any pending completion requests
     * 
     * Called when the user continues typing and previous results are no
     * longer needed. Providers with async operations should cancel them.
     */
    virtual void cancelPendingRequests() {}
    
    // =========================================================================
    // Optional Configuration
    // =========================================================================
    
    /**
     * @brief Whether this provider is enabled
     * 
     * Disabled providers are not queried for completions.
     * Default implementation always returns true.
     */
    virtual bool isEnabled() const { return true; }
    
    /**
     * @brief Set whether this provider is enabled
     * @param enabled New enabled state
     * 
     * Default implementation does nothing.
     */
    virtual void setEnabled(bool enabled) { Q_UNUSED(enabled); }
};

#endif // ICOMPLETIONPROVIDER_H
