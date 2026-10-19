#include "debugvalueinspector.h"

#include "../uistylehelper.h"
#include "debugvalueformatter.h"
#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

namespace {
constexpr int Reference = Qt::UserRole;
constexpr int Expression = Qt::UserRole + 1;
constexpr int RawValue = Qt::UserRole + 2;
constexpr int IndexedCount = Qt::UserRole + 3;
constexpr int NamedCount = Qt::UserRole + 4;
constexpr int Indexed = Qt::UserRole + 5;
constexpr int Loaded = Qt::UserRole + 6;
constexpr int PageAction = Qt::UserRole + 7;
constexpr int PageStart = Qt::UserRole + 8;
constexpr int MoreReference = Qt::UserRole + 9;
constexpr int PageSize = 100;
constexpr int TableRows = 50;
constexpr int TableColumns = 50;
} // namespace

DebugValueInspector::DebugValueInspector(QWidget *parent) : QWidget(parent) {
  setObjectName("debugValueInspector");
  QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  fixedFont.setPointSize(qMax(10, font().pointSize()));
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);
  auto *heading = new QHBoxLayout;
  m_title = new QLabel(tr("Value inspector"), this);
  m_title->setObjectName("debugValueTitle");
  m_title->setTextFormat(Qt::PlainText);
  m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_title->setWordWrap(true);
  heading->addWidget(m_title, 1);
  m_watch = new QPushButton(tr("Add to Watches"), this);
  m_watch->setEnabled(false);
  connect(m_watch, &QPushButton::clicked, this,
          [this]() { emit addWatchRequested(m_value.evaluateName); });
  heading->addWidget(m_watch);
  auto *copy = new QPushButton(tr("Copy"), this);
  copy->setToolTip(
      tr("Copy selected cells as tab-separated text, or the formatted value"));
  connect(copy, &QPushButton::clicked, this,
          &DebugValueInspector::copySelection);
  heading->addWidget(copy);
  layout->addLayout(heading);

  m_filter = new QLineEdit(this);
  m_filter->setObjectName("debugValueFilter");
  m_filter->setPlaceholderText(tr("Filter loaded names, values and types…"));
  m_filter->setClearButtonEnabled(true);
  connect(m_filter, &QLineEdit::textChanged, this,
          &DebugValueInspector::filterTree);
  layout->addWidget(m_filter);

  m_tabs = new QTabWidget(this);
  m_tabs->setObjectName("debugValueTabs");
  m_tree = new QTreeWidget(this);
  m_tree->setObjectName("debugValueTree");
  m_tree->setHeaderLabels({tr("Name / index"), tr("Value"), tr("Type")});
  m_tree->setUniformRowHeights(true);
  m_tree->setAlternatingRowColors(true);
  m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_tree->setFont(fixedFont);
  m_tree->header()->setStretchLastSection(false);
  m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_tree->setColumnWidth(0, 220);
  connect(m_tree, &QTreeWidget::itemExpanded, this,
          [this](QTreeWidgetItem *item) {
            if (!item->data(0, Loaded).toBool())
              loadChildren(item);
          });
  connect(m_tree, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem *item, int) {
            if (item->data(0, PageAction).toBool() && item->parent()) {
              loadChildren(item->parent(), item->data(0, PageStart).toInt());
            }
          });
  connect(m_tree, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *item) {
            if (item && !item->data(0, PageAction).toBool()) {
              m_text->setPlainText(DebugValueFormatter::format(
                  item->data(0, RawValue).toString()));
            }
          });
  m_tabs->addTab(m_tree, tr("Tree"));

  m_table = new QTableWidget(this);
  m_table->setObjectName("debugValueTable");
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_table->setAlternatingRowColors(true);
  m_table->setWordWrap(false);
  m_table->setFont(fixedFont);
  m_table->horizontalHeader()->setDefaultSectionSize(130);
  m_table->horizontalHeader()->setMinimumSectionSize(60);
  m_table->verticalHeader()->setDefaultSectionSize(28);
  m_tabs->addTab(m_table, tr("Table"));
  connect(m_table, &QTableWidget::cellActivated, this,
          [this](int rowIndex, int columnIndex) {
            if (!m_root || rowIndex >= m_root->childCount())
              return;
            auto *row = m_root->child(rowIndex);
            auto *target = row;
            const auto *header = m_table->horizontalHeaderItem(columnIndex);
            if (header) {
              for (int i = 0; i < row->childCount(); ++i) {
                if (row->child(i)->text(0) == header->text()) {
                  target = row->child(i);
                  break;
                }
              }
            }
            m_tabs->setCurrentIndex(0);
            row->setExpanded(true);
            target->setExpanded(true);
            m_tree->setCurrentItem(target);
            m_tree->scrollToItem(target);
          });

  m_text = new QPlainTextEdit(this);
  m_text->setObjectName("debugValueText");
  m_text->setReadOnly(true);
  m_text->setFont(fixedFont);
  m_text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  m_text->setPlaceholderText(tr("Evaluate an expression, or double-click a "
                                "variable or watch to inspect it."));
  m_tabs->addTab(m_text, tr("Text"));
  connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
    m_filter->setVisible(index != 2);
    if (index == 1)
      updateTable();
  });
  connect(m_tabs, &QTabWidget::tabBarClicked, this,
          [this]() { m_autoTable = false; });
  auto *copyShortcut = new QShortcut(QKeySequence::Copy, m_table);
  copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(copyShortcut, &QShortcut::activated, this,
          &DebugValueInspector::copySelection);
  layout->addWidget(m_tabs, 1);
  m_status = new QLabel(
      tr("Inspect nested values without scrolling through a single long line."),
      this);
  m_status->setObjectName("debugValueStatus");
  m_status->setTextFormat(Qt::PlainText);
  m_status->setWordWrap(true);
  layout->addWidget(m_status);
}

void DebugValueInspector::setDapClient(DapClient *client) {
  if (m_client)
    disconnect(m_client, nullptr, this, nullptr);
  clear();
  m_client = client;
  if (!client)
    return;
  connect(client, &DapClient::inspectionVariablesReceived, this,
          &DebugValueInspector::receiveChildren);
  connect(client, &DapClient::inspectionVariablesFailed, this,
          [this](int seq, const QString &message) {
            if (!m_requests.contains(seq))
              return;
            const Request request = m_requests.take(seq);
            request.item->setData(0, Loaded, false);
            m_status->setText(
                tr("Could not load children: %1. Collapse and expand to retry.")
                    .arg(message));
          });
  connect(client, &DapClient::stateChanged, this,
          [this](DapClient::State state) {
            if (state != DapClient::State::Stopped)
              invalidate();
          });
  connect(client, &QObject::destroyed, this, &DebugValueInspector::invalidate);
  connect(client, &DapClient::continued, this,
          &DebugValueInspector::invalidate);
}

bool DebugValueInspector::isIndexed(const DapVariable &value) {
  const QString type = value.type.toLower();
  return value.indexedVariables > 0 || type == "list" || type == "tuple" ||
         type == "ndarray" || type == "array" || type.endsWith(']');
}

bool DebugValueInspector::isIndex(const QString &name) {
  return indexForName(name) >= 0;
}

qlonglong DebugValueInspector::indexForName(QString name) {
  name = name.trimmed();
  if (name.startsWith('[') && name.endsWith(']'))
    name = name.mid(1, name.size() - 2);
  bool ok = false;
  const qlonglong index = name.toLongLong(&ok);
  return ok && index >= 0 ? index : -1;
}

QTreeWidgetItem *DebugValueInspector::appendValue(QTreeWidgetItem *parent,
                                                  const DapVariable &value) {
  auto *item =
      parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
  ++m_itemCount;
  item->setText(0, value.name);
  item->setText(1, DebugValueFormatter::summary(value.value));
  item->setText(2, value.type);
  item->setData(0, Reference, value.variablesReference);
  item->setData(0, Expression, value.evaluateName);
  item->setData(0, RawValue, value.value);
  item->setData(0, IndexedCount, value.indexedVariables);
  item->setData(0, NamedCount, value.namedVariables);
  item->setData(0, Indexed, isIndexed(value));
  for (int c = 0; c < 3; ++c)
    item->setToolTip(c, DebugValueFormatter::format(value.value));
  if (value.variablesReference > 0)
    item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  return item;
}

void DebugValueInspector::clear() {
  m_requests.clear();
  m_root = nullptr;
  m_value = {};
  m_live = false;
  m_itemCount = 0;
  m_tree->clear();
  m_table->clear();
  m_table->setRowCount(0);
  m_table->setColumnCount(0);
  m_text->clear();
  m_watch->setEnabled(false);
  m_title->setText(tr("Value inspector"));
  m_status->setText(
      tr("Evaluate an expression, or double-click a variable or watch."));
}

void DebugValueInspector::setValue(const DapVariable &value) {
  clear();
  m_value = value;
  m_live = m_client && m_client->state() == DapClient::State::Stopped;
  m_autoTable = true;
  m_filter->clear();
  m_title->setText(value.type.isEmpty()
                       ? value.name
                       : tr("%1  ·  %2").arg(value.name, value.type));
  m_watch->setEnabled(!value.evaluateName.isEmpty());
  m_text->setPlainText(DebugValueFormatter::format(value.value));
  m_root = appendValue(nullptr, value);
  m_tree->setCurrentItem(m_root);
  m_tabs->setCurrentIndex(value.variablesReference > 0 ? 0 : 2);
  m_status->setText(
      value.variablesReference > 0
          ? tr("Expand a row to inspect its children. Table view aligns nested "
               "sequences and records.")
          : tr("Formatted adapter value. Select text to copy any part."));
  m_root->setExpanded(true);
}

void DebugValueInspector::invalidate() {
  m_live = false;
  m_requests.clear();
  if (m_root)
    m_status->setText(
        tr("Snapshot from an earlier pause. Evaluate again to inspect the "
           "current value; unloaded children are unavailable."));
}

void DebugValueInspector::loadChildren(QTreeWidgetItem *item, int start,
                                       int referenceOverride) {
  if (!m_live || !m_client || !item || item->data(0, Reference).toInt() <= 0)
    return;
  for (const Request &request : std::as_const(m_requests)) {
    if (request.item == item)
      return;
  }
  if (m_itemCount >= 10000) {
    m_status->setText(tr("Tree preview limited to 10,000 items. Inspect a "
                         "narrower expression to continue."));
    return;
  }
  const QString filter =
      item->data(0, Indexed).toBool() ? QStringLiteral("indexed") : QString();
  const int total = item->data(0, IndexedCount).toInt() > 0
                        ? item->data(0, IndexedCount).toInt()
                        : item->data(0, NamedCount).toInt();

  const int count =
      total > 0 ? qMin(PageSize, qMax(1, total - start)) : PageSize;
  const int reference = referenceOverride > 0 ? referenceOverride
                        : start > 0 && item->data(0, MoreReference).toInt() > 0
                            ? item->data(0, MoreReference).toInt()
                            : item->data(0, Reference).toInt();
  const int seq = m_client->inspectVariables(reference, filter, start, count);
  if (seq > 0) {
    m_requests.insert(seq, {item, start});
    m_status->setText(tr("Loading children…"));
  }
}

void DebugValueInspector::receiveChildren(int seq,
                                          const QList<DapVariable> &variables) {
  if (!m_requests.contains(seq) || !m_live)
    return;
  const Request request = m_requests.take(seq);
  QTreeWidgetItem *parent = request.item;
  if (parent->data(0, Indexed).toBool() && request.start > 0 &&
      !request.prefixRetry &&
      std::none_of(variables.cbegin(), variables.cend(),
                   [&](const DapVariable &value) {
                     return indexForName(value.name) >= request.start;
                   }) &&
      std::any_of(
          variables.cbegin(), variables.cend(),
          [](const DapVariable &value) { return isIndex(value.name); })) {

    const int total = parent->data(0, IndexedCount).toInt();
    const int wanted = total > 0 ? qMin(total, request.start + PageSize)
                                 : request.start + PageSize;
    const int retry = m_client->inspectVariables(
        parent->data(0, Reference).toInt(), "indexed", 0, qMin(10000, wanted));
    if (retry > 0)
      m_requests.insert(retry, {parent, request.start, true});
    return;
  }

  if (parent->data(0, Indexed).toBool() &&
      std::none_of(
          variables.cbegin(), variables.cend(),
          [](const DapVariable &value) { return isIndex(value.name); })) {
    for (const DapVariable &value : variables) {
      const QString name = value.name.trimmed();
      if (name.startsWith('[') && name.endsWith(']') && name.contains(':') &&
          value.variablesReference > 0 &&
          value.variablesReference != parent->data(0, Reference).toInt()) {
        bool beginOk = false;
        bool endOk = false;
        const int colon = name.indexOf(':');
        const int begin = name.mid(1, colon - 1).toInt(&beginOk);
        const int end = name.mid(colon + 1).chopped(1).toInt(&endOk);
        if (!beginOk || !endOk || request.start < begin || request.start >= end)
          continue;
        if (request.start == 0) {
          parent->setData(0, Reference, value.variablesReference);
          parent->setData(0, IndexedCount, end);
        }
        if (parent == m_root) {
          QStringList details;
          for (const DapVariable &metadata : variables) {
            if (metadata.name == QLatin1String("shape") ||
                metadata.name == QLatin1String("dtype"))
              details << (metadata.name == QLatin1String("dtype")
                              ? metadata.value
                              : metadata.name + " " + metadata.value);
          }
          if (!details.isEmpty())
            m_title->setText(m_value.name + "  ·  " + details.join("  ·  "));
        }
        loadChildren(parent, request.start, value.variablesReference);
        return;
      }
    }
  }

  QList<DapVariable> members;
  for (const DapVariable &value : variables) {
    if (parent->data(0, Indexed).toBool()) {

      if (value.name == QLatin1String("more") && value.variablesReference > 0)
        parent->setData(0, MoreReference, value.variablesReference);
      if (value.name == QLatin1String("len()")) {
        bool ok = false;
        const int total = value.value.toInt(&ok);
        if (ok)
          parent->setData(0, IndexedCount, total);
      }
      const qlonglong index = indexForName(value.name);

      if (index < request.start || index >= request.start + PageSize)
        continue;
    }
    members << value;
  }
  if (parent->data(0, Indexed).toBool()) {
    std::stable_sort(members.begin(), members.end(),
                     [](const DapVariable &a, const DapVariable &b) {
                       return indexForName(a.name) < indexForName(b.name);
                     });
  }
  if (request.start == 0) {
    qDeleteAll(parent->takeChildren());
  } else if (parent->childCount() && parent->child(parent->childCount() - 1)
                                         ->data(0, PageAction)
                                         .toBool()) {
    delete parent->takeChild(parent->childCount() - 1);
  }
  const int shown = qMin(PageSize, static_cast<int>(members.size()));
  for (int i = 0; i < shown; ++i)
    appendValue(parent, members.at(i));
  parent->setData(0, Loaded, true);
  const int next = request.start + shown;
  const int total = parent->data(0, IndexedCount).toInt();
  const bool more = parent->data(0, Indexed).toBool() && shown == PageSize &&
                    (total == 0 || next < total);
  if (more) {
    auto *page = new QTreeWidgetItem(parent);
    page->setText(0, tr("Load next %1 items…").arg(PageSize));
    page->setToolTip(0,
                     tr("Double-click or press Enter to load another page."));
    page->setData(0, PageAction, true);
    page->setData(0, PageStart, next);
  }
  m_status->setText(
      members.size() > PageSize
          ? tr("Adapter returned %1 members; showing the first %2. Inspect a "
               "narrower expression for more.")
                .arg(members.size())
                .arg(PageSize)
          : tr("%1 children loaded%2. Expand rows for details.")
                .arg(next)
                .arg(total > 0 ? tr(" of %1").arg(total) : QString()));
  if (parent == m_root && m_autoTable && isIndexed(m_value)) {
    for (const DapVariable &value : members) {
      if (isIndex(value.name) &&
          (isIndexed(value) || value.type == QLatin1String("dict"))) {
        m_tabs->setCurrentIndex(1);
        break;
      }
    }
  }
  if (m_tabs->currentIndex() == 1)
    updateTable();
  filterTree();
}

void DebugValueInspector::updateTable() {
  if (!m_root)
    return;
  QList<QTreeWidgetItem *> rows;
  for (int i = 0; i < m_root->childCount() && rows.size() < TableRows; ++i) {
    auto *item = m_root->child(i);
    if (!item->data(0, PageAction).toBool())
      rows << item;
  }
  QStringList columns;
  bool nested = false;
  for (auto *row : rows) {
    if (row->data(0, Reference).toInt() > 0) {
      nested = true;
      if (!row->data(0, Loaded).toBool())
        loadChildren(row);
      for (int i = 0; i < row->childCount() && columns.size() < TableColumns;
           ++i) {
        auto *cell = row->child(i);
        if (!cell->data(0, PageAction).toBool() &&
            cell->text(0) != QLatin1String("special variables") &&
            cell->text(0) != QLatin1String("function variables") &&
            cell->text(0) != QLatin1String("len()") &&
            !columns.contains(cell->text(0)))
          columns << cell->text(0);
      }
    }
  }
  if (columns.isEmpty())
    columns << tr("Value");
  m_table->clear();
  m_table->setRowCount(rows.size());
  m_table->setColumnCount(columns.size());
  m_table->setHorizontalHeaderLabels(columns);
  QStringList labels;
  for (int r = 0; r < rows.size(); ++r) {
    auto *row = rows.at(r);
    labels << row->text(0);
    QHash<QString, QString> cells;
    for (int i = 0; i < row->childCount(); ++i) {
      auto *cell = row->child(i);
      if (!cell->data(0, PageAction).toBool())
        cells.insert(cell->text(0), cell->data(0, RawValue).toString());
    }
    for (int c = 0; c < columns.size(); ++c) {
      const bool scalar = !nested || row->data(0, Reference).toInt() <= 0;
      const QString text =
          scalar ? (c == 0 ? row->data(0, RawValue).toString() : QString())
                 : cells.value(columns.at(c));
      const QString display = DebugValueFormatter::tableCell(text);
      auto *cell =
          new QTableWidgetItem(DebugValueFormatter::summary(display, 256));
      cell->setData(Qt::UserRole, display);
      cell->setToolTip(tr("Double-click or press Enter to inspect in Tree.\n%1")
                           .arg(DebugValueFormatter::format(text)));
      bool numeric = false;
      display.toDouble(&numeric);
      if (numeric)
        cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      m_table->setItem(r, c, cell);
    }
  }
  m_table->setVerticalHeaderLabels(labels);
  m_tabs->setTabText(
      1, tr("Table (%1 × %2)").arg(rows.size()).arg(columns.size()));
  if (m_requests.isEmpty() && m_live) {
    m_status->setText(tr("%1 rows × %2 columns shown · preview limited to %3 "
                         "rows and %4 columns. Blank cells represent missing "
                         "entries; use Tree to inspect more.")
                          .arg(rows.size())
                          .arg(columns.size())
                          .arg(TableRows)
                          .arg(TableColumns));
  }
}

void DebugValueInspector::filterTree() {
  const QString query = m_filter->text().trimmed();
  std::function<bool(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *item) {
    bool matches = query.isEmpty() ||
                   item->text(0).contains(query, Qt::CaseInsensitive) ||
                   item->data(0, RawValue)
                       .toString()
                       .contains(query, Qt::CaseInsensitive) ||
                   item->text(2).contains(query, Qt::CaseInsensitive) ||
                   item->data(0, PageAction).toBool();
    for (int i = 0; i < item->childCount(); ++i)
      matches = visit(item->child(i)) || matches;
    item->setHidden(!matches);
    return matches;
  };
  if (m_root)
    visit(m_root);
  for (int r = 0; r < m_table->rowCount(); ++r) {
    bool matches =
        query.isEmpty() || m_table->verticalHeaderItem(r)->text().contains(
                               query, Qt::CaseInsensitive);
    for (int c = 0; c < m_table->columnCount(); ++c) {
      auto *item = m_table->item(r, c);
      matches = matches || (item && item->data(Qt::UserRole)
                                        .toString()
                                        .contains(query, Qt::CaseInsensitive));
    }
    m_table->setRowHidden(r, !matches);
  }
}

void DebugValueInspector::copySelection() {
  QString text;
  if (m_tabs->currentIndex() == 1 && !m_table->selectedRanges().isEmpty()) {
    const auto range = m_table->selectedRanges().first();
    QStringList lines;
    for (int r = range.topRow(); r <= range.bottomRow(); ++r) {
      QStringList cells;
      for (int c = range.leftColumn(); c <= range.rightColumn(); ++c) {
        auto *item = m_table->item(r, c);
        cells << (item ? item->data(Qt::UserRole).toString() : QString());
      }
      lines << cells.join('\t');
    }
    text = lines.join('\n');
  } else {
    text =
        m_text->textCursor().hasSelection()
            ? m_text->textCursor().selectedText().replace(QChar(0x2029), '\n')
            : m_text->toPlainText();
  }
  QApplication::clipboard()->setText(text);
}

void DebugValueInspector::applyTheme(const Theme &theme) {
  QPalette colors = palette();
  colors.setColor(QPalette::Window, theme.backgroundColor);
  colors.setColor(QPalette::WindowText, theme.foregroundColor);
  colors.setColor(QPalette::Base, theme.surfaceColor);
  colors.setColor(QPalette::AlternateBase, theme.surfaceAltColor);
  colors.setColor(QPalette::Text, theme.foregroundColor);
  colors.setColor(QPalette::Button, theme.surfaceColor);
  colors.setColor(QPalette::ButtonText, theme.foregroundColor);
  colors.setColor(QPalette::Highlight, theme.accentSoftColor);
  colors.setColor(QPalette::HighlightedText,
                  UIStyleHelper::readableText(theme, theme.accentSoftColor,
                                              theme.foregroundColor));
  setAutoFillBackground(true);
  setPalette(colors);
  m_tree->setPalette(colors);
  m_table->setPalette(colors);
  for (QHeaderView *header :
       {m_table->horizontalHeader(), m_table->verticalHeader()}) {
    header->setPalette(colors);
    header->viewport()->setPalette(colors);
    header->setStyleSheet(QString("QHeaderView { background: %1; }")
                              .arg(theme.surfaceColor.name()));
  }
  m_title->setStyleSheet(UIStyleHelper::headingStyle(theme, 13));
  m_status->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
  setStyleSheet(QString("QWidget#%1 { background: %2; color: %3; }")
                    .arg(objectName(), theme.backgroundColor.name(),
                         theme.foregroundColor.name()) +
                UIStyleHelper::formDialogStyle(theme) +
                UIStyleHelper::treeWidgetStyle(theme) +
                UIStyleHelper::tableWidgetStyle(theme) +
                UIStyleHelper::plainTextEditStyle(theme) +
                UIStyleHelper::lineEditStyle(theme) +
                UIStyleHelper::tabWidgetStyle(theme) +
                UIStyleHelper::secondaryButtonStyle(theme) +
                UIStyleHelper::scrollBarStyle(theme) +
                QString("QTableWidget { alternate-background-color: %1; }")
                    .arg(theme.surfaceAltColor.name()));
}
