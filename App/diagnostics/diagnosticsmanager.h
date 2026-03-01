#ifndef DIAGNOSTICSMANAGER_H
#define DIAGNOSTICSMANAGER_H

#include "../lsp/lspclient.h"
#include <QMap>
#include <QObject>
#include <QString>

enum class DiagnosticOrigin { Lsp, CliAnalyzer, Manual };

struct DocumentState {
  QString uri;
  QString filePath;
  QString languageId;
  int version = 0;
  bool isOpen = false;
};

class DiagnosticsManager : public QObject {
  Q_OBJECT

public:
  explicit DiagnosticsManager(QObject *parent = nullptr);
  ~DiagnosticsManager() = default;

  void upsertDiagnostics(const QString &uri,
                         const QList<LspDiagnostic> &diagnostics,
                         const QString &sourceId, int version = -1);

  void clearDiagnostics(const QString &uri);

  void clearAllForSource(const QString &sourceId);

  QList<LspDiagnostic> diagnosticsForFile(const QString &filePath) const;

  QList<LspDiagnostic> diagnosticsForUri(const QString &uri) const;

  int errorCount() const;
  int warningCount() const;
  int infoCount() const;

  void trackDocumentVersion(const QString &uri, int version);

  int documentVersion(const QString &uri) const;

  QStringList allUris() const;

signals:
  void diagnosticsChanged(const QString &uri);
  void countsChanged(int errors, int warnings, int infos);
  void fileCountsChanged(const QString &filePath, int errorCount,
                         int warningCount, int infoCount);

private:
  struct DiagnosticEntry {
    QList<LspDiagnostic> diagnostics;
    QString sourceId;
  };

  void recalcCounts();
  void emitFileCountsChanged(const QString &uri);

  QMap<QString, QList<DiagnosticEntry>> m_entriesByUri;
  QMap<QString, int> m_documentVersions;

  int m_errorCount = 0;
  int m_warningCount = 0;
  int m_infoCount = 0;
};

#endif
