#include "hackerlistwidget.h"
#include "../../../theme/themeengine.h"
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>

// --- HackerListDelegate ---

HackerListDelegate::HackerListDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void HackerListDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const auto &c = ThemeEngine::instance().activeTheme().colors;
    QRectF r = option.rect;

    // Hover / selection background
    if (option.state & QStyle::State_Selected) {
        QColor selBg = c.accentSoft;
        selBg.setAlphaF(0.4);
        painter->setPen(Qt::NoPen);
        painter->setBrush(selBg);
        painter->drawRoundedRect(r.adjusted(2, 1, -2, -1), 4, 4);

        // Subtle glow on selected
        qreal glow = ThemeEngine::instance().glowIntensity();
        if (glow > 0.01) {
            QColor gc = c.accentGlow;
            gc.setAlphaF(glow * 0.15);
            painter->setBrush(gc);
            painter->drawRoundedRect(r.adjusted(0, -1, 0, 1), 4, 4);
        }
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(c.treeHoverBg);
        painter->drawRoundedRect(r.adjusted(2, 1, -2, -1), 4, 4);
    }

    const int hPad = 12;
    const int iconSize = 16;
    qreal x = r.left() + hPad;
    qreal centerY = r.center().y();

    // Icon
    QVariant iconVar = index.data(IconRole);
    if (iconVar.isValid()) {
        QIcon icon = iconVar.value<QIcon>();
        if (!icon.isNull()) {
            QRectF iconRect(x, centerY - iconSize / 2.0, iconSize, iconSize);
            icon.paint(painter, iconRect.toRect());
            x += iconSize + 8;
        }
    }

    // Primary text
    QFont f = option.font;
    f.setFamily(QStringLiteral("monospace"));
    painter->setFont(f);

    QColor textColor = (option.state & QStyle::State_Selected)
                           ? c.textPrimary : c.textPrimary;
    painter->setPen(textColor);

    QString primaryText = index.data(Qt::DisplayRole).toString();
    QFontMetrics fm(f);
    int textH = fm.height();

    // Description text
    QString desc = index.data(DescriptionRole).toString();
    QString shortcut = index.data(ShortcutRole).toString();

    if (!desc.isEmpty()) {
        // Two-line layout
        qreal topY = centerY - textH;
        painter->drawText(QRectF(x, topY, r.right() - x - hPad, textH),
                          Qt::AlignVCenter | Qt::AlignLeft, primaryText);
        painter->setPen(c.textMuted);
        QFont smallFont = f;
        smallFont.setPointSizeF(f.pointSizeF() * 0.85);
        painter->setFont(smallFont);
        painter->drawText(QRectF(x, centerY, r.right() - x - hPad, textH),
                          Qt::AlignVCenter | Qt::AlignLeft, desc);
    } else {
        painter->drawText(QRectF(x, r.top(), r.right() - x - hPad, r.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, primaryText);
    }

    // Shortcut on the right
    if (!shortcut.isEmpty()) {
        painter->setPen(c.textMuted);
        QFont monoSmall = f;
        monoSmall.setPointSizeF(f.pointSizeF() * 0.85);
        painter->setFont(monoSmall);
        QFontMetrics sfm(monoSmall);
        int sw = sfm.horizontalAdvance(shortcut);
        painter->drawText(QRectF(r.right() - sw - hPad, r.top(), sw, r.height()),
                          Qt::AlignVCenter | Qt::AlignRight, shortcut);
    }

    painter->restore();
}

QSize HackerListDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    QString desc = index.data(DescriptionRole).toString();
    int h = desc.isEmpty() ? 32 : 48;
    return QSize(option.rect.width(), h);
}

// --- HackerListWidget ---

HackerListWidget::HackerListWidget(QWidget *parent)
    : HackerWidget(parent)
{
    setupList();
}

void HackerListWidget::setupList()
{
    m_list = new QListWidget(this);
    m_delegate = new HackerListDelegate(m_list);
    m_list->setItemDelegate(m_delegate);

    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setMouseTracking(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setStyleSheet(QStringLiteral("QListWidget { background: transparent; border: none; }"));

    setFocusProxy(m_list);

    connect(m_list, &QListWidget::currentItemChanged,
            this, &HackerListWidget::currentItemChanged);
    connect(m_list, &QListWidget::currentRowChanged,
            this, &HackerListWidget::currentRowChanged);
    connect(m_list, &QListWidget::itemActivated,
            this, &HackerListWidget::itemActivated);
    connect(m_list, &QListWidget::itemClicked,
            this, &HackerListWidget::itemClicked);
}

void HackerListWidget::addItem(const QString &text) { m_list->addItem(text); }
void HackerListWidget::addItem(QListWidgetItem *item) { m_list->addItem(item); }
QListWidgetItem *HackerListWidget::item(int row) const { return m_list->item(row); }
int HackerListWidget::count() const { return m_list->count(); }
int HackerListWidget::currentRow() const { return m_list->currentRow(); }
void HackerListWidget::setCurrentRow(int row) { m_list->setCurrentRow(row); }
QListWidgetItem *HackerListWidget::currentItem() const { return m_list->currentItem(); }
void HackerListWidget::clear() { m_list->clear(); }
QListWidgetItem *HackerListWidget::takeItem(int row) { return m_list->takeItem(row); }
void HackerListWidget::insertItem(int row, const QString &text) { m_list->insertItem(row, text); }
void HackerListWidget::insertItem(int row, QListWidgetItem *item) { m_list->insertItem(row, item); }
void HackerListWidget::scrollToItem(const QListWidgetItem *item) { m_list->scrollToItem(item); }

QSize HackerListWidget::sizeHint() const
{
    return QSize(250, 200);
}

void HackerListWidget::resizeEvent(QResizeEvent *event)
{
    HackerWidget::resizeEvent(event);
    m_list->setGeometry(rect());
}

void HackerListWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Transparent background — the list paints items via delegate
    const auto &c = colors();
    drawRoundedSurface(p, rect(), c.surfaceBase, radius());
}
