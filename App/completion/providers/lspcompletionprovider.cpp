#include "lspcompletionprovider.h"
#include "../../core/logging/logger.h"

LspCompletionProvider::LspCompletionProvider(LspClient *client, QObject *parent)
    : QObject(parent), m_client(client) {
  if (m_client) {
    connect(m_client, &LspClient::completionReceived, this,
            &LspCompletionProvider::onCompletionReceived);
  }
}

QStringList LspCompletionProvider::supportedLanguages() const {
  // LSP can support any language that has a server configured
  // For now, return wildcard to indicate we try for all languages
  return {"*"};
}

QStringList LspCompletionProvider::triggerCharacters() const {
  // Common trigger characters - could be queried from LSP capabilities
  return {".", "::", "->", "<"};
}

void LspCompletionProvider::setClient(LspClient *client) {
  if (m_client) {
    disconnect(m_client, &LspClient::completionReceived, this,
               &LspCompletionProvider::onCompletionReceived);
  }

  m_client = client;

  if (m_client) {
    connect(m_client, &LspClient::completionReceived, this,
            &LspCompletionProvider::onCompletionReceived);
  }
}

void LspCompletionProvider::requestCompletions(
    const CompletionContext &context,
    std::function<void(const QList<CompletionItem> &)> callback) {
  if (!isEnabled()) {
    callback({});
    return;
  }

  // Store callback for when response arrives
  m_lastRequestId++;
  m_pendingCallbacks[m_lastRequestId] = callback;

  // Request completion from LSP
  LspPosition pos;
  pos.line = context.line;
  pos.character = context.column;

  m_client->requestCompletion(context.documentUri, pos);
}

void LspCompletionProvider::resolveItem(
    const CompletionItem &item,
    std::function<void(const CompletionItem &)> callback) {
  // For now, just return the item as-is
  // Full implementation would call LSP completionItem/resolve
  callback(item);
}

void LspCompletionProvider::cancelPendingRequests() {
  // Clear all pending callbacks - they won't be called
  m_pendingCallbacks.clear();
}

void LspCompletionProvider::onCompletionReceived(
    int requestId, const QList<LspCompletionItem> &items) {
  // Note: The LspClient currently doesn't expose which requestId corresponds
  // to which completion request. For now, we call the most recent callback
  // and cancel older ones to avoid stale completions.
  // TODO: Improve LspClient to properly track completion request IDs
  Q_UNUSED(requestId);

  if (m_pendingCallbacks.isEmpty()) {
    return;
  }

  // Get the last callback (most recent request) - this is the one
  // most likely to match the current editor state
  auto it = m_pendingCallbacks.end();
  --it;
  auto callback = it.value();

  // Convert items
  QList<CompletionItem> completionItems;
  for (const LspCompletionItem &lspItem : items) {
    completionItems.append(convertItem(lspItem));
  }

  // Clear all callbacks (cancel stale requests) and call the latest
  m_pendingCallbacks.clear();
  callback(completionItems);
}

CompletionItem
LspCompletionProvider::convertItem(const LspCompletionItem &lspItem) const {
  CompletionItem item;
  item.label = lspItem.label;
  item.insertText =
      lspItem.insertText.isEmpty() ? lspItem.label : lspItem.insertText;
  item.detail = lspItem.detail;
  item.documentation = lspItem.documentation;
  item.kind = convertKind(lspItem.kind);
  item.priority = basePriority();
  item.providerId = id();

  // Check if it's a snippet (contains $1, ${1:}, etc.)
  item.isSnippet = item.insertText.contains('$');

  return item;
}

CompletionItemKind LspCompletionProvider::convertKind(int lspKind) const {
  // LSP CompletionItemKind values match our enum
  // (We designed our enum to match LSP)
  if (lspKind >= 1 && lspKind <= 25) {
    return static_cast<CompletionItemKind>(lspKind);
  }
  return CompletionItemKind::Text;
}
