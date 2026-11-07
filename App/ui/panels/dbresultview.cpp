#include "dbresultview.h"

#include "../../database/columnprofiler.h"
#include "../dialogs/styleddialog.h"
#include "../uistylehelper.h"
#include "dbglyphs.h"
#include <QLocale>
#include <QSaveFile>
#include <QSignalBlocker>

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {

constexpr int kMaxCellChars = 300;

QString singleLine(const QString &text) {
  QString t = text;
  t.replace(QLatin1String("\r\n"), QString(QChar(0x23CE)));
  t.replace('\n', QChar(0x23CE));
  t.replace('\r', QChar(0x23CE));
  t.replace('\t', QLatin1String("  "));
  if (t.size() > kMaxCellChars) {
    t = t.left(kMaxCellChars) + QChar(0x2026);
  }
  return t;
}

} // namespace

DbResultModel::DbResultModel(QObject *parent) : QAbstractTableModel(parent) {}

void DbResultModel::setResult(const DbResultSet &result) {
  beginResetModel();
  m_result = result;
  m_numeric = QVector<bool>(m_result.columns.size(), false);
  for (int c = 0; c < m_result.columns.size(); ++c) {
    int seen = 0;
    bool numeric = true;
    for (int r = 0; r < m_result.rows.size() && seen < 200; ++r) {
      const QVariant &v = m_result.rows[r].value(c);
      if (v.isNull() || !v.isValid()) {
        continue;
      }
      ++seen;
      if (!ColumnProfiler::parseNumber(v, nullptr)) {
        numeric = false;
        break;
      }
    }
    m_numeric[c] = numeric && seen > 0;
  }
  endResetModel();
}

void DbResultModel::setTheme(const Theme &theme) {
  m_theme = theme;
  if (rowCount() > 0 && columnCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
  }
}

int DbResultModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : m_result.rows.size();
}

int DbResultModel::columnCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : m_result.columns.size();
}

bool DbResultModel::isNumericColumn(int column) const {
  return column >= 0 && column < m_numeric.size() && m_numeric[column];
}

QVariant DbResultModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_result.rows.size()) {
    return {};
  }
  const QVector<QVariant> &row = m_result.rows[index.row()];
  const QVariant value =
      index.column() < row.size() ? row[index.column()] : QVariant();
  const bool isNull = value.isNull() || !value.isValid();
  switch (role) {
  case Qt::DisplayRole:
    if (isNull)
      return QStringLiteral("NULL");
    if (value.typeId() == QMetaType::QByteArray)
      return tr("BLOB · %1")
          .arg(QLocale().formattedDataSize(value.toByteArray().size()));
    return singleLine(ResultExporter::displayText(value));
  case Qt::ToolTipRole: {
    if (isNull) {
      return QStringLiteral("NULL");
    }
    if (value.typeId() == QMetaType::QByteArray)
      return tr("Binary value (%1 bytes). Double-click to inspect or save it.")
          .arg(value.toByteArray().size());
    QString text = ResultExporter::displayText(value);
    if (text.size() > 2000) {
      text = text.left(2000) + QChar(0x2026);
    }
    return text.toHtmlEscaped()
        .replace('\n', QLatin1String("<br>"))
        .prepend(QStringLiteral("<pre style='margin:0'>"))
        .append(QStringLiteral("</pre>"));
  }
  case Qt::TextAlignmentRole:
    return int(
        (isNumericColumn(index.column()) ? Qt::AlignRight : Qt::AlignLeft) |
        Qt::AlignVCenter);
  case Qt::ForegroundRole:
    if (isNull) {
      return UIStyleHelper::mutedTextColor(m_theme);
    }
    return {};
  case Qt::FontRole:
    if (isNull) {
      QFont f;
      f.setItalic(true);
      return f;
    }
    return {};
  case RawValueRole:
    return value;
  case IsNullRole:
    return isNull;
  default:
    return {};
  }
}

QVariant DbResultModel::headerData(int section, Qt::Orientation orientation,
                                   int role) const {
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      return section < m_result.columns.size() ? m_result.columns[section].name
                                               : QString();
    }
    return section + 1;
  }
  if (role == Qt::TextAlignmentRole && orientation == Qt::Vertical) {
    return int(Qt::AlignRight | Qt::AlignVCenter);
  }
  if (role == Qt::ToolTipRole && orientation == Qt::Horizontal &&
      section < m_result.columns.size() &&
      !m_result.columns[section].type.isEmpty()) {
    return m_result.columns[section].type;
  }
  return {};
}

void DbResultSortProxy::setFilterText(const QString &text) {
  m_filter = text.trimmed();
  invalidateFilter();
}

bool DbResultSortProxy::lessThan(const QModelIndex &l,
                                 const QModelIndex &r) const {
  const QVariant a = l.data(DbResultModel::RawValueRole);
  const QVariant b = r.data(DbResultModel::RawValueRole);
  const bool an = !a.isValid() || a.isNull();
  const bool bn = !b.isValid() || b.isNull();
  if (an || bn) {
    return an && !bn;
  }
  if (a.typeId() == QMetaType::LongLong && b.typeId() == QMetaType::LongLong)
    return a.toLongLong() < b.toLongLong();
  if (a.typeId() == QMetaType::QByteArray &&
      b.typeId() == QMetaType::QByteArray)
    return a.toByteArray() < b.toByteArray();
  double x = 0;
  double y = 0;
  if (ColumnProfiler::parseNumber(a, &x) &&
      ColumnProfiler::parseNumber(b, &y)) {
    return x < y;
  }
  return QString::compare(ResultExporter::displayText(a),
                          ResultExporter::displayText(b),
                          Qt::CaseInsensitive) < 0;
}

bool DbResultSortProxy::filterAcceptsRow(int row,
                                         const QModelIndex &parent) const {
  if (m_filter.isEmpty()) {
    return true;
  }
  const auto *m = sourceModel();
  for (int c = 0; c < m->columnCount(); ++c) {
    const QVariant v =
        m->index(row, c, parent).data(DbResultModel::RawValueRole);
    if (v.isNull() || !v.isValid()) {
      if (QStringLiteral("null").contains(m_filter, Qt::CaseInsensitive)) {
        return true;
      }
      continue;
    }
    if (ResultExporter::displayText(v).contains(m_filter,
                                                Qt::CaseInsensitive)) {
      return true;
    }
  }
  return false;
}

DbResultView::DbResultView(QWidget *parent) : QWidget(parent) {
  m_model = new DbResultModel(this);
  m_proxy = new DbResultSortProxy(this);
  m_proxy->setSourceModel(m_model);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto *bar = new QWidget(this);
  bar->setObjectName("dbResultBar");
  auto *barLayout = new QHBoxLayout(bar);
  barLayout->setContentsMargins(8, 4, 8, 4);
  barLayout->setSpacing(6);

  m_filter = new QLineEdit(bar);
  m_filter->setObjectName("dbResultFilter");
  m_filter->setPlaceholderText(tr("Filter loaded rows…"));
  m_filter->setAccessibleName(tr("Filter result rows"));
  m_filter->setToolTip(tr("Search the rows loaded in this result. Use SQL "
                          "WHERE to search the database."));
  m_filter->setClearButtonEnabled(true);
  m_filter->setMaximumWidth(260);
  barLayout->addWidget(m_filter);

  m_summary = new QLabel(bar);
  m_summary->setObjectName("dbResultSummary");
  m_summary->setTextFormat(Qt::PlainText);
  m_summary->setWordWrap(true);
  m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
  barLayout->addStretch(1);

  m_copyButton = new QToolButton(bar);
  m_copyButton->setText(tr("Copy"));
  m_copyButton->setToolTip(tr("Copy the selection (Ctrl+C)"));
  m_copyButton->setPopupMode(QToolButton::MenuButtonPopup);
  m_copyButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  barLayout->addWidget(m_copyButton);

  m_exportButton = new QToolButton(bar);
  m_exportButton->setText(tr("Export"));
  m_exportButton->setToolTip(tr("Save the visible rows to a file"));
  m_exportButton->setPopupMode(QToolButton::InstantPopup);
  m_exportButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  barLayout->addWidget(m_exportButton);

  m_insightsButton = new QToolButton(bar);
  m_insightsButton->setText(tr("Insights"));
  m_insightsButton->setToolTip(tr("Profile every column of this result"));
  m_insightsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  barLayout->addWidget(m_insightsButton);
  layout->addWidget(bar);

  m_empty = new QLabel(this);
  m_empty->setTextFormat(Qt::PlainText);
  m_empty->setWordWrap(true);
  m_empty->setObjectName("dbResultEmpty");
  m_empty->setAlignment(Qt::AlignCenter);
  m_empty->setContentsMargins(20, 24, 20, 24);
  m_empty->hide();
  layout->addWidget(m_empty);

  m_table = new QTableView(this);
  m_table->setObjectName("dbResultTable");
  m_table->setModel(m_proxy);
  m_table->setSortingEnabled(true);
  m_table->setAlternatingRowColors(true);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table->setWordWrap(false);
  m_table->setTextElideMode(Qt::ElideRight);
  m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_table->horizontalHeader()->setHighlightSections(false);
  m_table->horizontalHeader()->setSectionsMovable(true);
  m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft |
                                                   Qt::AlignVCenter);
  m_table->verticalHeader()->setDefaultSectionSize(24);
  m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  m_table->setCornerButtonEnabled(true);
  layout->addWidget(m_table, 1);

  auto *footer = new QWidget(this);
  footer->setObjectName("dbResultFooter");
  auto *footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(10, 5, 8, 5);
  footerLayout->addWidget(m_summary, 1);
  m_pager = new QWidget(footer);
  auto *pages = new QHBoxLayout(m_pager);
  pages->setContentsMargins(0, 0, 0, 0);
  pages->setSpacing(6);
  m_pageSize = new QComboBox(m_pager);
  m_pageSize->setObjectName("dbPageSize");
  m_pageSize->setAccessibleName(tr("Rows per page"));
  for (int size : {25, 50, 100, 200, 500, 1000})
    m_pageSize->addItem(tr("%1 / page").arg(size), size);
  m_pageSize->setCurrentIndex(3);
  pages->addWidget(m_pageSize);
  m_previous = new QToolButton(m_pager);
  m_previous->setObjectName("dbPreviousPage");
  m_previous->setText(tr("Previous"));
  m_previous->setAccessibleName(tr("Previous page"));
  pages->addWidget(m_previous);
  m_next = new QToolButton(m_pager);
  m_next->setObjectName("dbNextPage");
  m_next->setText(tr("Next"));
  m_next->setAccessibleName(tr("Next page"));
  pages->addWidget(m_next);
  m_reload = new QToolButton(m_pager);
  m_reload->setObjectName("dbReloadPage");
  m_reload->setText(tr("Reload"));
  m_reload->setToolTip(tr("Reload this page from the database"));
  pages->addWidget(m_reload);
  m_pager->hide();
  footerLayout->addWidget(m_pager);
  layout->addWidget(footer);
  connect(m_previous, &QToolButton::clicked, this, [this] {
    --m_page;
    requestPage();
  });
  connect(m_next, &QToolButton::clicked, this, [this] {
    const qint64 offset = m_pageOffsets[m_page] + m_model->rowCount();
    m_pageOffsets.resize(m_page + 1);
    m_pageOffsets.append(offset);
    ++m_page;
    requestPage();
  });
  connect(m_reload, &QToolButton::clicked, this, [this] { requestPage(); });
  connect(m_pageSize, QOverload<int>::of(&QComboBox::activated), this,
          [this] { requestPage(true); });
  m_filterTimer.setSingleShot(true);
  m_filterTimer.setInterval(300);
  connect(&m_filterTimer, &QTimer::timeout, this,
          [this] { requestPage(true); });
  connect(m_table->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this,
          [this](int column, Qt::SortOrder order) {
            if (!m_browsing)
              return;
            m_sortColumn = m_model->result().columns.value(column).name;
            m_sortOrder = order;
            requestPage(true);
          });

  auto *copyMenu = new QMenu(m_copyButton);
  copyMenu->addAction(tr("Copy"), QKeySequence::Copy, this,
                      [this]() { copySelection(false); });
  copyMenu->addAction(tr("Copy with headers"),
                      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this,
                      [this]() { copySelection(true); });
  copyMenu->addSeparator();
  for (auto f :
       {ResultExporter::Format::Csv, ResultExporter::Format::Json,
        ResultExporter::Format::Markdown, ResultExporter::Format::SqlInsert}) {
    copyMenu->addAction(tr("Copy as %1").arg(ResultExporter::formatName(f)),
                        this, [this, f]() { copyAs(f); });
  }
  m_copyButton->setMenu(copyMenu);
  connect(m_copyButton, &QToolButton::clicked, this,
          [this]() { copySelection(false); });

  auto *exportMenu = new QMenu(m_exportButton);
  for (auto f : {ResultExporter::Format::Csv, ResultExporter::Format::Tsv,
                 ResultExporter::Format::Json, ResultExporter::Format::Markdown,
                 ResultExporter::Format::SqlInsert}) {
    exportMenu->addAction(
        tr("%1…").arg(ResultExporter::formatName(f)), this, [this, f]() {
          const QString path = QFileDialog::getSaveFileName(
              this, tr("Export result"),
              QStringLiteral("result.") + ResultExporter::fileExtension(f));
          if (!path.isEmpty()) {
            if (exportToFile(path, f)) {
              emit statusMessage(tr("Saved %1").arg(path));
            } else {
              emit statusMessage(tr("Could not write %1").arg(path));
            }
          }
        });
  }
  m_exportButton->setMenu(exportMenu);

  connect(m_insightsButton, &QToolButton::clicked, this,
          &DbResultView::insightsRequested);
  connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &t) {
    if (m_browsing) {
      m_filterTimer.start();
      updateSummary();
    } else {
      m_proxy->setFilterText(t);
      updateSummary();
    }
  });
  connect(m_table, &QTableView::customContextMenuRequested, this,
          &DbResultView::showContextMenu);
  connect(m_table, &QTableView::doubleClicked, this,
          &DbResultView::showCellValue);

  auto *copyShortcut = new QShortcut(QKeySequence::Copy, m_table);
  copyShortcut->setContext(Qt::WidgetShortcut);
  connect(copyShortcut, &QShortcut::activated, this,
          [this]() { copySelection(false); });
  auto *copyHeadShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), m_table);
  copyHeadShortcut->setContext(Qt::WidgetShortcut);
  connect(copyHeadShortcut, &QShortcut::activated, this,
          [this]() { copySelection(true); });
}

void DbResultView::setResult(const DbResultSet &result, DbEngine engine,
                             const QString &tableName, qint64 elapsedMs) {
  m_engine = engine;
  m_tableName = tableName;
  m_elapsedMs = elapsedMs;
  m_filter->clear();
  m_proxy->setFilterText(QString());
  const QSignalBlocker headerBlocker(m_table->horizontalHeader());
  m_table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
  m_proxy->sort(-1);
  m_model->setResult(result);
  fitColumns();
  updateSummary();
}

void DbResultView::fitColumns() {
  m_table->ensurePolished();
  const QFontMetrics fm(m_table->font());
  const DbResultSet &rs = m_model->result();
  const int sample = qMin<int>(rs.rows.size(), 100);
  for (int c = 0; c < rs.columns.size(); ++c) {

    int width = fm.horizontalAdvance(rs.columns[c].name.toUpper()) + 52;
    for (int r = 0; r < sample; ++r) {
      const QString text =
          m_model->index(r, c).data(Qt::DisplayRole).toString();
      width = qMax(width, fm.horizontalAdvance(text.left(60)) + 28);
    }
    m_table->setColumnWidth(c, qBound(90, width, 400));
  }
}

void DbResultView::updateSummary() {
  const auto &result = m_model->result();
  const int shown = m_proxy->rowCount();
  QStringList parts;
  if (m_browsing) {
    const qint64 offset = m_pageOffsets.value(m_page);
    parts << (shown ? tr("Rows %1–%2").arg(offset + 1).arg(offset + shown)
                    : tr("0 rows"));
    parts << tr("Page %1").arg(m_page + 1);
    if (m_hasNext)
      parts << tr("More rows available");
  } else {
    parts << (shown == result.rows.size() ? tr("%n row(s)", nullptr, shown)
                                          : tr("%1 of %2 loaded rows")
                                                .arg(shown)
                                                .arg(result.rows.size()));
    if (result.truncated) {
      parts << (result.totalRowsKnown
                    ? tr("First %1 of %2 rows")
                          .arg(result.rows.size())
                          .arg(result.totalRows)
                    : tr("Result limited · more rows available"));
    }
  }
  parts << tr("%n column(s)", nullptr, result.columns.size());
  if (m_elapsedMs >= 0)
    parts << (m_elapsedMs >= 1000
                  ? tr("%1 s").arg(m_elapsedMs / 1000.0, 0, 'f', 2)
                  : tr("%1 ms").arg(m_elapsedMs));
  if (m_loading)
    parts << tr("Loading…");
  m_summary->setText(parts.join(QStringLiteral(" · ")));
  m_summary->setToolTip(
      m_browsing
          ? tr("Filtering and sorting apply to the whole table. Export saves "
               "this page.")
          : tr("Filtering, sorting, insights and export use the loaded rows."));
  const bool ready = !m_loading && !m_filterTimer.isActive();
  m_previous->setEnabled(ready && m_page > 0);
  m_next->setEnabled(ready && m_hasNext && shown > 0);
  m_reload->setEnabled(!m_loading);
  m_pageSize->setEnabled(!m_loading);
  m_empty->setVisible(!m_loading && shown == 0);
  m_empty->setText(!m_pageError.isEmpty() ? m_pageError
                   : !m_filter->text().isEmpty()
                       ? tr("No rows match your filter.\nClear the filter to "
                            "see the rows again.")
                       : (m_browsing
                              ? tr("This table has no rows on this page.")
                              : tr("This query returned no rows.")));
  m_exportButton->setEnabled(shown > 0 && !m_loading);
  m_insightsButton->setEnabled(shown > 0 && !m_loading);
}

void DbResultView::configureTableBrowser(const DbTableInfo &table,
                                         DbEngine engine) {
  m_browsing = true;
  m_browseTable = table;
  m_engine = engine;
  m_tableName = DbCatalog::qualifiedName(engine, table);
  m_filter->setPlaceholderText(tr("Filter entire table…"));
  m_filter->setToolTip(
      tr("Search all rows in this table, including rows on other pages."));
  m_pager->show();
  m_table->setSortingEnabled(false);
  m_table->horizontalHeader()->setSectionsClickable(true);
  m_table->horizontalHeader()->setSortIndicatorShown(true);
  m_proxy->sort(-1);
  updateSummary();
}

QString DbResultView::pageSql() const {
  return DbCatalog::tablePageSql(
      m_engine, m_browseTable, m_pageSize->currentData().toInt(), m_page,
      m_filter->text(), m_sortColumn, m_sortOrder == Qt::DescendingOrder,
      m_pageOffsets.value(m_page));
}

void DbResultView::requestPage(bool reset) {
  if (m_filterTimer.isActive()) {
    m_filterTimer.stop();
    reset = true;
  }
  m_resetPending = m_resetPending || reset;
  if (m_loading) {
    m_reloadPending = true;
    return;
  }
  if (m_resetPending) {
    m_page = 0;
    m_pageOffsets = {0};
    m_resetPending = false;
  }
  m_pageError.clear();
  setLoading(true);
  emit pageRequested();
}

void DbResultView::setLoading(bool loading) {
  m_loading = loading;
  updateSummary();
  if (!loading && m_reloadPending) {
    m_reloadPending = false;
    QTimer::singleShot(0, this, [this] { requestPage(); });
  }
}

void DbResultView::setPageResult(const DbResultSet &result, qint64 elapsedMs) {
  DbResultSet page = result;
  const int size = m_pageSize->currentData().toInt();
  m_hasNext = page.rows.size() > size || page.truncated;
  if (page.rows.size() > size)
    page.rows.resize(size);
  page.truncated = m_hasNext;
  page.totalRowsKnown = !m_hasNext;
  page.totalRows =
      m_pageOffsets.value(m_page) + page.rows.size() + (m_hasNext ? 1 : 0);
  m_elapsedMs = elapsedMs;
  m_model->setResult(page);
  if (m_page == 0)
    fitColumns();
  m_table->scrollToTop();
  updateSummary();
}

void DbResultView::setPageError(const QString &message) {
  m_filterTimer.stop();
  m_reloadPending = false;
  m_resetPending = false;
  m_pageError = message;
  m_hasNext = false;
  DbResultSet empty;
  empty.columns = m_model->result().columns;
  m_model->setResult(empty);
  updateSummary();
}

void DbResultView::selectedRowsAndColumns(QVector<int> *rows,
                                          QVector<int> *columns) const {
  const QModelIndexList selected = m_table->selectionModel()->selectedIndexes();
  QSet<int> colSet;
  QVector<int> proxyRows;
  QSet<int> seenRows;
  for (const QModelIndex &idx : selected) {
    const QModelIndex src = m_proxy->mapToSource(idx);
    colSet.insert(src.column());
    if (!seenRows.contains(idx.row())) {
      seenRows.insert(idx.row());
      proxyRows.append(idx.row());
    }
  }
  std::sort(proxyRows.begin(), proxyRows.end());
  rows->clear();
  for (int pr : proxyRows) {
    rows->append(m_proxy->mapToSource(m_proxy->index(pr, 0)).row());
  }
  columns->clear();
  for (int c : colSet) {
    columns->append(c);
  }
  std::sort(columns->begin(), columns->end());
}

QString DbResultView::selectionAs(ResultExporter::Format format,
                                  bool withHeader) const {
  QVector<int> rows;
  QVector<int> cols;
  selectedRowsAndColumns(&rows, &cols);
  if (rows.isEmpty()) {
    return {};
  }
  return ResultExporter::exportResult(
      m_model->result(), format, rows, cols, withHeader,
      m_tableName.isEmpty() ? QStringLiteral("table_name") : m_tableName,
      m_engine);
}

void DbResultView::copySelection(bool withHeader) {
  const QString text = selectionAs(ResultExporter::Format::Tsv, withHeader);
  if (text.isEmpty()) {
    return;
  }

  QVector<int> rows;
  QVector<int> cols;
  selectedRowsAndColumns(&rows, &cols);
  if (!withHeader && rows.size() == 1 && cols.size() == 1) {
    const QVariant v = m_model->result().rows[rows[0]].value(cols[0]);
    QApplication::clipboard()->setText(
        v.isNull() ? QString() : ResultExporter::displayText(v));
  } else {
    QApplication::clipboard()->setText(text);
  }
  emit statusMessage(tr("Copied %1 cell(s)").arg(rows.size() * cols.size()));
}

void DbResultView::copyAs(ResultExporter::Format format) {
  QString text = selectionAs(format, true);
  if (text.isEmpty()) {

    QVector<int> rows;
    for (int r = 0; r < m_proxy->rowCount(); ++r) {
      rows.append(m_proxy->mapToSource(m_proxy->index(r, 0)).row());
    }
    DbResultSet source = m_model->result();
    if (rows.isEmpty())
      source.rows.clear();
    text = ResultExporter::exportResult(
        source, format, rows, {}, true,
        m_tableName.isEmpty() ? QStringLiteral("table_name") : m_tableName,
        m_engine);
  }
  QApplication::clipboard()->setText(text);
  emit statusMessage(
      tr("Copied as %1").arg(ResultExporter::formatName(format)));
}

bool DbResultView::exportToFile(const QString &path,
                                ResultExporter::Format format) const {

  QVector<int> rows;
  for (int r = 0; r < m_proxy->rowCount(); ++r) {
    rows.append(m_proxy->mapToSource(m_proxy->index(r, 0)).row());
  }
  const QString table =
      m_tableName.isEmpty() ? QStringLiteral("table_name") : m_tableName;

  DbResultSet source = m_model->result();
  if (rows.isEmpty()) {
    source.rows.clear();
  }
  const QString text = ResultExporter::exportResult(source, format, rows, {},
                                                    true, table, m_engine);
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  const QByteArray bytes = text.toUtf8();
  return file.write(bytes) == bytes.size() && file.commit();
}

bool DbResultView::saveCellValue(const QModelIndex &index,
                                 const QString &path) const {
  if (!index.isValid())
    return false;
  const QVariant value =
      m_proxy->mapToSource(index).data(DbResultModel::RawValueRole);
  if (!value.isValid() || value.isNull())
    return false;
  const QByteArray bytes = value.typeId() == QMetaType::QByteArray
                               ? value.toByteArray()
                               : ResultExporter::displayText(value).toUtf8();
  QSaveFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() &&
         file.commit();
}

void DbResultView::showCellValue(const QModelIndex &index) {
  if (!index.isValid())
    return;
  const QModelIndex source = m_proxy->mapToSource(index);
  const QVariant value = source.data(DbResultModel::RawValueRole);
  const bool binary = value.typeId() == QMetaType::QByteArray;
  auto *dialog = new StyledDialog(this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(
      tr("Value · %1")
          .arg(m_model->result().columns.value(source.column()).name));
  dialog->resize(720, 480);
  auto *layout = new QVBoxLayout(dialog);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);
  auto *description = new QLabel(dialog);
  description->setTextFormat(Qt::PlainText);
  layout->addWidget(description);
  auto *tabs = new QTabWidget(dialog);
  tabs->setDocumentMode(true);
  layout->addWidget(tabs, 1);
  auto addText = [&](const QString &title, const QString &text) {
    auto *edit = new QPlainTextEdit(tabs);
    edit->setReadOnly(true);
    edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    edit->setPlainText(text);
    tabs->addTab(edit, title);
  };
  constexpr int previewBytes = 64 * 1024;
  QString copyText;
  if (binary) {
    const QByteArray bytes = value.toByteArray();
    const QByteArray preview = bytes.left(previewBytes);
    QString hex;
    for (qsizetype offset = 0; offset < preview.size(); offset += 16) {
      const QByteArray line = preview.mid(offset, 16);
      QString ascii;
      for (unsigned char byte : line)
        ascii += byte >= 32 && byte < 127 ? QChar(byte) : QChar('.');
      hex += QString("%1  %2  %3\n")
                 .arg(offset, 8, 16, QChar('0'))
                 .arg(QString::fromLatin1(line.toHex(' ')).leftJustified(47),
                      ascii);
    }
    addText(tr("Hex"), hex);
    addText(tr("Text (UTF-8)"), QString::fromUtf8(preview));
    description->setText(tr("Binary value · %1 bytes%2")
                             .arg(bytes.size())
                             .arg(bytes.size() > previewBytes
                                      ? tr(" · preview of the first 64 KiB; "
                                           "save to inspect all bytes")
                                      : QString()));
  } else {
    const QString full = value.isNull() ? QStringLiteral("NULL")
                                        : ResultExporter::displayText(value);
    constexpr int previewChars = 256 * 1024;
    QString text = full.left(previewChars);
    if (full.size() <= previewChars) {
      const QJsonDocument json = QJsonDocument::fromJson(text.toUtf8());
      if (json.isObject() || json.isArray())
        text = QString::fromUtf8(json.toJson(QJsonDocument::Indented));
    }
    addText(tr("Text"), text);
    copyText = full;
    description->setText(
        value.isNull() ? tr("NULL value")
                       : tr("Text value · %1 characters%2")
                             .arg(full.size())
                             .arg(full.size() > previewChars
                                      ? tr(" · preview of the first 256 KiB; "
                                           "save to inspect the full value")
                                      : QString()));
  }
  auto *buttons = new QHBoxLayout();
  auto *save =
      new QPushButton(binary ? tr("Save bytes…") : tr("Save value…"), dialog);
  save->setEnabled(!value.isNull());
  buttons->addWidget(save);
  connect(
      save, &QPushButton::clicked, dialog,
      [this, dialog, value, binary, description] {
        const QString path = QFileDialog::getSaveFileName(
            dialog, tr("Save cell value"), binary ? "value.bin" : "value.txt");
        if (path.isEmpty())
          return;
        const QByteArray bytes =
            binary ? value.toByteArray()
                   : ResultExporter::displayText(value).toUtf8();
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly) &&
            file.write(bytes) == bytes.size() && file.commit()) {
          description->setText(tr("Saved %1").arg(QFileInfo(path).fileName()));
          emit statusMessage(tr("Saved %1").arg(path));
        } else {
          description->setText(
              tr("Could not save the value: %1").arg(file.errorString()));
        }
      });
  buttons->addStretch();
  auto *copy = new QPushButton(binary ? tr("Copy hex") : tr("Copy"), dialog);
  connect(copy, &QPushButton::clicked, dialog, [value, binary, copyText] {
    QApplication::clipboard()->setText(
        binary ? QString::fromLatin1(value.toByteArray().toHex()) : copyText);
  });
  buttons->addWidget(copy);
  auto *close = new QPushButton(tr("Close"), dialog);
  connect(close, &QPushButton::clicked, dialog, &QDialog::accept);
  buttons->addWidget(close);
  layout->addLayout(buttons);
  dialog->applyTheme(m_theme);
  dialog->show();
}

void DbResultView::showContextMenu(const QPoint &pos) {
  const QModelIndex idx = m_table->indexAt(pos);
  QMenu menu(this);
  menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));
  QAction *copy = menu.addAction(tr("Copy"));
  QAction *copyHead = menu.addAction(tr("Copy with headers"));
  QMenu *copyAsMenu = menu.addMenu(tr("Copy as"));
  for (auto f :
       {ResultExporter::Format::Csv, ResultExporter::Format::Json,
        ResultExporter::Format::Markdown, ResultExporter::Format::SqlInsert}) {
    copyAsMenu->addAction(ResultExporter::formatName(f), this,
                          [this, f]() { copyAs(f); });
  }
  menu.addSeparator();
  QAction *view = menu.addAction(tr("View value…"));
  QAction *filterValue = menu.addAction(tr("Filter by this value"));
  QAction *clearFilter = menu.addAction(tr("Clear filter"));
  menu.addSeparator();
  QAction *insights = menu.addAction(tr("Column insights"));

  const bool hasSelection = m_table->selectionModel()->hasSelection();
  copy->setEnabled(hasSelection);
  copyHead->setEnabled(hasSelection);
  view->setEnabled(idx.isValid());
  filterValue->setEnabled(idx.isValid());
  clearFilter->setEnabled(!m_filter->text().isEmpty());

  QAction *chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
  if (chosen == copy) {
    copySelection(false);
  } else if (chosen == copyHead) {
    copySelection(true);
  } else if (chosen == view) {
    showCellValue(idx);
  } else if (chosen == filterValue) {
    const QVariant v =
        m_proxy->mapToSource(idx).data(DbResultModel::RawValueRole);
    m_filter->setText(v.isNull() ? QStringLiteral("NULL")
                                 : ResultExporter::displayText(v));
  } else if (chosen == clearFilter) {
    m_filter->clear();
  } else if (chosen == insights) {
    emit insightsRequested();
  }
}

void DbResultView::applyTheme(const Theme &theme) {
  m_theme = theme;
  m_model->setTheme(theme);
  const QColor text = UIStyleHelper::readableText(theme, theme.backgroundColor,
                                                  theme.foregroundColor);
  const QColor muted = UIStyleHelper::mutedTextColor(theme);
  const QColor alt = theme.surfaceColor;
  const QColor selection = theme.accentSoftColor.isValid()
                               ? theme.accentSoftColor
                               : theme.highlightColor;
  const QColor selectionText =
      UIStyleHelper::readableText(theme, selection, text);
  m_copyButton->setIcon(DbGlyphs::icon(DbGlyphs::Glyph::Copy, text));
  m_exportButton->setIcon(DbGlyphs::icon(DbGlyphs::Glyph::Export, text));
  m_insightsButton->setIcon(DbGlyphs::icon(DbGlyphs::Glyph::Insights, text));

  setStyleSheet(
      QString(
          "QLabel#dbResultEmpty { color: %10; }"
          "QWidget#dbResultFooter { background: %1; border-top: 1px solid %2; }"
          "QLabel#dbResultSummary { color: %10; }"
          "QComboBox#dbPageSize { background: %3; color: %4; border: 1px solid "
          "%2; padding: 3px 6px; border-radius: 4px; }"
          "QWidget#dbResultBar { background: %1; border-bottom: 1px solid %2; }"
          "QLineEdit#dbResultFilter { background: %3; color: %4; border: 1px "
          "solid %2;"
          "  border-radius: 4px; padding: 3px 8px; }"
          "QLineEdit#dbResultFilter:focus { border-color: %5; }"
          "QToolButton { background: transparent; color: %4; border: 1px solid "
          "transparent;"
          "  border-radius: 4px; padding: 3px 8px; }"
          "QToolButton:disabled { color: %10; }"
          "QToolButton:hover { background: %6; border-color: %2; }"
          "QToolButton::menu-button { border: none; width: 12px; }"
          "QToolButton::menu-indicator { image: none; width: 0; }"
          "QTableView#dbResultTable { background: %3; "
          "alternate-background-color: %7;"
          "  color: %4; border: none; gridline-color: %2; outline: none;"
          "  selection-background-color: %8; selection-color: %9; }"
          "QTableView#dbResultTable QTableCornerButton::section { background: "
          "%1; border: none;"
          "  border-bottom: 1px solid %2; border-right: 1px solid %2; }"
          "QHeaderView::section { background: %1; color: %10; border: none;"
          "  border-right: 1px solid %2; border-bottom: 1px solid %2; padding: "
          "4px 8px; }"
          "QHeaderView::section:hover { color: %4; }")
          .arg(theme.surfaceColor.name(), theme.borderColor.name(),
               theme.backgroundColor.name(), text.name(),
               theme.accentColor.name(), theme.hoverColor.name(), alt.name(),
               selection.name(), selectionText.name(), muted.name()));
  updateSummary();
}
