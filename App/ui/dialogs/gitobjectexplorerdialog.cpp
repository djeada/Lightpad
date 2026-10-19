#include "gitobjectexplorerdialog.h"

#include "../../theme/colorcontrast.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QSet>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QThreadPool>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

namespace {
constexpr int PageSize = 250;

QString bytes(qint64 value) {
  if (value < 1024)
    return QStringLiteral("%1 B").arg(value);
  if (value < 1024 * 1024)
    return QStringLiteral("%1 KiB").arg(value / 1024.0, 0, 'f', 1);
  return QStringLiteral("%1 MiB").arg(value / (1024.0 * 1024), 0, 'f', 1);
}

QTreeWidget *table(const QString &name, const QStringList &headers,
                   QWidget *parent) {
  auto *tree = new QTreeWidget(parent);
  tree->setObjectName(name);
  tree->setHeaderLabels(headers);
  tree->setRootIsDecorated(false);
  tree->setSelectionMode(QAbstractItemView::SingleSelection);
  tree->setAlternatingRowColors(true);
  tree->setUniformRowHeights(true);
  tree->setAllColumnsShowFocus(true);
  tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  tree->header()->setStretchLastSection(true);
  return tree;
}

void cancel(std::shared_ptr<std::atomic_bool> &token) {
  if (token)
    token->store(true);
}

template <typename Work, typename Done>
std::shared_ptr<std::atomic_bool> background(GitObjectExplorerDialog *dialog,
                                             Work work, Done done) {
  auto token = std::make_shared<std::atomic_bool>(false);
  QPointer<GitObjectExplorerDialog> receiver(dialog);
  QThreadPool::globalInstance()->start([receiver, token, work, done]() {
    auto result = work([token]() { return token->load(); });
    QMetaObject::invokeMethod(
        qApp,
        [receiver, token, done, result = std::move(result)]() {
          if (receiver && !token->load())
            done(receiver.data(), result);
        },
        Qt::QueuedConnection);
  });
  return token;
}

class ObjectNode : public QGraphicsRectItem {
public:
  ObjectNode(const QRectF &rect, std::function<void()> activate)
      : QGraphicsRectItem(rect), m_activate(std::move(activate)) {
    setCursor(Qt::PointingHandCursor);
  }

protected:
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override {
    m_activate();
    event->accept();
  }

private:
  std::function<void()> m_activate;
};

class ObjectRow : public QTreeWidgetItem {
public:
  using QTreeWidgetItem::QTreeWidgetItem;
  bool operator<(const QTreeWidgetItem &other) const override {
    const int column = treeWidget()->sortColumn();
    if (column == 2)
      return data(column, Qt::UserRole).toLongLong() <
             other.data(column, Qt::UserRole).toLongLong();
    if (column == 4)
      return data(column, Qt::UserRole).toULongLong() <
             other.data(column, Qt::UserRole).toULongLong();
    return QTreeWidgetItem::operator<(other);
  }
};
} // namespace

GitObjectExplorerDialog::GitObjectExplorerDialog(const QString &repositoryPath,
                                                 const Theme &theme,
                                                 QWidget *parent)
    : GitDialog(parent), m_repositoryPath(repositoryPath) {
  setWindowTitle(
      tr("Git Objects & Packfiles — %1").arg(QDir(repositoryPath).dirName()));
  setGitDescription(
      tr("Inspect stored objects, locate hashes and explore dependencies."));
  resize(1220, 800);
  setMinimumSize(820, 560);
  auto *layout = new QVBoxLayout(this);
  auto *heading = new QHBoxLayout;
  auto *title = new QLabel(tr("Objects & Packfiles"), this);
  useGitHeading(title);
  QFont titleFont = title->font();
  titleFont.setPointSize(titleFont.pointSize() + 4);
  titleFont.setBold(true);
  title->setFont(titleFont);
  heading->addWidget(title);
  heading->addStretch();
  auto *refreshButton = new QPushButton(tr("Refresh"), this);
  refreshButton->setObjectName("objectRefreshButton");
  auto *cancelButton = new QPushButton(tr("Stop"), this);
  cancelButton->setToolTip(
      tr("Stop background inspection, search or verification"));
  heading->addWidget(refreshButton);
  heading->addWidget(cancelButton);
  layout->addLayout(heading);
  m_summary = new QLabel(tr("Reading repository storage…"), this);
  m_summary->setWordWrap(true);
  layout->addWidget(m_summary);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setChildrenCollapsible(false);
  auto *storage = new QWidget(splitter);
  auto *storageLayout = new QVBoxLayout(storage);
  storageLayout->setContentsMargins(0, 0, 0, 0);
  storageLayout->addWidget(new QLabel(tr("Storage"), storage));
  m_packs =
      table("objectPackTable",
            {tr("Packfile"), tr("Objects"), tr("Size"), tr("State")}, storage);
  m_packs->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_packs->setColumnWidth(0, 170);
  m_packs->setMinimumWidth(260);
  storageLayout->addWidget(m_packs, 1);
  m_verify = new QPushButton(tr("Verify Selected Pack"), storage);
  m_verify->setObjectName("objectVerifyButton");
  m_verify->setEnabled(false);
  storageLayout->addWidget(m_verify);
  auto *storageHint = new QLabel(
      tr("Select a pack to browse its index. All storage searches every pack "
         "and loose object, including unreachable objects."),
      storage);
  storageHint->setWordWrap(true);
  storageLayout->addWidget(storageHint);

  auto *right = new QWidget(splitter);
  auto *rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  auto *searchRow = new QHBoxLayout;
  m_search = new QLineEdit(right);
  m_search->setObjectName("objectHashSearch");
  m_search->setPlaceholderText(
      tr("Find an object by full hash or prefix (4+ hex characters)…"));
  m_search->setAccessibleName(tr("Object hash or prefix"));
  m_search->setClearButtonEnabled(true);
  auto *all = new QPushButton(tr("Search All Storage"), right);
  all->setObjectName("objectSearchAllButton");
  searchRow->addWidget(m_search, 1);
  searchRow->addWidget(all);
  rightLayout->addLayout(searchRow);
  auto *vertical = new QSplitter(Qt::Vertical, right);
  m_objects = table("objectResultsTable",
                    {tr("Hash"), tr("Type"), tr("Size"), tr("Storage"),
                     tr("Offset"), tr("Encoding")},
                    vertical);
  m_objects->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_objects->setColumnWidth(0, 145);
  m_objects->header()->setSectionResizeMode(3, QHeaderView::Interactive);
  m_objects->setColumnWidth(3, 190);
  m_objects->setProperty("gitEmptyText",
                         tr("No matching objects. Try another hash prefix or "
                            "search all storage."));
  m_objects->setSortingEnabled(true);
  m_objects->sortItems(0, Qt::AscendingOrder);
  m_objects->header()->setToolTip(
      tr("Sort the current page by a column. Hash search spans the entire "
         "selected storage scope."));
  vertical->addWidget(m_objects);

  auto *detail = new QWidget(vertical);
  auto *detailLayout = new QVBoxLayout(detail);
  detailLayout->setContentsMargins(0, 0, 0, 0);
  auto *navigation = new QHBoxLayout;
  m_back = new QPushButton(tr("← Back"), detail);
  m_forward = new QPushButton(tr("Forward →"), detail);
  m_back->setObjectName("objectBackButton");
  m_forward->setObjectName("objectForwardButton");
  m_copy = new QPushButton(tr("Copy Hash"), detail);
  m_copy->setObjectName("objectCopyHashButton");
  m_graph = new QPushButton(tr("Commit DAG"), detail);
  m_graph->setObjectName("objectCommitGraphButton");
  navigation->addWidget(m_back);
  navigation->addWidget(m_forward);
  navigation->addStretch();
  navigation->addWidget(m_copy);
  navigation->addWidget(m_graph);
  detailLayout->addLayout(navigation);
  m_objectInfo = new QLabel(
      tr("Select an object to inspect its contents and connections."), detail);
  m_objectInfo->setObjectName("objectDetailSummary");
  m_objectInfo->setTextFormat(Qt::PlainText);
  m_objectInfo->setWordWrap(true);
  m_objectInfo->setMinimumWidth(0);
  m_objectInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
  detailLayout->addWidget(m_objectInfo);
  m_detailTabs = new QTabWidget(detail);
  m_preview = new QPlainTextEdit(detail);
  m_preview->setObjectName("objectPreview");
  m_preview->setReadOnly(true);
  m_preview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
  m_detailTabs->addTab(m_preview, tr("Content"));
  m_links =
      table("objectRelationsTable",
            {tr("Relationship / path"), tr("Type"), tr("Object hash")}, detail);
  m_detailTabs->addTab(m_links, tr("Relationships"));
  auto *dagPage = new QWidget(detail);
  auto *dagLayout = new QVBoxLayout(dagPage);
  dagLayout->setContentsMargins(0, 0, 0, 0);
  auto *dagTools = new QHBoxLayout;
  auto *dagHint = new QLabel(
      tr("Direct dependencies · double-click to follow · drag to pan"),
      dagPage);
  dagHint->setWordWrap(true);
  dagTools->addWidget(dagHint, 1);
  auto *zoomOut = new QPushButton(QStringLiteral("−"), dagPage);
  auto *fit = new QPushButton(tr("Fit"), dagPage);
  auto *zoomIn = new QPushButton(QStringLiteral("+"), dagPage);
  zoomOut->setToolTip(tr("Zoom out"));
  zoomIn->setToolTip(tr("Zoom in"));
  for (auto *button : {zoomOut, fit, zoomIn}) {
    button->setMaximumWidth(45);
    dagTools->addWidget(button);
  }
  dagLayout->addLayout(dagTools);
  m_scene = new QGraphicsScene(this);
  m_dag = new QGraphicsView(m_scene, dagPage);
  m_dag->setObjectName("objectDagView");
  m_dag->setRenderHint(QPainter::Antialiasing);
  m_dag->setDragMode(QGraphicsView::ScrollHandDrag);
  m_dag->setToolTip(tr("Object links point from an object to its dependencies. "
                       "Double-click a node to explore it; drag to pan."));
  dagLayout->addWidget(m_dag, 1);
  m_detailTabs->addTab(dagPage, tr("Object DAG"));
  connect(zoomOut, &QPushButton::clicked, this, [this]() {
    if (m_dag->transform().m11() > 0.15)
      m_dag->scale(0.8, 0.8);
  });
  connect(zoomIn, &QPushButton::clicked, this, [this]() {
    if (m_dag->transform().m11() < 3)
      m_dag->scale(1.25, 1.25);
  });
  connect(fit, &QPushButton::clicked, this, [this]() {
    m_dag->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
  });
  connect(m_detailTabs, &QTabWidget::currentChanged, this,
          [this, dagPage](int index) {
            if (m_detailTabs->widget(index) == dagPage)
              m_dag->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
          });
  m_verification = new QPlainTextEdit(detail);
  m_verification->setObjectName("objectVerificationOutput");
  m_verification->setReadOnly(true);
  m_verification->setPlainText(
      tr("Select a pack and choose Verify Selected Pack to check its contents "
         "and index checksums."));
  m_detailTabs->addTab(m_verification, tr("Verification"));
  detailLayout->addWidget(m_detailTabs, 1);
  vertical->addWidget(detail);
  vertical->setSizes({260, 350});
  rightLayout->addWidget(vertical, 1);
  auto *paging = new QHBoxLayout;
  m_status = new QLabel(right);
  m_status->setObjectName("objectSearchStatus");
  m_status->setWordWrap(true);
  m_status->setTextFormat(Qt::PlainText);
  m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_previous = new QPushButton(tr("Previous"), right);
  m_next = new QPushButton(tr("Next 250"), right);
  m_previous->setObjectName("objectPreviousPageButton");
  m_next->setObjectName("objectNextPageButton");
  m_previous->setEnabled(false);
  m_next->setEnabled(false);
  paging->addWidget(m_status, 1);
  paging->addWidget(m_previous);
  paging->addWidget(m_next);
  rightLayout->addLayout(paging);
  splitter->addWidget(storage);
  splitter->addWidget(right);
  splitter->setStretchFactor(1, 3);
  splitter->setSizes({350, 850});
  layout->addWidget(splitter, 1);
  auto *close = new QPushButton(tr("Close"), this);
  close->setProperty("gitDismiss", true);
  auto *footer = new QHBoxLayout;
  footer->addWidget(new QLabel(
      tr("Read-only inspection · local objects · no checkout required"), this));
  footer->addStretch();
  footer->addWidget(close);
  layout->addLayout(footer);

  m_debounce = new QTimer(this);
  m_debounce->setSingleShot(true);
  m_debounce->setInterval(250);
  connect(m_search, &QLineEdit::textChanged, this, [this]() {
    cancel(m_searchCancel);
    m_debounce->start();
  });
  connect(m_debounce, &QTimer::timeout, this, [this]() { search(); });
  connect(m_search, &QLineEdit::returnPressed, this, [this]() {
    m_debounce->stop();
    search();
  });
  connect(m_packs, &QTreeWidget::currentItemChanged, this, [this]() {
    const auto *item = m_packs->currentItem();
    m_verify->setEnabled(item &&
                         !item->data(0, Qt::UserRole).toString().isEmpty());
    search();
  });
  connect(m_objects, &QTreeWidget::currentItemChanged, this, [this]() {
    if (const auto *item = m_objects->currentItem())
      inspect(item->data(0, Qt::UserRole).toString());
  });
  connect(m_links, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem *item) {
            inspect(item->data(0, Qt::UserRole).toString());
          });
  connect(all, &QPushButton::clicked, this, [this]() {
    const QSignalBlocker blocker(m_packs);
    if (m_packs->topLevelItemCount() > 0)
      m_packs->setCurrentItem(m_packs->topLevelItem(0));
    m_verify->setEnabled(false);
    search();
  });
  connect(m_previous, &QPushButton::clicked, this, [this]() {
    m_skip = qMax<qint64>(0, m_skip - PageSize);
    search(false);
  });
  connect(m_next, &QPushButton::clicked, this, [this]() {
    m_skip += PageSize;
    search(false);
  });
  connect(m_back, &QPushButton::clicked, this, [this]() { navigate(-1); });
  connect(m_forward, &QPushButton::clicked, this, [this]() { navigate(1); });
  connect(m_copy, &QPushButton::clicked, this,
          [this]() { QApplication::clipboard()->setText(m_details.hash); });
  connect(m_graph, &QPushButton::clicked, this,
          [this]() { emit viewCommitRequested(m_details.hash); });
  connect(m_verify, &QPushButton::clicked, this,
          &GitObjectExplorerDialog::verifySelectedPack);
  connect(refreshButton, &QPushButton::clicked, this,
          &GitObjectExplorerDialog::refresh);
  connect(cancelButton, &QPushButton::clicked, this, [this]() {
    cancelRequests();
    m_status->setText(tr("Cancelled. Refresh or search to continue."));
    m_verification->appendPlainText(tr("Current operation cancelled."));
  });
  connect(close, &QPushButton::clicked, this, &QDialog::accept);
  auto *find = new QShortcut(QKeySequence::Find, this);
  connect(find, &QShortcut::activated, this, [this]() {
    m_search->setFocus();
    m_search->selectAll();
  });
  auto *refreshKey = new QShortcut(QKeySequence::Refresh, this);
  connect(refreshKey, &QShortcut::activated, this,
          &GitObjectExplorerDialog::refresh);
  auto *backKey = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Left), this);
  auto *forwardKey = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Right), this);
  connect(backKey, &QShortcut::activated, this, [this]() { navigate(-1); });
  connect(forwardKey, &QShortcut::activated, this, [this]() { navigate(1); });
  m_packs->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_packs, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            const auto *item = m_packs->itemAt(pos);
            if (!item || item->data(0, Qt::UserRole).toString().isEmpty())
              return;
            const QString path = item->data(0, Qt::UserRole).toString();
            QMenu menu(this);
            menu.addAction(tr("Copy Pack Path"), this, [path]() {
              QApplication::clipboard()->setText(path);
            });
            menu.exec(m_packs->viewport()->mapToGlobal(pos));
          });
  m_objects->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_objects, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            const auto *item = m_objects->itemAt(pos);
            if (!item)
              return;
            const QString hash = item->data(0, Qt::UserRole).toString();
            const QString path = item->data(3, Qt::UserRole).toString();
            QMenu menu(this);
            menu.addAction(tr("Copy Hash"), this, [hash]() {
              QApplication::clipboard()->setText(hash);
            });
            menu.addAction(tr("Copy Storage Path"), this, [path]() {
              QApplication::clipboard()->setText(path);
            });
            menu.addAction(tr("Find All Copies of This Hash"), this,
                           [this, hash]() {
                             const QSignalBlocker blocker(m_packs);
                             m_packs->setCurrentItem(m_packs->topLevelItem(0));
                             m_verify->setEnabled(false);
                             searchHash(hash);
                           });
            menu.exec(m_objects->viewport()->mapToGlobal(pos));
          });
  setKeyboardDefault(nullptr);
  updateNavigation();
  applyTheme(theme);
  refresh();
}

GitObjectExplorerDialog::~GitObjectExplorerDialog() { cancelRequests(); }

void GitObjectExplorerDialog::cancelRequests() {
  m_debounce->stop();
  for (auto *token :
       {&m_scanCancel, &m_searchCancel, &m_inspectCancel, &m_verifyCancel})
    cancel(*token);
}

void GitObjectExplorerDialog::refresh() {
  cancelRequests();
  const QString repository = m_repositoryPath;
  const QString selected =
      m_packs->currentItem()
          ? m_packs->currentItem()->data(0, Qt::UserRole).toString()
          : QString();
  m_status->setText(tr("Reading pack headers and indexes…"));
  m_scanCancel = background(
      this,
      [repository](const GitObjectDatabase::Cancelled &cancelled) {
        return GitObjectDatabase::scan(repository, cancelled);
      },
      [selected](GitObjectExplorerDialog *self, const GitObjectStore &store) {
        self->m_store = store;
        QSignalBlocker blocker(self->m_packs);
        self->m_packs->clear();
        auto *all = new QTreeWidgetItem(self->m_packs,
                                        {tr("All storage"), QString(),
                                         QString(), tr("Local + alternates")});
        QTreeWidgetItem *current = all;
        qint64 count = 0, size = 0;
        for (const auto &pack : store.packs) {
          count += pack.objectCount;
          size += pack.size;
          QStringList flags = pack.flags;
          if (pack.alternate)
            flags << tr("alternate");
          const QFontMetrics packMetrics(self->m_packs->font());
          const QString label =
              packMetrics.elidedText(pack.name, Qt::ElideMiddle, 155);
          auto *row = new QTreeWidgetItem(
              self->m_packs,
              {label, QString::number(pack.objectCount), bytes(pack.size),
               pack.error.isEmpty()
                   ? (flags.isEmpty() ? tr("Indexed") : flags.join(", "))
                   : tr("Needs attention")});
          row->setData(0, Qt::UserRole, pack.path);
          const QString tip =
              tr("%1\nPack v%2 · Index v%3 (%4)\n%5")
                  .arg(pack.path)
                  .arg(pack.packVersion)
                  .arg(pack.indexVersion)
                  .arg(bytes(pack.indexSize),
                       pack.error.isEmpty() ? tr("Use Verify Selected Pack for "
                                                 "a full integrity check.")
                                            : pack.error);
          for (int column = 0; column < 4; ++column)
            row->setToolTip(column, tip);
          if (pack.path == selected)
            current = row;
        }
        self->m_packs->setCurrentItem(current);
        self->m_verify->setEnabled(current != all);
        const QString packCount = store.packs.size() == 1
                                      ? tr("1 pack")
                                      : tr("%1 packs").arg(store.packs.size());
        const QString storeCount =
            store.objectDirectories.size() == 1
                ? tr("1 object store")
                : tr("%1 object stores").arg(store.objectDirectories.size());
        self->m_summary->setText(
            tr("%1 · %2 packed copies · %3 loose (primary) · %4 · %5 · %6")
                .arg(packCount)
                .arg(count)
                .arg(store.looseObjects)
                .arg(bytes(size), store.hashLength == 64 ? "SHA-256" : "SHA-1",
                     storeCount));
        if (store.objectDirectories.isEmpty()) {
          self->m_status->setText(store.errors.join('\n'));
          return;
        }
        self->search();
      });
}

void GitObjectExplorerDialog::searchHash(const QString &hash) {
  m_pendingHash = hash;
  m_search->setText(hash);
  if (!m_store.objectDirectories.isEmpty())
    search();
}

void GitObjectExplorerDialog::search(bool resetPage) {
  m_debounce->stop();
  if (m_store.objectDirectories.isEmpty())
    return;
  cancel(m_searchCancel);
  if (resetPage)
    m_skip = 0;
  const auto store = m_store;
  const QString prefix = m_search->text();
  const QString pack =
      m_packs->currentItem()
          ? m_packs->currentItem()->data(0, Qt::UserRole).toString()
          : QString();
  const qint64 skip = m_skip;
  m_previous->setEnabled(false);
  m_next->setEnabled(false);
  m_status->setText(tr("Searching %1…")
                        .arg(pack.isEmpty() ? tr("all local storage")
                                            : QFileInfo(pack).fileName()));
  m_searchCancel = background(
      this,
      [store, prefix, pack,
       skip](const GitObjectDatabase::Cancelled &cancelled) {
        return GitObjectDatabase::search(store, prefix, pack, skip, PageSize,
                                         cancelled);
      },
      [skip](GitObjectExplorerDialog *self,
             const GitObjectSearchResult &result) {
        QSignalBlocker blocker(self->m_objects);
        self->m_objects->setSortingEnabled(false);
        self->m_objects->clear();
        for (const auto &object : result.objects) {
          const QString storage = object.packPath.isEmpty()
                                      ? tr("Loose")
                                      : QFileInfo(object.packPath).fileName();
          const QFontMetrics metrics(self->m_objects->font());
          const QString storageLabel =
              metrics.elidedText(storage, Qt::ElideMiddle, 170);
          auto *row = new ObjectRow(
              self->m_objects,
              {object.hash.left(12),
               object.type.isEmpty() ? tr("Unavailable") : object.type,
               object.size < 0 ? QStringLiteral("—") : bytes(object.size),
               storageLabel,
               object.packPath.isEmpty() ? QStringLiteral("—")
                                         : QString::number(object.offset),
               object.representation});
          row->setData(0, Qt::UserRole, object.hash);
          row->setData(2, Qt::UserRole, object.size);
          row->setData(3, Qt::UserRole,
                       object.packPath.isEmpty() ? object.loosePath
                                                 : object.packPath);
          row->setData(4, Qt::UserRole, QVariant::fromValue(object.offset));
          row->setToolTip(0, object.hash);
          row->setToolTip(2, tr("%1 bytes").arg(object.size));
          row->setToolTip(3, object.packPath.isEmpty() ? object.loosePath
                                                       : object.packPath);
          row->setToolTip(5, tr("CRC32: %1\nDelta base: %2")
                                 .arg(object.crc, object.deltaBase));
        }
        self->m_objects->setSortingEnabled(true);
        self->m_previous->setEnabled(skip > 0);
        self->m_next->setEnabled(skip + result.objects.size() <
                                 result.totalMatches);
        QString message = result.objects.isEmpty()
                              ? tr("No matching objects.")
                              : tr("%1–%2 of %3 storage locations (duplicate "
                                   "copies are shown separately)")
                                    .arg(skip + 1)
                                    .arg(skip + result.objects.size())
                                    .arg(result.totalMatches);
        QStringList errors = self->m_store.errors + result.errors;
        if (!errors.isEmpty())
          message +=
              tr("\nSome storage could not be read: %1").arg(errors.join("; "));
        self->m_status->setText(message);
        if (!self->m_pendingHash.isEmpty() && !result.objects.isEmpty()) {
          const QString hash = result.objects.first().hash;
          self->m_pendingHash.clear();
          for (int i = 0; i < self->m_objects->topLevelItemCount(); ++i) {
            auto *item = self->m_objects->topLevelItem(i);
            if (item->data(0, Qt::UserRole).toString() == hash) {
              self->m_objects->setCurrentItem(item);
              break;
            }
          }
          self->inspect(hash);
        }
      });
}

void GitObjectExplorerDialog::inspect(const QString &hash, bool remember) {
  cancel(m_inspectCancel);
  m_details = {};
  m_preview->clear();
  m_links->clear();
  m_scene->clear();
  if (remember &&
      (m_historyIndex < 0 || m_history.value(m_historyIndex) != hash)) {
    m_history = m_history.mid(0, m_historyIndex + 1);
    m_history << hash;
    m_historyIndex = m_history.size() - 1;
  }
  m_copy->setEnabled(false);
  m_graph->setEnabled(false);
  m_objectInfo->setText(tr("Loading %1…").arg(hash));
  updateNavigation();
  const QString repository = m_repositoryPath;
  m_inspectCancel = background(
      this,
      [repository, hash](const GitObjectDatabase::Cancelled &cancelled) {
        return GitObjectDatabase::inspect(repository, hash, cancelled);
      },
      [](GitObjectExplorerDialog *self, const GitObjectDetails &details) {
        self->showDetails(details);
      });
}

void GitObjectExplorerDialog::showDetails(const GitObjectDetails &details) {
  m_details = details;
  m_links->clear();
  m_scene->clear();
  m_preview->clear();
  m_copy->setEnabled(!details.hash.isEmpty());
  m_graph->setEnabled(details.error.isEmpty() && details.type == "commit");
  if (!details.error.isEmpty()) {
    m_objectInfo->setText(details.hash + '\n' + details.error);
    return;
  }
  QString summary = tr("%1 · %2 · %3 bytes")
                        .arg(details.hash, details.type)
                        .arg(details.size);
  if (details.truncated)
    summary += tr(" · preview limited to 256 KiB");
  if (details.linksTruncated)
    summary += tr(" · relationships truncated");
  m_objectInfo->setText(summary);
  if (details.binary) {
    QString text =
        tr("Binary object — hexadecimal preview (first 4096 bytes)\n\n");
    const QByteArray data = details.preview.left(4096);
    for (int i = 0; i < data.size(); i += 16) {
      const QByteArray chunk = data.mid(i, 16);
      QString ascii;
      for (uchar c : chunk)
        ascii += c >= 32 && c < 127 ? QChar(c) : QChar('.');
      text += QStringLiteral("%1  %2  %3\n")
                  .arg(i, 8, 16, QLatin1Char('0'))
                  .arg(QString::fromLatin1(chunk.toHex(' ')).leftJustified(47),
                       ascii);
    }
    m_preview->setPlainText(text);
  } else {
    m_preview->setPlainText(QString::fromUtf8(details.preview));
  }
  for (const auto &link : details.links) {
    auto *row =
        new QTreeWidgetItem(m_links, {link.label, link.type, link.hash});
    row->setData(0, Qt::UserRole, link.hash);
    row->setToolTip(0,
                    tr("Double-click or press Enter to inspect this object."));
  }
  drawDag(details);
}

void GitObjectExplorerDialog::drawDag(const GitObjectDetails &details) {
  m_scene->clear();
  const QColor foreground = m_theme.foregroundColor;
  auto addNode = [&](const QString &hash, const QString &label, qreal x,
                     qreal y, bool root) {
    auto *node = new ObjectNode(QRectF(0, 0, 220, 60), [this, hash]() {
      QTimer::singleShot(0, this, [this, hash]() { inspect(hash); });
    });
    node->setPos(x, y);
    node->setBrush(root ? m_theme.accentSoftColor : m_theme.surfaceColor);
    node->setPen(QPen(m_theme.accentColor, root ? 2 : 1));
    node->setToolTip(label + '\n' + hash);
    m_scene->addItem(node);
    QString displayLabel = label;
    displayLabel.replace('\n', QLatin1Char(' '));
    displayLabel.replace('\t', QLatin1Char(' '));
    const QFontMetrics metrics(m_dag->font());
    auto *text = new QGraphicsSimpleTextItem(
        metrics.elidedText(displayLabel, Qt::ElideRight, 195) + '\n' +
            hash.left(16),
        node);
    text->setBrush(foreground);
    text->setPos(12, 10);
    text->setAcceptedMouseButtons(Qt::NoButton);
  };
  const int shown = qMin(24, int(details.links.size()));

  addNode(details.hash, details.type, 360, 0, true);
  QSet<QString> drawn;
  int index = 0;
  for (int i = 0; i < shown; ++i) {
    const auto &link = details.links.at(i);
    if (drawn.contains(link.hash))
      continue;
    drawn.insert(link.hash);
    const qreal x = (index % 4) * 240;
    const qreal y = 130 + (index / 4) * 130;
    const QPointF start(470, 60), end(x + 110, y);
    m_scene->addLine(QLineF(start, end), QPen(m_theme.accentColor, 1));
    const qreal angle = std::atan2(end.y() - start.y(), end.x() - start.x());
    const QPointF a =
        end - QPointF(std::cos(angle - 0.5) * 10, std::sin(angle - 0.5) * 10);
    const QPointF b =
        end - QPointF(std::cos(angle + 0.5) * 10, std::sin(angle + 0.5) * 10);
    m_scene->addPolygon(QPolygonF{end, a, b}, QPen(m_theme.accentColor),
                        QBrush(m_theme.accentColor));
    addNode(link.hash, link.label + " · " + link.type, x, y, false);
    ++index;
  }
  auto *hint = m_scene->addSimpleText(
      details.links.size() > shown
          ? tr("Showing %1 of %2 links. Use Relationships for the full list.")
                .arg(shown)
                .arg(details.links.size())
          : tr("Double-click a node to follow its links. Use Back to return."));
  hint->setBrush(foreground);
  hint->setPos(0, m_scene->itemsBoundingRect().bottom() + 20);
  m_scene->setSceneRect(
      m_scene->itemsBoundingRect().adjusted(-20, -20, 20, 20));
  m_dag->resetTransform();
  m_dag->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

void GitObjectExplorerDialog::navigate(int delta) {
  const int next = m_historyIndex + delta;
  if (next < 0 || next >= m_history.size())
    return;
  m_historyIndex = next;
  inspect(m_history.at(next), false);
}

void GitObjectExplorerDialog::updateNavigation() {
  m_back->setEnabled(m_historyIndex > 0);
  m_forward->setEnabled(m_historyIndex + 1 < m_history.size());
  m_copy->setEnabled(!m_details.hash.isEmpty());
  m_graph->setEnabled(m_details.error.isEmpty() && m_details.type == "commit");
}

void GitObjectExplorerDialog::verifySelectedPack() {
  const auto *item = m_packs->currentItem();
  if (!item)
    return;
  const QString path = item->data(0, Qt::UserRole).toString();
  if (path.isEmpty())
    return;
  cancel(m_verifyCancel);
  const QString index = path.left(path.size() - 5) + ".idx";
  m_verification->setPlainText(tr("Verifying %1…\nThis may take a while for "
                                  "large packs. Cancel stops the check.")
                                   .arg(path));
  m_detailTabs->setCurrentWidget(m_verification);
  const QString repository = m_repositoryPath;
  m_verifyCancel = background(
      this,
      [repository, index](const GitObjectDatabase::Cancelled &cancelled) {
        bool ok = false;
        const QString message =
            GitObjectDatabase::verifyPack(repository, index, &ok, cancelled);
        return qMakePair(ok, message);
      },
      [path](GitObjectExplorerDialog *self,
             const QPair<bool, QString> &result) {
        self->m_verification->setPlainText(
            path + '\n' +
            (result.first ? tr("Verified\n") : tr("Verification failed\n")) +
            result.second);
      });
}

void GitObjectExplorerDialog::applyTheme(const Theme &theme) {
  GitDialog::applyTheme(theme);
  for (auto *tree : {m_packs, m_objects, m_links})
    tree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  m_dag->setBackgroundBrush(theme.backgroundColor);
  if (!m_details.hash.isEmpty() && m_details.error.isEmpty())
    drawDag(m_details);
}
