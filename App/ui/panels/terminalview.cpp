#include "terminalview.h"

#include "../../theme/colorcontrast.h"
#include "../../theme/themeengine.h"
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextFragment>

class TerminalHighlighter : public QSyntaxHighlighter {
public:
  explicit TerminalHighlighter(QTextDocument *document)
      : QSyntaxHighlighter(document) {}

  QColor background, foreground;
  bool enabled = true;

protected:
  void highlightBlock(const QString &text) override {
    if (!enabled) {
      return;
    }
    const auto &c = ThemeEngine::instance().activeTheme().colors;
    struct Rule {
      QRegularExpression expression;
      QColor ThemeColors::*color;
      bool bold;
    };
    static const Rule rules[] = {
        {QRegularExpression(QStringLiteral(
             R"(\b\d+(?:\.\d+)?(?:\s*(?:ms|us|ns|s|MB|GB|KiB|MiB|%))?\b)")),
         &ThemeColors::ansiCyan, false},
        {QRegularExpression(QStringLiteral(R"([|\[\]{}]|(?:^|\s)[ABC]:)")),
         &ThemeColors::ansiMagenta, true},
        {QRegularExpression(
             QStringLiteral(R"(\b(?:warning|warn|deprecated|median|range)\b)"),
             QRegularExpression::CaseInsensitiveOption),
         &ThemeColors::ansiYellow, true},
        {QRegularExpression(
             QStringLiteral(
                 R"(\b(?:error|fatal|failed|failure|exception|traceback)\b)"),
             QRegularExpression::CaseInsensitiveOption),
         &ThemeColors::ansiRed, true},
        {QRegularExpression(
             QStringLiteral(
                 R"(\b(?:success|passed|finished|OK)\b|exit code 0\b)"),
             QRegularExpression::CaseInsensitiveOption),
         &ThemeColors::ansiGreen, true},
        {QRegularExpression(QStringLiteral(
             R"(^[^\s]+@[^\s:]+|(?:~|/)[^\s]*[\$#]|^[╭╰]─.*?[❯$#])")),
         &ThemeColors::ansiBlue, true},
    };
    for (const auto &rule : rules) {
      const QColor color = c.*(rule.color);
      auto matches = rule.expression.globalMatch(text);
      while (matches.hasNext()) {
        const auto match = matches.next();
        const int start = match.capturedStart();
        const int end = match.capturedEnd();
        for (auto it = currentBlock().begin(); !it.atEnd(); ++it) {
          const auto fragment = it.fragment();
          const auto original = fragment.charFormat();
          if (original.isAnchor() ||
              original.hasProperty(QTextFormat::BackgroundBrush) ||
              (original.hasProperty(QTextFormat::ForegroundBrush) &&
               original.foreground().color() != foreground))
            continue;
          const int left =
              qMax(start, fragment.position() - currentBlock().position());
          const int right =
              qMin(end, fragment.position() - currentBlock().position() +
                            fragment.length());
          if (left >= right)
            continue;
          QTextCharFormat format;
          format.setForeground(ColorContrast::ensure(
              color.isValid() ? color : foreground, background));
          if (rule.bold)
            format.setFontWeight(QFont::Bold);
          setFormat(left, right - left, format);
        }
      }
    }
  }
};

TerminalView::TerminalView(QWidget *parent)
    : QPlainTextEdit(parent),
      m_highlighter(new TerminalHighlighter(document())),
      m_background("#101418"), m_foreground("#b8c9c1"), m_accent("#7dffb2"),
      m_selection("#213a35"), m_border("#263832"), m_glow("#7dffb2"),
      m_scanlines(false), m_glowIntensity(0.3) {
  setFrameShape(QFrame::NoFrame);
  viewport()->setAttribute(Qt::WA_Hover, true);
  viewport()->setAutoFillBackground(false);
  setAttribute(Qt::WA_Hover, true);
}

void TerminalView::setVisualTheme(const QColor &background,
                                  const QColor &foreground,
                                  const QColor &accent, const QColor &selection,
                                  const QColor &border, const QColor &glow,
                                  bool scanlines, qreal glowIntensity) {
  m_background = background.isValid() ? background : m_background;
  m_foreground = foreground.isValid() ? foreground : m_foreground;
  m_accent = accent.isValid() ? accent : m_accent;
  m_selection = selection.isValid() ? selection : m_selection;
  m_border = border.isValid() ? border : m_border;
  m_glow = glow.isValid() ? glow : m_accent;
  m_scanlines = scanlines;
  m_glowIntensity = qBound(0.0, glowIntensity, 1.0);

  QPalette pal = palette();
  pal.setColor(QPalette::Base, m_background);
  pal.setColor(QPalette::Text, m_foreground);
  pal.setColor(QPalette::Highlight, m_selection);
  pal.setColor(QPalette::HighlightedText, m_foreground);
  setPalette(pal);
  m_highlighter->background = m_background;
  m_highlighter->foreground = m_foreground;
  m_highlighter->rehighlight();
  viewport()->update();
}

void TerminalView::paintEvent(QPaintEvent *event) {
  {
    QPainter backgroundPainter(viewport());
    backgroundPainter.setRenderHint(QPainter::Antialiasing, false);
    backgroundPainter.fillRect(viewport()->rect(), m_background);
  }

  QPlainTextEdit::paintEvent(event);

  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing, false);
  const QRect r = viewport()->rect();
  painter.fillRect(QRect(0, 0, 2, r.height()), withAlpha(m_accent, 0.45));

  const QRect cursor = cursorRect();
  if (cursor.isValid() && hasFocus() && cursorWidth() > 0) {
    QColor row = withAlpha(m_accent, 0.025 + 0.030 * m_glowIntensity);
    painter.fillRect(QRect(0, cursor.y(), r.width(), cursor.height()), row);

    QColor caretGlow = withAlpha(m_accent, 0.14 + 0.14 * m_glowIntensity);
    painter.fillRect(QRect(cursor.x() - 1, cursor.y(), 2, cursor.height()),
                     caretGlow);
  }

  if (m_scanlines) {
    painter.setPen(withAlpha(m_foreground, 0.018));
    for (int y = r.top(); y < r.bottom(); y += 5) {
      painter.drawLine(r.left(), y, r.right(), y);
    }
  }
}

void TerminalView::setDecorationsEnabled(bool enabled) {

  m_highlighter->enabled = enabled;
}

QColor TerminalView::withAlpha(const QColor &color, qreal alpha) const {
  QColor c = color;
  c.setAlphaF(qBound(0.0, alpha, 1.0));
  return c;
}
