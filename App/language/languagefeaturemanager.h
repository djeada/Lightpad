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
  QStringList environmentVariables;
  bool enabled = true;
};

enum class ServerHealthStatus { Unknown, Starting, Running, Error, Stopped };

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

  static QString detectProjectRoot(const QString &filePath);

  ServerHealthStatus serverHealth(const QString &languageId) const;
  DiagnosticsServerConfig serverConfig(const QString &languageId) const;
  QString lastServerError(const QString &languageId) const;
  void restartServer(const QString &languageId);

  void loadSettingsOverrides();

signals:
  void serverStarted(const QString &languageId);
  void serverError(const QString &languageId, const QString &message);
  void serverHealthChanged(const QString &languageId,
                           ServerHealthStatus status);

private:
  LspClient *ensureClient(const QString &languageId);
  DiagnosticsServerConfig configForLanguage(const QString &languageId) const;
  void onDiagnosticsReceived(const QString &languageId, const QString &uri,
                             const QList<LspDiagnostic> &diagnostics);

  struct PendingDocument {
    QString filePath;
    QString languageId;
    QString text;
  };

  void flushPendingDocuments(const QString &languageId);

  DiagnosticsManager *m_diagnosticsManager;
  QMap<QString, LspClient *> m_clients;
  QMap<QString, QString> m_fileToLanguage;
  QMap<QString, int> m_fileVersions;
  QList<DiagnosticsServerConfig> m_serverConfigs;
  QMap<QString, ServerHealthStatus> m_serverHealth;
  QMap<QString, QString> m_lastServerErrors;
  QMap<QString, QList<PendingDocument>> m_pendingDocuments;
};

#endif
