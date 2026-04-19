#include "hackerspinbox.h"
#include "../../../theme/themeengine.h"
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainterPath>
#include <QResizeEvent>

HackerSpinBox::HackerSpinBox(QWidget *parent) : HackerWidget(parent) {
  setupInnerSpin();
  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setMouseTracking(true);
}

void HackerSpinBox::setupInnerSpin() {
  m_spin = new QSpinBox(this);
  m_spin->setFrame(false);
  m_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  m_spin->setStyleSheet(
      QStringLiteral("background:transparent; border:none; padding:0;"));

  QFont f = m_spin->font();
  f.setFamily(QStringLiteral("monospace"));
  m_spin->setFont(f);

  m_spin->installEventFilter(this);
  setFocusProxy(m_spin);

  connect(m_spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &HackerSpinBox::valueChanged);

  connect(&ThemeEngine::instance(), &ThemeEngine::themeChanged, this, [this]() {
    const auto &c = colors();
    m_spin->setStyleSheet(
        QStringLiteral(
            "background:transparent; border:none; padding:0; color:%1;")
            .arg(c.inputFg.name()));
  });
}

void HackerSpinBox::setValue(int value) { m_spin->setValue(value); }
int HackerSpinBox::value() const { return m_spin->value(); }
void HackerSpinBox::setRange(int min, int max) { m_spin->setRange(min, max); }
void HackerSpinBox::setMinimum(int min) { m_spin->setMinimum(min); }
void HackerSpinBox::setMaximum(int max) { m_spin->setMaximum(max); }
int HackerSpinBox::minimum() const { return m_spin->minimum(); }
int HackerSpinBox::maximum() const { return m_spin->maximum(); }
void HackerSpinBox::setSingleStep(int step) { m_spin->setSingleStep(step); }
int HackerSpinBox::singleStep() const { return m_spin->singleStep(); }
void HackerSpinBox::setPrefix(const QString &prefix) {
  m_spin->setPrefix(prefix);
}
void HackerSpinBox::setSuffix(const QString &suffix) {
  m_spin->setSuffix(suffix);
}
void HackerSpinBox::setWrapping(bool wrapping) {
  m_spin->setWrapping(wrapping);
}

QSize HackerSpinBox::sizeHint() const { return QSize(120, 36); }
QSize HackerSpinBox::minimumSizeHint() const { return QSize(60, 36); }

QRectF HackerSpinBox::upArrowRect() const {
  return QRectF(width() - 28, 1, 27, height() / 2.0 - 1);
}

QRectF HackerSpinBox::downArrowRect() const {
  return QRectF(width() - 28, height() / 2.0, 27, height() / 2.0 - 1);
}

void HackerSpinBox::resizeEvent(QResizeEvent *event) {
  HackerWidget::resizeEvent(event);
  const int hPad = 12;
  const int arrowW = 28;
  const int vPad = (height() - m_spin->sizeHint().height()) / 2;
  m_spin->setGeometry(hPad, vPad, width() - hPad - arrowW,
                      m_spin->sizeHint().height());
}

bool HackerSpinBox::eventFilter(QObject *obj, QEvent *event) {
  if (obj == m_spin) {
    if (event->type() == QEvent::FocusIn)
      startAnimation(m_focusAnim, m_focusProgress, 1.0);
    else if (event->type() == QEvent::FocusOut)
      startAnimation(m_focusAnim, m_focusProgress, 0.0);
  }
  return HackerWidget::eventFilter(obj, event);
}

void HackerSpinBox::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const QRectF r = rect().adjusted(1, 1, -1, -1);
  const int rad = radius();
  const auto &c = colors();

  drawRoundedSurface(p, r, c.inputBg, rad);

  QColor borderColor =
      blendColors(c.inputBorder, c.inputBorderFocus, m_focusProgress);
  p.setPen(QPen(borderColor, 1.0));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(r, rad, rad);

  qreal divX = width() - 28.5;
  p.setPen(QPen(c.borderSubtle, 1.0));
  p.drawLine(QPointF(divX, 4), QPointF(divX, height() - 4));

  {
    QRectF ar = upArrowRect();
    if (m_upHovered) {
      drawRoundedSurface(p, ar, c.surfaceRaised, 2);
    }
    QColor arrowColor = m_upHovered ? c.textPrimary : c.textMuted;
    p.setPen(QPen(arrowColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    QPointF center = ar.center();
    QPainterPath up;
    up.moveTo(center.x() - 4, center.y() + 2);
    up.lineTo(center.x(), center.y() - 2);
    up.lineTo(center.x() + 4, center.y() + 2);
    p.drawPath(up);
  }

  {
    QRectF ar = downArrowRect();
    if (m_downHovered) {
      drawRoundedSurface(p, ar, c.surfaceRaised, 2);
    }
    QColor arrowColor = m_downHovered ? c.textPrimary : c.textMuted;
    p.setPen(QPen(arrowColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    QPointF center = ar.center();
    QPainterPath down;
    down.moveTo(center.x() - 4, center.y() - 2);
    down.lineTo(center.x(), center.y() + 2);
    down.lineTo(center.x() + 4, center.y() - 2);
    p.drawPath(down);
  }

  if (m_focusProgress > 0.01) {
    qreal intensity = m_focusProgress * glowAmount();
    drawGlowBorder(p, r, c.accentGlow, intensity * 0.5, rad);
  }
}
