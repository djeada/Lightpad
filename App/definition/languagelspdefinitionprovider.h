#ifndef LANGUAGELSPDEFINITIONPROVIDER_H
#define LANGUAGELSPDEFINITIONPROVIDER_H

#include "idefinitionprovider.h"
#include "../lsp/lspclient.h"
#include <QStringList>
#include <QUrl>

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
  explicit LanguageLspDefinitionProvider(const LanguageServerConfig &config,
                                         QObject *parent = nullptr);
  ~LanguageLspDefinitionProvider() override;

  QString id() const override;
  bool supports(const QString &languageId) const override;
  int requestDefinition(const DefinitionRequest &req) override;

  bool isServerAvailable() const;
  QString serverCommand() const;
  QStringList supportedLanguages() const;

  static QList<LanguageServerConfig> defaultConfigs();

  static QString filePathToUri(const QString &filePath);
  static QString uriToFilePath(const QString &uri);

private:
  bool ensureServerStarted();

  LanguageServerConfig m_config;
  LspClient *m_client;
  int m_nextRequestId;
  int m_activeProviderRequestId;
  bool m_serverStartAttempted;
};

#endif
