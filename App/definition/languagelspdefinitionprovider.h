#ifndef LANGUAGELSPDEFINITIONPROVIDER_H
#define LANGUAGELSPDEFINITIONPROVIDER_H

#include "../lsp/lspclient.h"
#include "idefinitionprovider.h"
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <functional>

struct LanguageServerConfig {
  QString providerId;
  QString displayName;
  QStringList supportedLanguages;
  QString serverCommand;
  QStringList serverArguments;
};

class LanguageLspDefinitionProvider : public IDefinitionProvider {
  Q_OBJECT

public:
  using ClientResolver = std::function<LspClient *(const QString &filePath)>;

  explicit LanguageLspDefinitionProvider(const LanguageServerConfig &config,
                                         QObject *parent = nullptr);
  ~LanguageLspDefinitionProvider() override;

  QString id() const override;
  bool supports(const QString &languageId) const override;
  int requestDefinition(const DefinitionRequest &req) override;

  void setClientResolver(ClientResolver resolver);

  bool isServerAvailable() const;
  QString serverCommand() const;
  QStringList supportedLanguages() const;

  static QList<LanguageServerConfig> defaultConfigs();

  static QString filePathToUri(const QString &filePath);
  static QString uriToFilePath(const QString &uri);

private slots:
  void onDefinitionReceived(int lspRequestId,
                            const QList<LspLocation> &locations);
  void onRequestFailed(int lspRequestId, const QString &method,
                       const QString &message);

private:
  void failLater(int providerRequestId, const QString &message);
  void attachClient(LspClient *client);

  LanguageServerConfig m_config;
  ClientResolver m_clientResolver;
  QPointer<LspClient> m_activeClient;
  int m_nextRequestId;
  int m_activeProviderRequestId;
  int m_activeLspRequestId;
};

#endif
