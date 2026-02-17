#ifndef LSPDEFINITIONPROVIDER_H
#define LSPDEFINITIONPROVIDER_H

#include "idefinitionprovider.h"
#include "../lsp/lspclient.h"
#include <QUrl>

class LspDefinitionProvider : public IDefinitionProvider {
  Q_OBJECT

public:
  explicit LspDefinitionProvider(LspClient *client,
                                 QObject *parent = nullptr);
  ~LspDefinitionProvider() override = default;

  QString id() const override;
  bool supports(const QString &languageId) const override;
  int requestDefinition(const DefinitionRequest &req) override;

  static QString filePathToUri(const QString &filePath);
  static QString uriToFilePath(const QString &uri);

private:
  LspClient *m_client;
  int m_nextRequestId;
  int m_activeProviderRequestId;
};

#endif
