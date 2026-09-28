#include "lspcompletionprovider.h"
#include "../../core/logging/logger.h"

LspCompletionProvider::LspCompletionProvider(LspClient *client, QObject *parent)
    : QObject(parent), m_client(client) {
  if (m_client) {
    connect(m_client, &LspClient::completionReceived, this,
            &LspCompletionProvider::onCompletionReceived);
    connect(m_client, &LspClient::requestFailed, this,
            &LspCompletionProvider::onRequestFailed);
  }
}

QStringList LspCompletionProvider::supportedLanguages() const { return {"*"}; }

QStringList LspCompletionProvider::triggerCharacters() const {

  return {".", "::", "->", "<"};
}

void LspCompletionProvider::setClient(LspClient *client) {
  if (m_client) {
    disconnect(m_client, &LspClient::completionReceived, this,
               &LspCompletionProvider::onCompletionReceived);
    disconnect(m_client, &LspClient::requestFailed, this,
               &LspCompletionProvider::onRequestFailed);
  }

  m_client = client;

  if (m_client) {
    connect(m_client, &LspClient::completionReceived, this,
            &LspCompletionProvider::onCompletionReceived);
    connect(m_client, &LspClient::requestFailed, this,
            &LspCompletionProvider::onRequestFailed);
  }
}

void LspCompletionProvider::requestCompletions(
    const CompletionContext &context,
    std::function<void(const QList<CompletionItem> &)> callback) {
  if (!isEnabled() || context.documentUri.isEmpty()) {
    callback({});
    return;
  }

  m_lastRequestId++;
  m_pendingCallbacks[m_lastRequestId] = callback;

  LspPosition pos;
  pos.line = context.line;
  pos.character = context.column;

  m_client->requestCompletion(context.documentUri, pos);
}

void LspCompletionProvider::resolveItem(
    const CompletionItem &item,
    std::function<void(const CompletionItem &)> callback) {

  callback(item);
}

void LspCompletionProvider::cancelPendingRequests() {

  m_pendingCallbacks.clear();
}

void LspCompletionProvider::onCompletionReceived(
    int requestId, const QList<LspCompletionItem> &items) {

  Q_UNUSED(requestId);

  QList<CompletionItem> completionItems;
  for (const LspCompletionItem &lspItem : items) {
    completionItems.append(convertItem(lspItem));
  }

  flushPendingCallbacks(completionItems);
}

void LspCompletionProvider::onRequestFailed(int requestId,
                                            const QString &method,
                                            const QString &message) {
  Q_UNUSED(requestId);
  Q_UNUSED(message);

  if (method != QLatin1String("textDocument/completion")) {
    return;
  }

  flushPendingCallbacks({});
}

void LspCompletionProvider::flushPendingCallbacks(
    const QList<CompletionItem> &items) {
  if (m_pendingCallbacks.isEmpty()) {
    return;
  }

  auto it = m_pendingCallbacks.end();
  --it;
  auto callback = it.value();

  m_pendingCallbacks.clear();
  callback(items);
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

  item.isSnippet = lspItem.insertTextFormat == 2;

  return item;
}

CompletionItemKind LspCompletionProvider::convertKind(int lspKind) const {

  if (lspKind >= 1 && lspKind <= 25) {
    return static_cast<CompletionItemKind>(lspKind);
  }
  return CompletionItemKind::Text;
}
