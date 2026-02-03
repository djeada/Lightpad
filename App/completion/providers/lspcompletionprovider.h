#ifndef LSPCOMPLETIONPROVIDER_H
#define LSPCOMPLETIONPROVIDER_H

#include "../../lsp/lspclient.h"
#include "../icompletionprovider.h"
#include <QMap>
#include <functional>

/**
 * @brief Completion provider using Language Server Protocol
 *
 * Bridges the LspClient to the completion system, providing
 * context-aware completions from language servers.
 */
class LspCompletionProvider : public QObject, public ICompletionProvider {
  Q_OBJECT

public:
  explicit LspCompletionProvider(LspClient *client, QObject *parent = nullptr);

  QString id() const override { return "lsp"; }
  QString displayName() const override { return "Language Server"; }
  int basePriority() const override { return 10; } // Highest priority
  QStringList supportedLanguages() const override;

  QStringList triggerCharacters() const override;
  int minimumPrefixLength() const override {
    return 0;
  } // LSP handles its own triggering

  void requestCompletions(
      const CompletionContext &context,
      std::function<void(const QList<CompletionItem> &)> callback) override;

  void
  resolveItem(const CompletionItem &item,
              std::function<void(const CompletionItem &)> callback) override;

  void cancelPendingRequests() override;

  bool isEnabled() const override {
    return m_enabled && m_client && m_client->isReady();
  }
  void setEnabled(bool enabled) override { m_enabled = enabled; }

  /**
   * @brief Set the LspClient to use
   */
  void setClient(LspClient *client);

  /**
   * @brief Get the current LspClient
   */
  LspClient *client() const { return m_client; }

private slots:
  void onCompletionReceived(int requestId,
                            const QList<LspCompletionItem> &items);

private:
  CompletionItem convertItem(const LspCompletionItem &lspItem) const;
  CompletionItemKind convertKind(int lspKind) const;

  LspClient *m_client;
  bool m_enabled = true;

  // Map request ID to callback
  QMap<int, std::function<void(const QList<CompletionItem> &)>>
      m_pendingCallbacks;
  int m_lastRequestId = 0;
};

#endif // LSPCOMPLETIONPROVIDER_H
