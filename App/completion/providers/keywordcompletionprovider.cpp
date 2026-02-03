#include "keywordcompletionprovider.h"
#include "../../core/logging/logger.h"
#include "../languagekeywordsregistry.h"

KeywordCompletionProvider::KeywordCompletionProvider() {
  // Ensure default keywords are initialized
  LanguageKeywordsRegistry &registry = LanguageKeywordsRegistry::instance();
  if (registry.registeredLanguages().isEmpty()) {
    registry.initializeDefaults();
  }
}

void KeywordCompletionProvider::requestCompletions(
    const CompletionContext &context,
    std::function<void(const QList<CompletionItem> &)> callback) {
  QList<CompletionItem> items;

  if (!m_enabled) {
    callback(items);
    return;
  }

  // Get keywords for the language
  LanguageKeywordsRegistry &registry = LanguageKeywordsRegistry::instance();

  QString langId = context.languageId;
  QStringList matchingKeywords;

  // Try exact language match first
  if (registry.hasLanguage(langId)) {
    matchingKeywords = registry.keywordsWithPrefix(langId, context.prefix);
  } else {
    // Fall back to getting all registered languages
    // This provides basic completion even for unknown languages
    for (const QString &lang : registry.registeredLanguages()) {
      matchingKeywords.append(
          registry.keywordsWithPrefix(lang, context.prefix));
    }
    matchingKeywords.removeDuplicates();
  }

  // Convert to CompletionItems
  for (const QString &keyword : matchingKeywords) {
    CompletionItem item;
    item.label = keyword;
    item.kind = CompletionItemKind::Keyword;
    item.priority = basePriority();
    item.providerId = id();
    items.append(item);
  }

  callback(items);
}
