#include "hackerlineedit.h"
#include "../../../theme/themeengine.h"
#include <QPaintEvent>
#include <QResizeEvent>

HackerLineEdit::HackerLineEdit(QWidget *parent)
    : HackerWidget(parent)
{
    setupInnerEdit();
}

HackerLineEdit::HackerLineEdit(const QString &placeholder, QWidget *parent)
    : HackerWidget(parent)
{
    setupInnerEdit();
    m_edit->setPlaceholderText(placeholder);
}

void HackerLineEdit::setupInnerEdit()
{
    m_edit = new QLineEdit(this);
    m_edit->setFrame(false);
    m_edit->setStyleSheet(
        QStringLiteral("background:transparent; border:none; padding:0;"));

    QFont f = m_edit->font();
    f.setFamily(QStringLiteral("monospace"));
    m_edit->setFont(f);

    m_edit->installEventFilter(this);
    setFocusProxy(m_edit);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    connect(m_edit, &QLineEdit::textChanged, this, &HackerLineEdit::textChanged);
    connect(m_edit, &QLineEdit::returnPressed, this, &HackerLineEdit::returnPressed);
    connect(m_edit, &QLineEdit::editingFinished, this, &HackerLineEdit::editingFinished);

    // Update style colors when theme changes
    connect(&ThemeEngine::instance(), &ThemeEngine::themeChanged, this, [this]() {
        const auto &c = colors();
        m_edit->setStyleSheet(
            QStringLiteral("background:transparent; border:none; padding:0; color:%1;")
                .arg(c.textPrimary.name()));
    });
}

void HackerLineEdit::setText(const QString &text) { m_edit->setText(text); }
QString HackerLineEdit::text() const { return m_edit->text(); }
void HackerLineEdit::setPlaceholderText(const QString &text) { m_edit->setPlaceholderText(text); }
QString HackerLineEdit::placeholderText() const { return m_edit->placeholderText(); }
void HackerLineEdit::setReadOnly(bool ro) { m_edit->setReadOnly(ro); }
bool HackerLineEdit::isReadOnly() const { return m_edit->isReadOnly(); }
void HackerLineEdit::setEchoMode(QLineEdit::EchoMode mode) { m_edit->setEchoMode(mode); }
void HackerLineEdit::clear() { m_edit->clear(); }
void HackerLineEdit::selectAll() { m_edit->selectAll(); }
void HackerLineEdit::setFocus() { m_edit->setFocus(); }
void HackerLineEdit::setMaxLength(int length) { m_edit->setMaxLength(length); }

QSize HackerLineEdit::sizeHint() const
{
    return QSize(200, 36);
}

QSize HackerLineEdit::minimumSizeHint() const
{
    return QSize(80, 36);
}

void HackerLineEdit::resizeEvent(QResizeEvent *event)
{
    HackerWidget::resizeEvent(event);
    const int hPad = 12;
    const int vPad = (height() - m_edit->sizeHint().height()) / 2;
    m_edit->setGeometry(hPad, vPad, width() - 2 * hPad, m_edit->sizeHint().height());
}

bool HackerLineEdit::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_edit) {
        if (event->type() == QEvent::FocusIn) {
            startAnimation(m_focusAnim, m_focusProgress, 1.0);
        } else if (event->type() == QEvent::FocusOut) {
            startAnimation(m_focusAnim, m_focusProgress, 0.0);
        }
    }
    return HackerWidget::eventFilter(obj, event);
}

void HackerLineEdit::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -1, -1);
    const int rad = radius();
    const auto &c = colors();

    // Background
    drawRoundedSurface(p, r, c.inputBg, rad);

    // Bottom border: transitions from default to accent on focus
    QColor borderColor = blendColors(c.inputBorder, c.accentPrimary, m_focusProgress);
    qreal borderY = r.bottom();
    QPen borderPen(borderColor, 2.0);
    p.setPen(borderPen);
    p.drawLine(QPointF(r.left() + rad, borderY), QPointF(r.right() - rad, borderY));

    // Glow under bottom border on focus
    if (m_focusProgress > 0.01) {
        qreal intensity = m_focusProgress * glowAmount();
        QColor glowColor = c.accentGlow;
        for (int i = 3; i >= 1; --i) {
            qreal spread = i * 1.5;
            QColor gc = glowColor;
            gc.setAlphaF(intensity * (0.08 / i));
            p.setPen(QPen(gc, spread * 2));
            p.drawLine(QPointF(r.left() + rad, borderY + spread),
                       QPointF(r.right() - rad, borderY + spread));
        }
    }

    // Side / top border
    QColor edgeBorder = blendColors(c.borderDefault, c.borderFocus, m_focusProgress);
    p.setPen(QPen(edgeBorder, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, rad, rad);
}
