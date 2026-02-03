#ifndef SNIPPETREGISTRY_H
#define SNIPPETREGISTRY_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

/**
 * @brief Represents a code snippet
 */
struct Snippet {
  QString prefix;      // Trigger prefix
  QString label;       // Display name
  QString body;        // Snippet body with placeholders
  QString description; // Description for documentation
  QString languageId;  // Language this snippet is for

  /**
   * @brief Check if snippet has tabstop placeholders
   */
  bool hasPlaceholders() const { return body.contains('$'); }

  /**
   * @brief Expand the snippet body (replace simple placeholders)
   * @return Expanded text (tabstops still need editor handling)
   */
  QString expandedBody() const;
};

/**
 * @brief Registry for code snippets
 *
 * Manages snippets for different programming languages.
 * Snippets can be loaded from JSON files.
 */
class SnippetRegistry {
public:
  static SnippetRegistry &instance();

  /**
   * @brief Register a snippet
   * @param languageId Language identifier
   * @param snippet Snippet to register
   */
  void registerSnippet(const QString &languageId, const Snippet &snippet);

  /**
   * @brief Load snippets from JSON file
   * @param languageId Language identifier
   * @param jsonPath Path to JSON file
   * @return true if loaded successfully
   *
   * JSON format:
   * {
   *     "snippets": [
   *         {
   *             "prefix": "for",
   *             "label": "For Loop",
   *             "body": "for (${1:int} ${2:i} = 0; $2 < ${3:count}; $2++)
   * {\n\t$0\n}", "description": "For loop with iterator"
   *         }
   *     ]
   * }
   */
  bool loadFromJson(const QString &languageId, const QString &jsonPath);

  /**
   * @brief Load snippets from JSON data
   */
  bool loadFromJsonData(const QString &languageId, const QByteArray &jsonData);

  /**
   * @brief Get all snippets for a language
   */
  QList<Snippet> snippets(const QString &languageId) const;

  /**
   * @brief Get snippets matching a prefix
   */
  QList<Snippet> snippetsWithPrefix(const QString &languageId,
                                    const QString &prefix) const;

  /**
   * @brief Check if a language has snippets
   */
  bool hasSnippets(const QString &languageId) const;

  /**
   * @brief Get all language IDs with registered snippets
   */
  QStringList registeredLanguages() const;

  /**
   * @brief Clear all snippets
   */
  void clear();

  /**
   * @brief Initialize with default snippets
   */
  void initializeDefaults();

private:
  SnippetRegistry() = default;
  ~SnippetRegistry() = default;
  SnippetRegistry(const SnippetRegistry &) = delete;
  SnippetRegistry &operator=(const SnippetRegistry &) = delete;

  QMap<QString, QList<Snippet>> m_snippets;
};

#endif // SNIPPETREGISTRY_H
