#ifndef SNIPPETREGISTRY_H
#define SNIPPETREGISTRY_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

struct Snippet {
  QString prefix;
  QString label;
  QString body;
  QString description;
  QString languageId;

  bool hasPlaceholders() const { return body.contains('$'); }

  QString expandedBody() const;
};

class SnippetRegistry {
public:
  static SnippetRegistry &instance();

  void registerSnippet(const QString &languageId, const Snippet &snippet);

  bool loadFromJson(const QString &languageId, const QString &jsonPath);

  bool loadFromJsonData(const QString &languageId, const QByteArray &jsonData);

  QList<Snippet> snippets(const QString &languageId) const;

  QList<Snippet> snippetsWithPrefix(const QString &languageId,
                                    const QString &prefix) const;

  bool hasSnippets(const QString &languageId) const;

  QStringList registeredLanguages() const;

  void clear();

  void initializeDefaults();

private:
  SnippetRegistry() = default;
  ~SnippetRegistry() = default;
  SnippetRegistry(const SnippetRegistry &) = delete;
  SnippetRegistry &operator=(const SnippetRegistry &) = delete;

  QMap<QString, QList<Snippet>> m_snippets;
};

#endif
