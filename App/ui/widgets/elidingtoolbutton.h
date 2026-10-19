#ifndef ELIDINGTOOLBUTTON_H
#define ELIDINGTOOLBUTTON_H

#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolButton>

class ElidingToolButton : public QToolButton {
public:
  using QToolButton::QToolButton;

  QSize sizeHint() const override {
    const auto hint = QToolButton::sizeHint();
    return {qBound(100, hint.width(), 200), hint.height()};
  }
  QSize minimumSizeHint() const override {
    return {100, QToolButton::minimumSizeHint().height()};
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QStylePainter painter(this);
    QStyleOptionToolButton option;
    initStyleOption(&option);
    const QRect button = style()->subControlRect(QStyle::CC_ToolButton, &option,
                                                 QStyle::SC_ToolButton, this);
    const int iconSpace = icon().isNull() ? 0 : iconSize().width() + 6;
    const int margin =
        2 * style()->pixelMetric(QStyle::PM_ButtonMargin, &option, this);
    option.text = fontMetrics().elidedText(
        text(), Qt::ElideRight, qMax(0, button.width() - iconSpace - margin));
    painter.drawComplexControl(QStyle::CC_ToolButton, option);
  }
};

#endif
