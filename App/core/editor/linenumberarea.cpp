#include "linenumberarea.h"
#include "../textarea.h"
#include <QContextMenuEvent>
#include <QHelpEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QToolTip>
#include <limits>

#include "../../core/lightpadpage.h"
#include "../../core/lightpadtabwidget.h"
#include "../../dap/breakpointmanager.h"
#include "../../ui/mainwindow.h"
#include "codefolding.h"

LineNumberArea::LineNumberArea(TextArea *editor, QWidget *parent)
    : QWidget(parent ? parent : editor), m_editor(editor),
      m_gitIntegration(nullptr), m_backgroundColor(QColor(40, 40, 40)),
      m_textColor(QColor(Qt::gray).lighter(150)), m_heatmapEnabled(false),
      m_blameTextWidth(0), m_foldingEnabled(true) {
  if (editor) {
    m_font = editor->font();
  }
}

QSize LineNumberArea::sizeHint() const { return QSize(calculateWidth(), 0); }

int LineNumberArea::calculateWidth() const {
  if (!m_editor) {
    return 0;
  }

  int digits = 1;
  int max = qMax(1, m_editor->blockCount());
  while (max >= 10) {
    max /= 10;
    ++digits;
  }

  int space = DIFF_INDICATOR_WIDTH + BREAKPOINT_AREA_WIDTH + PADDING +
              QFontMetrics(m_font).horizontalAdvance(QLatin1Char('9')) * digits;
  if (m_foldingEnabled) {
    space += FOLD_INDICATOR_WIDTH;
  }
  if (m_blameTextWidth > 0) {
    space += BLAME_PADDING + m_blameTextWidth;
  }
  return space;
}

void LineNumberArea::setFont(const QFont &font) {
  m_font = font;
  updateBlameTextWidth();
  update();
}

void LineNumberArea::setBackgroundColor(const QColor &color) {
  m_backgroundColor = color;
  update();
}

void LineNumberArea::setTextColor(const QColor &color) {
  m_textColor = color;
  update();
}

void LineNumberArea::setGitDiffLines(const QList<QPair<int, int>> &diffLines) {
  m_gitDiffLines = diffLines;
  update();
}

void LineNumberArea::clearGitDiffLines() {
  m_gitDiffLines.clear();
  update();
}

void LineNumberArea::setGitBlameLines(const QMap<int, QString> &blameLines) {
  m_gitBlameLines = blameLines;
  updateBlameTextWidth();
  updateGeometry();
  update();
}

void LineNumberArea::clearGitBlameLines() {
  m_gitBlameLines.clear();
  m_blameTextWidth = 0;
  updateGeometry();
  update();
  QToolTip::hideText();
}

void LineNumberArea::setFoldingEnabled(bool enabled) {
  m_foldingEnabled = enabled;
  updateGeometry();
  update();
}

void LineNumberArea::setRichBlameData(
    const QMap<int, GitBlameLineInfo> &blameData) {
  m_richBlameData = blameData;
}

void LineNumberArea::setGitIntegration(GitIntegration *git) {
  m_gitIntegration = git;
}

void LineNumberArea::setHeatmapData(const QMap<int, qint64> &timestamps) {
  m_heatmapTimestamps = timestamps;
  if (m_heatmapEnabled)
    update();
}

void LineNumberArea::setHeatmapEnabled(bool enabled) {
  m_heatmapEnabled = enabled;
  update();
}

bool LineNumberArea::isHeatmapEnabled() const { return m_heatmapEnabled; }

QColor LineNumberArea::heatmapColor(qint64 timestamp) const {
  if (m_heatmapTimestamps.isEmpty())
    return Qt::transparent;

  qint64 minTs = std::numeric_limits<qint64>::max();
  qint64 maxTs = 0;
  for (auto it = m_heatmapTimestamps.cbegin(); it != m_heatmapTimestamps.cend();
       ++it) {
    if (it.value() < minTs)
      minTs = it.value();
    if (it.value() > maxTs)
      maxTs = it.value();
  }

  if (maxTs == minTs)
    return QColor(80, 80, 120, 40);

  double t = double(timestamp - minTs) / double(maxTs - minTs);

  int r, g, b;
  if (t > 0.5) {

    double s = (t - 0.5) * 2.0;
    r = 200 + int(55 * s);
    g = 120 + int(80 * s);
    b = 50;
  } else {

    double s = t * 2.0;
    r = 60 + int(140 * s);
    g = 60 + int(60 * s);
    b = 120 - int(70 * s);
  }

  return QColor(r, g, b, 45);
}

QString
LineNumberArea::buildRichBlameTooltip(const GitBlameLineInfo &info) const {
  QString html = QStringLiteral(
      "<div style='font-family: sans-serif; padding: 4px;'>"
      "<div style='font-size: 13px; font-weight: bold; "
      "margin-bottom: 4px;'>%1</div>"
      "<div style='color: #aaa; font-size: 11px; margin-bottom: 6px;'>"
      "<b>%2</b> &lt;%3&gt;<br>"
      "%4 (%5)</div>"
      "<div style='font-size: 12px; padding: 4px; "
      "background: rgba(255,255,255,0.05); border-radius: 3px;'>%6</div>"
      "</div>");

  return html.arg(info.shortHash.toHtmlEscaped())
      .arg(info.author.toHtmlEscaped())
      .arg(info.authorEmail.toHtmlEscaped())
      .arg(info.date.toHtmlEscaped())
      .arg(info.relativeDate.toHtmlEscaped())
      .arg(info.summary.toHtmlEscaped());
}

QString LineNumberArea::buildDiffHunkTooltip(const GitDiffHunk &hunk) const {
  if (hunk.lines.isEmpty()) {
    return QString();
  }

  QString html =
      QStringLiteral("<div style='font-family: monospace; font-size: 11px; "
                     "white-space: pre; padding: 4px;'>");

  html +=
      QStringLiteral("<div style='color: #888; margin-bottom: 4px;'>%1</div>")
          .arg(hunk.header.toHtmlEscaped());

  for (const QString &line : hunk.lines) {
    QString escaped = line.toHtmlEscaped();
    if (!line.isEmpty() && line[0] == '+') {
      html += QStringLiteral("<div style='background: rgba(76,175,80,0.2); "
                             "color: #4caf50;'>%1</div>")
                  .arg(escaped);
    } else if (!line.isEmpty() && line[0] == '-') {
      html += QStringLiteral("<div style='background: rgba(244,67,54,0.2); "
                             "color: #f44336;'>%1</div>")
                  .arg(escaped);
    } else {
      html += QStringLiteral("<div>%1</div>").arg(escaped);
    }
  }

  html += QStringLiteral("</div>");
  return html;
}

void LineNumberArea::updateBlameTextWidth() {
  if (m_gitBlameLines.isEmpty()) {
    m_blameTextWidth = 0;
    return;
  }

  int maxWidth = 0;
  QFontMetrics metrics(m_font);
  for (auto it = m_gitBlameLines.cbegin(); it != m_gitBlameLines.cend(); ++it) {
    maxWidth = qMax(maxWidth, metrics.horizontalAdvance(it.value()));
  }
  m_blameTextWidth = qMin(maxWidth, MAX_BLAME_WIDTH);
}

int LineNumberArea::lineAtPosition(int y) const {
  if (!m_editor) {
    return -1;
  }

  QTextBlock block = m_editor->firstVisibleBlock();
  QRectF blockRect = m_editor->blockBoundingGeometry(block);
  blockRect.translate(m_editor->contentOffset());
  qreal top = blockRect.top();
  qreal bottom = top + blockRect.height();
  int blockNumber = block.blockNumber();

  while (block.isValid() && top <= height()) {
    if (block.isVisible() && y >= top && y <= bottom) {
      return blockNumber + 1;
    }
    block = block.next();
    top = bottom;
    if (!block.isValid()) {
      break;
    }
    blockRect = m_editor->blockBoundingGeometry(block);
    blockRect.translate(m_editor->contentOffset());
    bottom = top + blockRect.height();
    ++blockNumber;
  }

  return -1;
}

int LineNumberArea::numberAreaWidth() const {
  const int areaWidth = width();
  return areaWidth -
         (m_blameTextWidth > 0 ? (m_blameTextWidth + BLAME_PADDING) : 0);
}

QString LineNumberArea::resolveFilePath() const {
  if (!m_editor) {
    return QString();
  }
  return m_editor->resolveFilePath();
}

bool LineNumberArea::event(QEvent *event) {
  if (event->type() == QEvent::ToolTip && m_editor) {
    auto *helpEvent = static_cast<QHelpEvent *>(event);
    QTextBlock block = m_editor->firstVisibleBlock();
    QRectF blockRect = m_editor->blockBoundingGeometry(block);
    blockRect.translate(m_editor->contentOffset());
    qreal top = blockRect.top();
    qreal bottom = top + blockRect.height();
    int blockNumber = block.blockNumber();
    const int fontHeight = QFontMetrics(m_font).height();
    const int numArea = numberAreaWidth();

    while (block.isValid() && top <= height()) {
      if (block.isVisible() && helpEvent->pos().y() >= top &&
          helpEvent->pos().y() <= bottom) {
        int lineNum = blockNumber + 1;
        int hoverX = helpEvent->pos().x();

        if (hoverX < DIFF_INDICATOR_WIDTH + 2 && m_gitIntegration) {

          for (const auto &diffLine : m_gitDiffLines) {
            if (diffLine.first == lineNum) {
              QString filePath = resolveFilePath();
              if (!filePath.isEmpty()) {
                GitDiffHunk hunk =
                    m_gitIntegration->getDiffHunkAtLine(filePath, lineNum);
                QString tooltip = buildDiffHunkTooltip(hunk);
                if (!tooltip.isEmpty()) {
                  QToolTip::showText(helpEvent->globalPos(), tooltip, this);
                  return true;
                }
              }
              break;
            }
          }
        }

        if (hoverX > numArea) {
          auto richIt = m_richBlameData.find(lineNum);
          if (richIt != m_richBlameData.end()) {
            QString tooltip = buildRichBlameTooltip(richIt.value());

            if (m_gitIntegration) {
              QList<GitCommitFileStat> stats =
                  m_gitIntegration->getCommitFileStats(
                      richIt.value().shortHash);
              if (!stats.isEmpty()) {
                tooltip += QStringLiteral(
                    "<div style='margin-top: 6px; font-size: 11px; "
                    "color: #aaa; border-top: 1px solid #555; "
                    "padding-top: 4px;'>");
                int shown = 0;
                for (const auto &stat : stats) {
                  if (shown >= 8) {
                    tooltip +=
                        QStringLiteral("<div>... and %1 more files</div>")
                            .arg(stats.size() - shown);
                    break;
                  }
                  tooltip +=
                      QStringLiteral(
                          "<div><span style='color:#4caf50;'>+%1</span> "
                          "<span style='color:#f44336;'>-%2</span> %3</div>")
                          .arg(stat.additions)
                          .arg(stat.deletions)
                          .arg(stat.filePath.toHtmlEscaped());
                  ++shown;
                }
                tooltip += QStringLiteral("</div>");
              }
            }

            QToolTip::showText(helpEvent->globalPos(), tooltip, this);
          } else {

            auto it = m_gitBlameLines.find(lineNum);
            if (it != m_gitBlameLines.end()) {
              QToolTip::showText(helpEvent->globalPos(), it.value(), this);
            } else {
              QToolTip::hideText();
            }
          }
        } else {
          QToolTip::hideText();
        }
        return true;
      }

      block = block.next();
      top = bottom;
      if (!block.isValid()) {
        break;
      }
      blockRect = m_editor->blockBoundingGeometry(block);
      blockRect.translate(m_editor->contentOffset());
      bottom = top + blockRect.height();
      ++blockNumber;
    }

    QToolTip::hideText();
    return true;
  }

  return QWidget::event(event);
}

void LineNumberArea::contextMenuEvent(QContextMenuEvent *event) {
  if (!m_editor) {
    return;
  }

  int clickedLine = lineAtPosition(event->pos().y());

  if (clickedLine <= 0) {
    return;
  }

  QString filePath = resolveFilePath();

  QMenu menu(this);
  QAction *breakpointAction = menu.addAction(tr("Toggle Breakpoint"));
  menu.addSeparator();
  QAction *blameAction = menu.addAction(tr("Git Blame"));

  if (!filePath.isEmpty()) {
    bool hasBreakpoint =
        BreakpointManager::instance().hasBreakpoint(filePath, clickedLine);
    breakpointAction->setCheckable(true);
    breakpointAction->setChecked(hasBreakpoint);
  } else {
    breakpointAction->setEnabled(false);
  }

  MainWindow *mainWindow = m_editor->mainWindow;
  if (mainWindow && !filePath.isEmpty()) {
    bool enabled = mainWindow->isGitBlameEnabledForFile(filePath);
    blameAction->setCheckable(true);
    blameAction->setChecked(enabled);
  } else {
    blameAction->setEnabled(false);
  }

  QAction *selected = menu.exec(event->globalPos());
  if (!selected) {
    return;
  }

  if (selected == breakpointAction) {
    if (filePath.isEmpty()) {
      return;
    }
    BreakpointManager::instance().toggleBreakpoint(filePath, clickedLine);
  } else if (selected == blameAction && mainWindow) {
    bool enabled = mainWindow->isGitBlameEnabledForFile(filePath);
    mainWindow->showGitBlameForCurrentFile(!enabled);
    mainWindow->setGitBlameEnabledForFile(filePath, !enabled);
  }
}

void LineNumberArea::mousePressEvent(QMouseEvent *event) {
  if (!m_editor || event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  int clickedLine = lineAtPosition(event->pos().y());
  if (clickedLine <= 0) {
    QWidget::mousePressEvent(event);
    return;
  }

  if (m_foldingEnabled && m_editor->m_codeFolding) {
    int foldX = numberAreaWidth() - FOLD_INDICATOR_WIDTH;
    if (event->pos().x() >= foldX && event->pos().x() < numberAreaWidth()) {
      int blockNumber = clickedLine - 1;
      CodeFoldingManager *folding = m_editor->m_codeFolding;
      if (folding->isFoldable(blockNumber) || folding->isFolded(blockNumber)) {
        m_editor->toggleFoldAtLine(blockNumber);
        event->accept();
        return;
      }
    }
  }

  if (event->pos().x() > numberAreaWidth()) {
    QWidget::mousePressEvent(event);
    return;
  }

  QString filePath = resolveFilePath();
  if (filePath.isEmpty()) {
    QWidget::mousePressEvent(event);
    return;
  }

  BreakpointManager::instance().toggleBreakpoint(filePath, clickedLine);
  event->accept();
}

void LineNumberArea::paintEvent(QPaintEvent *event) {
  if (!m_editor) {
    return;
  }

  QPainter painter(this);
  painter.setFont(m_font);
  painter.fillRect(event->rect(), m_backgroundColor);

  QMap<int, int> diffLineMap;
  for (const auto &diffLine : m_gitDiffLines) {
    diffLineMap[diffLine.first] = diffLine.second;
  }

  QTextBlock block = m_editor->firstVisibleBlock();
  int blockNumber = block.blockNumber();

  QRectF blockRect = m_editor->blockBoundingGeometry(block);
  blockRect.translate(m_editor->contentOffset());
  qreal top = blockRect.top();
  qreal bottom = top + blockRect.height();

  const int fontHeight = QFontMetrics(m_font).height();
  const int areaWidth = width();
  const int numberArea = numberAreaWidth();
  const int numberStartX = DIFF_INDICATOR_WIDTH + BREAKPOINT_AREA_WIDTH;
  const int numberTextWidth =
      numberArea - numberStartX - (m_foldingEnabled ? FOLD_INDICATOR_WIDTH : 0);

  QString filePath = resolveFilePath();
  QMap<int, Breakpoint> breakpointLines;
  if (!filePath.isEmpty()) {
    const QList<Breakpoint> breakpoints =
        BreakpointManager::instance().breakpointsForFile(filePath);
    for (const Breakpoint &bp : breakpoints) {
      int displayLine =
          (bp.verified && bp.boundLine > 0) ? bp.boundLine : bp.line;
      if (displayLine <= 0) {
        continue;
      }
      if (!breakpointLines.contains(displayLine) || bp.enabled) {
        breakpointLines[displayLine] = bp;
      }
    }
  }

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      QString number = QString::number(blockNumber + 1);

      int lineNum = blockNumber + 1;
      if (m_heatmapEnabled && m_heatmapTimestamps.contains(lineNum)) {
        QColor heat = heatmapColor(m_heatmapTimestamps[lineNum]);
        painter.fillRect(DIFF_INDICATOR_WIDTH, static_cast<int>(top),
                         areaWidth - DIFF_INDICATOR_WIDTH,
                         static_cast<int>(bottom - top), heat);
      }

      painter.setPen(m_textColor);
      painter.drawText(numberStartX, top, numberTextWidth, fontHeight,
                       Qt::AlignCenter, number);

      if (diffLineMap.contains(lineNum)) {
        int diffType = diffLineMap[lineNum];
        QColor diffColor;
        switch (diffType) {
        case 0:
          diffColor = QColor(76, 175, 80);
          break;
        case 1:
          diffColor = QColor(33, 150, 243);
          break;
        default:
          diffColor = QColor(244, 67, 54);
          break;
        }
        painter.fillRect(0, static_cast<int>(top), DIFF_INDICATOR_WIDTH,
                         static_cast<int>(bottom - top), diffColor);
      }

      if (breakpointLines.contains(lineNum)) {
        const Breakpoint &bp = breakpointLines[lineNum];
        QColor baseColor(231, 76, 60);
        if (m_editor && m_editor->mainWindow) {
          baseColor = m_editor->mainWindow->getTheme().errorColor;
        }

        QColor markerColor = baseColor;
        if (!bp.enabled) {
          markerColor = QColor(140, 140, 140);
        } else if (!bp.verified) {
          markerColor = baseColor.lighter(115);
        }

        const int markerDiameter =
            qMax(6, qMin(fontHeight - 2, BREAKPOINT_AREA_WIDTH - 4));
        const int markerX =
            DIFF_INDICATOR_WIDTH + (BREAKPOINT_AREA_WIDTH - markerDiameter) / 2;
        const int markerY =
            static_cast<int>(top + (fontHeight - markerDiameter) / 2.0);

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(markerColor);
        painter.drawEllipse(markerX, markerY, markerDiameter, markerDiameter);
        painter.restore();
      }

      if (m_foldingEnabled && m_editor->m_codeFolding) {
        CodeFoldingManager *folding = m_editor->m_codeFolding;
        bool foldable = folding->isFoldable(blockNumber);
        bool folded = folding->isFolded(blockNumber);

        if (foldable || folded) {
          const int foldX = numberArea - FOLD_INDICATOR_WIDTH;
          const int indicatorSize =
              qMin(fontHeight - 4, FOLD_INDICATOR_WIDTH - 2);
          const int ix = foldX + (FOLD_INDICATOR_WIDTH - indicatorSize) / 2;
          const int iy =
              static_cast<int>(top + (fontHeight - indicatorSize) / 2.0);

          painter.save();
          painter.setRenderHint(QPainter::Antialiasing, true);
          painter.setPen(QPen(m_textColor, 1));
          painter.setBrush(Qt::NoBrush);
          painter.drawRect(ix, iy, indicatorSize, indicatorSize);

          int midY = iy + indicatorSize / 2;
          int midX = ix + indicatorSize / 2;
          int margin = 2;
          painter.drawLine(ix + margin, midY, ix + indicatorSize - margin,
                           midY);
          if (folded) {
            painter.drawLine(midX, iy + margin, midX,
                             iy + indicatorSize - margin);
          }
          painter.restore();
        }
      }

      if (m_blameTextWidth > 0) {
        auto it = m_gitBlameLines.find(lineNum);
        if (it != m_gitBlameLines.end()) {
          QRect blameRect(numberArea + BLAME_PADDING, static_cast<int>(top),
                          areaWidth - numberArea - BLAME_PADDING, fontHeight);
          painter.setPen(QColor(Qt::gray).lighter(120));
          painter.drawText(blameRect, Qt::AlignVCenter | Qt::AlignLeft,
                           QFontMetrics(m_font).elidedText(
                               it.value(), Qt::ElideRight, blameRect.width()));
        }
      }
    }

    block = block.next();
    top = bottom;
    if (!block.isValid()) {
      break;
    }
    blockRect = m_editor->blockBoundingGeometry(block);
    blockRect.translate(m_editor->contentOffset());
    bottom = top + blockRect.height();
    ++blockNumber;
  }
}
