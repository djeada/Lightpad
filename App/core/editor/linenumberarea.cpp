#include "linenumberarea.h"
#include "../textarea.h"
#include <QPainter>
#include <QPaintEvent>
#include <QTextBlock>
#include <QScrollBar>

LineNumberArea::LineNumberArea(TextArea* editor, QWidget* parent)
    : QWidget(parent ? parent : editor)
    , m_editor(editor)
    , m_backgroundColor(QColor(40, 40, 40))
    , m_textColor(QColor(Qt::gray).lighter(150))
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
    return space;
}

void LineNumberArea::setFont(const QFont& font)
{
    m_font = font;
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
    
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(m_textColor);
            painter.drawText(DIFF_INDICATOR_WIDTH, top, 
                           areaWidth - DIFF_INDICATOR_WIDTH, fontHeight,
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
