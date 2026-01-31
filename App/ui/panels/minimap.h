#ifndef MINIMAP_H
#define MINIMAP_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSyntaxHighlighter>

/**
 * @brief Minimap widget for code navigation
 * 
 * Displays a zoomed-out view of the document content with:
 * - Scrollable code preview
 * - Syntax highlighting (inherited from source)
 * - Current viewport indicator
 * - Click-to-scroll navigation
 */
class Minimap : public QWidget {
    Q_OBJECT

public:
    explicit Minimap(QWidget* parent = nullptr);
    ~Minimap();

    /**
     * @brief Set the source text editor to track
     * @param editor The QPlainTextEdit to mirror
     */
    void setSourceEditor(QPlainTextEdit* editor);

    /**
     * @brief Get the source editor
     */
    QPlainTextEdit* sourceEditor() const;

    /**
     * @brief Set the minimap scale factor
     * @param scale Scale factor (0.1 - 0.3 recommended)
     */
    void setScale(qreal scale);

    /**
     * @brief Get current scale factor
     */
    qreal scale() const;

    /**
     * @brief Set whether minimap is visible
     */
    void setMinimapVisible(bool visible);

    /**
     * @brief Check if minimap is visible
     */
    bool isMinimapVisible() const;

    /**
     * @brief Update the minimap content
     */
    void updateContent();

    /**
     * @brief Set the viewport indicator color
     */
    void setViewportColor(const QColor& color);

    /**
     * @brief Set the background color
     */
    void setBackgroundColor(const QColor& color);

signals:
    /**
     * @brief Emitted when user clicks on minimap to scroll
     */
    void scrollRequested(int lineNumber);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSourceTextChanged();
    void onSourceScrollChanged(int value);
    void onSourceCursorPositionChanged();

private:
    void updateViewportRect();
    void scrollToY(int y);
    int lineNumberFromY(int y) const;
    void renderDocument();
    int scrollOffsetInPixels() const;  // Helper to compute scroll offset in pixels

    QPlainTextEdit* m_sourceEditor;
    qreal m_scale;
    bool m_visible;
    bool m_isDragging;
    
    // Cached rendering
    QImage m_documentImage;
    bool m_documentDirty;
    
    // Viewport indicator
    QRect m_viewportRect;
    QColor m_viewportColor;
    QColor m_backgroundColor;
    
    // Character dimensions at scale
    qreal m_charWidth;
    qreal m_lineHeight;
    int m_maxVisibleLines;
    int m_scrollOffset;
};

#endif // MINIMAP_H
