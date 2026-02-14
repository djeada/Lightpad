#ifndef PROBLEMSPANEL_H
#define PROBLEMSPANEL_H

#include "../../lsp/lspclient.h"
#include "../../settings/theme.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

class ProblemsPanel : public QWidget {
  Q_OBJECT

public:
  explicit ProblemsPanel(QWidget *parent = nullptr);
  ~ProblemsPanel();

  void setDiagnostics(const QString &uri,
                      const QList<LspDiagnostic> &diagnostics);

  void clearAll();

  void clearFile(const QString &uri);

  int totalCount() const;

  int errorCount() const;
  int warningCount() const;
  int infoCount() const;

  int problemCountForFile(const QString &filePath) const;

  int errorCountForFile(const QString &filePath) const;

  int warningCountForFile(const QString &filePath) const;

  bool isAutoRefreshEnabled() const;

  void setAutoRefreshEnabled(bool enabled);

  void applyTheme(const Theme &theme);

  void onFileSaved(const QString &filePath);

signals:

  void problemClicked(const QString &filePath, int line, int column);

  void countsChanged(int errors, int warnings, int infos);

  void fileCountsChanged(const QString &filePath, int errorCount,
                         int warningCount, int infoCount);

  void refreshRequested(const QString &filePath);

private slots:
  void onItemDoubleClicked(QTreeWidgetItem *item, int column);
  void onFilterChanged(int index);
  void onAutoRefreshToggled(bool checked);

private:
  void setupUI();
  void updateCounts();
  void rebuildTree();
  QString severityIcon(LspDiagnosticSeverity severity) const;
  QString severityText(LspDiagnosticSeverity severity) const;
  const QList<LspDiagnostic> *
  findDiagnosticsForFile(const QString &filePath) const;

  QTreeWidget *m_tree;
  QWidget *m_header;
  QLabel *m_titleLabel;
  QLabel *m_statusLabel;
  QComboBox *m_filterCombo;
  QCheckBox *m_autoRefreshCheckBox;

  QMap<QString, QList<LspDiagnostic>> m_diagnostics;

  int m_errorCount;
  int m_warningCount;
  int m_infoCount;
  int m_hintCount;

  int m_currentFilter;
  bool m_autoRefreshEnabled;
  Theme m_theme;
};

#endif
