#include "linenumberarea.h"
#include "../textarea.h"
#include <QPainter>
#include <QPaintEvent>
#include <QTextBlock>
#include <QScrollBar>
#include <QToolTip>
#include <QHelpEvent>

LineNumberArea::LineNumberArea(TextArea* editor, QWidget* parent)
    : QWidget(parent ? parent : editor)
    , m_editor(editor)
    , m_backgroundColor(QColor(40, 40, 40))
    , m_textColor(QColor(Qt::gray).lighter(150))
    , m_blameTextWidth(0)
{
    if (editor) {
        m_font = editor->font();
    }
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(calculateWidth(), 0);
}

int LineNumberArea::calculateWidth() const
{
    if (!m_editor) {
        return 0;
    }
    
    int digits = 1;
    int max = qMax(1, m_editor->blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    
    int space = DIFF_INDICATOR_WIDTH + PADDING + 
                QFontMetrics(m_font).horizontalAdvance(QLatin1Char('9')) * digits;
    if (m_blameTextWidth > 0) {
        space += BLAME_PADDING + m_blameTextWidth;
    }
    return space;
}

void LineNumberArea::setFont(const QFont& font)
{
    m_font = font;
    updateBlameTextWidth();
    update();
}

void LineNumberArea::setBackgroundColor(const QColor& color)
{
    m_backgroundColor = color;
    update();
}

void LineNumberArea::setTextColor(const QColor& color)
{
    m_textColor = color;
    update();
}

void LineNumberArea::setGitDiffLines(const QList<QPair<int, int>>& diffLines)
{
    m_gitDiffLines = diffLines;
    update();
}

void LineNumberArea::clearGitDiffLines()
{
    m_gitDiffLines.clear();
    update();
}

void LineNumberArea::setGitBlameLines(const QMap<int, QString>& blameLines)
{
    m_gitBlameLines = blameLines;
    updateBlameTextWidth();
    updateGeometry();
    update();
}

void LineNumberArea::clearGitBlameLines()
{
    m_gitBlameLines.clear();
    m_blameTextWidth = 0;
    updateGeometry();
    update();
    QToolTip::hideText();
}

void LineNumberArea::updateBlameTextWidth()
{
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

bool LineNumberArea::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip && m_editor) {
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        QTextBlock block = m_editor->firstVisibleBlock();
        QRectF blockRect = m_editor->blockBoundingGeometry(block);
        blockRect.translate(m_editor->contentOffset());
        qreal top = blockRect.top();
        qreal bottom = top + blockRect.height();
        int blockNumber = block.blockNumber();
        const int fontHeight = QFontMetrics(m_font).height();
        
        while (block.isValid() && top <= height()) {
            if (block.isVisible() && helpEvent->pos().y() >= top && helpEvent->pos().y() <= bottom) {
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

void LineNumberArea::paintEvent(QPaintEvent* event)
{
    if (!m_editor) {
        return;
    }
    
    QPainter painter(this);
    painter.setFont(m_font);
    painter.fillRect(event->rect(), m_backgroundColor);
    
    // Build a map for quick git diff lookup
    QMap<int, int> diffLineMap;
    for (const auto& diffLine : m_gitDiffLines) {
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
    const int numberAreaWidth = areaWidth - (m_blameTextWidth > 0 ? (m_blameTextWidth + BLAME_PADDING) : 0);
    
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(m_textColor);
            painter.drawText(DIFF_INDICATOR_WIDTH, top, 
                           numberAreaWidth - DIFF_INDICATOR_WIDTH, fontHeight,
                           Qt::AlignCenter, number);
            
            // Draw git diff indicator
            int lineNum = blockNumber + 1;  // 1-based
            if (diffLineMap.contains(lineNum)) {
                int diffType = diffLineMap[lineNum];
                QColor diffColor;
                switch (diffType) {
                    case 0: diffColor = QColor(76, 175, 80); break;   // Green - added
                    case 1: diffColor = QColor(33, 150, 243); break;  // Blue - modified
                    default: diffColor = QColor(244, 67, 54); break;  // Red - deleted
                }
                painter.fillRect(0, static_cast<int>(top), 
                               DIFF_INDICATOR_WIDTH, static_cast<int>(bottom - top), 
                               diffColor);
            }

            if (m_blameTextWidth > 0) {
                auto it = m_gitBlameLines.find(lineNum);
                if (it != m_gitBlameLines.end()) {
                    QRect blameRect(numberAreaWidth + BLAME_PADDING, static_cast<int>(top),
                                    areaWidth - numberAreaWidth - BLAME_PADDING, fontHeight);
                    painter.setPen(QColor(Qt::gray).lighter(120));
                    painter.drawText(blameRect, Qt::AlignVCenter | Qt::AlignLeft,
                                     QFontMetrics(m_font).elidedText(it.value(), Qt::ElideRight, blameRect.width()));
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
