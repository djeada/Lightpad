#ifndef MINIMAP_H
#define MINIMAP_H

#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QWidget>

class Minimap : public QWidget {
  Q_OBJECT

public:
  explicit Minimap(QWidget *parent = nullptr);
  ~Minimap();

  void setSourceEditor(QPlainTextEdit *editor);

  QPlainTextEdit *sourceEditor() const;

  void setScale(qreal scale);

  qreal scale() const;

  void setMinimapVisible(bool visible);

  bool isMinimapVisible() const;

  void updateContent();

  void setViewportColor(const QColor &color);

  void setBackgroundColor(const QColor &color);

signals:

  void scrollRequested(int lineNumber);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onSourceTextChanged();
  void onSourceScrollChanged(int value);
  void onSourceCursorPositionChanged();

private:
  void updateViewportRect();
  void scrollToY(int y);
  int lineNumberFromY(int y) const;
  void renderDocument();
  int scrollOffsetInPixels() const;

  QPlainTextEdit *m_sourceEditor;
  qreal m_scale;
  bool m_visible;
  bool m_isDragging;

  QImage m_documentImage;
  bool m_documentDirty;

  QRect m_viewportRect;
  QColor m_viewportColor;
  QColor m_backgroundColor;

  qreal m_charWidth;
  qreal m_lineHeight;
  int m_maxVisibleLines;
  int m_scrollOffset;

  bool m_updatePending;
};

#endif
