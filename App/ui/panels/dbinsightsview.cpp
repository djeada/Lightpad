#include "dbinsightsview.h"

#include "../uistylehelper.h"

#include <QApplication>
#include <QHeaderView>
#include <QLocale>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace {

enum Column {
  ColName,
  ColKind,
  ColCompleteness,
  ColDistinct,
  ColMin,
  ColMax,
  ColAverage,
  ColTop,
  ColCount
};

constexpr int kFractionRole = Qt::UserRole + 1;

class BarDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  QColor track;
  QColor fill;
  QColor text;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const double frac = index.data(kFractionRole).toDouble();
    const QString label = index.data(Qt::DisplayRole).toString();
    opt.text.clear();
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF cell = QRectF(option.rect).adjusted(8, 0, -8, 0);
    const QRectF bar(cell.left(), option.rect.bottom() - 11, cell.width(), 5);
    painter->setPen(Qt::NoPen);
    painter->setBrush(track);
    painter->drawRoundedRect(bar, 3, 3);
    if (frac > 0) {
      painter->setBrush(fill);
      painter->drawRoundedRect(
          QRectF(bar.left(), bar.top(), bar.width() * frac, bar.height()), 3,
          3);
    }
    painter->setPen(text);
    QFont f = option.font;
    f.setPointSizeF(f.pointSizeF() * 0.85);
    painter->setFont(f);
    painter->drawText(QRectF(cell.left(), option.rect.top(), cell.width(),
                             bar.top() - option.rect.top() + 1),
                      Qt::AlignLeft | Qt::AlignBottom, label);
    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(qMax(s.height(), 40));
    return s;
  }
};

QString number(double v) {
  if (v == static_cast<qint64>(v) && qAbs(v) < 1e15) {
    return QLocale().toString(static_cast<qint64>(v));
  }
  return QLocale().toString(v, 'g', 6);
}

QString ellipsize(QString s, int max = 40) {
  s.replace('\n', QChar(0x23CE));
  return s.size() > max ? s.left(max - 1) + QChar(0x2026) : s;
}

} // namespace

DbInsightsView::DbInsightsView(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_header = new QLabel(this);
  m_header->setObjectName("dbInsightsHeader");
  m_header->setContentsMargins(10, 6, 10, 6);
  layout->addWidget(m_header);

  m_table = new QTableWidget(0, ColCount, this);
  m_table->setObjectName("dbInsightsTable");
  m_table->setHorizontalHeaderLabels(
      {tr("Column"), tr("Kind"), tr("Completeness"), tr("Distinct"), tr("Min"),
       tr("Max"), tr("Mean / median"), tr("Most frequent")});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->verticalHeader()->hide();
  m_table->setShowGrid(false);
  m_table->setAlternatingRowColors(true);
  m_table->horizontalHeader()->setStretchLastSection(true);
  m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_table->setItemDelegateForColumn(ColCompleteness, new BarDelegate(m_table));
  layout->addWidget(m_table, 1);

  m_empty = new QLabel(
      tr("Run a query, then open Insights to profile its columns."), this);
  m_empty->setAlignment(Qt::AlignCenter);
  m_empty->setObjectName("dbInsightsEmpty");
  layout->addWidget(m_empty, 1);

  clear();
}

void DbInsightsView::clear() {
  m_profiles.clear();
  m_table->setRowCount(0);
  m_table->hide();
  m_empty->show();
  m_header->setText(QString());
  m_header->hide();
}

void DbInsightsView::setResult(const DbResultSet &result,
                               const QString &title) {
  m_profiles = ColumnProfiler::profileAll(result, 5);
  m_table->setRowCount(m_profiles.size());
  for (int r = 0; r < m_profiles.size(); ++r) {
    const ColumnProfile &p = m_profiles[r];
    auto set = [&](int col, const QString &text,
                   Qt::Alignment align = Qt::AlignLeft) {
      auto *item = new QTableWidgetItem(text);
      item->setTextAlignment(align | Qt::AlignVCenter);
      item->setToolTip(text);
      m_table->setItem(r, col, item);
      return item;
    };
    set(ColName, p.name)->setFont([&] {
      QFont f = m_table->font();
      f.setBold(true);
      return f;
    }());
    set(ColKind, p.kindName());
    const double fill = p.rows ? double(p.nonNull()) / p.rows : 0.0;
    auto *comp = set(ColCompleteness,
                     p.nulls == 0 ? tr("100% filled")
                                  : tr("%1% filled · %2 NULL")
                                        .arg(100.0 - p.nullPercent(), 0, 'f', 1)
                                        .arg(QLocale().toString(p.nulls)));
    comp->setData(kFractionRole, fill);
    QString distinct =
        QLocale().toString(p.distinct) +
        (p.distinctIsLowerBound ? QStringLiteral("+") : QString());
    if (p.isUnique()) {
      distinct += tr("  unique");
    }
    set(ColDistinct, distinct, Qt::AlignRight);
    set(ColMin, ellipsize(p.min));
    set(ColMax, ellipsize(p.max));
    set(ColAverage,
        p.hasNumbers
            ? QStringLiteral("%1 / %2").arg(number(p.mean), number(p.median))
            : (p.kind == ColumnKind::Text || p.kind == ColumnKind::Temporal
                   ? (p.minLength == p.maxLength
                          ? tr("length %1").arg(p.minLength)
                          : tr("length %1–%2")
                                .arg(p.minLength)
                                .arg(p.maxLength))
                   : QString()),
        Qt::AlignRight);
    QStringList top;
    for (const ValueCount &vc : p.top) {
      top << QStringLiteral("%1 ×%2")
                 .arg(ellipsize(vc.value, 24))
                 .arg(vc.count);
    }
    set(ColTop, top.join(QStringLiteral("   ")));
  }
  m_table->resizeColumnsToContents();
  m_table->setColumnWidth(ColCompleteness,
                          qMax(170, m_table->columnWidth(ColCompleteness)));
  for (int c : {ColName, ColMin, ColMax, ColAverage}) {
    m_table->setColumnWidth(c, qMin(m_table->columnWidth(c), 240));
  }
  m_empty->hide();
  m_table->show();
  m_header->setText(
      tr("%1%2 rows × %3 columns")
          .arg(title.isEmpty() ? QString() : title + QStringLiteral(" · "))
          .arg(QLocale().toString(qint64(result.rows.size())))
          .arg(result.columns.size()) +
      (result.truncated ? tr(" (first %1 of %2 rows)")
                              .arg(result.rows.size())
                              .arg(result.totalRows)
                        : QString()));
  m_header->show();
}

void DbInsightsView::applyTheme(const Theme &theme) {
  m_theme = theme;
  const QColor text = UIStyleHelper::readableText(theme, theme.backgroundColor,
                                                  theme.foregroundColor);
  const QColor muted = UIStyleHelper::mutedTextColor(theme);
  const QColor selection = theme.accentSoftColor.isValid()
                               ? theme.accentSoftColor
                               : theme.highlightColor;
  if (auto *d = dynamic_cast<BarDelegate *>(
          m_table->itemDelegateForColumn(ColCompleteness))) {
    d->track = theme.surfaceAltColor.isValid() ? theme.surfaceAltColor
                                               : theme.borderColor;
    d->fill = theme.accentColor;
    d->text = muted;
  }
  setStyleSheet(
      QString("QLabel#dbInsightsHeader { background: %1; color: %2; "
              "border-bottom: 1px solid %3; }"
              "QLabel#dbInsightsEmpty { color: %4; background: %5; }"
              "QTableWidget#dbInsightsTable { background: %5; "
              "alternate-background-color: %1;"
              "  color: %6; border: none; outline: none; "
              "selection-background-color: %7; selection-color: %6; }"
              "QHeaderView::section { background: %1; color: %4; border: none;"
              "  border-right: 1px solid %3; border-bottom: 1px solid %3; "
              "padding: 4px 8px; }")
          .arg(theme.surfaceColor.name(), muted.name(),
               theme.borderColor.name(), muted.name(),
               theme.backgroundColor.name(), text.name(), selection.name()));
  m_table->viewport()->update();
}
