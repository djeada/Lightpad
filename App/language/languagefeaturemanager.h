#ifndef LANGUAGEFEATUREMANAGER_H
#define LANGUAGEFEATUREMANAGER_H

#include "../diagnostics/diagnosticsmanager.h"
#include "../language/languagecatalog.h"
#include "../lsp/lspclient.h"
#include <QMap>
#include <QObject>
#include <QString>

struct DiagnosticsServerConfig {
  QString languageId;
  QString command;
  QStringList arguments;
};

class LanguageFeatureManager : public QObject {
  Q_OBJECT

public:
  explicit LanguageFeatureManager(DiagnosticsManager *diagnosticsManager,
                                  QObject *parent = nullptr);
  ~LanguageFeatureManager();

  void openDocument(const QString &filePath, const QString &languageId,
                    const QString &text);
  void changeDocument(const QString &filePath, int version,
                      const QString &text);
  void saveDocument(const QString &filePath);
  void closeDocument(const QString &filePath);

  LspClient *clientForFile(const QString &filePath) const;

  QString resolveLanguageId(const QString &filePath,
                            const QString &override = {}) const;

  bool isLanguageSupported(const QString &languageId) const;

  QStringList supportedLanguages() const;

  static QList<DiagnosticsServerConfig> defaultServerConfigs();

signals:
  void serverStarted(const QString &languageId);
  void serverError(const QString &languageId, const QString &message);

private:
  LspClient *ensureClient(const QString &languageId);
  DiagnosticsServerConfig configForLanguage(const QString &languageId) const;
  void onDiagnosticsReceived(const QString &languageId, const QString &uri,
                             const QList<LspDiagnostic> &diagnostics);

  DiagnosticsManager *m_diagnosticsManager;
  QMap<QString, LspClient *> m_clients;
  QMap<QString, QString> m_fileToLanguage;
  QMap<QString, int> m_fileVersions;
  QList<DiagnosticsServerConfig> m_serverConfigs;
};

#endif
