#ifndef LANGUAGEKEYWORDSREGISTRY_H
#define LANGUAGEKEYWORDSREGISTRY_H

#include <QJsonDocument>
#include <QMap>
#include <QString>
#include <QStringList>

class LanguageKeywordsRegistry {
public:
  static LanguageKeywordsRegistry &instance();

  void registerLanguage(const QString &languageId, const QStringList &keywords);

  bool loadFromJson(const QString &languageId, const QString &jsonPath);

  bool loadFromJsonData(const QString &languageId, const QByteArray &jsonData);

  QStringList keywords(const QString &languageId) const;

  QStringList keywordsWithPrefix(const QString &languageId,
                                 const QString &prefix) const;

  bool hasLanguage(const QString &languageId) const;

  QStringList registeredLanguages() const;

  void clear();

  void initializeDefaults();

private:
  LanguageKeywordsRegistry() = default;
  ~LanguageKeywordsRegistry() = default;
  LanguageKeywordsRegistry(const LanguageKeywordsRegistry &) = delete;
  LanguageKeywordsRegistry &
  operator=(const LanguageKeywordsRegistry &) = delete;

  QMap<QString, QStringList> m_keywords;
};

#endif
