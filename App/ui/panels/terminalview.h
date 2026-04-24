#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

#include <QColor>
#include <QPlainTextEdit>

class TerminalView : public QPlainTextEdit {
  Q_OBJECT

public:
  explicit TerminalView(QWidget *parent = nullptr);

  void setVisualTheme(const QColor &background, const QColor &foreground,
                      const QColor &accent, const QColor &selection,
                      const QColor &border, const QColor &glow,
                      bool scanlines, qreal glowIntensity);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor withAlpha(const QColor &color, qreal alpha) const;

  QColor m_background;
  QColor m_foreground;
  QColor m_accent;
  QColor m_selection;
  QColor m_border;
  QColor m_glow;
  bool m_scanlines;
  qreal m_glowIntensity;
};

#endif
