#ifndef COMPLETIONPROVIDERREGISTRY_H
#define COMPLETIONPROVIDERREGISTRY_H

#include "icompletionprovider.h"
#include <QList>
#include <QMap>
#include <QObject>
#include <memory>

class CompletionProviderRegistry : public QObject {
  Q_OBJECT

public:
  static CompletionProviderRegistry &instance();

  void registerProvider(std::shared_ptr<ICompletionProvider> provider);

  bool unregisterProvider(const QString &providerId);

  std::shared_ptr<ICompletionProvider>
  getProvider(const QString &providerId) const;

  QList<std::shared_ptr<ICompletionProvider>>
  providersForLanguage(const QString &languageId) const;

  QList<std::shared_ptr<ICompletionProvider>> allProviders() const;

  QStringList allProviderIds() const;

  QStringList allTriggerCharacters(const QString &languageId) const;

  bool hasProvidersForLanguage(const QString &languageId) const;

  int providerCount() const;

  void clear();

signals:

  void providerRegistered(const QString &providerId);

  void providerUnregistered(const QString &providerId);

private:
  CompletionProviderRegistry() = default;
  ~CompletionProviderRegistry() = default;
  CompletionProviderRegistry(const CompletionProviderRegistry &) = delete;
  CompletionProviderRegistry &
  operator=(const CompletionProviderRegistry &) = delete;

  QMap<QString, std::shared_ptr<ICompletionProvider>> m_providers;
};

#endif
