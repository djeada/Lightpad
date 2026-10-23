#include "gitobjectexplorerdialog.h"

#include "../../theme/colorcontrast.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QThreadPool>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtEndian>
#include <algorithm>
#include <functional>

namespace {
constexpr int PageSize = 250;
constexpr int LooseScopeRole = Qt::UserRole + 1;

QString hexDump(const QByteArray &data, quint64 start = 0) {
  QString text;
  for (int i = 0; i < data.size(); i += 16) {
    const auto chunk = data.mid(i, 16);
    QString ascii;
    for (uchar c : chunk)
      ascii += c >= 32 && c < 127 ? QChar(c) : QChar('.');
    text += QStringLiteral("%1  %2  %3\n")
                .arg(start + i, 10, 16, QLatin1Char('0'))
                .arg(QString::fromLatin1(chunk.toHex(' ')).leftJustified(47),
                     ascii);
  }
  return text;
}

QString decodedText(const GitStoredObject &object, int hashLength) {
  if (object.type == "tree") {
    QString text;
    qsizetype pos = 0;
    while (pos < object.preview.size()) {
      const int space = object.preview.indexOf(' ', pos);
      const int zero = object.preview.indexOf('\0', pos);
      const int hashBytes = hashLength / 2;
      if (space < pos || zero < space ||
          zero + 1 + hashBytes > object.preview.size())
        break;
      const auto mode = object.preview.mid(pos, space - pos);
      const auto name = object.preview.mid(space + 1, zero - space - 1);
      const auto hash = object.preview.mid(zero + 1, hashBytes).toHex();
      const QString type = mode == "40000"    ? "tree"
                           : mode == "160000" ? "commit"
                                              : "blob";
      text += QStringLiteral("%1 %2 %3\t%4\n")
                  .arg(QString::fromLatin1(mode).rightJustified(6, '0'), type,
                       QString::fromLatin1(hash), QString::fromUtf8(name));
      pos = zero + 1 + hashBytes;
    }
    return text;
  }
  if (object.preview.contains('\0'))
    return QObject::tr("Binary content · first 4096 decoded bytes\n\n") +
           hexDump(object.preview.left(4096));
  return QString::fromUtf8(object.preview);
}

class ObjectTable : public QTreeWidget {
public:
  using QTreeWidget::QTreeWidget;

protected:
  void resizeEvent(QResizeEvent *event) override {
    QTreeWidget::resizeEvent(event);
    // Keep the metadata columns at the user's widths and give the pack name
    // the remaining space. All sections still support manual resizing.
    if (objectName() != "objectPackTable" || width() == m_lastWidth)
      return;
    m_lastWidth = width();
    int available = viewport()->width();
    for (int column = 1; column < columnCount(); ++column)
      available -= columnWidth(column);
    setColumnWidth(0, qMax(header()->minimumSectionSize(), available));
  }

private:
  int m_lastWidth = -1;
};

class ObjectDagView : public QGraphicsView {
public:
  using QGraphicsView::QGraphicsView;

  void readableOverview() {
    m_automatic = true;
    updateOverview();
  }

  void zoom(qreal factor) {
    m_automatic = false;
    const qreal current = transform().m11();
    const qreal next = qBound(0.2, current * factor, 3.0);
    scale(next / current, next / current);
  }

  void actualSize() {
    m_automatic = false;
    resetTransform();
  }

  void fitAll() {
    m_automatic = false;
    fitInView(sceneRect(), Qt::KeepAspectRatio);
  }

protected:
  void resizeEvent(QResizeEvent *event) override {
    QGraphicsView::resizeEvent(event);
    if (m_automatic)
      updateOverview();
  }

  void showEvent(QShowEvent *event) override {
    QGraphicsView::showEvent(event);
    if (m_automatic)
      QTimer::singleShot(0, this, [this]() { updateOverview(); });
  }

  void wheelEvent(QWheelEvent *event) override {
    if (!event->modifiers().testFlag(Qt::ShiftModifier)) {
      zoom(event->angleDelta().y() > 0 ? 1.2 : 1 / 1.2);
      event->accept();
      return;
    }
    QGraphicsView::wheelEvent(event);
  }

private:
  void updateOverview() {
    const QRectF bounds = sceneRect();
    if (bounds.isEmpty())
      return;
    const qreal factor =
        qBound(0.85,
               qMin((viewport()->width() - 8) / bounds.width(),
                    (viewport()->height() - 8) / bounds.height()),
               1.0);
    setTransform(QTransform::fromScale(factor, factor));
    centerOn(qMin(bounds.center().x(),
                  bounds.left() + viewport()->width() / (2 * factor)),
             bounds.center().y());
  }

  bool m_automatic = true;
};

QString bytes(qint64 value) {
  if (value < 1024)
    return QStringLiteral("%1 B").arg(value);
  if (value < 1024 * 1024)
    return QStringLiteral("%1 KiB").arg(value / 1024.0, 0, 'f', 1);
  return QStringLiteral("%1 MiB").arg(value / (1024.0 * 1024), 0, 'f', 1);
}

QTreeWidget *table(const QString &name, const QStringList &headers,
                   QWidget *parent) {
  auto *tree = new ObjectTable(parent);
  tree->setObjectName(name);
  tree->setHeaderLabels(headers);
  tree->setRootIsDecorated(false);
  tree->setSelectionMode(QAbstractItemView::SingleSelection);
  tree->setAlternatingRowColors(true);
  tree->setUniformRowHeights(true);
  tree->setTextElideMode(Qt::ElideMiddle);
  tree->setAllColumnsShowFocus(true);
  tree->setProperty("gitFlexibleColumns", true);
  tree->header()->setSectionResizeMode(QHeaderView::Interactive);
  tree->header()->setDefaultSectionSize(100);
  tree->header()->setStretchLastSection(false);
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
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsFocusable);
  }

protected:
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *,
             QWidget *) override {
    QPen outline = pen();
    if (m_hovered || hasFocus())
      outline.setWidthF(outline.widthF() + 1.5);
    painter->setPen(outline);
    painter->setBrush(m_hovered ? QBrush(ColorContrast::mix(
                                      brush().color(), pen().color(), 0.15))
                                : brush());
    painter->drawRoundedRect(rect(), 10, 10);
  }

  void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
    m_press = m_last = event->screenPos();
    m_dragged = false;
    setFocus();
    event->accept();
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
    m_dragged = m_dragged || (event->screenPos() - m_press).manhattanLength() >=
                                 QApplication::startDragDistance();
    if (auto *view =
            qobject_cast<QGraphicsView *>(event->widget()->parentWidget())) {
      const QPoint delta = event->screenPos() - m_last;
      view->horizontalScrollBar()->setValue(
          view->horizontalScrollBar()->value() - delta.x());
      view->verticalScrollBar()->setValue(view->verticalScrollBar()->value() -
                                          delta.y());
    }
    m_last = event->screenPos();
    event->accept();
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
    if (event->button() == Qt::LeftButton && !m_dragged &&
        (event->screenPos() - m_press).manhattanLength() <
            QApplication::startDragDistance())
      m_activate();
    event->accept();
  }

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override {
    event->accept();
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override {
    QMenu menu;
    menu.addAction(QObject::tr("Open object"), [this]() { m_activate(); });
    menu.addAction(QObject::tr("Copy hash"), [this]() {
      QApplication::clipboard()->setText(data(0).toString());
    });
    menu.exec(event->screenPos());
    event->accept();
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
        event->key() == Qt::Key_Space) {
      m_activate();
      event->accept();
    } else
      QGraphicsRectItem::keyPressEvent(event);
  }

  void hoverEnterEvent(QGraphicsSceneHoverEvent *) override {
    m_hovered = true;
    update();
  }
  void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override {
    m_hovered = false;
    update();
  }

private:
  std::function<void()> m_activate;
  QPoint m_press, m_last;
  bool m_hovered = false;
  bool m_dragged = false;
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
  resize(1400, 900);
  setMinimumSize(560, 560);
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
  splitter->setObjectName("objectStorageSplitter");
  splitter->setChildrenCollapsible(false);
  auto *storageSplit = new QSplitter(Qt::Vertical, splitter);
  storageSplit->setObjectName("objectPackTrailSplitter");
  storageSplit->setChildrenCollapsible(false);
  auto *storage = new QWidget(storageSplit);
  auto *storageLayout = new QVBoxLayout(storage);
  storageLayout->setContentsMargins(0, 0, 0, 0);
  storageLayout->addWidget(new QLabel(tr("Repository storage"), storage));
  m_packs = table(
      "objectPackTable",
      {tr("Packfile / loose files"), tr("Objects"), tr("Size"), tr("State")},
      storage);
  m_packs->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_packs->setColumnWidth(0, 135);
  m_packs->setColumnWidth(1, 72);
  m_packs->setColumnWidth(2, 82);
  m_packs->setColumnWidth(3, 84);
  m_packs->setMinimumHeight(90);
  storageLayout->addWidget(m_packs, 1);
  m_verify = new QPushButton(tr("Verify Pack"), storage);
  m_verify->setToolTip(tr("Verify the selected pack and its index"));
  m_verify->setObjectName("objectVerifyButton");
  m_verify->setEnabled(false);
  storageLayout->addWidget(m_verify);
  m_packs->setToolTip(
      tr("Select a pack to browse its index. All storage searches every pack "
         "and loose object, including unreachable objects. Drag the divider "
         "below to resize this table."));
  auto *trailPanel = new QWidget(storageSplit);
  auto *trailLayout = new QVBoxLayout(trailPanel);
  trailLayout->setContentsMargins(0, 0, 0, 0);
  trailLayout->addWidget(new QLabel(tr("Exploration trail"), trailPanel));
  m_trail = table("objectExplorationTrail", {tr("Visited object"), tr("Type")},
                  trailPanel);
  m_trail->setColumnWidth(0, 145);
  m_trail->setColumnWidth(1, 70);
  trailLayout->addWidget(m_trail, 1);
  storageSplit->setSizes({310, 400});
  connect(m_trail, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem *item) {
            inspect(item->data(0, Qt::UserRole).toString());
          });

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
  m_search->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  auto *all = new QPushButton(tr("Search All Storage"), right);
  all->setObjectName("objectSearchAllButton");
  searchRow->addWidget(m_search, 1);
  searchRow->addWidget(all);
  rightLayout->addLayout(searchRow);
  auto *vertical = new QSplitter(Qt::Vertical, right);
  vertical->setObjectName("objectResultsInspectorSplitter");
  m_objects = table("objectResultsTable",
                    {tr("Hash"), tr("Type"), tr("Size"), tr("Storage"),
                     tr("Offset"), tr("Encoding")},
                    vertical);
  m_objects->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_objects->setColumnWidth(0, 145);
  m_objects->header()->setSectionResizeMode(3, QHeaderView::Interactive);
  m_objects->setColumnWidth(3, 190);
  m_objects->setColumnWidth(1, 70);
  m_objects->setColumnWidth(2, 75);
  m_objects->setColumnWidth(4, 85);
  m_objects->setMinimumHeight(120);
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
  m_breadcrumb = new QLabel(detail);
  m_breadcrumb->setObjectName("objectBreadcrumb");
  m_breadcrumb->setTextFormat(Qt::RichText);
  m_breadcrumb->setTextInteractionFlags(Qt::LinksAccessibleByMouse |
                                        Qt::LinksAccessibleByKeyboard);
  m_breadcrumb->setWordWrap(true);
  connect(m_breadcrumb, &QLabel::linkActivated, this,
          [this](const QString &index) {
            navigate(index.toInt() - m_historyIndex);
          });
  detailLayout->addWidget(m_breadcrumb);
  m_detailTabs = new QTabWidget(detail);
  m_detailTabs->setObjectName("objectDetailTabs");
  m_preview = new QPlainTextEdit(detail);
  m_preview->setObjectName("objectPreview");
  m_preview->setReadOnly(true);
  m_preview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
  m_detailTabs->addTab(m_preview, tr("Content"));
  m_links =
      table("objectRelationsTable",
            {tr("Relationship / path"), tr("Type"), tr("Object hash")}, detail);
  m_links->setColumnWidth(0, 220);
  m_links->setColumnWidth(1, 70);
  m_links->setColumnWidth(2, 320);
  m_detailTabs->addTab(m_links, tr("Relationships"));
  auto *dagPage = new QWidget(detail);
  auto *dagLayout = new QVBoxLayout(dagPage);
  dagLayout->setContentsMargins(0, 0, 0, 0);
  auto *dagTools = new QHBoxLayout;
  auto *dagHint = new QLabel(
      tr("Click a card to inspect · scroll to zoom · drag to pan"), dagPage);
  dagHint->setWordWrap(true);
  dagHint->setMinimumWidth(0);
  dagLayout->addWidget(dagHint);
  dagTools->addStretch();
  auto *zoomOut = new QPushButton(QStringLiteral("−"), dagPage);
  auto *fit = new QPushButton(tr("Fit"), dagPage);
  auto *actualSize = new QPushButton(tr("100%"), dagPage);
  auto *zoomIn = new QPushButton(QStringLiteral("+"), dagPage);
  auto *expand = new QPushButton(tr("Expand DAG"), dagPage);
  auto *previewToggle = new QPushButton(tr("Preview"), dagPage);
  previewToggle->setObjectName("objectDagPreviewToggle");
  previewToggle->setCheckable(true);
  previewToggle->setChecked(true);
  expand->setCheckable(true);
  expand->setObjectName("objectDagExpandButton");
  zoomOut->setObjectName("objectDagZoomOutButton");
  zoomIn->setObjectName("objectDagZoomInButton");
  fit->setObjectName("objectDagFitButton");
  actualSize->setObjectName("objectDagActualSizeButton");
  zoomOut->setToolTip(tr("Zoom out"));
  zoomIn->setToolTip(tr("Zoom in"));
  fit->setToolTip(tr("Fit all dependencies in the viewport"));
  actualSize->setToolTip(tr("Restore full-size, readable cards"));
  for (auto *button :
       {zoomOut, fit, actualSize, zoomIn, previewToggle, expand}) {
    button->setMinimumWidth(48);
    dagTools->addWidget(button);
  }
  dagLayout->addLayout(dagTools);
  auto *dagSplit = new QSplitter(Qt::Vertical, dagPage);
  dagSplit->setChildrenCollapsible(false);
  m_scene = new QGraphicsScene(this);
  auto *dagView = new ObjectDagView(m_scene, dagSplit);
  m_dag = dagView;
  m_dag->setObjectName("objectDagView");
  m_dag->setMinimumHeight(180);
  m_dag->setRenderHint(QPainter::Antialiasing);
  m_dag->setRenderHint(QPainter::TextAntialiasing);
  m_dag->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  m_dag->setDragMode(QGraphicsView::ScrollHandDrag);
  m_dag->setToolTip(
      tr("Object links point from an object to its dependencies. "
         "Click a card to inspect it; scroll to zoom; drag to pan."));
  dagSplit->addWidget(m_dag);
  m_dagPreview = new QPlainTextEdit(dagSplit);
  m_dagPreview->setObjectName("objectDagPreview");
  m_dagPreview->setReadOnly(true);
  m_dagPreview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_dagPreview->setPlaceholderText(
      tr("Select a card to read its contents here."));
  m_dagPreview->setMinimumHeight(85);
  dagSplit->addWidget(m_dagPreview);
  dagSplit->setStretchFactor(0, 3);
  dagSplit->setStretchFactor(1, 1);
  dagSplit->setSizes({450, 140});
  dagLayout->addWidget(dagSplit, 1);
  connect(previewToggle, &QPushButton::toggled, m_dagPreview,
          &QWidget::setVisible);
  auto *dagStatus = new QLabel(dagPage);
  dagStatus->setObjectName("objectDagStatus");
  dagStatus->setWordWrap(true);
  dagLayout->addWidget(dagStatus);
  m_detailTabs->addTab(dagPage, tr("Object DAG"));
  connect(zoomOut, &QPushButton::clicked, this,
          [dagView]() { dagView->zoom(0.8); });
  connect(zoomIn, &QPushButton::clicked, this,
          [dagView]() { dagView->zoom(1.25); });
  connect(fit, &QPushButton::clicked, this, [dagView]() { dagView->fitAll(); });
  connect(actualSize, &QPushButton::clicked, this,
          [dagView]() { dagView->actualSize(); });
  connect(expand, &QPushButton::toggled, this, [this, expand](bool expanded) {
    m_objects->setVisible(!expanded);
    m_previous->setVisible(!expanded);
    m_next->setVisible(!expanded);
    m_status->setVisible(!expanded);
    expand->setText(expanded ? tr("Show Results") : tr("Expand DAG"));
  });
  connect(m_detailTabs, &QTabWidget::currentChanged, this,
          [this, dagPage, dagView, expand](int index) {
            if (m_detailTabs->widget(index) == dagPage) {
              expand->setChecked(true);
              dagView->readableOverview();
            } else
              expand->setChecked(false);
          });
  m_verification = new QPlainTextEdit(detail);
  m_verification->setObjectName("objectVerificationOutput");
  m_verification->setReadOnly(true);
  m_verification->setPlainText(
      tr("Select a pack and choose Verify Selected Pack to check its contents "
         "and index checksums."));
  m_detailTabs->addTab(m_verification, tr("Verification"));
  m_diskPage = new QWidget(detail);
  m_diskPage->setObjectName("objectDiskPage");
  auto *diskLayout = new QVBoxLayout(m_diskPage);
  diskLayout->setContentsMargins(0, 0, 0, 0);
  auto *pathRow = new QHBoxLayout;
  m_diskPath = new QLineEdit(m_diskPage);
  m_diskPath->setObjectName("objectDiskPath");
  m_diskPath->setReadOnly(true);
  m_diskPath->setPlaceholderText(tr("Select a pack or loose object file…"));
  m_diskPath->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  auto *copyPath = new QPushButton(tr("Copy Path"), m_diskPage);
  copyPath->setObjectName("objectDiskCopyPath");
  connect(copyPath, &QPushButton::clicked, this,
          [this]() { QApplication::clipboard()->setText(m_diskPath->text()); });
  pathRow->addWidget(m_diskPath, 1);
  pathRow->addWidget(copyPath);
  diskLayout->addLayout(pathRow);
  m_diskSummary =
      new QLabel(tr("Select a stored object to decode its bytes."), m_diskPage);
  m_diskSummary->setObjectName("objectDiskSummary");
  m_diskSummary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  diskLayout->addWidget(m_diskSummary);
  auto *diskTabs = new QTabWidget(m_diskPage);
  diskTabs->setObjectName("objectDiskTabs");
  for (auto **edit : {&m_diskDecoded, &m_diskBytes, &m_diskDetails}) {
    *edit = new QPlainTextEdit(diskTabs);
    (*edit)->setReadOnly(true);
    (*edit)->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    (*edit)->setLineWrapMode(QPlainTextEdit::NoWrap);
  }
  m_diskDecoded->setObjectName("objectDiskDecoded");
  m_diskBytes->setObjectName("objectDiskBytes");
  m_diskDetails->setObjectName("objectDiskDetails");
  diskTabs->addTab(m_diskDecoded, tr("Decoded content"));
  diskTabs->addTab(m_diskBytes, tr("Stored bytes"));
  diskTabs->addTab(m_diskDetails, tr("Entry details"));
  diskLayout->addWidget(diskTabs, 1);
  m_detailTabs->addTab(m_diskPage, tr("On disk"));
  detailLayout->addWidget(m_detailTabs, 1);
  vertical->addWidget(detail);
  vertical->setStretchFactor(0, 1);
  vertical->setStretchFactor(1, 3);
  vertical->setSizes({150, 460});
  rightLayout->addWidget(vertical, 1);
  auto *paging = new QHBoxLayout;
  m_status = new QLabel(right);
  m_status->setObjectName("objectSearchStatus");
  m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
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
  splitter->addWidget(storageSplit);
  splitter->addWidget(right);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 3);
  splitter->setSizes({480, 1120});
  layout->addWidget(splitter, 1);
  auto *close = new QPushButton(tr("Close"), this);
  close->setProperty("gitDismiss", true);
  auto *footer = new QHBoxLayout;
  auto *footerHint = new QLabel(
      tr("Read-only inspection · local objects · no checkout required"), this);
  footerHint->setWordWrap(true);
  footerHint->setMinimumWidth(0);
  footer->addWidget(footerHint, 1);
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
  connect(m_packs, &QTreeWidget::currentItemChanged, this, [this, expand]() {
    const auto *item = m_packs->currentItem();
    m_verify->setEnabled(item &&
                         !item->data(0, Qt::UserRole).toString().isEmpty());
    m_pendingHash.clear();
    {
      const QSignalBlocker blocker(m_search);
      m_search->clear();
    }
    clearInspection();
    expand->setChecked(false);
    showStorageOverview();
    search();
  });
  connect(m_objects, &QTreeWidget::currentItemChanged, this, [this]() {
    if (const auto *item = m_objects->currentItem()) {
      inspect(item->data(0, Qt::UserRole).toString(), true,
              item->data(0, Qt::UserRole + 1).value<GitObjectLocation>());
      m_detailTabs->setCurrentWidget(m_diskPage);
    }
  });
  connect(m_objects, &QTreeWidget::itemClicked, this,
          [this]() { m_detailTabs->setCurrentWidget(m_diskPage); });
  connect(m_links, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem *item) {
            inspect(item->data(0, Qt::UserRole).toString());
          });
  connect(m_links, &QTreeWidget::itemClicked, this,
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
  for (auto *token : {&m_scanCancel, &m_searchCancel, &m_inspectCancel,
                      &m_verifyCancel, &m_diskCancel})
    cancel(*token);
}

void GitObjectExplorerDialog::refresh() {
  cancelRequests();
  clearInspection();
  const QString repository = m_repositoryPath;
  const QString selected =
      m_packs->currentItem()
          ? m_packs->currentItem()->data(0, Qt::UserRole).toString()
          : QString();
  const bool selectedLoose =
      m_packs->currentItem() &&
      m_packs->currentItem()->data(0, LooseScopeRole).toBool();
  m_status->setText(tr("Reading pack headers and indexes…"));
  m_scanCancel = background(
      this,
      [repository](const GitObjectDatabase::Cancelled &cancelled) {
        return GitObjectDatabase::scan(repository, cancelled);
      },
      [selected, selectedLoose](GitObjectExplorerDialog *self,
                                const GitObjectStore &store) {
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
          auto *row = new QTreeWidgetItem(
              self->m_packs,
              {pack.name, QString::number(pack.objectCount), bytes(pack.size),
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
        auto *loose = new QTreeWidgetItem(self->m_packs,
                                          {tr("Loose objects"),
                                           QString::number(store.looseObjects),
                                           QStringLiteral("—"), tr("Files")});
        loose->setData(0, LooseScopeRole, true);
        loose->setToolTip(
            0, tr("Browse individual zlib-compressed object files.\n%1")
                   .arg(store.objectDirectories.join('\n')));
        loose->setToolTip(1, tr("Loose files in the primary object store; "
                                "browsing also includes alternates."));
        if (selectedLoose)
          current = loose;
        self->m_packs->setCurrentItem(current);
        self->m_verify->setEnabled(
            !current->data(0, Qt::UserRole).toString().isEmpty());
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
  if (m_packs->topLevelItemCount() > 0) {
    const QSignalBlocker blocker(m_packs);
    m_packs->setCurrentItem(m_packs->topLevelItem(0));
    m_verify->setEnabled(false);
  }
  m_pendingHash = hash;
  m_search->setText(hash);
  if (!m_store.objectDirectories.isEmpty())
    search();
}

void GitObjectExplorerDialog::focusDag() { m_detailTabs->setCurrentIndex(2); }

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
  const bool looseOnly =
      m_packs->currentItem() &&
      m_packs->currentItem()->data(0, LooseScopeRole).toBool();
  m_previous->setEnabled(false);
  m_next->setEnabled(false);
  m_status->setText(tr("Searching…"));
  m_status->setToolTip(pack.isEmpty() ? tr("Searching all local storage")
                                      : pack);
  m_searchCancel = background(
      this,
      [store, prefix, pack, looseOnly,
       skip](const GitObjectDatabase::Cancelled &cancelled) {
        return GitObjectDatabase::search(store, prefix, pack, skip, PageSize,
                                         cancelled, looseOnly);
      },
      [skip](GitObjectExplorerDialog *self,
             const GitObjectSearchResult &result) {
        QSignalBlocker blocker(self->m_objects);
        self->m_objects->setSortingEnabled(false);
        self->m_objects->clear();
        for (const auto &object : result.objects) {
          const QString storage =
              object.packPath.isEmpty()
                  ? QFileInfo(object.loosePath).dir().dirName() + '/' +
                        QFileInfo(object.loosePath).fileName()
                  : QFileInfo(object.packPath).fileName();
          auto *row = new ObjectRow(
              self->m_objects,
              {object.hash.left(12),
               object.type.isEmpty() ? tr("Unavailable") : object.type,
               object.size < 0 ? QStringLiteral("—") : bytes(object.size),
               storage,
               object.packPath.isEmpty() ? QStringLiteral("—")
                                         : QString::number(object.offset),
               object.representation});
          row->setData(0, Qt::UserRole, object.hash);
          row->setData(0, Qt::UserRole + 1, QVariant::fromValue(object));
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
                              : tr("%1–%2 / %3 locations")
                                    .arg(skip + 1)
                                    .arg(skip + result.objects.size())
                                    .arg(result.totalMatches);
        QStringList errors = self->m_store.errors + result.errors;
        QString tip = tr("Each row is one stored copy of an object. A hash can "
                         "have copies in multiple packs or loose storage.");
        if (!errors.isEmpty()) {
          message += tr(" · %1").arg(errors.join("; "));
          tip +=
              tr("\nSome storage could not be read: %1").arg(errors.join("; "));
        }
        self->m_status->setText(message);
        self->m_status->setToolTip(tip);
        if ((!self->m_pendingHash.isEmpty() || result.totalMatches == 1) &&
            !result.objects.isEmpty()) {
          const QString hash = result.objects.first().hash;
          self->m_pendingHash.clear();
          for (int i = 0; i < self->m_objects->topLevelItemCount(); ++i) {
            auto *item = self->m_objects->topLevelItem(i);
            if (item->data(0, Qt::UserRole).toString() == hash) {
              self->m_objects->setCurrentItem(item);
              break;
            }
          }
          if (self->m_detailTabs->currentWidget() == self->m_verification)
            self->m_detailTabs->setCurrentWidget(self->m_preview);
          self->inspect(hash, true, result.objects.first());
        }
      });
}

void GitObjectExplorerDialog::showStorageOverview() {
  const auto *row = m_packs->currentItem();
  if (!row)
    return;
  const QString path = row->data(0, Qt::UserRole).toString();
  if (row->data(0, LooseScopeRole).toBool()) {
    m_diskPath->setText(m_store.objectDirectories.value(0));
    m_diskPath->setToolTip(m_store.objectDirectories.join('\n'));
    m_diskSummary->setText(
        tr("Loose object files · select a row above to decode a file"));
    m_diskDecoded->setPlainText(
        tr("Each loose object is one zlib-compressed file.\n"
           "Its filename is the object hash, split after two characters:\n\n"
           "  objects/ab/cdef…\n\n"
           "Inflating the file produces:\n"
           "  <type> <size>\\0<object contents>\n\n"
           "Select an object above to read its actual file and decoded "
           "contents."));
    m_diskDetails->setPlainText(tr("Object directories:\n%1")
                                    .arg(m_store.objectDirectories.join('\n')));
    m_diskBytes->setPlainText(
        tr("Select a loose object to view its compressed bytes."));
  } else if (!path.isEmpty()) {
    const auto pack = std::find_if(
        m_store.packs.cbegin(), m_store.packs.cend(),
        [&](const GitPackInfo &info) { return info.path == path; });
    if (pack == m_store.packs.cend())
      return;
    QFile file(path);
    const bool opened = file.open(QIODevice::ReadOnly);
    const QByteArray header = opened ? file.read(12) : QByteArray();
    m_diskPath->setText(path);
    m_diskPath->setToolTip(path);
    m_diskSummary->setText(tr("Packfile · %1 objects · select an index row "
                              "above to decode its entry")
                               .arg(pack->objectCount));
    m_diskDecoded->setPlainText(
        opened && header.size() == 12
            ? tr("PACKFILE HEADER · bytes 0–11\n\n"
                 "Signature:       %1\n"
                 "Format version:  %2\n"
                 "Object entries:  %3\n"
                 "File size:       %4 bytes\n\n"
                 "The index above maps object hashes to byte offsets in this "
                 "file.\n"
                 "Select an entry to read its header, decompress its data "
                 "and,\n"
                 "if needed, apply its delta to reconstruct the original "
                 "object.")
                  .arg(QString::fromLatin1(header.left(4)))
                  .arg(qFromBigEndian<quint32>(header.constData() + 4))
                  .arg(qFromBigEndian<quint32>(header.constData() + 8))
                  .arg(file.size())
            : tr("Could not read the pack header: %1").arg(file.errorString()));
    if (opened) {
      file.seek(0);
      m_diskBytes->setPlainText(
          tr("First 4096 bytes · offsets are hexadecimal\n\n") +
          hexDump(file.read(4096)));
    }
    m_diskDetails->setPlainText(tr("Pack: %1\nIndex: %2\nIndex version: %3\n"
                                   "Index size: %4 bytes\nFlags: %5\n%6")
                                    .arg(pack->path, pack->indexPath)
                                    .arg(pack->indexVersion)
                                    .arg(pack->indexSize)
                                    .arg(pack->flags.join(", "), pack->error));
  } else {
    return;
  }
  m_detailTabs->setCurrentWidget(m_diskPage);
}

void GitObjectExplorerDialog::loadStorage(const GitObjectLocation &location) {
  const QString hash = m_details.hash;
  if (hash.isEmpty())
    return;
  const QString key = hash + ':' + location.packPath + ':' +
                      location.loosePath + ':' +
                      QString::number(location.offset);
  if (key == m_loadedDiskKey ||
      (location.hash.isEmpty() && m_loadedDiskKey.startsWith(hash + ':')))
    return;
  cancel(m_diskCancel);
  m_loadedDiskKey = key;
  m_diskPath->setText(location.packPath.isEmpty() ? location.loosePath
                                                  : location.packPath);
  m_diskSummary->setText(tr("Reading and decoding the selected stored copy…"));
  m_diskDecoded->setPlainText(tr("Decoding…"));
  m_diskBytes->clear();
  m_diskDetails->clear();
  const auto store = m_store;
  m_diskCancel = background(
      this,
      [store, location, hash](const GitObjectDatabase::Cancelled &cancelled) {
        auto target = location;
        if (target.hash.isEmpty()) {
          const auto found =
              GitObjectDatabase::search(store, hash, {}, 0, 1, cancelled);
          if (found.objects.isEmpty()) {
            GitStoredObject missing;
            missing.error =
                tr("No stored copy found. Refresh repository storage.");
            return missing;
          }
          target = found.objects.first();
        }
        return GitObjectDatabase::readStoredObject(store, target, cancelled);
      },
      [](GitObjectExplorerDialog *self, const GitStoredObject &object) {
        self->showStorage(object);
      });
}

void GitObjectExplorerDialog::showStorage(const GitStoredObject &object) {
  const bool packed = !object.location.packPath.isEmpty();
  const QString path =
      packed ? object.location.packPath : object.location.loosePath;
  m_diskPath->setText(path);
  m_diskPath->setToolTip(path);
  QStringList details{
      tr("Object: %1").arg(object.location.hash),
      tr("File: %1").arg(path),
      tr("Storage: %1").arg(packed ? tr("pack entry") : tr("loose file")),
      tr("Encoding: %1").arg(object.location.representation),
      tr("File offset: %1 (0x%2)")
          .arg(object.location.offset)
          .arg(object.location.offset, 0, 16),
      tr("Zlib stream offset: %1").arg(object.dataOffset),
      tr("Stored entry size: %1 bytes").arg(object.storedSize),
      tr("Decoded object: %1 · %2 bytes").arg(object.type).arg(object.size),
      tr("Header bytes: %1")
          .arg(QString::fromLatin1(object.header.toHex(' ')))};
  if (!packed)
    details << tr("Inflated header: %1")
                   .arg(QString::fromLatin1(object.header)
                            .replace(QChar('\0'), "\\0"));
  if (!object.location.crc.isEmpty())
    details << tr("CRC32 recorded by index: %1").arg(object.location.crc);
  if (!object.baseHash.isEmpty()) {
    details << tr("Delta base: %1").arg(object.baseHash)
            << tr("Base location: %1").arg(object.location.deltaBase)
            << tr("The selected entry's delta was inflated directly from this "
                  "file.\n"
                  "Git resolves the base by hash; these instructions "
                  "reconstruct the object:\n%1")
                   .arg(object.deltaOperations.join('\n'));
  }
  if (!object.error.isEmpty()) {
    m_diskSummary->setText(tr("Could not decode this stored copy"));
    m_diskDecoded->setPlainText(object.error);
    details << tr("Error: %1").arg(object.error);
  } else {
    QString summary =
        packed ? tr("Pack entry · offset %1 · %2 stored → %3 decoded")
                     .arg(object.location.offset)
                     .arg(bytes(object.storedSize), bytes(object.size))
               : tr("Loose file · %1 stored → %2 decoded")
                     .arg(bytes(object.storedSize), bytes(object.size));
    summary += object.hashVerified ? tr(" · object hash matches")
                                   : tr(" · partial decode");
    if (object.truncated)
      summary += tr(" · preview limited to 256 KiB");
    m_diskSummary->setText(summary);
    m_diskSummary->setToolTip(summary);
    QString text = decodedText(object, m_store.hashLength);
    if (object.truncated)
      text += tr("\n\n[Preview limited to 256 KiB]");
    m_diskDecoded->setPlainText(text);
    details << (object.hashVerified ? tr("Verification: decoded content hashes "
                                         "to the selected object ID.")
                                    : tr("Verification: incomplete; the decode "
                                         "limit was reached."));
  }
  m_diskDetails->setPlainText(details.join('\n'));
  m_diskBytes->setPlainText(
      tr("Stored bytes from this selected entry · file offsets are "
         "hexadecimal\n"
         "Showing %1 of %2 stored bytes\n\n")
          .arg(object.storedPreview.size())
          .arg(object.storedSize) +
      hexDump(object.storedPreview, object.location.offset));
}

void GitObjectExplorerDialog::clearInspection() {
  cancel(m_inspectCancel);
  cancel(m_diskCancel);
  m_loadedDiskKey.clear();
  m_diskPath->clear();
  m_diskDecoded->clear();
  m_diskBytes->clear();
  m_diskDetails->clear();
  m_diskSummary->setText(tr("Select a stored object to decode its bytes."));
  m_details = {};
  m_preview->clear();
  m_dagPreview->clear();
  m_links->clear();
  m_scene->clear();
  m_scene->setSceneRect(QRectF());
  m_objectInfo->setText(tr("Select an object in this storage to inspect it."));
  findChild<QLabel *>("objectDagStatus")
      ->setText(tr("Select an object to explore its dependencies."));
  updateNavigation();
  m_breadcrumb->setText(tr("No object selected"));
}

void GitObjectExplorerDialog::inspect(const QString &hash, bool remember,
                                      const GitObjectLocation &location) {
  if (hash == m_details.hash && m_details.error.isEmpty()) {
    loadStorage(location);
    return;
  }
  clearInspection();
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
      [location](GitObjectExplorerDialog *self,
                 const GitObjectDetails &details) {
        self->showDetails(details);
        self->loadStorage(location);
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
  m_dagPreview->setPlainText(m_preview->toPlainText());
  if (details.type == "blob" && m_detailTabs->currentWidget() == m_links)
    m_detailTabs->setCurrentWidget(m_preview);
  QTreeWidgetItem *visited = nullptr;
  for (int i = 0; i < m_trail->topLevelItemCount(); ++i)
    if (m_trail->topLevelItem(i)->data(0, Qt::UserRole).toString() ==
        details.hash)
      visited = m_trail->topLevelItem(i);
  if (!visited) {
    visited =
        new QTreeWidgetItem(m_trail, {details.hash.left(12), details.type});
    visited->setData(0, Qt::UserRole, details.hash);
    visited->setToolTip(0, details.hash + '\n' +
                               tr("Click to return to this object"));
  }
  m_trail->setCurrentItem(visited);
  for (const auto &link : details.links) {
    auto *row =
        new QTreeWidgetItem(m_links, {link.label, link.type, link.hash});
    row->setData(0, Qt::UserRole, link.hash);
    row->setToolTip(2, link.hash);
    row->setToolTip(0, tr("Click or press Enter to inspect this object."));
  }
  drawDag(details);
}

void GitObjectExplorerDialog::drawDag(const GitObjectDetails &details) {
  m_scene->clear();
  constexpr qreal CardWidth = 260;
  constexpr qreal CardHeight = 94;
  constexpr qreal RowHeight = 110;
  constexpr qreal DependencyX = 360;
  const int shown = qMin(24, int(details.links.size()));
  const qreal rootY = qMax(0, shown - 1) * RowHeight / 2;

  auto colorFor = [this](const QString &type) {
    const QColor color = type == "tree"   ? QColor("#38bdf8")
                         : type == "blob" ? QColor("#c4b5fd")
                         : type == "tag"  ? QColor("#fbbf24")
                                          : m_theme.accentColor;
    return ColorContrast::ensureOnAll(color, {m_theme.surfaceColor},
                                      ColorContrast::GlyphRatio);
  };
  auto addNode = [&](const QString &hash, const QString &type,
                     const QString &label, qreal x, qreal y, bool root) {
    auto *node =
        new ObjectNode(QRectF(0, 0, CardWidth, CardHeight), [this, hash]() {
          QTimer::singleShot(0, this, [this, hash]() { inspect(hash); });
        });
    node->setData(0, hash);
    node->setData(1, label);
    node->setPos(x, y);
    const QColor background =
        root ? m_theme.accentSoftColor : m_theme.surfaceColor;
    node->setBrush(background);
    node->setPen(QPen(colorFor(type), root ? 2.5 : 1.5));
    node->setToolTip(label + '\n' + type + '\n' + hash);
    node->setZValue(1);
    m_scene->addItem(node);

    auto text = [&](const QString &value, const QFont &font,
                    const QColor &color, qreal top) {
      auto *item = new QGraphicsSimpleTextItem(value, node);
      item->setFont(font);
      item->setBrush(ColorContrast::ensureOnAll(color, {background}));
      item->setPos(16, top);
      item->setAcceptedMouseButtons(Qt::NoButton);
    };
    QFont heading = m_dag->font();
    heading.setBold(true);
    text(root ? tr("INSPECTED · %1").arg(type.toUpper()) : type.toUpper(),
         heading, colorFor(type), 12);
    QString displayLabel = label;
    displayLabel.replace('\n', QLatin1Char(' '));
    displayLabel.replace('\t', QLatin1Char(' '));
    text(QFontMetrics(m_dag->font())
             .elidedText(displayLabel, Qt::ElideMiddle, CardWidth - 32),
         m_dag->font(), m_theme.foregroundColor, 35);
    const QFont hashFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    text(hash.left(22), hashFont, m_theme.foregroundColor, 60);
    text(root ? tr("Selected") : tr("Open →"), m_dag->font(), colorFor(type),
         78);
  };

  addNode(details.hash, details.type, tr("Selected object"), 0, rootY, true);
  int parent = 0;
  for (int i = 0; i < shown; ++i) {
    const auto &link = details.links.at(i);
    const qreal y = i * RowHeight;
    const QPointF start(CardWidth, rootY + CardHeight / 2);
    const QPointF end(DependencyX, y + CardHeight / 2);
    QPainterPath path(start);
    path.cubicTo(start + QPointF(45, 0), end - QPointF(45, 0), end);
    const QColor color = colorFor(link.type);
    m_scene->addPath(path, QPen(color, 2));
    m_scene->addPolygon(
        QPolygonF{end, end + QPointF(-11, -5), end + QPointF(-11, 5)},
        QPen(color), QBrush(color));
    const QString label = link.label == "parent" ? tr("Parent %1").arg(++parent)
                          : link.label == "tree" && details.type == "commit"
                              ? tr("Root tree")
                              : link.label;
    addNode(link.hash, link.type, label, DependencyX, y, false);
  }
  auto *status = findChild<QLabel *>("objectDagStatus");
  status->setText(details.links.isEmpty()
                      ? tr("No outgoing dependencies. Blobs are the leaves of "
                           "the object graph.")
                  : details.links.size() > shown || details.linksTruncated
                      ? tr("Showing %1 dependencies. More entries are "
                           "available in Relationships.")
                            .arg(shown)
                      : tr("%n direct dependencies · arrows point "
                           "to referenced objects · full hashes in tooltips",
                           nullptr, shown));
  m_scene->setSceneRect(
      m_scene->itemsBoundingRect().adjusted(-28, -28, 28, 28));
  static_cast<ObjectDagView *>(m_dag)->readableOverview();
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
  QStringList crumbs;
  const int first = qMax(0, m_historyIndex - 5);
  for (int i = first; i <= m_historyIndex; ++i) {
    const QString label = m_history.at(i).left(8);
    crumbs << (i == m_historyIndex
                   ? QStringLiteral("<b>%1</b>").arg(label)
                   : QStringLiteral("<a href=\"%1\">%2</a>").arg(i).arg(label));
  }
  m_breadcrumb->setText(crumbs.join(QStringLiteral(" &nbsp;›&nbsp; ")));
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
        for (int i = 0; i < self->m_packs->topLevelItemCount(); ++i) {
          auto *row = self->m_packs->topLevelItem(i);
          if (row->data(0, Qt::UserRole).toString() == path)
            row->setText(3, result.first ? tr("Verified")
                                         : tr("Verification failed"));
        }
        self->m_verification->setPlainText(
            path + '\n' +
            (result.first ? tr("Verified\n") : tr("Verification failed\n")) +
            result.second);
      });
}

void GitObjectExplorerDialog::applyTheme(const Theme &theme) {
  GitDialog::applyTheme(theme);
  for (auto *tree : {m_packs, m_objects, m_links, m_trail})
    tree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  m_dag->setBackgroundBrush(theme.backgroundColor);
  if (!m_details.hash.isEmpty() && m_details.error.isEmpty())
    drawDag(m_details);
}
