#include "keywordcompletionprovider.h"
#include "../../core/logging/logger.h"
#include "../../language/languagecatalog.h"
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

  QString langId = LanguageCatalog::normalize(context.languageId);
  if (langId.isEmpty()) {
    langId = context.languageId.trimmed().toLower();
  }
  QStringList matchingKeywords;

  // Try exact language match only.
  // Generic fallback across all languages creates noisy, incorrect suggestions.
  if (registry.hasLanguage(langId)) {
    matchingKeywords = registry.keywordsWithPrefix(langId, context.prefix);
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
