#include "minimap.h"
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QWheelEvent>

Minimap::Minimap(QWidget *parent)
    : QWidget(parent), m_sourceEditor(nullptr), m_scale(0.15), m_visible(true),
      m_isDragging(false), m_documentDirty(true),
      m_viewportColor(
          QColor(100, 149, 237, 60)) // Cornflower blue with transparency
      ,
      m_backgroundColor(QColor(30, 30, 30)), m_charWidth(1.5),
      m_lineHeight(3.0), m_maxVisibleLines(0), m_scrollOffset(0),
      m_updatePending(false) {
  setMinimumWidth(80);
  setMaximumWidth(120);
  setMouseTracking(true);
  setCursor(Qt::PointingHandCursor);
}

Minimap::~Minimap() {}

void Minimap::setSourceEditor(QPlainTextEdit *editor) {
  if (m_sourceEditor) {
    disconnect(m_sourceEditor, nullptr, this, nullptr);
  }

  m_sourceEditor = editor;

  if (m_sourceEditor) {
    connect(m_sourceEditor, &QPlainTextEdit::textChanged, this,
            &Minimap::onSourceTextChanged);
    connect(m_sourceEditor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &Minimap::onSourceScrollChanged);
    connect(m_sourceEditor, &QPlainTextEdit::cursorPositionChanged, this,
            &Minimap::onSourceCursorPositionChanged);

    m_documentDirty = true;
    updateContent();
  }
}

QPlainTextEdit *Minimap::sourceEditor() const { return m_sourceEditor; }

void Minimap::setScale(qreal scale) {
  m_scale = qBound(0.05, scale, 0.5);
  m_charWidth = 8.0 * m_scale;
  m_lineHeight = 14.0 * m_scale;
  m_documentDirty = true;
  update();
}

qreal Minimap::scale() const { return m_scale; }

void Minimap::setMinimapVisible(bool visible) {
  m_visible = visible;
  setVisible(visible);
}

bool Minimap::isMinimapVisible() const { return m_visible; }

void Minimap::updateContent() {
  if (!m_sourceEditor || !isVisible()) {
    return;
  }

  renderDocument();
  updateViewportRect();
  update();
}

void Minimap::setViewportColor(const QColor &color) {
  m_viewportColor = color;
  update();
}

void Minimap::setBackgroundColor(const QColor &color) {
  m_backgroundColor = color;
  m_documentDirty = true;
  update();
}

void Minimap::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  // Draw background
  painter.fillRect(rect(), m_backgroundColor);

  if (!m_sourceEditor) {
    return;
  }

  // Render document if dirty
  if (m_documentDirty) {
    renderDocument();
  }

  // Draw the cached document image
  if (!m_documentImage.isNull()) {
    int sourceY = scrollOffsetInPixels();
    QRect sourceRect(0, sourceY, m_documentImage.width(), height());
    QRect destRect(0, 0, width(), height());
    painter.drawImage(destRect, m_documentImage, sourceRect);
  }

  // Draw viewport indicator
  if (!m_viewportRect.isEmpty()) {
    painter.fillRect(m_viewportRect, m_viewportColor);

    // Draw viewport border
    QPen borderPen(m_viewportColor.lighter(150));
    borderPen.setWidth(1);
    painter.setPen(borderPen);
    painter.drawRect(m_viewportRect);
  }

  // Draw left border
  painter.setPen(QColor(60, 60, 60));
  painter.drawLine(0, 0, 0, height());
}

void Minimap::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_sourceEditor) {
    m_isDragging = true;
    scrollToY(event->pos().y());
  }
}

void Minimap::mouseMoveEvent(QMouseEvent *event) {
  if (m_isDragging && m_sourceEditor) {
    scrollToY(event->pos().y());
  }
}

void Minimap::mouseReleaseEvent(QMouseEvent *event) {
  Q_UNUSED(event);
  m_isDragging = false;
}

void Minimap::wheelEvent(QWheelEvent *event) {
  if (m_sourceEditor) {
    // Forward wheel event to source editor
    QApplication::sendEvent(m_sourceEditor->verticalScrollBar(), event);
  }
}

void Minimap::resizeEvent(QResizeEvent *event) {
  Q_UNUSED(event);
  m_maxVisibleLines = static_cast<int>(height() / m_lineHeight);
  m_documentDirty = true;
  updateViewportRect();
}

void Minimap::onSourceTextChanged() {
  m_documentDirty = true;
  // Debounce minimap updates to avoid blocking typing
  // Only schedule update if one isn't already pending
  if (!m_updatePending) {
    m_updatePending = true;
    QTimer::singleShot(150, this, [this]() {
      m_updatePending = false;
      if (isVisible()) {
        updateContent();
      }
    });
  }
}

void Minimap::onSourceScrollChanged(int value) {
  Q_UNUSED(value);
  updateViewportRect();
  update();
}

void Minimap::onSourceCursorPositionChanged() {
  // Defer cursor position update to not block typing
  QTimer::singleShot(0, this, QOverload<>::of(&QWidget::update));
}

void Minimap::updateViewportRect() {
  if (!m_sourceEditor) {
    m_viewportRect = QRect();
    return;
  }

  QScrollBar *vbar = m_sourceEditor->verticalScrollBar();
  int totalLines = m_sourceEditor->document()->blockCount();

  if (totalLines <= 0) {
    m_viewportRect = QRect();
    return;
  }

  // Calculate visible lines in source editor
  int firstVisibleLine = vbar->value();
  int visibleLineCount =
      m_sourceEditor->height() / m_sourceEditor->fontMetrics().lineSpacing();

  // Calculate minimap dimensions
  qreal lineHeightMinimap = m_lineHeight;
  int totalMinimapHeight = static_cast<int>(totalLines * lineHeightMinimap);

  // Calculate scroll offset to keep viewport visible
  int viewportTop = static_cast<int>(firstVisibleLine * lineHeightMinimap);
  int viewportHeight = static_cast<int>(visibleLineCount * lineHeightMinimap);

  // Adjust scroll offset if needed to keep viewport centered
  if (totalMinimapHeight > height()) {
    int maxScrollOffset =
        static_cast<int>((totalMinimapHeight - height()) / m_lineHeight);
    int desiredCenter = viewportTop + viewportHeight / 2;
    int desiredOffset =
        static_cast<int>((desiredCenter - height() / 2) / m_lineHeight);
    m_scrollOffset = qBound(0, desiredOffset, maxScrollOffset);
    viewportTop -= scrollOffsetInPixels();
  } else {
    m_scrollOffset = 0;
  }

  m_viewportRect = QRect(0, viewportTop, width(), qMax(viewportHeight, 10));
}

void Minimap::scrollToY(int y) {
  int lineNumber = lineNumberFromY(y);

  if (m_sourceEditor && lineNumber >= 0) {
    QTextBlock block =
        m_sourceEditor->document()->findBlockByNumber(lineNumber);
    if (block.isValid()) {
      QTextCursor cursor(block);
      m_sourceEditor->setTextCursor(cursor);
      m_sourceEditor->centerCursor();
    }
    emit scrollRequested(lineNumber);
  }
}

int Minimap::lineNumberFromY(int y) const {
  if (!m_sourceEditor) {
    return -1;
  }

  int adjustedY = y + scrollOffsetInPixels();
  int lineNumber = static_cast<int>(adjustedY / m_lineHeight);
  int totalLines = m_sourceEditor->document()->blockCount();

  return qBound(0, lineNumber, totalLines - 1);
}

int Minimap::scrollOffsetInPixels() const {
  return static_cast<int>(m_scrollOffset * m_lineHeight);
}

void Minimap::renderDocument() {
  if (!m_sourceEditor) {
    m_documentImage = QImage();
    m_documentDirty = false;
    return;
  }

  QTextDocument *doc = m_sourceEditor->document();
  int totalLines = doc->blockCount();

  if (totalLines == 0) {
    m_documentImage = QImage();
    m_documentDirty = false;
    return;
  }

  int imageWidth = width();
  int imageHeight = static_cast<int>(totalLines * m_lineHeight) + height();

  // Limit image height to prevent memory issues
  imageHeight = qMin(imageHeight, 10000);

  m_documentImage = QImage(imageWidth, imageHeight, QImage::Format_RGB32);
  m_documentImage.fill(m_backgroundColor);

  QPainter painter(&m_documentImage);
  painter.setRenderHint(QPainter::Antialiasing, false);

  // Get syntax highlighter colors if available
  QTextBlock block = doc->begin();
  int lineIndex = 0;

  while (block.isValid() && lineIndex * m_lineHeight < imageHeight) {
    QString text = block.text();
    qreal y = lineIndex * m_lineHeight;
    qreal x = 2; // Small left margin

    // Draw each character as a small rectangle
    // Use character formats from the block if available
    QTextLayout *layout = block.layout();

    if (layout && layout->lineCount() > 0) {
      // Use actual formatting from syntax highlighter
      QVector<QTextLayout::FormatRange> formats = layout->formats();

      int charIndex = 0;
      for (int i = 0; i < text.length() && x < imageWidth; ++i) {
        QChar ch = text[i];

        if (ch.isSpace()) {
          if (ch == '\t') {
            x += m_charWidth * 4;
          } else {
            x += m_charWidth;
          }
          charIndex++;
          continue;
        }

        // Find color for this character
        QColor charColor(150, 150, 150); // Default gray

        for (const auto &format : formats) {
          if (charIndex >= format.start &&
              charIndex < format.start + format.length) {
            if (format.format.hasProperty(QTextFormat::ForegroundBrush)) {
              charColor = format.format.foreground().color();
            }
            break;
          }
        }

        // Draw character as small rectangle
        painter.fillRect(QRectF(x, y, m_charWidth * 0.8, m_lineHeight * 0.7),
                         charColor);
        x += m_charWidth;
        charIndex++;
      }
    } else {
      // Fallback: simple gray rendering
      for (int i = 0; i < text.length() && x < imageWidth; ++i) {
        QChar ch = text[i];

        if (ch.isSpace()) {
          if (ch == '\t') {
            x += m_charWidth * 4;
          } else {
            x += m_charWidth;
          }
          continue;
        }

        QColor charColor(150, 150, 150);
        painter.fillRect(QRectF(x, y, m_charWidth * 0.8, m_lineHeight * 0.7),
                         charColor);
        x += m_charWidth;
      }
    }

    block = block.next();
    lineIndex++;
  }

  m_documentDirty = false;
}
