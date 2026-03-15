#ifndef LINENUMBERAREA_H
#define LINENUMBERAREA_H

#include <QColor>
#include <QFont>
#include <QList>
#include <QMap>
#include <QPair>
#include <QWidget>

#include "../../git/gitintegration.h"
#include "../../lsp/lspclient.h"

class TextArea;
class QPaintEvent;
class QContextMenuEvent;
class QMouseEvent;

class LineNumberArea : public QWidget {
  Q_OBJECT

public:
  explicit LineNumberArea(TextArea *editor, QWidget *parent = nullptr);

  QSize sizeHint() const override;

  int calculateWidth() const;

  void setFont(const QFont &font);

  void setBackgroundColor(const QColor &color);

  void setTextColor(const QColor &color);

  void setGitDiffLines(const QList<QPair<int, int>> &diffLines);

  void clearGitDiffLines();
  void setGitBlameLines(const QMap<int, QString> &blameLines);
  void clearGitBlameLines();

  void setRichBlameData(const QMap<int, GitBlameLineInfo> &blameData);

  void setGitIntegration(GitIntegration *git);

  void setHeatmapData(const QMap<int, qint64> &timestamps);

  void setHeatmapEnabled(bool enabled);
  bool isHeatmapEnabled() const;

  void setFoldingEnabled(bool enabled);

  void
  setDiagnosticLines(const QMap<int, LspDiagnosticSeverity> &diagnosticLines);
  void clearDiagnosticLines();

protected:
  void paintEvent(QPaintEvent *event) override;
  bool event(QEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  void updateBlameTextWidth();
  int lineAtPosition(int y) const;
  int numberAreaWidth() const;
  QString resolveFilePath() const;
  QString buildRichBlameTooltip(const GitBlameLineInfo &info) const;
  QString buildDiffHunkTooltip(const GitDiffHunk &hunk) const;
  QColor heatmapColor(qint64 timestamp) const;

  TextArea *m_editor;
  GitIntegration *m_gitIntegration;
  QFont m_font;
  QColor m_backgroundColor;
  QColor m_textColor;
  QList<QPair<int, int>> m_gitDiffLines;
  QMap<int, QString> m_gitBlameLines;
  QMap<int, GitBlameLineInfo> m_richBlameData;
  QMap<int, qint64> m_heatmapTimestamps;
  qint64 m_heatmapMinTimestamp = 0;
  qint64 m_heatmapMaxTimestamp = 0;
  bool m_heatmapRangeValid = false;
  bool m_heatmapEnabled;
  int m_blameTextWidth;
  bool m_foldingEnabled;

  QMap<int, LspDiagnosticSeverity> m_diagnosticLines;

  static constexpr int DIFF_INDICATOR_WIDTH = 3;
  static constexpr int BREAKPOINT_AREA_WIDTH = 16;
  static constexpr int FOLD_INDICATOR_WIDTH = 14;
  static constexpr int DIAGNOSTIC_MARKER_SIZE = 10;
  static constexpr int PADDING = 10;
  static constexpr int BLAME_PADDING = 12;
  static constexpr int MAX_BLAME_WIDTH = 280;
};

#endif
