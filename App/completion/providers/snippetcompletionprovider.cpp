#include "snippetcompletionprovider.h"
#include "../../core/logging/logger.h"
#include "../../language/languagecatalog.h"
#include "../snippetregistry.h"

SnippetCompletionProvider::SnippetCompletionProvider() {
  // Ensure default snippets are initialized
  SnippetRegistry &registry = SnippetRegistry::instance();
  if (registry.registeredLanguages().isEmpty()) {
    registry.initializeDefaults();
  }
}

void SnippetCompletionProvider::requestCompletions(
    const CompletionContext &context,
    std::function<void(const QList<CompletionItem> &)> callback) {
  QList<CompletionItem> items;

  if (!m_enabled) {
    callback(items);
    return;
  }

  SnippetRegistry &registry = SnippetRegistry::instance();
  QString languageId = LanguageCatalog::normalize(context.languageId);
  if (languageId.isEmpty()) {
    languageId = context.languageId.trimmed().toLower();
  }

  // Get snippets matching the prefix
  QList<Snippet> matchingSnippets =
      registry.snippetsWithPrefix(languageId, context.prefix);

  // Convert to CompletionItems
  for (const Snippet &snippet : matchingSnippets) {
    CompletionItem item;
    item.label = snippet.prefix;
    item.detail = snippet.label;
    item.documentation = snippet.description;
    item.insertText = snippet.body;
    item.kind = CompletionItemKind::Snippet;
    item.isSnippet = snippet.hasPlaceholders();
    item.priority = basePriority();
    item.providerId = id();
    items.append(item);
  }

  callback(items);
}
