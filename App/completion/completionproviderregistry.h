#ifndef COMPLETIONPROVIDERREGISTRY_H
#define COMPLETIONPROVIDERREGISTRY_H

#include "icompletionprovider.h"
#include <QObject>
#include <QList>
#include <QMap>
#include <memory>

/**
 * @brief Central registry for completion providers
 * 
 * Manages registration and retrieval of completion providers.
 * Provides methods to query providers by language and collect
 * trigger characters.
 * 
 * This is a singleton class - use instance() to access it.
 * 
 * Example usage:
 * @code
 * auto& registry = CompletionProviderRegistry::instance();
 * 
 * // Register a provider
 * registry.registerProvider(std::make_shared<KeywordProvider>());
 * 
 * // Get providers for a language
 * auto providers = registry.providersForLanguage("cpp");
 * for (auto& provider : providers) {
 *     provider->requestCompletions(context, callback);
 * }
 * @endcode
 */
class CompletionProviderRegistry : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the registry instance
     */
    static CompletionProviderRegistry& instance();
    
    /**
     * @brief Register a completion provider
     * 
     * If a provider with the same ID is already registered,
     * it will be replaced and providerUnregistered will be emitted
     * before providerRegistered.
     * 
     * @param provider The provider to register (shared ownership)
     */
    void registerProvider(std::shared_ptr<ICompletionProvider> provider);
    
    /**
     * @brief Unregister a provider by ID
     * @param providerId The ID of the provider to unregister
     * @return true if provider was found and removed
     */
    bool unregisterProvider(const QString& providerId);
    
    /**
     * @brief Get a provider by ID
     * @param providerId The provider ID
     * @return Pointer to provider or nullptr if not found
     */
    std::shared_ptr<ICompletionProvider> getProvider(const QString& providerId) const;
    
    /**
     * @brief Get all providers that support a language
     * 
     * Returns providers sorted by priority (lowest first).
     * 
     * @param languageId The language to query for
     * @return List of providers supporting this language
     */
    QList<std::shared_ptr<ICompletionProvider>> providersForLanguage(
        const QString& languageId
    ) const;
    
    /**
     * @brief Get all registered providers
     * @return List of all providers
     */
    QList<std::shared_ptr<ICompletionProvider>> allProviders() const;
    
    /**
     * @brief Get all registered provider IDs
     * @return List of provider IDs
     */
    QStringList allProviderIds() const;
    
    /**
     * @brief Get all trigger characters for a language
     * 
     * Collects trigger characters from all providers that
     * support the given language.
     * 
     * @param languageId The language to query for
     * @return Combined list of unique trigger characters
     */
    QStringList allTriggerCharacters(const QString& languageId) const;
    
    /**
     * @brief Check if any providers support a language
     * @param languageId The language to check
     * @return true if at least one provider supports it
     */
    bool hasProvidersForLanguage(const QString& languageId) const;
    
    /**
     * @brief Get number of registered providers
     * @return Provider count
     */
    int providerCount() const;
    
    /**
     * @brief Clear all registered providers
     * 
     * Emits providerUnregistered for each removed provider.
     */
    void clear();

signals:
    /**
     * @brief Emitted when a provider is registered
     * @param providerId The ID of the registered provider
     */
    void providerRegistered(const QString& providerId);
    
    /**
     * @brief Emitted when a provider is unregistered
     * @param providerId The ID of the unregistered provider
     */
    void providerUnregistered(const QString& providerId);

private:
    // Singleton pattern - private constructor
    CompletionProviderRegistry() = default;
    ~CompletionProviderRegistry() = default;
    CompletionProviderRegistry(const CompletionProviderRegistry&) = delete;
    CompletionProviderRegistry& operator=(const CompletionProviderRegistry&) = delete;
    
    // Providers stored by ID
    QMap<QString, std::shared_ptr<ICompletionProvider>> m_providers;
};

#endif // COMPLETIONPROVIDERREGISTRY_H
