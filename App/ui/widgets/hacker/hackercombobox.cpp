#include "hackercombobox.h"
#include "../../../theme/themeengine.h"
#include <QAbstractItemView>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainterPath>

HackerComboBox::HackerComboBox(QWidget *parent) : HackerWidget(parent) {
  setupInnerCombo();
  setFocusPolicy(Qt::StrongFocus);
  setCursor(Qt::PointingHandCursor);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  m_chevronAnim = new QPropertyAnimation(this, "chevronRotation", this);
  m_chevronAnim->setEasingCurve(QEasingCurve::InOutCubic);
}

void HackerComboBox::setupInnerCombo() {
  m_combo = new QComboBox(this);
  m_combo->hide();
  m_combo->installEventFilter(this);

  connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &HackerComboBox::currentIndexChanged);
  connect(m_combo, &QComboBox::currentTextChanged, this,
          &HackerComboBox::currentTextChanged);

  connect(&ThemeEngine::instance(), &ThemeEngine::themeChanged, this, [this]() {
    const auto &c = colors();
    m_combo->view()->setStyleSheet(
        QStringLiteral("QAbstractItemView {"
                       "  background: %1; color: %2; border: 1px solid %3;"
                       "  selection-background-color: %4; selection-color: %5;"
                       "}")
            .arg(c.surfacePopover.name(), c.textPrimary.name(),
                 c.borderDefault.name(), c.accentSoft.name(),
                 c.textPrimary.name()));
  });
}

void HackerComboBox::addItem(const QString &text, const QVariant &userData) {
  m_combo->addItem(text, userData);
  update();
}
void HackerComboBox::addItem(const QIcon &icon, const QString &text,
                             const QVariant &userData) {
  m_combo->addItem(icon, text, userData);
  update();
}
void HackerComboBox::addItems(const QStringList &texts) {
  m_combo->addItems(texts);
  update();
}
void HackerComboBox::insertItem(int index, const QString &text,
                                const QVariant &userData) {
  m_combo->insertItem(index, text, userData);
  update();
}
void HackerComboBox::removeItem(int index) {
  m_combo->removeItem(index);
  update();
}
void HackerComboBox::clear() {
  m_combo->clear();
  update();
}
int HackerComboBox::currentIndex() const { return m_combo->currentIndex(); }
void HackerComboBox::setCurrentIndex(int index) {
  m_combo->setCurrentIndex(index);
}
QString HackerComboBox::currentText() const { return m_combo->currentText(); }
int HackerComboBox::count() const { return m_combo->count(); }
QString HackerComboBox::itemText(int index) const {
  return m_combo->itemText(index);
}
QVariant HackerComboBox::itemData(int index) const {
  return m_combo->itemData(index);
}

QSize HackerComboBox::sizeHint() const {
  QFont f = font();
  f.setFamily(QStringLiteral("monospace"));
  QFontMetrics fm(f);
  int maxW = 80;
  for (int i = 0; i < m_combo->count(); ++i)
    maxW = qMax(maxW, fm.horizontalAdvance(m_combo->itemText(i)));
  return QSize(maxW + 48, 36);
}

QSize HackerComboBox::minimumSizeHint() const { return QSize(80, 36); }

void HackerComboBox::setChevronRotation(qreal r) {
  if (qFuzzyCompare(m_chevronRotation, r))
    return;
  m_chevronRotation = r;
  update();
}

void HackerComboBox::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const QRectF r = rect().adjusted(1, 1, -1, -1);
  const int rad = radius();
  const auto &c = colors();

  QColor bg = blendColors(c.inputBg, c.surfaceRaised, m_hoverProgress * 0.3);
  drawRoundedSurface(p, r, bg, rad);

  QColor borderColor =
      blendColors(c.inputBorder, c.borderFocus, m_focusProgress);
  p.setPen(QPen(borderColor, 1.0));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(r, rad, rad);

  QFont f = font();
  f.setFamily(QStringLiteral("monospace"));
  p.setFont(f);
  QRectF textRect = r.adjusted(12, 0, -32, 0);
  p.setPen(c.textPrimary);
  QString displayText = m_combo->currentText();
  if (displayText.isEmpty() && m_combo->count() == 0) {
    p.setPen(c.inputPlaceholder);
    displayText = QStringLiteral("Select...");
  }
  p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, displayText);

  p.save();
  qreal chevronX = r.right() - 20;
  qreal chevronY = r.center().y();
  p.translate(chevronX, chevronY);
  p.rotate(m_chevronRotation);

  QPen chevPen(blendColors(c.textMuted, c.textPrimary, m_hoverProgress), 1.5);
  chevPen.setCapStyle(Qt::RoundCap);
  chevPen.setJoinStyle(Qt::RoundJoin);
  p.setPen(chevPen);
  p.setBrush(Qt::NoBrush);

  QPainterPath chevron;
  chevron.moveTo(-4, -2);
  chevron.lineTo(0, 2);
  chevron.lineTo(4, -2);
  p.drawPath(chevron);
  p.restore();
}

void HackerComboBox::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_combo->showPopup();

    m_chevronAnim->stop();
    m_chevronAnim->setDuration(animDuration());
    m_chevronAnim->setStartValue(m_chevronRotation);
    m_chevronAnim->setEndValue(180.0);
    m_chevronAnim->start();
  }
  HackerWidget::mousePressEvent(event);
}

bool HackerComboBox::eventFilter(QObject *obj, QEvent *event) {
  if (obj == m_combo && event->type() == QEvent::Hide) {
    m_chevronAnim->stop();
    m_chevronAnim->setDuration(animDuration());
    m_chevronAnim->setStartValue(m_chevronRotation);
    m_chevronAnim->setEndValue(0.0);
    m_chevronAnim->start();
  }
  return HackerWidget::eventFilter(obj, event);
}
