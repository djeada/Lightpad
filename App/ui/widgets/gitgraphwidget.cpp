#include "gitgraphwidget.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QToolTip>
#include <QWheelEvent>

const QList<QColor> GitGraphWidget::s_laneColors = {
    QColor(0x4E, 0xC9, 0xB0), QColor(0xCE, 0x91, 0x78),
    QColor(0x56, 0x9C, 0xD6), QColor(0xDC, 0xDC, 0xAA),
    QColor(0xC5, 0x86, 0xC0), QColor(0xD7, 0xBA, 0x7D),
    QColor(0x6A, 0x99, 0x55), QColor(0xD1, 0x6D, 0x6D),
};

GitGraphWidget::GitGraphWidget(GitIntegration *git, const Theme &theme,
                               QWidget *parent)
    : QWidget(parent), m_git(git), m_theme(theme), m_maxLanes(0),
      m_scrollOffset(0), m_selectedIndex(-1) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);

  m_scrollBar = new QScrollBar(Qt::Vertical, this);
  m_scrollBar->setRange(0, 0);
  m_scrollBar->setPageStep(height());
  connect(m_scrollBar, &QScrollBar::valueChanged, this, [&](int value) {
    if (m_syncingScrollBar) {
      return;
    }
    m_scrollOffset = value;
    update();
  });
}

void GitGraphWidget::setLogOptions(const GitLogOptions &options) {
  m_logOptions = options;
  loadGraph(qMax(m_pageSize, 200));
}

void GitGraphWidget::setWorkingTreeState(const GitRepositoryState &state) {
  m_workingState = state;
  const bool show = state.valid && state.hasChanges();
  if (show == m_showWip) {
    if (show) {
      update();
    }
    return;
  }
  m_showWip = show;
  relayout();
}

void GitGraphWidget::clearCompareAnchor() {
  if (m_compareAnchor.isEmpty()) {
    return;
  }
  m_compareAnchor.clear();
  emit compareAnchorChanged(QString());
  update();
}

int GitGraphWidget::totalRows() const {
  return m_nodes.size() + (m_showWip ? 1 : 0);
}

int GitGraphWidget::rowForIndex(int index) const {
  return index + (m_showWip ? 1 : 0);
}

int GitGraphWidget::contentHeight() const { return totalRows() * m_rowHeight; }

void GitGraphWidget::loadGraph(int maxCount) {
  m_nodes.clear();
  m_hashToIndex.clear();
  m_maxLanes = 0;
  m_selectedIndex = -1;
  m_hoverIndex = -1;
  m_loadingMore = false;
  m_historyExhausted = false;

  if (!m_git || !m_git->isValidRepository()) {
    syncScrollBarRange();
    update();
    return;
  }

  const int initialCount = qMax(1, maxCount);
  QList<GitCommitInfo> commits =
      m_git->getLogPage(m_logOptions, 0, initialCount);

  if (commits.size() < initialCount) {
    m_historyExhausted = true;
    emit historyExhausted();
  }

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

  loadRefDecorations();
  loadAnchorDecorations();

  layoutGraph();
  rebuildFilterCache();
  relayout();
}

void GitGraphWidget::setTheme(const Theme &theme) {
  m_theme = theme;
  update();
}

void GitGraphWidget::setFilter(const QString &filter) {
  if (m_filter == filter) {
    return;
  }
  m_filter = filter;
  rebuildFilterCache();
  update();
}

QString GitGraphWidget::selectedHash() const {
  if (m_selectedIndex >= 0 && m_selectedIndex < m_nodes.size()) {
    return m_nodes.at(m_selectedIndex).info.hash;
  }
  return QString();
}

void GitGraphWidget::selectCommit(const QString &hash, bool emitSignal) {
  auto it = m_hashToIndex.constFind(hash);
  if (it == m_hashToIndex.constEnd()) {
    return;
  }
  const int index = it.value();
  m_selectedIndex = index;

  const int rowTop = rowForIndex(index) * m_rowHeight;
  if (rowTop < m_scrollOffset ||
      rowTop + m_rowHeight > m_scrollOffset + height()) {
    setScrollOffset(qMax(0, rowTop - height() / 2 + m_rowHeight));
  }
  update();

  if (emitSignal) {
    emit commitSelected(hash);
  }
}

void GitGraphWidget::loadRefDecorations() {
  if (!m_git || !m_git->isValidRepository()) {
    m_refs.clear();
    return;
  }

  m_refs = m_git->getCommitRefsMap();
}

void GitGraphWidget::loadAnchorDecorations() {
  if (!m_git || !m_git->isValidRepository()) {
    return;
  }

  const QMap<QString, QStringList> worktrees = m_git->getWorktreeAnchors();
  for (auto it = worktrees.constBegin(); it != worktrees.constEnd(); ++it) {
    for (const QString &name : it.value()) {
      GitRefDecoration decoration;
      decoration.kind = GitRefDecoration::Kind::Worktree;
      decoration.name = name;
      m_refs[it.key()].append(decoration);
    }
  }

  const QMap<QString, QStringList> stashes = m_git->getStashAnchors();
  for (auto it = stashes.constBegin(); it != stashes.constEnd(); ++it) {
    for (const QString &name : it.value()) {
      GitRefDecoration decoration;
      decoration.kind = GitRefDecoration::Kind::Stash;
      decoration.name = name;
      m_refs[it.key()].append(decoration);
    }
  }
}

int GitGraphWidget::matchingCommitCount() const {
  if (m_filter.trimmed().isEmpty()) {
    return m_nodes.size();
  }
  int count = 0;
  for (int i = 0; i < m_nodes.size(); ++i) {
    if (rowMatches(i)) {
      ++count;
    }
  }
  return count;
}

bool GitGraphWidget::rowMatches(int index) const {
  if (index < 0 || index >= m_filterCache.size()) {
    return true;
  }
  return m_filterCache.at(index);
}

void GitGraphWidget::rebuildFilterCache() {
  const QString needle = m_filter.trimmed();
  m_filterCache.resize(m_nodes.size());

  for (int i = 0; i < m_nodes.size(); ++i) {
    if (needle.isEmpty()) {
      m_filterCache[i] = true;
      continue;
    }
    const GraphCommitNode &node = m_nodes.at(i);
    bool matches =
        node.info.subject.contains(needle, Qt::CaseInsensitive) ||
        node.info.shortHash.contains(needle, Qt::CaseInsensitive) ||
        node.info.hash.contains(needle, Qt::CaseInsensitive) ||
        node.info.author.contains(needle, Qt::CaseInsensitive) ||
        node.info.authorEmail.contains(needle, Qt::CaseInsensitive) ||
        node.info.date.contains(needle, Qt::CaseInsensitive) ||
        node.info.relativeDate.contains(needle, Qt::CaseInsensitive);

    if (!matches) {
      const auto refIt = m_refs.constFind(node.info.hash);
      if (refIt != m_refs.constEnd()) {
        for (const GitRefDecoration &decoration : *refIt) {
          if (decoration.name.contains(needle, Qt::CaseInsensitive)) {
            matches = true;
            break;
          }
        }
      }
    }
    m_filterCache[i] = matches;
  }
}

void GitGraphWidget::requestMoreCommits() {
  if (m_loadingMore || m_historyExhausted || !m_git ||
      !m_git->isValidRepository()) {
    return;
  }

  m_loadingMore = true;
  update();

  QPointer<GitGraphWidget> self(this);
  m_git->getLogPageAsync(m_logOptions, m_nodes.size(), m_pageSize,
                         [self](QList<GitCommitInfo> page) {
                           if (!self) {
                             return;
                           }

                           self->m_loadingMore = false;

                           if (page.isEmpty()) {
                             self->m_historyExhausted = true;
                             emit self->historyExhausted();
                             self->update();
                             return;
                           }

                           if (page.size() < self->m_pageSize) {
                             self->m_historyExhausted = true;
                             emit self->historyExhausted();
                           }

                           const int previousScroll = self->m_scrollOffset;
                           for (const GitCommitInfo &info : page) {
                             if (self->m_hashToIndex.contains(info.hash)) {
                               continue;
                             }
                             GraphCommitNode node;
                             node.info = info;
                             node.parents = info.parents;
                             node.column = 0;
                             node.color = Qt::white;
                             self->m_nodes.append(node);
                           }

                           self->m_hashToIndex.clear();
                           for (int i = 0; i < self->m_nodes.size(); ++i) {
                             self->m_hashToIndex[self->m_nodes[i].info.hash] =
                                 i;
                           }

                           self->layoutGraph();
                           self->rebuildFilterCache();
                           self->relayout();
                           self->setScrollOffset(previousScroll);
                           self->update();
                           emit self->commitsAppended(self->m_nodes.size());
                         });
}

void GitGraphWidget::layoutGraph() {

  QList<QString> activeLanes;

  for (int i = 0; i < m_nodes.size(); ++i) {
    GraphCommitNode &node = m_nodes[i];
    const QString &hash = node.info.hash;

    int col = -1;
    for (int l = 0; l < activeLanes.size(); ++l) {
      if (activeLanes[l] == hash) {
        col = l;
        break;
      }
    }

    if (col == -1) {

      col = activeLanes.indexOf(QString());
      if (col == -1) {
        col = activeLanes.size();
        activeLanes.append(QString());
      }
    }

    node.column = col;
    node.color = laneColor(col);

    if (!node.parents.isEmpty()) {
      activeLanes[col] = node.parents[0];
    } else {
      activeLanes[col] = QString();
    }

    for (int p = 1; p < node.parents.size(); ++p) {
      const QString &parentHash = node.parents[p];

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

    while (!activeLanes.isEmpty() && activeLanes.last().isEmpty())
      activeLanes.removeLast();

    if (activeLanes.size() > m_maxLanes)
      m_maxLanes = activeLanes.size();
  }
}

void GitGraphWidget::relayout() {
  setMinimumHeight(contentHeight());
  syncScrollBarRange();
  clampScrollOffset();
  update();
}

QColor GitGraphWidget::laneColor(int lane) const {
  return s_laneColors[lane % s_laneColors.size()];
}

int GitGraphWidget::commitAtY(int y) const {
  const int row = (y + m_scrollOffset) / m_rowHeight;
  if (row < 0) {
    return -1;
  }
  if (m_showWip && row == 0) {
    return WIP_INDEX;
  }
  const int idx = row - (m_showWip ? 1 : 0);
  if (idx >= 0 && idx < m_nodes.size())
    return idx;
  return -1;
}

void GitGraphWidget::clampScrollOffset() {
  setScrollOffset(
      qBound(0, m_scrollOffset, qMax(0, contentHeight() - height())));
}

void GitGraphWidget::setScrollOffset(int offset) {
  offset = qBound(0, offset, qMax(0, contentHeight() - height()));
  if (offset == m_scrollOffset) {
    return;
  }
  m_scrollOffset = offset;
  m_syncingScrollBar = true;
  m_scrollBar->setValue(offset);
  m_syncingScrollBar = false;
  update();
}

void GitGraphWidget::syncScrollBarRange() {
  const int maxExtent = qMax(0, contentHeight() - height());
  m_syncingScrollBar = true;
  m_scrollBar->setRange(0, maxExtent);
  m_scrollBar->setPageStep(qMax(1, height()));
  m_scrollBar->setSingleStep(m_rowHeight);
  m_scrollBar->setValue(m_scrollOffset);
  m_syncingScrollBar = false;
}

void GitGraphWidget::zoomIn() { applyZoom(4); }

void GitGraphWidget::zoomOut() { applyZoom(-4); }

void GitGraphWidget::resetZoom() {
  applyZoom(DEFAULT_ROW_HEIGHT - m_rowHeight);
}

void GitGraphWidget::applyZoom(int deltaRows) {
  const int oldRowHeight = m_rowHeight;
  m_rowHeight = qBound(MIN_ROW_HEIGHT, m_rowHeight + deltaRows, MAX_ROW_HEIGHT);
  if (m_rowHeight == oldRowHeight) {
    return;
  }

  const float ratio = float(m_rowHeight) / float(oldRowHeight);
  setScrollOffset(int(float(m_scrollOffset + height() / 2) * ratio) -
                  height() / 2);
  relayout();
}

void GitGraphWidget::drawRefBadges(QPainter &painter,
                                   const GraphCommitNode &node, int y,
                                   const QFontMetrics &fm,
                                   const QColor &fgColor, bool dimmed) const {
  auto it = m_refs.constFind(node.info.hash);
  if (it == m_refs.constEnd() || it->isEmpty()) {
    return;
  }

  QFont badgeFont = painter.font();
  badgeFont.setPointSize(qMax(7, badgeFont.pointSize() - 2));
  painter.setFont(badgeFont);
  const QFontMetrics badgeFm(badgeFont);

  QColor headBg = m_theme.accentColor;
  if (headBg.alpha() == 0 || headBg == m_theme.backgroundColor) {
    headBg = m_theme.highlightColor;
  }
  const QColor headFg =
      headBg.lightnessF() > 0.55 ? QColor(Qt::black) : QColor(Qt::white);

  QColor tagBg = fgColor;
  tagBg.setAlpha(dimmed ? 8 : 28);

  QColor remoteFg = fgColor;
  remoteFg.setAlpha(dimmed ? 45 : 150);

  QColor dimFg = fgColor;
  dimFg.setAlpha(dimmed ? 40 : 255);

  int x = GRAPH_LEFT_MARGIN + (m_maxLanes + 1) * LANE_WIDTH +
          TEXT_LEFT_PADDING + 72;

  const int pillHeight = qMin(m_rowHeight - 10, 20);

  for (const GitRefDecoration &decoration : *it) {

    QString glyph;
    switch (decoration.kind) {
    case GitRefDecoration::Kind::LocalBranch:
      glyph = QStringLiteral("⎇ ");
      break;
    case GitRefDecoration::Kind::RemoteBranch:
      glyph = QStringLiteral("☁ ");
      break;
    case GitRefDecoration::Kind::Tag:
      glyph = QStringLiteral("◆ ");
      break;
    case GitRefDecoration::Kind::Worktree:
      glyph = QStringLiteral("⌂ ");
      break;
    case GitRefDecoration::Kind::Stash:
      glyph = QStringLiteral("▤ ");
      break;
    }

    bool isHead = decoration.isHead;
    QString label = glyph + decoration.name;
    if (isHead) {
      label = tr("HEAD → ") + label;
    }

    int textWidth = badgeFm.horizontalAdvance(label);
    int pillWidth = textWidth + 14;

    QRect pillRect(x, y + (m_rowHeight - pillHeight) / 2, pillWidth,
                   pillHeight);

    switch (decoration.kind) {
    case GitRefDecoration::Kind::LocalBranch: {
      if (isHead) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(headBg);
      } else {
        QPen outlinePen(dimFg, 1.2);
        outlinePen.setStyle(Qt::SolidLine);
        painter.setPen(outlinePen);
        painter.setBrush(Qt::NoBrush);
      }
      painter.drawRoundedRect(pillRect, pillHeight / 2, pillHeight / 2);
      painter.setPen(isHead ? headFg : dimFg);
      painter.drawText(pillRect.adjusted(7, 0, -7, 0),
                       Qt::AlignVCenter | Qt::AlignLeft, label);
      break;
    }
    case GitRefDecoration::Kind::RemoteBranch: {
      QPen outlinePen(remoteFg, 1.0);
      outlinePen.setStyle(Qt::DashLine);
      painter.setPen(outlinePen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRoundedRect(pillRect, pillHeight / 2, pillHeight / 2);
      painter.setPen(remoteFg);
      painter.drawText(pillRect.adjusted(7, 0, -7, 0),
                       Qt::AlignVCenter | Qt::AlignLeft, label);
      break;
    }
    case GitRefDecoration::Kind::Tag: {
      painter.setPen(Qt::NoPen);
      painter.setBrush(tagBg);
      painter.drawRoundedRect(pillRect, pillHeight / 2, pillHeight / 2);
      painter.setPen(dimmed ? remoteFg : fgColor);
      painter.drawText(pillRect.adjusted(7, 0, -7, 0),
                       Qt::AlignVCenter | Qt::AlignLeft, label);
      break;
    }
    case GitRefDecoration::Kind::Worktree: {

      QPen outlinePen(dimFg, 1.2);
      painter.setPen(outlinePen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(pillRect);
      painter.setPen(dimFg);
      painter.drawText(pillRect.adjusted(7, 0, -7, 0),
                       Qt::AlignVCenter | Qt::AlignLeft, label);
      break;
    }
    case GitRefDecoration::Kind::Stash: {
      QPen outlinePen(remoteFg, 1.0, Qt::DotLine);
      painter.setPen(outlinePen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(pillRect);
      painter.setPen(remoteFg);
      painter.drawText(pillRect.adjusted(7, 0, -7, 0),
                       Qt::AlignVCenter | Qt::AlignLeft, label);
      break;
    }
    }

    x += pillWidth + 5;
  }
}

void GitGraphWidget::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QColor bgColor = m_theme.backgroundColor;
  QColor fgColor = m_theme.foregroundColor;
  QColor selColor = m_theme.highlightColor;

  painter.fillRect(rect(), bgColor);

  if (m_nodes.isEmpty()) {
    painter.setPen(QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 120));
    painter.setFont(font());
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("No commits to display.\nMake a commit or reload the "
                        "repository."));
    return;
  }

  const QColor muted(fgColor.red(), fgColor.green(), fgColor.blue(), 140);
  const QColor veryMuted(fgColor.red(), fgColor.green(), fgColor.blue(), 60);

  int graphWidth = GRAPH_LEFT_MARGIN + (m_maxLanes + 1) * LANE_WIDTH;
  int textX = graphWidth + TEXT_LEFT_PADDING;

  QFont commitFont = font();
  commitFont.setPointSize(10);
  QFont hashFont = commitFont;
  hashFont.setFamily("monospace");
  hashFont.setPointSize(9);
  QFontMetrics fm(commitFont);

  const int wipRows = m_showWip ? 1 : 0;
  int firstVisible = m_scrollOffset / m_rowHeight - wipRows;
  int lastVisible = (m_scrollOffset + height()) / m_rowHeight + 1 - wipRows;
  firstVisible = qMax(0, firstVisible);
  lastVisible = qMin(m_nodes.size() - 1, lastVisible);

  QPen edgePen;
  edgePen.setWidthF(m_rowHeight >= 24 ? 2.0 : 1.4);
  edgePen.setCapStyle(Qt::RoundCap);
  edgePen.setJoinStyle(Qt::RoundJoin);

  for (int i = firstVisible; i <= lastVisible; ++i) {
    const GraphCommitNode &node = m_nodes[i];
    const bool dimmed = !rowMatches(i);
    int y = rowForIndex(i) * m_rowHeight - m_scrollOffset + m_rowHeight / 2;
    int x = GRAPH_LEFT_MARGIN + node.column * LANE_WIDTH + LANE_WIDTH / 2;

    for (const QString &parentHash : node.parents) {
      auto it = m_hashToIndex.find(parentHash);
      if (it == m_hashToIndex.end())
        continue;

      int parentIdx = it.value();
      const GraphCommitNode &parentNode = m_nodes[parentIdx];
      int py = rowForIndex(parentIdx) * m_rowHeight - m_scrollOffset +
               m_rowHeight / 2;
      int px =
          GRAPH_LEFT_MARGIN + parentNode.column * LANE_WIDTH + LANE_WIDTH / 2;

      edgePen.setColor(node.color);
      painter.setPen(edgePen);

      if (node.column == parentNode.column) {
        painter.drawLine(x, y, px, py);
      } else {
        QPainterPath path;
        path.moveTo(x, y);
        int midY = (y + py) / 2;
        path.cubicTo(x, midY, px, midY, px, py);
        painter.drawPath(path);
      }
    }
    Q_UNUSED(dimmed);
  }

  for (int i = firstVisible; i <= lastVisible; ++i) {
    const GraphCommitNode &node = m_nodes[i];
    const bool selected = (i == m_selectedIndex);
    const bool hovered = (i == m_hoverIndex);
    const bool dimmed = !rowMatches(i);

    int y = rowForIndex(i) * m_rowHeight - m_scrollOffset;
    int cx = GRAPH_LEFT_MARGIN + node.column * LANE_WIDTH + LANE_WIDTH / 2;
    int cy = y + m_rowHeight / 2;

    if (hovered && !selected) {
      QColor hoverFill = fgColor;
      hoverFill.setAlpha(14);
      painter.fillRect(0, y, width(), m_rowHeight, hoverFill);
    }

    if (selected) {
      QColor selectionFill = selColor;
      selectionFill.setAlpha(52);
      painter.fillRect(0, y, width(), m_rowHeight, selectionFill);

      painter.fillRect(0, y, 3, m_rowHeight, selColor);
    }

    bool isMerge = node.parents.size() > 1;

    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawEllipse(QPointF(cx, cy), DOT_RADIUS + 1.5, DOT_RADIUS + 1.5);

    painter.setBrush(node.color);
    if (dimmed) {
      QColor dimDot = node.color;
      dimDot.setAlpha(50);
      painter.setBrush(dimDot);
    }

    if (isMerge) {
      QPolygonF diamond;
      diamond << QPointF(cx, cy - DOT_RADIUS - 1)
              << QPointF(cx + DOT_RADIUS + 1, cy)
              << QPointF(cx, cy + DOT_RADIUS + 1)
              << QPointF(cx - DOT_RADIUS - 1, cy);
      painter.drawPolygon(diamond);
    } else {
      painter.drawEllipse(QPointF(cx, cy), DOT_RADIUS, DOT_RADIUS);
    }

    auto refIt = m_refs.constFind(node.info.hash);
    const bool isHead =
        refIt != m_refs.constEnd() &&
        std::any_of(refIt->cbegin(), refIt->cend(),
                    [](const GitRefDecoration &d) { return d.isHead; });
    if (isHead) {
      QPen headPen(selColor, 2.0);
      painter.setPen(headPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(QPointF(cx, cy), DOT_RADIUS + 3.5, DOT_RADIUS + 3.5);
    }

    if (!m_compareAnchor.isEmpty() && node.info.hash == m_compareAnchor) {
      painter.setPen(QPen(m_theme.warningColor, 2.0));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(QRectF(cx - DOT_RADIUS - 5, cy - DOT_RADIUS - 5,
                              2 * (DOT_RADIUS + 5), 2 * (DOT_RADIUS + 5)));
      painter.setFont(hashFont);
      painter.setPen(m_theme.warningColor);
      painter.drawText(0, y, GRAPH_LEFT_MARGIN + LANE_WIDTH, m_rowHeight,
                       Qt::AlignVCenter | Qt::AlignLeft, tr("A"));
    }

    painter.setFont(hashFont);
    painter.setPen(
        dimmed ? veryMuted
               : QColor(fgColor.red(), fgColor.green(), fgColor.blue(), 150));
    painter.drawText(textX, y, 70, m_rowHeight, Qt::AlignVCenter,
                     node.info.shortHash);

    drawRefBadges(painter, node, y, fm, fgColor, dimmed);

    painter.setFont(commitFont);
    painter.setPen(dimmed ? veryMuted : fgColor);
    int authorX = width() - 250;
    int subjectX = textX + 75;

    if (refIt != m_refs.constEnd() && !refIt->isEmpty()) {
      QFont badgeFont = commitFont;
      badgeFont.setPointSize(qMax(7, badgeFont.pointSize() - 2));
      const QFontMetrics badgeFm(badgeFont);
      for (const GitRefDecoration &decoration : *refIt) {

        int width = badgeFm.horizontalAdvance(decoration.name) +
                    badgeFm.horizontalAdvance(QStringLiteral("⎇ "));
        if (decoration.isHead) {
          width += badgeFm.horizontalAdvance(tr("HEAD → "));
        }
        subjectX += width + 19;
      }
    }

    int subjectW = authorX - subjectX - 10;
    if (subjectW > 40) {
      QString elided =
          fm.elidedText(node.info.subject, Qt::ElideRight, subjectW);
      painter.drawText(subjectX, y, subjectW, m_rowHeight, Qt::AlignVCenter,
                       elided);
    }

    painter.setPen(dimmed ? veryMuted : muted);
    QString meta = node.info.author + "  " + node.info.relativeDate;
    painter.drawText(authorX, y, 240, m_rowHeight, Qt::AlignVCenter,
                     fm.elidedText(meta, Qt::ElideRight, 240));
  }

  if (m_showWip) {

    const int y = -m_scrollOffset;
    if (y + m_rowHeight > 0 && y < height()) {
      const int lane = m_nodes.isEmpty() ? 0 : m_nodes.first().column;
      const int cx = GRAPH_LEFT_MARGIN + lane * LANE_WIDTH + LANE_WIDTH / 2;
      const int cy = y + m_rowHeight / 2;

      if (m_selectedIndex == WIP_INDEX) {
        QColor selectionFill = selColor;
        selectionFill.setAlpha(52);
        painter.fillRect(0, y, width(), m_rowHeight, selectionFill);
        painter.fillRect(0, y, 3, m_rowHeight, selColor);
      } else if (m_hoverIndex == WIP_INDEX) {
        QColor hoverFill = fgColor;
        hoverFill.setAlpha(14);
        painter.fillRect(0, y, width(), m_rowHeight, hoverFill);
      }

      if (!m_nodes.isEmpty()) {
        QPen dashed(m_theme.gitModifiedColor, 1.6, Qt::DashLine);
        painter.setPen(dashed);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(cx, cy, cx, y + m_rowHeight + m_rowHeight / 2);
      }

      painter.setBrush(bgColor);
      painter.setPen(QPen(m_theme.gitModifiedColor, 1.8, Qt::DashLine));
      painter.drawEllipse(QPointF(cx, cy), DOT_RADIUS + 1, DOT_RADIUS + 1);

      painter.setFont(hashFont);
      painter.setPen(m_theme.gitModifiedColor);
      painter.drawText(textX, y, 70, m_rowHeight, Qt::AlignVCenter, tr("WIP"));

      painter.setFont(commitFont);
      painter.setPen(m_theme.gitModifiedColor);
      QStringList bits;
      if (m_workingState.stagedCount > 0) {
        bits << tr("%1 staged").arg(m_workingState.stagedCount);
      }
      if (m_workingState.workingTreeCount() > 0) {
        bits << tr("%1 unstaged").arg(m_workingState.workingTreeCount());
      }
      if (m_workingState.conflictedCount > 0) {
        bits << tr("%1 conflicted").arg(m_workingState.conflictedCount);
      }
      const QString label =
          bits.isEmpty() ? tr("Uncommitted changes")
                         : tr("Uncommitted changes — %1").arg(bits.join(", "));
      painter.drawText(
          textX + 75, y, qMax(40, width() - textX - 90), m_rowHeight,
          Qt::AlignVCenter,
          fm.elidedText(label, Qt::ElideRight, qMax(40, width() - textX - 90)));
    }
  }

  painter.setFont(commitFont);
  if (m_loadingMore) {
    painter.setPen(muted);
    painter.drawText(
        QRect(0, contentHeight() - m_scrollOffset, width(), m_rowHeight),
        Qt::AlignCenter, tr("Loading more history…"));
  } else if (m_historyExhausted &&
             m_scrollOffset + height() >= contentHeight()) {
    painter.setPen(veryMuted);
    painter.drawText(
        QRect(0, contentHeight() - m_scrollOffset, width(), m_rowHeight),
        Qt::AlignCenter, tr("Beginning of history"));
  }
}

void GitGraphWidget::selectRow(int index, bool emitSignal) {
  if (index == m_selectedIndex) {
    return;
  }
  m_selectedIndex = index;

  const int row = index == WIP_INDEX ? 0 : rowForIndex(index);
  const int rowTop = row * m_rowHeight;
  if (rowTop < m_scrollOffset ||
      rowTop + m_rowHeight > m_scrollOffset + height()) {
    setScrollOffset(rowTop - height() / 2 + m_rowHeight);
  }
  update();

  if (!emitSignal) {
    return;
  }
  if (index == WIP_INDEX) {
    emit workingTreeSelected();
  } else if (index >= 0 && index < m_nodes.size()) {
    emit commitSelected(m_nodes[index].info.hash);
  }
}

void GitGraphWidget::mousePressEvent(QMouseEvent *event) {
  setFocus();
  const int idx = commitAtY(event->pos().y());

  if (event->modifiers().testFlag(Qt::ControlModifier) && idx >= 0 &&
      idx < m_nodes.size()) {
    const QString hash = m_nodes[idx].info.hash;
    if (m_compareAnchor.isEmpty()) {
      m_compareAnchor = hash;
      emit compareAnchorChanged(m_compareAnchor);
      update();
    } else if (m_compareAnchor == hash) {
      clearCompareAnchor();
    } else {
      const QString from = m_compareAnchor;
      clearCompareAnchor();
      emit compareRequested(from, hash);
    }
    return;
  }

  selectRow(idx, true);
}

void GitGraphWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  int idx = commitAtY(event->pos().y());
  if (idx >= 0)
    emit commitDoubleClicked(m_nodes[idx].info.hash);
}

void GitGraphWidget::mouseMoveEvent(QMouseEvent *event) {
  const int idx = commitAtY(event->pos().y());
  if (idx != m_hoverIndex) {
    m_hoverIndex = idx;
    update();
  }

  if (idx == WIP_INDEX) {
    QToolTip::showText(event->globalPosition().toPoint(),
                       tr("Uncommitted changes — not part of the commit graph "
                          "until you commit them."),
                       this);
    return;
  }

  if (idx >= 0) {
    const GraphCommitNode &node = m_nodes[idx];
    QString tip = node.info.subject;
    tip +=
        QLatin1Char('\n') + node.info.author + " · " + node.info.relativeDate;
    tip += QLatin1Char('\n') + node.info.hash;
    QToolTip::showText(event->globalPosition().toPoint(), tip, this);
  } else {
    QToolTip::hideText();
  }
}

void GitGraphWidget::leaveEvent(QEvent *) {
  if (m_hoverIndex != -1) {
    m_hoverIndex = -1;
    update();
  }
  QToolTip::hideText();
}

void GitGraphWidget::contextMenuEvent(QContextMenuEvent *event) {
  const int idx = commitAtY(event->pos().y());
  if (idx < 0 || idx >= m_nodes.size()) {
    return;
  }
  selectRow(idx, true);
  emit contextMenuRequested(m_nodes[idx].info.hash, event->globalPos());
}

void GitGraphWidget::wheelEvent(QWheelEvent *event) {
  if (event->modifiers() & Qt::ControlModifier) {
    applyZoom(event->angleDelta().y() > 0 ? 4 : -4);
    event->accept();
    return;
  }

  setScrollOffset(m_scrollOffset - event->angleDelta().y());
  update();

  const int totalHeight = contentHeight();
  const int nearEndThreshold = totalHeight - (height() * 3 / 2);
  if (m_scrollOffset >= nearEndThreshold && totalHeight > 0) {
    requestMoreCommits();
  }
}

void GitGraphWidget::keyPressEvent(QKeyEvent *event) {
  Q_UNUSED(m_rowHeight);
  const int firstRow = m_showWip ? WIP_INDEX : 0;
  switch (event->key()) {
  case Qt::Key_Down:
    if (m_selectedIndex == WIP_INDEX) {
      selectRow(0, true);
    } else if (m_selectedIndex < 0) {
      selectRow(firstRow, true);
    } else if (m_selectedIndex + 1 < m_nodes.size()) {
      selectRow(m_selectedIndex + 1, true);
    }
    break;
  case Qt::Key_Up:
    if (m_selectedIndex == 0 && m_showWip) {
      selectRow(WIP_INDEX, true);
    } else if (m_selectedIndex > 0) {
      selectRow(m_selectedIndex - 1, true);
    }
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    if (m_selectedIndex >= 0 && m_selectedIndex < m_nodes.size()) {
      emit commitDoubleClicked(m_nodes[m_selectedIndex].info.hash);
    }
    break;
  case Qt::Key_Space:

    if (m_selectedIndex >= 0 && m_selectedIndex < m_nodes.size()) {
      const QString hash = m_nodes[m_selectedIndex].info.hash;
      if (m_compareAnchor.isEmpty()) {
        m_compareAnchor = hash;
        emit compareAnchorChanged(m_compareAnchor);
      } else if (m_compareAnchor == hash) {
        clearCompareAnchor();
      } else {
        const QString from = m_compareAnchor;
        clearCompareAnchor();
        emit compareRequested(from, hash);
      }
    }
    break;
  case Qt::Key_PageDown:
    setScrollOffset(m_scrollOffset + height() * 3 / 4);
    break;
  case Qt::Key_PageUp:
    setScrollOffset(m_scrollOffset - height() * 3 / 4);
    break;
  case Qt::Key_Home:
    setScrollOffset(0);
    break;
  case Qt::Key_End:
    setScrollOffset(contentHeight());
    break;
  default:
    QWidget::keyPressEvent(event);
    return;
  }

  update();
  event->accept();

  const int totalHeight = contentHeight();
  const int nearEndThreshold = totalHeight - (height() * 3 / 2);
  if (m_scrollOffset >= nearEndThreshold && totalHeight > 0) {
    requestMoreCommits();
  }
}

void GitGraphWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  m_scrollBar->setGeometry(width() - SCROLLBAR_WIDTH, 0, SCROLLBAR_WIDTH,
                           height());
  m_scrollBar->setPageStep(qMax(1, height()));
  clampScrollOffset();
}
