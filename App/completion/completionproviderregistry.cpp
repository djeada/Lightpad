#include "completionproviderregistry.h"
#include "../core/logging/logger.h"
#include <QSet>
#include <algorithm>

CompletionProviderRegistry &CompletionProviderRegistry::instance() {
  static CompletionProviderRegistry instance;
  return instance;
}

void CompletionProviderRegistry::registerProvider(
    std::shared_ptr<ICompletionProvider> provider) {
  if (!provider) {
    Logger::instance().warning(
        "Attempted to register null completion provider");
    return;
  }

  QString providerId = provider->id();
  if (providerId.isEmpty()) {
    Logger::instance().warning(
        "Attempted to register completion provider with empty ID");
    return;
  }

  if (m_providers.contains(providerId)) {
    Logger::instance().warning(
        QString("Completion provider '%1' already registered, replacing")
            .arg(providerId));
    emit providerUnregistered(providerId);
  }

  m_providers[providerId] = provider;

  Logger::instance().info(QString("Registered completion provider '%1' (%2)")
                              .arg(providerId)
                              .arg(provider->displayName()));

  emit providerRegistered(providerId);
}

bool CompletionProviderRegistry::unregisterProvider(const QString &providerId) {
  if (!m_providers.contains(providerId)) {
    Logger::instance().warning(
        QString("Attempted to unregister non-existent completion provider '%1'")
            .arg(providerId));
    return false;
  }

  m_providers.remove(providerId);

  Logger::instance().info(
      QString("Unregistered completion provider '%1'").arg(providerId));

  emit providerUnregistered(providerId);
  return true;
}

std::shared_ptr<ICompletionProvider>
CompletionProviderRegistry::getProvider(const QString &providerId) const {
  auto it = m_providers.find(providerId);
  if (it != m_providers.end()) {
    return it.value();
  }
  return nullptr;
}

QList<std::shared_ptr<ICompletionProvider>>
CompletionProviderRegistry::providersForLanguage(
    const QString &languageId) const {
  QList<std::shared_ptr<ICompletionProvider>> result;

  for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
    auto &provider = it.value();
    if (provider && provider->isEnabled() &&
        provider->supportsLanguage(languageId)) {
      result.append(provider);
    }
  }

  std::sort(result.begin(), result.end(),
            [](const std::shared_ptr<ICompletionProvider> &a,
               const std::shared_ptr<ICompletionProvider> &b) {
              return a->basePriority() < b->basePriority();
            });

  return result;
}

QList<std::shared_ptr<ICompletionProvider>>
CompletionProviderRegistry::allProviders() const {
  return m_providers.values();
}

QStringList CompletionProviderRegistry::allProviderIds() const {
  return m_providers.keys();
}

QStringList CompletionProviderRegistry::allTriggerCharacters(
    const QString &languageId) const {
  QSet<QString> triggers;

  for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
    auto &provider = it.value();
    if (provider && provider->isEnabled() &&
        provider->supportsLanguage(languageId)) {
      for (const QString &trigger : provider->triggerCharacters()) {
        if (!trigger.isEmpty()) {
          triggers.insert(trigger);
        }
      }
    }
  }

  return triggers.values();
}

bool CompletionProviderRegistry::hasProvidersForLanguage(
    const QString &languageId) const {
  for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
    auto &provider = it.value();
    if (provider && provider->isEnabled() &&
        provider->supportsLanguage(languageId)) {
      return true;
    }
  }
  return false;
}

int CompletionProviderRegistry::providerCount() const {
  return m_providers.size();
}

void CompletionProviderRegistry::clear() {

  QStringList ids = m_providers.keys();
  for (const QString &id : ids) {
    emit providerUnregistered(id);
  }

  m_providers.clear();
  Logger::instance().info("Cleared all completion providers from registry");
}
