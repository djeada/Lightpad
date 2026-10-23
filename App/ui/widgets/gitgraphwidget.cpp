#include "gitgraphwidget.h"
#include "../../theme/colorcontrast.h"
#include "../uistylehelper.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScrollBar>
#include <QToolTip>
#include <QWheelEvent>

GitGraphWidget::GitGraphWidget(GitIntegration *git, const Theme &theme,
                               QWidget *parent)
    : QWidget(parent), m_git(git), m_theme(theme), m_maxLanes(0),
      m_scrollOffset(0), m_selectedIndex(-1) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumHeight(140);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  refreshThemeColors();

  m_scrollBar = new QScrollBar(Qt::Vertical, this);
  m_scrollBar->setRange(0, 0);
  m_scrollBar->setPageStep(height());
  connect(m_scrollBar, &QScrollBar::valueChanged, this, [&](int value) {
    if (m_syncingScrollBar) {
      return;
    }
    m_scrollOffset = value;
    update();
    if (m_scrollOffset + height() >= contentHeight() - m_rowHeight * 5)
      requestMoreCommits();
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

void GitGraphWidget::setCompareAnchor(const QString &hash) {
  if (!m_hashToIndex.contains(hash) || m_compareAnchor == hash)
    return;
  m_compareAnchor = hash;
  emit compareAnchorChanged(hash);
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
  m_scrollOffset = 0;
  m_loadingMore = false;
  m_historyExhausted = false;
  ++m_loadGeneration;

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
    if (m_logOptions.firstParentOnly && node.parents.size() > 1)
      node.parents = {node.parents.first()};
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
  refreshThemeColors();
  for (GraphCommitNode &node : m_nodes) {
    node.color = laneColor(node.column);
  }
  update();
}

void GitGraphWidget::refreshThemeColors() {
  const QColor bg = m_theme.backgroundColor;
  m_laneColors = UIStyleHelper::seriesColors(m_theme);
  m_mutedText = UIStyleHelper::mutedTextColor(m_theme);
  m_secondaryText = UIStyleHelper::secondaryTextColor(m_theme);
  m_dimmedText = ColorContrast::ensure(
      ColorContrast::mix(m_theme.foregroundColor, bg, 0.65), bg, 2.0);
  m_warningText = ColorContrast::ensure(m_theme.warningColor, bg);
  m_modifiedText = ColorContrast::ensure(m_theme.gitModifiedColor, bg);
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

QList<GitCommitInfo> GitGraphWidget::loadedCommits() const {
  QList<GitCommitInfo> commits;
  commits.reserve(m_nodes.size());
  for (const auto &node : m_nodes)
    commits << node.info;
  return commits;
}

bool GitGraphWidget::matchesCommit(const QString &hash) const {
  const auto it = m_hashToIndex.constFind(hash);
  return it != m_hashToIndex.constEnd() && rowMatches(it.value());
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
  const quint64 generation = m_loadGeneration;
  m_git->getLogPageAsync(
      m_logOptions, m_nodes.size(), m_pageSize,
      [self, generation](QList<GitCommitInfo> page) {
        if (!self || self->m_loadGeneration != generation) {
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
          if (self->m_logOptions.firstParentOnly && node.parents.size() > 1)
            node.parents = {node.parents.first()};
          node.column = 0;
          node.color = Qt::white;
          self->m_nodes.append(node);
        }

        self->m_hashToIndex.clear();
        for (int i = 0; i < self->m_nodes.size(); ++i) {
          self->m_hashToIndex[self->m_nodes[i].info.hash] = i;
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
  syncScrollBarRange();
  clampScrollOffset();
  update();
}

QColor GitGraphWidget::laneColor(int lane) const {
  if (m_laneColors.isEmpty()) {
    return m_theme.foregroundColor;
  }
  return m_laneColors[lane % m_laneColors.size()];
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

int GitGraphWidget::drawRefBadges(QPainter &painter,
                                  const GraphCommitNode &node, int y,
                                  int startX, int maxX, const QColor &fgColor,
                                  bool dimmed) const {
  auto it = m_refs.constFind(node.info.hash);
  if (it == m_refs.constEnd() || it->isEmpty()) {
    return startX;
  }

  QFont badgeFont = painter.font();
  badgeFont.setPointSize(qMax(8, badgeFont.pointSize() - 1));
  painter.setFont(badgeFont);
  const QFontMetrics badgeFm(badgeFont);

  QColor headBg = m_theme.accentColor;
  if (headBg.alpha() == 0 || headBg == m_theme.backgroundColor) {
    headBg = m_theme.highlightColor;
  }
  const QColor headFg = UIStyleHelper::readableText(m_theme, headBg);

  QColor tagBg = fgColor;
  tagBg.setAlpha(dimmed ? 8 : 28);
  const QColor tagFg =
      dimmed
          ? m_dimmedText
          : UIStyleHelper::readableText(
                m_theme, ColorContrast::flatten(tagBg, m_theme.backgroundColor),
                fgColor);

  const QColor remoteFg = dimmed ? m_dimmedText : m_secondaryText;
  const QColor dimFg = dimmed ? m_dimmedText : fgColor;

  int x = startX;
  int drawn = 0;

  const int pillHeight = qMin(m_rowHeight - 2, 22);

  for (const GitRefDecoration &decoration : *it) {
    const bool more = drawn + 1 < it->size();
    const int available = maxX - x - (more ? 35 : 0);
    if (available < 45 || drawn >= 2)
      break;

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

    label = badgeFm.elidedText(label, Qt::ElideMiddle, available - 14);
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
      painter.setPen(tagFg);
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
    ++drawn;
  }
  if (drawn < it->size() && maxX - x >= 25) {
    painter.setPen(remoteFg);
    painter.drawText(QRect(x, y, maxX - x, m_rowHeight), Qt::AlignVCenter,
                     QStringLiteral("+%1").arg(it->size() - drawn));
    x += 30;
  }
  return x == startX ? startX : x + 8;
}

void GitGraphWidget::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QColor bgColor = m_theme.backgroundColor;
  QColor fgColor = m_theme.foregroundColor;
  QColor selColor = m_theme.accentColor;

  painter.fillRect(rect(), bgColor);

  if (m_nodes.isEmpty() && !m_showWip) {
    painter.setPen(m_mutedText);
    painter.setFont(font());
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("No commits to display.\nMake a commit or reload the "
                        "repository."));
    return;
  }

  const QColor muted = m_mutedText;
  const QColor veryMuted = m_dimmedText;

  const int laneWidth =
      qMin(LANE_WIDTH, qMax(6, (width() / 4 - GRAPH_LEFT_MARGIN) /
                                   qMax(1, m_maxLanes + 1)));
  int graphWidth = GRAPH_LEFT_MARGIN + (m_maxLanes + 1) * laneWidth;
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

  for (int i = 0; i < m_nodes.size(); ++i) {
    const GraphCommitNode &node = m_nodes[i];
    const bool dimmed = !rowMatches(i);
    int y = rowForIndex(i) * m_rowHeight - m_scrollOffset + m_rowHeight / 2;
    int x = GRAPH_LEFT_MARGIN + node.column * laneWidth + laneWidth / 2;

    for (const QString &parentHash : node.parents) {
      auto it = m_hashToIndex.find(parentHash);
      if (it == m_hashToIndex.end())
        continue;

      int parentIdx = it.value();
      const GraphCommitNode &parentNode = m_nodes[parentIdx];
      int py = rowForIndex(parentIdx) * m_rowHeight - m_scrollOffset +
               m_rowHeight / 2;
      if ((y < 0 && py < 0) || (y > height() && py > height()))
        continue;
      int px =
          GRAPH_LEFT_MARGIN + parentNode.column * laneWidth + laneWidth / 2;

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
    int cx = GRAPH_LEFT_MARGIN + node.column * laneWidth + laneWidth / 2;
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
      painter.setPen(m_warningText);
      painter.drawText(0, y, GRAPH_LEFT_MARGIN + LANE_WIDTH, m_rowHeight,
                       Qt::AlignVCenter | Qt::AlignLeft, tr("A"));
    }

    painter.setFont(hashFont);
    painter.setPen(dimmed ? veryMuted : m_secondaryText);
    painter.drawText(textX, y, 70, m_rowHeight, Qt::AlignVCenter,
                     node.info.shortHash);

    painter.setFont(commitFont);
    const int authorWidth = qBound(130, width() / 5, 240);
    const int authorX = width() - authorWidth - SCROLLBAR_WIDTH - 12;
    const int badgesX = textX + 75;
    const int badgeBudget = qMin(260, qMax(0, (authorX - badgesX) / 2));
    const int subjectX = drawRefBadges(painter, node, y, badgesX,
                                       badgesX + badgeBudget, fgColor, dimmed);
    painter.setFont(commitFont);
    painter.setPen(dimmed ? veryMuted : fgColor);

    int subjectW = authorX - subjectX - 10;
    if (subjectW > 40) {
      QString elided =
          fm.elidedText(node.info.subject, Qt::ElideRight, subjectW);
      painter.drawText(subjectX, y, subjectW, m_rowHeight, Qt::AlignVCenter,
                       elided);
    }

    painter.setPen(dimmed ? veryMuted : muted);
    QString meta = node.info.author + "  " + node.info.relativeDate;
    painter.drawText(authorX, y, authorWidth, m_rowHeight, Qt::AlignVCenter,
                     fm.elidedText(meta, Qt::ElideRight, authorWidth));
  }

  if (m_showWip) {

    const int y = -m_scrollOffset;
    if (y + m_rowHeight > 0 && y < height()) {
      int headIndex = -1;
      for (int i = 0; i < m_nodes.size(); ++i) {
        const auto decorations = m_refs.value(m_nodes[i].info.hash);
        if (std::any_of(
                decorations.cbegin(), decorations.cend(),
                [](const GitRefDecoration &ref) { return ref.isHead; })) {
          headIndex = i;
          break;
        }
      }
      const int lane = headIndex < 0 ? 0 : m_nodes[headIndex].column;
      const int cx = GRAPH_LEFT_MARGIN + lane * laneWidth + laneWidth / 2;
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

      if (headIndex >= 0) {
        QPen dashed(m_theme.gitModifiedColor, 1.6, Qt::DashLine);
        painter.setPen(dashed);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(cx, cy, cx,
                         rowForIndex(headIndex) * m_rowHeight - m_scrollOffset +
                             m_rowHeight / 2);
      }

      painter.setBrush(bgColor);
      painter.setPen(QPen(m_theme.gitModifiedColor, 1.8, Qt::DashLine));
      painter.drawEllipse(QPointF(cx, cy), DOT_RADIUS + 1, DOT_RADIUS + 1);

      painter.setFont(hashFont);
      painter.setPen(m_modifiedText);
      painter.drawText(textX, y, 70, m_rowHeight, Qt::AlignVCenter, tr("WIP"));

      painter.setFont(commitFont);
      painter.setPen(m_modifiedText);
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
    painter.setPen(muted);
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
    for (const auto &ref : m_refs.value(node.info.hash))
      tip += QLatin1Char('\n') + ref.name;
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
  syncScrollBarRange();
  clampScrollOffset();
  if (m_selectedIndex >= 0)
    selectCommit(selectedHash());
}
