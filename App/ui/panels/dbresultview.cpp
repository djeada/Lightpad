#include "dbresultview.h"

#include "../../database/columnprofiler.h"
#include "../uistylehelper.h"
#include "dbglyphs.h"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
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
    return isNull ? QStringLiteral("NULL")
                  : singleLine(ResultExporter::displayText(value));
  case Qt::ToolTipRole: {
    if (isNull) {
      return QStringLiteral("NULL");
    }
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
  m_filter->setPlaceholderText(tr("Filter rows…"));
  m_filter->setClearButtonEnabled(true);
  m_filter->setMaximumWidth(260);
  barLayout->addWidget(m_filter);

  m_summary = new QLabel(bar);
  m_summary->setObjectName("dbResultSummary");
  m_summary->setTextFormat(Qt::RichText);
  barLayout->addWidget(m_summary, 1);

  m_copyButton = new QToolButton(bar);
  m_copyButton->setText(tr("Copy"));
  m_copyButton->setToolTip(tr("Copy the selection (Ctrl+C)"));
  m_copyButton->setPopupMode(QToolButton::MenuButtonPopup);
  m_copyButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  barLayout->addWidget(m_copyButton);

  m_exportButton = new QToolButton(bar);
  m_exportButton->setText(tr("Export"));
  m_exportButton->setToolTip(tr("Save the rows to a file"));
  m_exportButton->setPopupMode(QToolButton::InstantPopup);
  m_exportButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  barLayout->addWidget(m_exportButton);

  m_insightsButton = new QToolButton(bar);
  m_insightsButton->setText(tr("Insights"));
  m_insightsButton->setToolTip(tr("Profile every column of this result"));
  m_insightsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  barLayout->addWidget(m_insightsButton);
  layout->addWidget(bar);

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
    m_proxy->setFilterText(t);
    updateSummary();
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
  m_table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
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
  const DbResultSet &rs = m_model->result();
  const int shown = m_proxy->rowCount();
  const QColor muted = UIStyleHelper::mutedTextColor(m_theme);
  QStringList parts;
  if (shown != rs.rows.size()) {
    parts << tr("%1 of %2 rows").arg(shown).arg(rs.rows.size());
  } else {
    parts << tr("%n row(s)", nullptr, shown);
  }
  parts << tr("%n column(s)", nullptr, rs.columns.size());
  if (m_elapsedMs >= 0) {
    parts << (m_elapsedMs >= 1000
                  ? tr("%1 s").arg(m_elapsedMs / 1000.0, 0, 'f', 2)
                  : tr("%1 ms").arg(m_elapsedMs));
  }
  QString html = QStringLiteral("<span style='color:%1'>%2</span>")
                     .arg(muted.name(), parts.join(QStringLiteral(" · ")));
  if (rs.truncated) {
    html +=
        QStringLiteral(" &nbsp;<span style='color:%1'><b>%2</b></span>")
            .arg(m_theme.warningColor.isValid() ? m_theme.warningColor.name()
                                                : QStringLiteral("#d29922"),
                 tr("Showing the first %1 of %2 rows")
                     .arg(rs.rows.size())
                     .arg(rs.totalRows));
  }
  m_summary->setText(html);
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
    text = ResultExporter::exportResult(
        m_model->result(), format, rows, {}, true,
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
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  return file.write(text.toUtf8()) >= 0;
}

void DbResultView::showCellValue(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }
  const QModelIndex src = m_proxy->mapToSource(index);
  const QVariant v = src.data(DbResultModel::RawValueRole);
  QString text =
      v.isNull() ? QStringLiteral("NULL") : ResultExporter::displayText(v);
  const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
  if (!doc.isNull() && (doc.isObject() || doc.isArray())) {
    text = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
  }
  auto *dialog = new QDialog(this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(m_model->result().columns.value(src.column()).name);
  dialog->resize(560, 380);
  auto *layout = new QVBoxLayout(dialog);
  auto *edit = new QPlainTextEdit(dialog);
  edit->setReadOnly(true);
  edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  edit->setPlainText(text);
  layout->addWidget(edit);
  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  auto *copy = new QPushButton(tr("Copy"), dialog);
  connect(copy, &QPushButton::clicked, dialog,
          [text]() { QApplication::clipboard()->setText(text); });
  auto *close = new QPushButton(tr("Close"), dialog);
  connect(close, &QPushButton::clicked, dialog, &QDialog::accept);
  buttons->addWidget(copy);
  buttons->addWidget(close);
  layout->addLayout(buttons);
  dialog->setStyleSheet(styleSheet());
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
          "QWidget#dbResultBar { background: %1; border-bottom: 1px solid %2; }"
          "QLineEdit#dbResultFilter { background: %3; color: %4; border: 1px "
          "solid %2;"
          "  border-radius: 4px; padding: 3px 8px; }"
          "QLineEdit#dbResultFilter:focus { border-color: %5; }"
          "QToolButton { background: transparent; color: %4; border: 1px solid "
          "transparent;"
          "  border-radius: 4px; padding: 3px 8px; }"
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
