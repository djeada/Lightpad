#include "gitgraphwidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

const QList<QColor> GitGraphWidget::s_laneColors = {
    QColor(0x4E, 0xC9, 0xB0), // teal
    QColor(0xCE, 0x91, 0x78), // salmon
    QColor(0x56, 0x9C, 0xD6), // blue
    QColor(0xDC, 0xDC, 0xAA), // yellow
    QColor(0xC5, 0x86, 0xC0), // purple
    QColor(0xD7, 0xBA, 0x7D), // gold
    QColor(0x6A, 0x99, 0x55), // green
    QColor(0xD1, 0x6D, 0x6D), // red
};

GitGraphWidget::GitGraphWidget(GitIntegration *git, const Theme &theme,
                               QWidget *parent)
    : QWidget(parent), m_git(git), m_theme(theme), m_maxLanes(0),
      m_scrollOffset(0), m_selectedIndex(-1) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
}

void GitGraphWidget::loadGraph(int maxCount, const QString &branch) {
  m_nodes.clear();
  m_hashToIndex.clear();
  m_maxLanes = 0;
  m_selectedIndex = -1;
  m_scrollOffset = 0;

  if (!m_git || !m_git->isValidRepository())
    return;

  QList<GitCommitInfo> commits = m_git->getCommitLog(maxCount, branch);
  m_nodes.reserve(commits.size());
  for (int i = 0; i < commits.size(); ++i) {
    GraphCommitNode node;
    node.info = commits[i];
    node.parents = commits[i].parents;
    node.column = 0;
    node.color = Qt::white;
    m_nodes.append(node);
    m_hashToIndex[commits[i].hash] = i;
  }

  layoutGraph();
  setMinimumHeight(m_nodes.size() * ROW_HEIGHT);
  update();
}

void GitGraphWidget::setTheme(const Theme &theme) {
  m_theme = theme;
  update();
}

void GitGraphWidget::layoutGraph() {
  // Lane assignment algorithm: active lanes track which commit hash
  // is expected next in each lane.
  QList<QString> activeLanes; // lane -> next expected hash

  for (int i = 0; i < m_nodes.size(); ++i) {
    GraphCommitNode &node = m_nodes[i];
    const QString &hash = node.info.hash;

    // Find if this commit is already expected in a lane
    int col = -1;
    for (int l = 0; l < activeLanes.size(); ++l) {
      if (activeLanes[l] == hash) {
        col = l;
        break;
      }
    }

    if (col == -1) {
      // New branch: find first empty lane or append
      col = activeLanes.indexOf(QString());
      if (col == -1) {
        col = activeLanes.size();
        activeLanes.append(QString());
      }
    }

    node.column = col;
    node.color = laneColor(col);

    // Replace this lane with first parent (continuation)
    if (!node.parents.isEmpty()) {
      activeLanes[col] = node.parents[0];
    } else {
      activeLanes[col] = QString(); // root commit, lane ends
    }

    // Additional parents get their own lanes (merge edges)
    for (int p = 1; p < node.parents.size(); ++p) {
      const QString &parentHash = node.parents[p];
      // Check if parent already occupies a lane
      bool found = false;
      for (int l = 0; l < activeLanes.size(); ++l) {
        if (activeLanes[l] == parentHash) {
          found = true;
          break;
        }
      }
      if (!found) {
        int emptyLane = activeLanes.indexOf(QString());
        if (emptyLane == -1) {
          activeLanes.append(parentHash);
        } else {
          activeLanes[emptyLane] = parentHash;
        }
      }
    }

    // Compact: remove trailing empty lanes
    while (!activeLanes.isEmpty() && activeLanes.last().isEmpty())
      activeLanes.removeLast();

    if (activeLanes.size() > m_maxLanes)
      m_maxLanes = activeLanes.size();
  }
}

QColor GitGraphWidget::laneColor(int lane) const {
  return s_laneColors[lane % s_laneColors.size()];
}

int GitGraphWidget::commitAtY(int y) const {
  int idx = (y + m_scrollOffset) / ROW_HEIGHT;
  if (idx >= 0 && idx < m_nodes.size())
    return idx;
  return -1;
}

void GitGraphWidget::paintEvent(QPaintEvent * /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QColor bgColor = m_theme.backgroundColor;
  QColor fgColor = m_theme.foregroundColor;
  QColor selColor = m_theme.highlightColor;
  selColor.setAlpha(60);

  painter.fillRect(rect(), bgColor);

  int graphWidth = GRAPH_LEFT_MARGIN + (m_maxLanes + 1) * LANE_WIDTH;
  int textX = graphWidth + TEXT_LEFT_PADDING;

  QFont commitFont = font();
  commitFont.setPointSize(10);
  QFont hashFont = commitFont;
  hashFont.setFamily("monospace");
  hashFont.setPointSize(9);
  QFontMetrics fm(commitFont);

  int firstVisible = m_scrollOffset / ROW_HEIGHT;
  int lastVisible = (m_scrollOffset + height()) / ROW_HEIGHT + 1;
  firstVisible = qMax(0, firstVisible);
  lastVisible = qMin(m_nodes.size() - 1, lastVisible);

  // Draw edges first (behind dots)
  for (int i = firstVisible; i <= lastVisible; ++i) {
    const GraphCommitNode &node = m_nodes[i];
    int y = i * ROW_HEIGHT - m_scrollOffset + ROW_HEIGHT / 2;
    int x = GRAPH_LEFT_MARGIN + node.column * LANE_WIDTH + LANE_WIDTH / 2;

    for (const QString &parentHash : node.parents) {
      auto it = m_hashToIndex.find(parentHash);
      if (it == m_hashToIndex.end())
        continue;

      int parentIdx = it.value();
      const GraphCommitNode &parentNode = m_nodes[parentIdx];
      int py = parentIdx * ROW_HEIGHT - m_scrollOffset + ROW_HEIGHT / 2;
      int px =
          GRAPH_LEFT_MARGIN + parentNode.column * LANE_WIDTH + LANE_WIDTH / 2;

      QPen edgePen(node.color, 1.5);
      painter.setPen(edgePen);

      if (node.column == parentNode.column) {
        // Straight line down
        painter.drawLine(x, y, px, py);
      } else {
        // Curved merge/branch line
        QPainterPath path;
        path.moveTo(x, y);
        int midY = (y + py) / 2;
        path.cubicTo(x, midY, px, midY, px, py);
        painter.drawPath(path);
      }
    }
  }

  // Draw commit dots and text
  for (int i = firstVisible; i <= lastVisible; ++i) {
    const GraphCommitNode &node = m_nodes[i];
    int y = i * ROW_HEIGHT - m_scrollOffset;
    int cx = GRAPH_LEFT_MARGIN + node.column * LANE_WIDTH + LANE_WIDTH / 2;
    int cy = y + ROW_HEIGHT / 2;

    // Selection highlight
    if (i == m_selectedIndex) {
      painter.fillRect(0, y, width(), ROW_HEIGHT, selColor);
    }

    // Commit dot
    bool isMerge = node.parents.size() > 1;
    painter.setPen(Qt::NoPen);
    painter.setBrush(node.color);
    if (isMerge) {
      // Diamond for merge commits
      QPolygonF diamond;
      diamond << QPointF(cx, cy - DOT_RADIUS - 1)
              << QPointF(cx + DOT_RADIUS + 1, cy)
              << QPointF(cx, cy + DOT_RADIUS + 1)
              << QPointF(cx - DOT_RADIUS - 1, cy);
      painter.drawPolygon(diamond);
    } else {
      painter.drawEllipse(QPointF(cx, cy), DOT_RADIUS, DOT_RADIUS);
    }

    // Hash
    painter.setFont(hashFont);
    painter.setPen(QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 150));
    painter.drawText(textX, y, 70, ROW_HEIGHT, Qt::AlignVCenter,
                     node.info.shortHash);

    // Subject
    painter.setFont(commitFont);
    painter.setPen(fgColor);
    int subjectX = textX + 75;
    int authorX = width() - 250;
    int subjectW = authorX - subjectX - 10;
    QString elided =
        fm.elidedText(node.info.subject, Qt::ElideRight, subjectW);
    painter.drawText(subjectX, y, subjectW, ROW_HEIGHT, Qt::AlignVCenter,
                     elided);

    // Author + date
    painter.setPen(QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 140));
    QString meta = node.info.author + "  " + node.info.relativeDate;
    painter.drawText(authorX, y, 240, ROW_HEIGHT, Qt::AlignVCenter,
                     fm.elidedText(meta, Qt::ElideRight, 240));
  }
}

void GitGraphWidget::mousePressEvent(QMouseEvent *event) {
  int idx = commitAtY(event->pos().y());
  if (idx != m_selectedIndex) {
    m_selectedIndex = idx;
    update();
    if (idx >= 0)
      emit commitSelected(m_nodes[idx].info.hash);
  }
}

void GitGraphWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  int idx = commitAtY(event->pos().y());
  if (idx >= 0)
    emit commitDoubleClicked(m_nodes[idx].info.hash);
}

void GitGraphWidget::wheelEvent(QWheelEvent *event) {
  int delta = event->angleDelta().y();
  m_scrollOffset -= delta;
  m_scrollOffset =
      qBound(0, m_scrollOffset, qMax(0, m_nodes.size() * ROW_HEIGHT - height()));
  update();
}

void GitGraphWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  m_scrollOffset =
      qBound(0, m_scrollOffset, qMax(0, m_nodes.size() * ROW_HEIGHT - height()));
}
