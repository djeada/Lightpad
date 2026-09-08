#ifndef GITGRAPHWIDGET_H
#define GITGRAPHWIDGET_H

#include "../../git/gitintegration.h"
#include "../../git/gitrepositorystate.h"
#include "../../settings/theme.h"
#include <QWidget>

class QPaintEvent;
class QMouseEvent;
class QKeyEvent;
class QScrollBar;

struct GraphCommitNode {
  GitCommitInfo info;
  int column;
  QStringList parents;
  QColor color;
};

class GitGraphWidget : public QWidget {
  Q_OBJECT

public:
  explicit GitGraphWidget(GitIntegration *git, const Theme &theme,
                          QWidget *parent = nullptr);

  static constexpr int WIP_INDEX = -2;

  void loadGraph(int maxCount = 200);
  void setTheme(const Theme &theme);

  void setLogOptions(const GitLogOptions &options);
  GitLogOptions logOptions() const { return m_logOptions; }

  void setWorkingTreeState(const GitRepositoryState &state);
  bool hasWorkingTreeNode() const { return m_showWip; }

  void zoomIn();
  void zoomOut();
  void resetZoom();
  int rowHeight() const { return m_rowHeight; }

  QString compareAnchor() const { return m_compareAnchor; }
  void clearCompareAnchor();

  void setFilter(const QString &filter);
  QString filter() const { return m_filter; }

  int loadedCommitCount() const { return m_nodes.size(); }

  int matchingCommitCount() const;
  QString selectedHash() const;

  void selectCommit(const QString &hash, bool emitSignal = false);

signals:
  void commitSelected(const QString &hash);
  void workingTreeSelected();
  void compareRequested(const QString &fromHash, const QString &toHash);
  void compareAnchorChanged(const QString &hash);
  void commitDoubleClicked(const QString &hash);
  void commitsAppended(int totalLoaded);
  void historyExhausted();
  void contextMenuRequested(const QString &hash, const QPoint &globalPos);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void layoutGraph();
  void loadAnchorDecorations();
  void selectRow(int index, bool emitSignal);
  int totalRows() const;
  int rowForIndex(int index) const;
  int contentHeight() const;
  void requestMoreCommits();
  void loadRefDecorations();
  void drawRefBadges(QPainter &painter, const GraphCommitNode &node, int y,
                     const QFontMetrics &fm, const QColor &fgColor,
                     bool dimmed) const;
  bool rowMatches(int index) const;
  void rebuildFilterCache();
  int commitAtY(int y) const;
  QColor laneColor(int lane) const;
  void clampScrollOffset();
  void setScrollOffset(int offset);
  void syncScrollBarRange();
  void applyZoom(int deltaRows);
  void relayout();

  GitIntegration *m_git;
  Theme m_theme;
  QList<GraphCommitNode> m_nodes;
  QMap<QString, int> m_hashToIndex;
  QMap<QString, QList<GitRefDecoration>> m_refs;
  GitLogOptions m_logOptions;
  GitRepositoryState m_workingState;
  bool m_showWip = false;
  QString m_compareAnchor;
  bool m_loadingMore = false;
  bool m_historyExhausted = false;
  int m_pageSize = 100;
  int m_maxLanes;
  int m_scrollOffset;
  int m_selectedIndex;
  int m_hoverIndex = -1;

  QString m_filter;
  QVector<bool> m_filterCache;

  QScrollBar *m_scrollBar;
  bool m_syncingScrollBar = false;
  int m_rowHeight = 28;

  static constexpr int DEFAULT_ROW_HEIGHT = 28;
  static constexpr int MIN_ROW_HEIGHT = 16;
  static constexpr int MAX_ROW_HEIGHT = 48;
  static constexpr int LANE_WIDTH = 16;
  static constexpr int GRAPH_LEFT_MARGIN = 8;
  static constexpr int TEXT_LEFT_PADDING = 12;
  static constexpr int DOT_RADIUS = 4;
  static constexpr int SCROLLBAR_WIDTH = 12;

  static const QList<QColor> s_laneColors;
};

#endif
