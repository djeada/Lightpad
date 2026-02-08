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

#include "../../core/lightpadpage.h"
#include "../../core/lightpadtabwidget.h"
#include "../../dap/breakpointmanager.h"
#include "../../ui/mainwindow.h"
#include "codefolding.h"

LineNumberArea::LineNumberArea(TextArea *editor, QWidget *parent)
    : QWidget(parent ? parent : editor), m_editor(editor),
      m_backgroundColor(QColor(40, 40, 40)),
      m_textColor(QColor(Qt::gray).lighter(150)), m_blameTextWidth(0),
      m_foldingEnabled(true) {
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

    while (block.isValid() && top <= height()) {
      if (block.isVisible() && helpEvent->pos().y() >= top &&
          helpEvent->pos().y() <= bottom) {
        int lineNum = blockNumber + 1;
        auto it = m_gitBlameLines.find(lineNum);
        if (it != m_gitBlameLines.end()) {
          QToolTip::showText(helpEvent->globalPos(), it.value(), this);
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

  // Check if click is in folding indicator area
  if (m_foldingEnabled && m_editor->m_codeFolding) {
    int foldX = numberAreaWidth() - FOLD_INDICATOR_WIDTH;
    if (event->pos().x() >= foldX && event->pos().x() < numberAreaWidth()) {
      int blockNumber = clickedLine - 1; // Convert to 0-based
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

  // Build a map for quick git diff lookup
  QMap<int, int> diffLineMap;
  for (const auto &diffLine : m_gitDiffLines) {
    diffLineMap[diffLine.first] = diffLine.second;
  }

  QTextBlock block = m_editor->firstVisibleBlock();
  int blockNumber = block.blockNumber();

  // Use TextArea's public/protected methods via friendship
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
      painter.setPen(m_textColor);
      painter.drawText(numberStartX, top, numberTextWidth, fontHeight,
                       Qt::AlignCenter, number);

      // Draw git diff indicator
      int lineNum = blockNumber + 1; // 1-based
      if (diffLineMap.contains(lineNum)) {
        int diffType = diffLineMap[lineNum];
        QColor diffColor;
        switch (diffType) {
        case 0:
          diffColor = QColor(76, 175, 80);
          break; // Green - added
        case 1:
          diffColor = QColor(33, 150, 243);
          break; // Blue - modified
        default:
          diffColor = QColor(244, 67, 54);
          break; // Red - deleted
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
        const int markerX = DIFF_INDICATOR_WIDTH +
                            (BREAKPOINT_AREA_WIDTH - markerDiameter) / 2;
        const int markerY = static_cast<int>(
            top + (fontHeight - markerDiameter) / 2.0);

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(markerColor);
        painter.drawEllipse(markerX, markerY, markerDiameter, markerDiameter);
        painter.restore();
      }

      // Draw folding indicators
      if (m_foldingEnabled && m_editor->m_codeFolding) {
        CodeFoldingManager *folding = m_editor->m_codeFolding;
        bool foldable = folding->isFoldable(blockNumber);
        bool folded = folding->isFolded(blockNumber);

        if (foldable || folded) {
          const int foldX = numberArea - FOLD_INDICATOR_WIDTH;
          const int indicatorSize = qMin(fontHeight - 4, FOLD_INDICATOR_WIDTH - 2);
          const int ix = foldX + (FOLD_INDICATOR_WIDTH - indicatorSize) / 2;
          const int iy = static_cast<int>(top + (fontHeight - indicatorSize) / 2.0);

          painter.save();
          painter.setRenderHint(QPainter::Antialiasing, true);
          painter.setPen(QPen(m_textColor, 1));
          painter.setBrush(Qt::NoBrush);
          painter.drawRect(ix, iy, indicatorSize, indicatorSize);

          // Draw minus (unfolded) or plus (folded)
          int midY = iy + indicatorSize / 2;
          int midX = ix + indicatorSize / 2;
          int margin = 2;
          painter.drawLine(ix + margin, midY, ix + indicatorSize - margin, midY);
          if (folded) {
            painter.drawLine(midX, iy + margin, midX, iy + indicatorSize - margin);
          }
          painter.restore();
        }
      }

      if (m_blameTextWidth > 0) {
        auto it = m_gitBlameLines.find(lineNum);
        if (it != m_gitBlameLines.end()) {
          QRect blameRect(
              numberArea + BLAME_PADDING, static_cast<int>(top),
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
