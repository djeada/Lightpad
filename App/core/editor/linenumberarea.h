#ifndef LINENUMBERAREA_H
#define LINENUMBERAREA_H

#include <QWidget>
#include <QList>
#include <QPair>
#include <QColor>
#include <QFont>
#include <QMap>

class TextArea;
class QPaintEvent;

/**
 * @brief Line number gutter widget for code editors
 * 
 * Displays line numbers alongside a TextArea and optionally
 * shows git diff indicators (added/modified/deleted lines).
 */
class LineNumberArea : public QWidget {
    Q_OBJECT

public:
    explicit LineNumberArea(TextArea* editor, QWidget* parent = nullptr);
    
    QSize sizeHint() const override;
    
    /**
     * @brief Calculate the required width for line numbers
     * @return Width in pixels
     */
    int calculateWidth() const;
    
    /**
     * @brief Set the font used for line numbers
     */
    void setFont(const QFont& font);
    
    /**
     * @brief Set the background color
     */
    void setBackgroundColor(const QColor& color);
    
    /**
     * @brief Set the text color for line numbers
     */
    void setTextColor(const QColor& color);
    
    /**
     * @brief Set git diff indicators
     * @param diffLines List of (line number, type) pairs where type is:
     *                  0 = added, 1 = modified, 2 = deleted
     */
    void setGitDiffLines(const QList<QPair<int, int>>& diffLines);
    
    /**
     * @brief Clear git diff indicators
     */
    void clearGitDiffLines();
    void setGitBlameLines(const QMap<int, QString>& blameLines);
    void clearGitBlameLines();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool event(QEvent* event) override;

private:
    void updateBlameTextWidth();

    TextArea* m_editor;
    QFont m_font;
    QColor m_backgroundColor;
    QColor m_textColor;
    QList<QPair<int, int>> m_gitDiffLines;
    QMap<int, QString> m_gitBlameLines;
    int m_blameTextWidth;
    
    static constexpr int DIFF_INDICATOR_WIDTH = 3;
    static constexpr int PADDING = 10;
    static constexpr int BLAME_PADDING = 12;
    static constexpr int MAX_BLAME_WIDTH = 280;
};

#endif // LINENUMBERAREA_H
