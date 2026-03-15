#include "diagnosticsmanager.h"
#include "../core/logging/logger.h"
#include "diagnosticutils.h"

DiagnosticsManager::DiagnosticsManager(QObject *parent) : QObject(parent) {}

void DiagnosticsManager::upsertDiagnostics(
    const QString &uri, const QList<LspDiagnostic> &diagnostics,
    const QString &sourceId, int version) {

  if (version >= 0 && m_documentVersions.contains(uri)) {
    int currentVersion = m_documentVersions.value(uri);
    if (version < currentVersion) {
      LOG_DEBUG(QString("Dropping stale diagnostics for %1 (version %2 < %3)")
                    .arg(uri)
                    .arg(version)
                    .arg(currentVersion));
      return;
    }
  }

  QList<DiagnosticEntry> &entries = m_entriesByUri[uri];

  bool replaced = false;
  for (int i = 0; i < entries.size(); ++i) {
    if (entries[i].sourceId == sourceId) {
      entries[i].diagnostics = diagnostics;
      replaced = true;
      break;
    }
  }

  if (!replaced) {
    DiagnosticEntry entry;
    entry.diagnostics = diagnostics;
    entry.sourceId = sourceId;
    entries.append(entry);
  }

  recalcCounts();
  emit diagnosticsChanged(uri);
  emitFileCountsChanged(uri);

  LOG_DEBUG(QString("Diagnostics updated for %1 [%2]: %3 item(s)")
                .arg(uri, sourceId)
                .arg(diagnostics.size()));
}

void DiagnosticsManager::clearDiagnostics(const QString &uri) {
  if (!m_entriesByUri.contains(uri))
    return;

  m_entriesByUri.remove(uri);
  recalcCounts();
  emit diagnosticsChanged(uri);
  emitFileCountsChanged(uri);
}

void DiagnosticsManager::clearAllForSource(const QString &sourceId) {
  QStringList affectedUris;

  for (auto it = m_entriesByUri.begin(); it != m_entriesByUri.end();) {
    QList<DiagnosticEntry> &entries = it.value();
    for (int i = entries.size() - 1; i >= 0; --i) {
      if (entries[i].sourceId == sourceId) {
        entries.removeAt(i);
      }
    }
    if (entries.isEmpty()) {
      affectedUris.append(it.key());
      it = m_entriesByUri.erase(it);
    } else {
      affectedUris.append(it.key());
      ++it;
    }
  }

  recalcCounts();
  for (const QString &uri : affectedUris) {
    emit diagnosticsChanged(uri);
    emitFileCountsChanged(uri);
  }
}

QList<LspDiagnostic>
DiagnosticsManager::diagnosticsForFile(const QString &filePath) const {
  QString uri = DiagnosticUtils::filePathToUri(filePath);
  return diagnosticsForUri(uri);
}

QList<LspDiagnostic>
DiagnosticsManager::diagnosticsForUri(const QString &uri) const {
  QList<LspDiagnostic> result;
  if (!m_entriesByUri.contains(uri))
    return result;

  for (const DiagnosticEntry &entry : m_entriesByUri.value(uri)) {
    result.append(entry.diagnostics);
  }
  return result;
}

int DiagnosticsManager::errorCount() const { return m_errorCount; }
int DiagnosticsManager::warningCount() const { return m_warningCount; }
int DiagnosticsManager::infoCount() const { return m_infoCount; }

void DiagnosticsManager::trackDocumentVersion(const QString &uri, int version) {
  m_documentVersions[uri] = version;
}

int DiagnosticsManager::documentVersion(const QString &uri) const {
  return m_documentVersions.value(uri, 0);
}

QStringList DiagnosticsManager::allUris() const {
  return m_entriesByUri.keys();
}

void DiagnosticsManager::recalcCounts() {
  int oldErrors = m_errorCount;
  int oldWarnings = m_warningCount;
  int oldInfos = m_infoCount;

  m_errorCount = 0;
  m_warningCount = 0;
  m_infoCount = 0;

  for (auto it = m_entriesByUri.constBegin(); it != m_entriesByUri.constEnd();
       ++it) {
    for (const DiagnosticEntry &entry : it.value()) {
      for (const LspDiagnostic &diag : entry.diagnostics) {
        switch (diag.severity) {
        case LspDiagnosticSeverity::Error:
          ++m_errorCount;
          break;
        case LspDiagnosticSeverity::Warning:
          ++m_warningCount;
          break;
        case LspDiagnosticSeverity::Information:
        case LspDiagnosticSeverity::Hint:
          ++m_infoCount;
          break;
        }
      }
    }
  }

  if (m_errorCount != oldErrors || m_warningCount != oldWarnings ||
      m_infoCount != oldInfos) {
    emit countsChanged(m_errorCount, m_warningCount, m_infoCount);
  }
}

void DiagnosticsManager::emitFileCountsChanged(const QString &uri) {
  QString filePath = DiagnosticUtils::uriToFilePath(uri);
  int errors = 0, warnings = 0, infos = 0;

  if (m_entriesByUri.contains(uri)) {
    for (const DiagnosticEntry &entry : m_entriesByUri.value(uri)) {
      for (const LspDiagnostic &diag : entry.diagnostics) {
        switch (diag.severity) {
        case LspDiagnosticSeverity::Error:
          ++errors;
          break;
        case LspDiagnosticSeverity::Warning:
          ++warnings;
          break;
        case LspDiagnosticSeverity::Information:
        case LspDiagnosticSeverity::Hint:
          ++infos;
          break;
        }
      }
    }
  }

  emit fileCountsChanged(filePath, errors, warnings, infos);
}
