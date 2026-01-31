#ifndef LANGUAGEKEYWORDSREGISTRY_H
#define LANGUAGEKEYWORDSREGISTRY_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QJsonDocument>

/**
 * @brief Registry for language keywords
 * 
 * Manages keywords for different programming languages. Keywords can be
 * loaded from JSON files or registered programmatically.
 * 
 * This is a singleton class - use instance() to access it.
 */
class LanguageKeywordsRegistry {
public:
    /**
     * @brief Get the singleton instance
     */
    static LanguageKeywordsRegistry& instance();
    
    /**
     * @brief Register keywords for a language
     * @param languageId Language identifier (e.g., "cpp", "python")
     * @param keywords List of keywords
     */
    void registerLanguage(const QString& languageId, const QStringList& keywords);
    
    /**
     * @brief Load keywords from a JSON file
     * @param languageId Language identifier
     * @param jsonPath Path to JSON file (can be resource path)
     * @return true if loaded successfully
     * 
     * JSON format:
     * {
     *     "keywords": ["keyword1", "keyword2", ...]
     * }
     */
    bool loadFromJson(const QString& languageId, const QString& jsonPath);
    
    /**
     * @brief Load keywords from embedded JSON data
     * @param languageId Language identifier
     * @param jsonData JSON data as byte array
     * @return true if parsed successfully
     */
    bool loadFromJsonData(const QString& languageId, const QByteArray& jsonData);
    
    /**
     * @brief Get keywords for a language
     * @param languageId Language identifier
     * @return List of keywords, empty if language not registered
     */
    QStringList keywords(const QString& languageId) const;
    
    /**
     * @brief Get all keywords matching a prefix
     * @param languageId Language identifier
     * @param prefix Prefix to match (case-insensitive)
     * @return List of matching keywords
     */
    QStringList keywordsWithPrefix(const QString& languageId, const QString& prefix) const;
    
    /**
     * @brief Check if a language has registered keywords
     * @param languageId Language identifier
     * @return true if language has keywords
     */
    bool hasLanguage(const QString& languageId) const;
    
    /**
     * @brief Get list of all registered language IDs
     * @return List of language IDs
     */
    QStringList registeredLanguages() const;
    
    /**
     * @brief Clear all registered keywords
     */
    void clear();
    
    /**
     * @brief Initialize with default keywords for built-in languages
     * 
     * Registers keywords for: cpp, python, js
     */
    void initializeDefaults();

private:
    LanguageKeywordsRegistry() = default;
    ~LanguageKeywordsRegistry() = default;
    LanguageKeywordsRegistry(const LanguageKeywordsRegistry&) = delete;
    LanguageKeywordsRegistry& operator=(const LanguageKeywordsRegistry&) = delete;
    
    QMap<QString, QStringList> m_keywords;
};

#endif // LANGUAGEKEYWORDSREGISTRY_H
