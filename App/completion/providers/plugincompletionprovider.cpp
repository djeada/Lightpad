#include "plugincompletionprovider.h"
#include "../../core/logging/logger.h"
#include "../../syntax/syntaxpluginregistry.h"

PluginCompletionProvider::PluginCompletionProvider() {}

QStringList PluginCompletionProvider::supportedLanguages() const {
  // Return languages we have plugins for
  const_cast<PluginCompletionProvider *>(this)->ensureCachePopulated();
  return m_cachedKeywords.keys();
}

void PluginCompletionProvider::requestCompletions(
    const CompletionContext &context,
    std::function<void(const QList<CompletionItem> &)> callback) {
  QList<CompletionItem> items;

  if (!m_enabled) {
    callback(items);
    return;
  }

  ensureCachePopulated();

  QString langId = context.languageId.toLower();
  QStringList keywords = m_cachedKeywords.value(langId);

  // Filter by prefix
  for (const QString &keyword : keywords) {
    if (keyword.startsWith(context.prefix, Qt::CaseInsensitive)) {
      CompletionItem item;
      item.label = keyword;
      item.kind = CompletionItemKind::Keyword;
      item.priority = basePriority();
      item.providerId = id();
      items.append(item);
    }
  }

  callback(items);
}

void PluginCompletionProvider::refreshCache() {
  m_cachePopulated = false;
  m_cachedKeywords.clear();
}

void PluginCompletionProvider::ensureCachePopulated() {
  if (m_cachePopulated) {
    return;
  }

  SyntaxPluginRegistry &registry = SyntaxPluginRegistry::instance();
  QStringList langIds = registry.getAllLanguageIds();

  for (const QString &langId : langIds) {
    ISyntaxPlugin *plugin = registry.getPluginByLanguageId(langId);
    if (plugin) {
      QStringList keywords = plugin->keywords();
      if (!keywords.isEmpty()) {
        m_cachedKeywords[langId.toLower()] = keywords;
        Logger::instance().info(QString("Cached %1 keywords from plugin '%2'")
                                    .arg(keywords.size())
                                    .arg(langId));
      }
    }
  }

  m_cachePopulated = true;
}
