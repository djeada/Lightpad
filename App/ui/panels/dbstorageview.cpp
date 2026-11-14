#include "dbstorageview.h"
#include "../paneltypography.h"
#include "../uistylehelper.h"
#include "dbglyphs.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLocale>
#include <QScrollBar>
#include <QVBoxLayout>

namespace {
constexpr int SchemaRole = Qt::UserRole + 1;
constexpr int TableRole = Qt::UserRole + 2;
constexpr int NumberRole = Qt::UserRole + 3;
class StorageItem : public QTreeWidgetItem {
public:
  using QTreeWidgetItem::QTreeWidgetItem;
  bool operator<(const QTreeWidgetItem &other) const override {
    const int column = treeWidget()->sortColumn();
    if (column > 0) {
      const qint64 left = data(column, NumberRole).toLongLong(),
                   right = other.data(column, NumberRole).toLongLong();
      if ((left < 0) != (right < 0))
        return treeWidget()->header()->sortIndicatorOrder() ==
                       Qt::AscendingOrder
                   ? right < 0
                   : left < 0;
      return left < right;
    }
    return text(column).compare(other.text(column), Qt::CaseInsensitive) < 0;
  }
};
QString bytesDetail(qint64 bytes) {
  return bytes < 0 ? QObject::tr("Unavailable")
                   : QLocale().toString(bytes) + QObject::tr(" bytes");
}
} // namespace

DbStorageView::DbStorageView(QWidget *parent) : QWidget(parent) {
  setObjectName("dbStorageView");
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 6, 8, 6);
  m_summary = new QLabel(this);
  m_summary->setObjectName("dbStorageSummary");
  m_summary->setWordWrap(true);
  layout->addWidget(m_summary);
  auto *actions = new QHBoxLayout;
  m_refresh = new QPushButton(tr("Refresh sizes"), this);
  m_refresh->setObjectName("dbRefreshSizes");
  m_browse = new QPushButton(tr("Browse table"), this);
  m_browse->setObjectName("dbStorageBrowse");
  m_count = new QPushButton(tr("Count rows"), this);
  m_count->setObjectName("dbStorageCount");
  actions->addWidget(m_refresh);
  actions->addWidget(m_browse);
  actions->addWidget(m_count);
  actions->addStretch();
  layout->addLayout(actions);
  m_status = new QLabel(this);
  m_status->setObjectName("dbStorageStatus");
  m_status->setWordWrap(true);
  m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(m_status);
  m_tree = new QTreeWidget(this);
  m_tree->setObjectName("dbStorageTree");
  m_tree->setAccessibleName(tr("Database and table storage"));
  m_tree->setHeaderLabels(
      {tr("Object"), tr("Rows"), tr("Data"), tr("Indexes"), tr("Total")});
  m_tree->setRootIsDecorated(true);
  m_tree->setUniformRowHeights(true);
  m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tree->setTextElideMode(Qt::ElideMiddle);
  m_tree->header()->setStretchLastSection(false);
  m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
  m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_tree->setSortingEnabled(true);
  m_tree->sortItems(0, Qt::AscendingOrder);
  layout->addWidget(m_tree, 1);
  connect(m_tree, &QTreeWidget::itemSelectionChanged, this,
          &DbStorageView::updateActions);
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
          [this] { m_browse->click(); });
  connect(m_refresh, &QPushButton::clicked, this, [this] {
    if (!m_connection)
      return;
    if (m_connection->statisticsLoading())
      m_connection->cancelStatistics();
    else
      m_connection->refreshStatistics(true);
  });
  auto selected = [this](bool count) {
    auto *item = m_tree->currentItem();
    if (!item || item->data(0, TableRole).toString().isEmpty())
      return;
    const QString schema = item->data(0, SchemaRole).toString(),
                  table = item->data(0, TableRole).toString();
    if (count)
      emit countRowsRequested(schema, table);
    else
      emit browseTableRequested(schema, table);
  };
  connect(m_browse, &QPushButton::clicked, this,
          [selected] { selected(false); });
  connect(m_count, &QPushButton::clicked, this, [selected] { selected(true); });
  refresh();
}

void DbStorageView::setConnection(DbConnection *connection) {
  if (m_connection == connection) {
    refresh();
    return;
  }
  if (m_connection)
    disconnect(m_connection, nullptr, this, nullptr);
  m_connection = connection;
  m_requests.clear();
  if (connection) {
    connect(connection, &DbConnection::statisticsChanged, this,
            &DbStorageView::refresh);
    connect(connection, &DbConnection::schemaLoaded, this,
            &DbStorageView::refresh);
    connect(connection, &DbConnection::stateChanged, this,
            [this](DbConnectionState state) {
              if (state != DbConnectionState::Connected)
                m_requests.clear();
              refresh();
            });
    connect(connection, &QObject::destroyed, this, &DbStorageView::refresh);
    connect(connection, &DbConnection::requestStarted, this,
            [this](quint64 id) {
              m_requests.insert(id);
              updateActions();
            });
    connect(connection, &DbConnection::requestFinished, this,
            [this](quint64 id) {
              m_requests.remove(id);
              updateActions();
            });
  }
  refresh();
}

void DbStorageView::refresh() {
  QString schema, selected;
  if (auto *item = m_tree->currentItem()) {
    schema = item->data(0, SchemaRole).toString();
    selected = item->data(0, TableRole).toString();
  }
  const int scroll = m_tree->verticalScrollBar()->value();
  m_tree->setUpdatesEnabled(false);
  m_tree->setSortingEnabled(false);
  m_tree->clear();
  if (!m_connection) {
    m_summary->setText(tr("Select a connection to inspect storage."));
    m_status->clear();
  } else {
    const auto &stats = m_connection->statistics();
    m_summary->setText(
        m_connection->profile().name + " · " +
        tr("%1 tables and views").arg(m_connection->schema().tables.size()) +
        (stats.updatedAt.isValid()
             ? tr(" · Updated %1").arg(stats.updatedAt.toString("HH:mm:ss"))
             : QString()));
    QString status =
        m_connection->statisticsLoading()
            ? (m_connection->isBusy() ? tr("Waiting for the running query…")
                                      : tr("Loading storage statistics…"))
        : stats.tableSizesDeferred
            ? tr("Table sizes are deferred for this large SQLite database. "
                 "Refresh sizes to load them.")
            : tr("≈ Estimate · — Unavailable · Count rows for an exact total.");
    if (!m_connection->isConnected())
      status = tr("Connect to refresh these statistics.");
    else if (!m_connection->lastStatisticsError().isEmpty())
      status =
          tr("Some storage statistics are unavailable: %1")
              .arg(m_connection->lastStatisticsError().section('\n', 0, 0));
    m_status->setText(status);
    m_status->setToolTip(
        DbStatisticsQueries::description(m_connection->profile().engine) +
        '\n' + m_connection->lastStatisticsError());
    auto fill = [&](QTreeWidgetItem *item, const DbStorageInfo *info) {
      const qint64 numbers[] = {
          info ? info->rows : -1, info ? info->dataBytes : -1,
          info ? info->indexBytes : -1, info ? info->totalBytes : -1};
      for (int c = 1; c < 5; ++c) {
        item->setData(c, NumberRole, numbers[c - 1]);
        const QString text =
            c == 1 ? DbStatisticsQueries::formatRows(
                         numbers[0], info && info->rowsEstimated)
                   : (info && info->sizeEstimated && numbers[c - 1] >= 0
                          ? QString::fromUtf8("≈ ")
                          : QString()) +
                         DbStatisticsQueries::formatBytes(numbers[c - 1]);
        item->setText(c, text);
        item->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
        item->setToolTip(c, c == 1
                                ? text + (info && info->rowsEstimated
                                              ? tr(" · estimate, not COUNT(*)")
                                              : QString())
                                : bytesDetail(numbers[c - 1]));
      }
    };
    QMap<QString, QTreeWidgetItem *> databases;
    for (auto it = stats.databases.cbegin(); it != stats.databases.cend();
         ++it) {
      auto *root = new StorageItem(m_tree);
      root->setText(0, it.key());
      fill(root, &it.value());
      root->setExpanded(true);
      QString detail =
          DbStatisticsQueries::description(m_connection->profile().engine);
      if (it->freeBytes >= 0)
        detail += tr("\nFree pages: %1").arg(bytesDetail(it->freeBytes));
      if (it->walBytes >= 0)
        detail += tr("\nWAL file: %1").arg(bytesDetail(it->walBytes));
      root->setToolTip(0, detail);
      databases[it.key()] = root;
    }
    const auto engine = m_connection->profile().engine;
    for (const auto &table : m_connection->schema().tables) {
      const QString database =
          engine == DbEngine::Sqlite || engine == DbEngine::MySql
              ? table.schema
              : m_connection->currentDatabase();
      QTreeWidgetItem *root = databases.value(database);
      if (!root) {
        root = new StorageItem(m_tree);
        root->setText(0, database.isEmpty() ? m_connection->profile().name
                                            : database);
        fill(root, nullptr);
        root->setExpanded(true);
        databases[database] = root;
      }
      auto *item = new StorageItem(root);
      item->setText(
          0, (engine != DbEngine::Sqlite && engine != DbEngine::MySql &&
                      !table.schema.isEmpty()
                  ? table.schema + '.'
                  : QString()) +
                 table.name + (table.isView ? tr(" (view)") : QString()));
      item->setData(0, SchemaRole, table.schema);
      item->setData(0, TableRole, table.name);
      item->setToolTip(
          0,
          table.schema + '.' + table.name +
              (table.isView ? tr("\nViews do not have ordinary table storage.")
                            : QString()));
      fill(item, stats.table(table.schema, table.name));
    }
  }
  m_tree->setSortingEnabled(true);
  m_tree->setUpdatesEnabled(true);
  selectTable(schema, selected);
  m_tree->verticalScrollBar()->setValue(scroll);
  fitColumns();
  updateActions();
}

void DbStorageView::selectTable(const QString &schema, const QString &table) {
  if (table.isEmpty())
    return;
  for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
    if ((*it)->data(0, SchemaRole).toString() == schema &&
        (*it)->data(0, TableRole).toString() == table) {
      m_tree->setCurrentItem(*it);
      m_tree->scrollToItem(*it);
      return;
    }
}

void DbStorageView::updateActions() {
  const bool connected = m_connection && m_connection->isConnected();
  const bool loading = m_connection && m_connection->statisticsLoading();
  m_refresh->setEnabled(connected);
  m_refresh->setText(loading ? tr("Cancel refresh") : tr("Refresh sizes"));
  const bool selected =
      m_tree->currentItem() &&
      !m_tree->currentItem()->data(0, TableRole).toString().isEmpty();
  m_browse->setEnabled(connected && selected && !loading &&
                       !m_connection->isBusy() && m_requests.isEmpty());
  m_count->setEnabled(m_browse->isEnabled());
}

void DbStorageView::fitColumns() {
  const QFontMetrics metrics(m_tree->font());
  m_tree->setMinimumHeight(metrics.height() + 12 + m_tree->header()->height());
  for (int column = 1; column < 5; ++column) {
    int width = metrics.horizontalAdvance(m_tree->headerItem()->text(column));
    int sampled = 0;
    for (QTreeWidgetItemIterator it(m_tree); *it && sampled < 2000;
         ++it, ++sampled)
      width = qMax(width, metrics.horizontalAdvance((*it)->text(column)));
    m_tree->setColumnWidth(column, width + 36);
  }
}

void DbStorageView::applyTheme(const Theme &theme) {
  const QString text = UIStyleHelper::readableText(theme, theme.backgroundColor,
                                                   theme.foregroundColor)
                           .name();
  m_tree->setStyleSheet(
      QString("QTreeWidget { background: %1; color: %2; border: none; } "
              "QTreeWidget::item { padding: 5px 8px; } QHeaderView::section { "
              "background: %3; color: %2; padding: 6px 8px; border: none; } "
              "QTreeWidget::item:selected { background: %4; }")
          .arg(theme.backgroundColor.name(), text, theme.surfaceColor.name(),
               theme.accentSoftColor.name()));
  setStyleSheet(QString("QWidget#dbStorageView { background: %1; color: %2; } "
                        "QLabel { color: %2; }")
                    .arg(theme.backgroundColor.name(), text));
  PanelTypography::apply(this, PanelTypography::contentFont(this));
  fitColumns();
}
