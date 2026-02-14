#ifndef LSPCOMPLETIONPROVIDER_H
#define LSPCOMPLETIONPROVIDER_H

#include "../../lsp/lspclient.h"
#include "../icompletionprovider.h"
#include <QMap>
#include <functional>

class LspCompletionProvider : public QObject, public ICompletionProvider {
  Q_OBJECT

public:
  explicit LspCompletionProvider(LspClient *client, QObject *parent = nullptr);

  QString id() const override { return "lsp"; }
  QString displayName() const override { return "Language Server"; }
  int basePriority() const override { return 10; }
  QStringList supportedLanguages() const override;

  QStringList triggerCharacters() const override;
  int minimumPrefixLength() const override { return 0; }

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

  void setClient(LspClient *client);

  LspClient *client() const { return m_client; }

private slots:
  void onCompletionReceived(int requestId,
                            const QList<LspCompletionItem> &items);

private:
  CompletionItem convertItem(const LspCompletionItem &lspItem) const;
  CompletionItemKind convertKind(int lspKind) const;

  LspClient *m_client;
  bool m_enabled = true;

  QMap<int, std::function<void(const QList<CompletionItem> &)>>
      m_pendingCallbacks;
  int m_lastRequestId = 0;
};

#endif
