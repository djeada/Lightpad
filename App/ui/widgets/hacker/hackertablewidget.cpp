#include "hackertablewidget.h"
#include "../../../theme/themeengine.h"
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>

// --- HackerTableDelegate ---

HackerTableDelegate::HackerTableDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void HackerTableDelegate::paint(QPainter *painter,
                                 const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const auto &c = ThemeEngine::instance().activeTheme().colors;
    QRectF r = option.rect;

    // Alternating row background
    if (index.row() % 2 == 0) {
        painter->fillRect(r, c.surfaceBase);
    } else {
        painter->fillRect(r, c.surfaceRaised);
    }

    // Hover highlight
    if (option.state & QStyle::State_MouseOver) {
        QColor hoverBg = c.treeHoverBg;
        hoverBg.setAlphaF(0.5);
        painter->fillRect(r, hoverBg);
    }

    // Selection
    if (option.state & QStyle::State_Selected) {
        QColor selBg = c.accentSoft;
        selBg.setAlphaF(0.4);
        painter->fillRect(r, selBg);
    }

    // Cell text
    QFont f = option.font;
    f.setFamily(QStringLiteral("monospace"));
    painter->setFont(f);
    painter->setPen(c.textPrimary);

    QRectF textRect = r.adjusted(8, 0, -8, 0);
    QString text = index.data(Qt::DisplayRole).toString();
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

    // Bottom cell border
    painter->setPen(QPen(c.borderSubtle, 0.5));
    painter->drawLine(r.bottomLeft(), r.bottomRight());

    painter->restore();
}

// --- HackerTableHeader ---

HackerTableHeader::HackerTableHeader(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
{
    setFixedHeight(28);
    setSectionsClickable(true);
    setHighlightSections(false);
    setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void HackerTableHeader::paintSection(QPainter *painter, const QRect &rect,
                                      int logicalIndex) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const auto &c = ThemeEngine::instance().activeTheme().colors;

    // Background
    painter->fillRect(rect, c.surfaceRaised);

    // Text
    QFont f = painter->font();
    f.setFamily(QStringLiteral("monospace"));
    f.setBold(true);
    painter->setFont(f);
    painter->setPen(c.textMuted);

    QString text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
    QRect textRect = rect.adjusted(8, 0, -8, 0);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

    // Bottom border
    painter->setPen(QPen(c.borderDefault, 1.0));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    // Right separator
    painter->setPen(QPen(c.borderSubtle, 0.5));
    painter->drawLine(rect.topRight(), rect.bottomRight());

    painter->restore();
}

// --- HackerTableWidget ---

HackerTableWidget::HackerTableWidget(QWidget *parent)
    : HackerWidget(parent)
{
    setupTable();
}

HackerTableWidget::HackerTableWidget(int rows, int columns, QWidget *parent)
    : HackerWidget(parent)
{
    setupTable(rows, columns);
}

void HackerTableWidget::setupTable()
{
    m_table = new QTableWidget(this);
    m_delegate = new HackerTableDelegate(m_table);
    m_table->setItemDelegate(m_delegate);

    m_horizontalHeader = new HackerTableHeader(Qt::Horizontal, m_table);
    m_table->setHorizontalHeader(m_horizontalHeader);

    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setShowGrid(false);
    m_table->setMouseTracking(true);
    m_table->setAlternatingRowColors(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setStyleSheet(
        QStringLiteral("QTableWidget { background: transparent; border: none; }"
                        "QTableCornerButton::section { background: transparent; border: none; }"));

    setFocusProxy(m_table);

    connect(m_table, &QTableWidget::cellClicked,
            this, &HackerTableWidget::cellClicked);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, &HackerTableWidget::cellDoubleClicked);
    connect(m_table, &QTableWidget::currentCellChanged,
            this, &HackerTableWidget::currentCellChanged);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &HackerTableWidget::itemSelectionChanged);
}

void HackerTableWidget::setupTable(int rows, int columns)
{
    setupTable();
    m_table->setRowCount(rows);
    m_table->setColumnCount(columns);
}

void HackerTableWidget::setRowCount(int rows) { m_table->setRowCount(rows); }
void HackerTableWidget::setColumnCount(int columns) { m_table->setColumnCount(columns); }
int HackerTableWidget::rowCount() const { return m_table->rowCount(); }
int HackerTableWidget::columnCount() const { return m_table->columnCount(); }
void HackerTableWidget::setItem(int row, int column, QTableWidgetItem *item) { m_table->setItem(row, column, item); }
QTableWidgetItem *HackerTableWidget::item(int row, int column) const { return m_table->item(row, column); }
void HackerTableWidget::setHorizontalHeaderLabels(const QStringList &labels) { m_table->setHorizontalHeaderLabels(labels); }
void HackerTableWidget::setVerticalHeaderLabels(const QStringList &labels) { m_table->setVerticalHeaderLabels(labels); }
void HackerTableWidget::setColumnWidth(int column, int width) { m_table->setColumnWidth(column, width); }
void HackerTableWidget::setRowHeight(int row, int height) { m_table->setRowHeight(row, height); }
void HackerTableWidget::clear() { m_table->clear(); }
void HackerTableWidget::clearContents() { m_table->clearContents(); }
void HackerTableWidget::setSelectionMode(QAbstractItemView::SelectionMode mode) { m_table->setSelectionMode(mode); }
void HackerTableWidget::setSelectionBehavior(QAbstractItemView::SelectionBehavior behavior) { m_table->setSelectionBehavior(behavior); }
void HackerTableWidget::setAlternatingRowColors(bool enable) { m_table->setAlternatingRowColors(enable); }
void HackerTableWidget::setSortingEnabled(bool enable) { m_table->setSortingEnabled(enable); }

QSize HackerTableWidget::sizeHint() const
{
    return QSize(400, 250);
}

void HackerTableWidget::resizeEvent(QResizeEvent *event)
{
    HackerWidget::resizeEvent(event);
    m_table->setGeometry(rect());
}

void HackerTableWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto &c = colors();
    drawRoundedSurface(p, rect(), c.surfaceBase, radius());
}
