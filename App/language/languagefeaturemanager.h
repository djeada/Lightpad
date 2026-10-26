#ifndef LANGUAGEFEATUREMANAGER_H
#define LANGUAGEFEATUREMANAGER_H

#include "../diagnostics/diagnosticsmanager.h"
#include "../language/languagecatalog.h"
#include "../lsp/lspclient.h"
#include "../python/pythonprojectenvironment.h"
#include <QMap>
#include <QObject>
#include <QString>
#include <functional>

struct DiagnosticsServerConfig {
  QString languageId;
  QString command;
  QStringList arguments;
  QStringList environmentVariables;
  bool enabled = true;
  QJsonObject settings;
  QJsonObject initializationOptions;
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
  void openExternalDocument(const QString &filePath, const QString &uri,
                            const QString &sourceFile, const QString &text);
  QString documentUriForFile(const QString &filePath) const;
  bool isExternalDocument(const QString &filePath) const;

  LspClient *clientForFile(const QString &filePath) const;

  QString resolveLanguageId(const QString &filePath,
                            const QString &override = {}) const;

  bool isLanguageSupported(const QString &languageId) const;

  QStringList supportedLanguages() const;

  static QList<DiagnosticsServerConfig> defaultServerConfigs();

  static QString detectProjectRoot(const QString &filePath,
                                   const QString &workspaceFolder = {});

  ServerHealthStatus serverHealth(const QString &languageId) const;
  DiagnosticsServerConfig serverConfig(const QString &languageId) const;
  QString lastServerError(const QString &languageId) const;
  void restartServer(const QString &languageId);
  void setServerConfig(const DiagnosticsServerConfig &config);

  void loadSettingsOverrides();
  void setWorkspaceFolder(const QString &folder);
  void setPythonEnvironmentResolver(
      std::function<PythonEnvironmentPreference(const QString &)> resolver);

signals:
  void serverStarted(const QString &languageId);
  void serverError(const QString &languageId, const QString &message);

  void serverUnavailable(const QString &languageId, const QString &message);
  void serverHealthChanged(const QString &languageId,
                           ServerHealthStatus status);

private:
  LspClient *ensureClient(const QString &languageId, const QString &filePath);
  DiagnosticsServerConfig configForLanguage(const QString &languageId) const;
  void onDiagnosticsReceived(const QString &languageId, const QString &uri,
                             const QList<LspDiagnostic> &diagnostics,
                             int version);
  void onClientLost(const QString &clientKey, LspClient *client);

  void flushPendingDocuments(const QString &languageId);
  void queuePendingDocument(const QString &languageId, const QString &filePath);
  bool isPendingDocument(const QString &languageId,
                         const QString &filePath) const;
  void detachDocument(const QString &filePath);
  QStringList trackedFilesForLanguage(const QString &languageId) const;

  DiagnosticsManager *m_diagnosticsManager;
  QMap<QString, LspClient *> m_clients;
  QMap<QString, QString> m_clientLanguages;
  QMap<QString, QString> m_fileToClient;
  QMap<QString, QString> m_documentUris;
  QMap<QString, QString> m_externalSources;
  QMap<QString, QString> m_fileToLanguage;
  QMap<QString, int> m_fileVersions;
  QMap<QString, QString> m_fileTexts;
  QList<DiagnosticsServerConfig> m_serverConfigs;
  QMap<QString, ServerHealthStatus> m_serverHealth;
  QMap<QString, QString> m_lastServerErrors;
  QMap<QString, QStringList> m_pendingDocuments;
  QString m_workspaceFolder;
  std::function<PythonEnvironmentPreference(const QString &)>
      m_pythonEnvironmentResolver;
};

#endif
