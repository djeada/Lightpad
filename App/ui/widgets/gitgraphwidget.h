#ifndef GITGRAPHWIDGET_H
#define GITGRAPHWIDGET_H

#include "../../git/gitintegration.h"
#include "../../settings/theme.h"
#include <QWidget>

class QScrollBar;
class QPaintEvent;
class QMouseEvent;

/**
 * @brief Commit graph node with layout info for DAG rendering
 */
struct GraphCommitNode {
  GitCommitInfo info;
  int column;          ///< Lane column (0-based)
  QStringList parents; ///< Parent hashes
  QColor color;        ///< Branch color for this lane
};

/**
 * @brief DAG commit graph widget (GitLens-style)
 *
 * Renders a visual commit graph with branch lanes, merge lines,
 * and commit dots alongside commit metadata.
 */
class GitGraphWidget : public QWidget {
  Q_OBJECT

public:
  explicit GitGraphWidget(GitIntegration *git, const Theme &theme,
                          QWidget *parent = nullptr);

  void loadGraph(int maxCount = 200, const QString &branch = QString());
  void setTheme(const Theme &theme);

signals:
  void commitSelected(const QString &hash);
  void commitDoubleClicked(const QString &hash);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void layoutGraph();
  int commitAtY(int y) const;
  QColor laneColor(int lane) const;

  GitIntegration *m_git;
  Theme m_theme;
  QList<GraphCommitNode> m_nodes;
  QMap<QString, int> m_hashToIndex; ///< hash -> index in m_nodes
  int m_maxLanes;
  int m_scrollOffset;
  int m_selectedIndex;

  static constexpr int ROW_HEIGHT = 28;
  static constexpr int LANE_WIDTH = 16;
  static constexpr int GRAPH_LEFT_MARGIN = 8;
  static constexpr int TEXT_LEFT_PADDING = 12;
  static constexpr int DOT_RADIUS = 4;

  static const QList<QColor> s_laneColors;
};

#endif // GITGRAPHWIDGET_H
