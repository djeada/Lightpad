#include "lightpadtreeview.h"
#include "../core/lightpadpage.h"
#include "../test_templates/testfileclassifier.h"
#include "../theme/colorcontrast.h"
#include "../theme/themedefinition.h"
#include "../ui/mainwindow.h"
#include "../ui/uistylehelper.h"
#include "filedirtreecontroller.h"
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QUrl>
#include <functional>

namespace {
QStringList localDropPaths(const QMimeData *mime) {
  QStringList paths;
  for (const QUrl &url : mime->urls()) {
    if (!url.isLocalFile() || url.toLocalFile().isEmpty()) {
      return {};
    }
    paths.append(url.toLocalFile());
  }
  return FileDirTreeModel::topLevelPaths(paths);
}

Qt::DropAction dropAction(const QDropEvent *event) {
  bool copyRequested = event->modifiers().testFlag(Qt::ControlModifier);
#ifdef Q_OS_MAC
  copyRequested = event->modifiers().testFlag(Qt::AltModifier);
#endif
  const bool internal = qobject_cast<LightpadTreeView *>(event->source());
  Qt::DropAction action = copyRequested ? Qt::CopyAction
                          : event->modifiers().testFlag(Qt::ShiftModifier)
                              ? Qt::MoveAction
                          : internal ? Qt::MoveAction
                                     : Qt::CopyAction;
  if (!event->possibleActions().testFlag(action)) {
    action = event->possibleActions().testFlag(Qt::CopyAction)
                 ? Qt::CopyAction
                 : Qt::IgnoreAction;
  }
  return action;
}

class ExplorerTreeDelegate : public QStyledItemDelegate {
public:
  explicit ExplorerTreeDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void setTheme(const ThemeDefinition &theme) {
    m_textColor = theme.colors.textPrimary;
    m_hoverBackground = ColorContrast::flatten(theme.colors.treeHoverBg,
                                               theme.colors.surfaceBase);
    m_selectedBackground = ColorContrast::flatten(theme.colors.treeSelectedBg,
                                                  theme.colors.surfaceBase);
    m_selectedBorder = theme.colors.accentPrimary;
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!painter) {
      return;
    }

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const bool isSelected = opt.state.testFlag(QStyle::State_Selected);
    const bool isHovered = opt.state.testFlag(QStyle::State_MouseOver);
    const QRect rowRect = opt.rect.adjusted(4, 1, -4, -1);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (isSelected || isHovered) {
      const QColor bg = isSelected ? m_selectedBackground : m_hoverBackground;
      const QColor border = isSelected ? m_selectedBorder : bg.lighter(112);
      painter->setPen(border);
      painter->setBrush(bg);
      painter->drawRoundedRect(rowRect, 7, 7);
      if (isSelected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_selectedBorder);
        painter->drawRoundedRect(QRect(rowRect.left() + 2, rowRect.top() + 4, 3,
                                       rowRect.height() - 8),
                                 1, 1);
      }
    }

    const QString badge = index.data(GitStatusBadgeRole).toString();
    const QColor badgeColor =
        index.data(GitStatusBadgeColorRole).value<QColor>();
    const int badgeSpace = (!badge.isEmpty() && badgeColor.isValid()) ? 18 : 0;

    opt.rect = rowRect.adjusted(6, 0, -4 - badgeSpace, 0);
    opt.showDecorationSelected = false;
    opt.state &= ~QStyle::State_HasFocus;
    if (index.model() && index.model()->hasChildren(index)) {
      opt.font.setWeight(QFont::DemiBold);
    }
    const QColor rowBackground =
        isSelected ? m_selectedBackground
                   : (isHovered ? m_hoverBackground : QColor());
    const QVariant itemForeground = index.data(Qt::ForegroundRole);
    QColor textColor = itemForeground.canConvert<QColor>()
                           ? itemForeground.value<QColor>()
                           : m_textColor;
    if (!textColor.isValid())
      textColor = m_textColor;
    if (rowBackground.isValid())
      textColor = ColorContrast::ensure(textColor, rowBackground,
                                        ColorContrast::SecondaryTextRatio);
    opt.palette.setColor(QPalette::Highlight, Qt::transparent);
    opt.palette.setColor(QPalette::HighlightedText, textColor);
    opt.palette.setColor(QPalette::Text, textColor);

    QStyledItemDelegate::paint(painter, opt, index);

    if (!badge.isEmpty() && badgeColor.isValid()) {
      QFont badgeFont = opt.font;
      badgeFont.setPixelSize(10);
      badgeFont.setWeight(QFont::Bold);
      painter->setFont(badgeFont);
      painter->setPen(rowBackground.isValid()
                          ? ColorContrast::ensure(badgeColor, rowBackground,
                                                  ColorContrast::GlyphRatio)
                          : badgeColor);
      QRect badgeRect(rowRect.right() - 18, rowRect.top(), 16,
                      rowRect.height());
      painter->drawText(badgeRect, Qt::AlignCenter, badge);
    }

    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), 24));
    return size + QSize(0, 2);
  }

private:
  QColor m_textColor = QApplication::palette().color(QPalette::Text);
  QColor m_hoverBackground =
      QApplication::palette().color(QPalette::AlternateBase);
  QColor m_selectedBackground =
      QApplication::palette().color(QPalette::Highlight);
  QColor m_selectedBorder = QApplication::palette().color(QPalette::Link);
};
} // namespace

LightpadTreeView::LightpadTreeView(LightpadPage *parent)
    : QTreeView(parent), parentPage(parent),
      fileModel(new FileDirTreeModel(this)),
      fileController(new FileDirTreeController(fileModel, this)) {

  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  setDefaultDropAction(Qt::MoveAction);
  setAutoExpandDelay(650);
  setAutoScrollMargin(32);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setAnimated(true);
  setMouseTracking(true);
  setFrameShape(QFrame::NoFrame);
  setIndentation(14);
  setUniformRowHeights(true);
  setIconSize(QSize(18, 18));
  setSelectionBehavior(QAbstractItemView::SelectRows);

  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setAllColumnsShowFocus(false);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  setItemDelegate(new ExplorerTreeDelegate(this));

  setupShortcuts();
  fileController->setRemovalGuard([this](const QStringList &paths) {
    return !parentPage || !parentPage->getMainWindow() ||
           parentPage->getMainWindow()->preparePathsForRemoval(paths);
  });

  connect(fileController, &FileDirTreeController::actionCompleted, parentPage,
          &LightpadPage::updateModel);
  connect(fileController, &FileDirTreeController::fileRemoved, parentPage,
          &LightpadPage::closeTabPage);
  connect(fileModel, &FileDirTreeModel::pathMoved, this,
          [this](const QString &oldPath, const QString &newPath) {
            if (parentPage && parentPage->getMainWindow()) {
              parentPage->getMainWindow()->retargetOpenPaths(oldPath, newPath);
            }
          });
  connect(fileController, &FileDirTreeController::pathCreated, this,
          [this](const QString &path, bool isDirectory) {
            if (!parentPage) {
              return;
            }
            parentPage->revealPath(path);

            if (!isDirectory && parentPage->getMainWindow()) {
              parentPage->getMainWindow()->openFileAndAddToNewTab(path);
            }
          });
  connect(fileController, &FileDirTreeController::statusMessage, this,
          [this](const QString &message) {
            if (parentPage && parentPage->getMainWindow()) {
              parentPage->getMainWindow()->statusBar()->showMessage(message,
                                                                    5000);
            }
          });
}

LightpadTreeView::~LightpadTreeView() {}

void LightpadTreeView::applyTheme(const ThemeDefinition &theme) {
  setStyleSheet(UIStyleHelper::treeViewStyle(theme));
  if (auto *delegate = dynamic_cast<ExplorerTreeDelegate *>(itemDelegate())) {
    delegate->setTheme(theme);
  }
  viewport()->update();
}

void LightpadTreeView::setupShortcuts() {
  struct ShortcutSpec {
    QKeySequence sequence;
    std::function<void()> handler;
  };

  const QList<ShortcutSpec> specs = {
      {QKeySequence(Qt::Key_F2), [this]() { renameSelection(); }},
      {QKeySequence::Delete, [this]() { deleteSelection(DeleteMode::Trash); }},
      {QKeySequence(Qt::SHIFT | Qt::Key_Delete),
       [this]() { deleteSelection(DeleteMode::Permanent); }},
      {QKeySequence::Copy, [this]() { copySelection(); }},
      {QKeySequence::Cut, [this]() { cutSelection(); }},
      {QKeySequence::Paste, [this]() { pasteIntoTarget(); }},
      {QKeySequence(Qt::CTRL | Qt::Key_D), [this]() { duplicateSelection(); }},
      {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N),
       [this]() { promptNewFolder(); }},
      {QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N),
       [this]() { promptNewFile(); }},
  };

  for (const ShortcutSpec &spec : specs) {
    auto *action = new QAction(this);
    action->setShortcut(spec.sequence);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(action, &QAction::triggered, this, spec.handler);
    addAction(action);
  }
}

QStringList LightpadTreeView::selectedPaths() const {
  if (!parentPage) {
    return {};
  }

  QStringList paths;
  const QModelIndexList indexes =
      selectionModel() ? selectionModel()->selectedRows(0) : QModelIndexList();
  for (const QModelIndex &index : indexes) {
    const QString path = parentPage->getFilePath(index);
    if (!path.isEmpty() && !paths.contains(path)) {
      paths << path;
    }
  }
  return FileDirTreeModel::topLevelPaths(paths);
}

QString LightpadTreeView::rootPath() const {
  if (!parentPage) {
    return QString();
  }

  const QString projectRoot = parentPage->getProjectRootPath();
  if (!projectRoot.isEmpty()) {
    return projectRoot;
  }

  const QModelIndex root = rootIndex();
  return root.isValid() ? parentPage->getFilePath(root) : QString();
}

QString LightpadTreeView::targetDirectory() const {
  const QStringList selection = selectedPaths();
  if (!selection.isEmpty()) {
    const QFileInfo info(selection.first());
    if (info.isDir()) {
      return info.absoluteFilePath();
    }
    if (info.exists()) {
      return info.absolutePath();
    }
  }
  return rootPath();
}

void LightpadTreeView::promptNewFile() {
  const QString directory = targetDirectory();
  if (!directory.isEmpty()) {
    fileController->handleNewFile(directory);
  }
}

void LightpadTreeView::promptNewFolder() {
  const QString directory = targetDirectory();
  if (!directory.isEmpty()) {
    fileController->handleNewDirectory(directory);
  }
}

void LightpadTreeView::copySelection() {
  const QStringList selection = selectedPaths();
  if (!selection.isEmpty()) {
    fileController->handleCopy(selection);
  }
}

void LightpadTreeView::cutSelection() {
  const QStringList selection = selectedPaths();
  if (!selection.isEmpty()) {
    fileController->handleCut(selection);
  }
}

void LightpadTreeView::pasteIntoTarget() {
  const QString directory = targetDirectory();
  if (!directory.isEmpty()) {
    fileController->handlePaste(directory);
  }
}

void LightpadTreeView::renameSelection() {
  const QStringList selection = selectedPaths();
  if (selection.size() == 1) {
    fileController->handleRename(selection.first());
  }
}

void LightpadTreeView::duplicateSelection() {
  fileController->handleDuplicate(selectedPaths());
}

void LightpadTreeView::deleteSelection(DeleteMode mode) {
  const QStringList selection = selectedPaths();
  if (!selection.isEmpty()) {
    fileController->handleRemove(selection, mode);
  }
}

void LightpadTreeView::keyPressEvent(QKeyEvent *event) {
  if (!event) {
    return;
  }

  if ((event->key() == Qt::Key_Enter) || (event->key() == Qt::Key_Return)) {
    QModelIndex idx = currentIndex();
    if (idx.isValid() && parentPage) {
      parentPage->activateTreeIndex(idx);
      event->accept();
      return;
    }
  }

  QTreeView::keyPressEvent(event);
}

void LightpadTreeView::contextMenuEvent(QContextMenuEvent *event) {
  if (!event) {
    return;
  }
  showContextMenu(event->reason() == QContextMenuEvent::Keyboard
                      ? visualRect(currentIndex()).center()
                      : event->pos());
  event->accept();
}

void LightpadTreeView::showPathContextMenu(const QString &path,
                                           const QPoint &globalPos) {
  QMenu menu(this);
  if (parentPage && parentPage->getMainWindow()) {
    menu.setStyleSheet(UIStyleHelper::contextMenuStyle(
        parentPage->getMainWindow()->getTheme()));
  }
  buildEntryMenu(menu, path, {path});
  menu.exec(globalPos);
}

void LightpadTreeView::showContextMenu(const QPoint &pos) {
  if (!parentPage) {
    return;
  }

  QMenu menu;
  if (parentPage->getMainWindow()) {
    menu.setStyleSheet(UIStyleHelper::contextMenuStyle(
        parentPage->getMainWindow()->getTheme()));
  }

  const QModelIndex idx = indexAt(pos);
  if (!idx.isValid()) {

    if (QItemSelectionModel *selection = selectionModel()) {
      selection->clearSelection();
    }
    buildRootMenu(menu);
  } else {
    QStringList selection = selectedPaths();
    const QString clickedPath = parentPage->getFilePath(idx);
    if (!selection.contains(clickedPath)) {
      setCurrentIndex(idx);
      selection = QStringList{clickedPath};
    }
    buildEntryMenu(menu, clickedPath, selection);
  }

  if (!menu.isEmpty()) {
    menu.exec(viewport()->mapToGlobal(pos));
  }
}

void LightpadTreeView::buildRootMenu(QMenu &menu) {
  const QString root = rootPath();
  if (root.isEmpty()) {
    return;
  }

  const QString rootName = QFileInfo(root).fileName();
  QAction *header = menu.addAction(
      tr("%1 (project root)").arg(rootName.isEmpty() ? root : rootName));
  header->setEnabled(false);
  menu.addSeparator();

  connect(menu.addAction(tr("New File…")), &QAction::triggered, this,
          [this, root]() { fileController->handleNewFile(root); });
  connect(menu.addAction(tr("New Folder…")), &QAction::triggered, this,
          [this, root]() { fileController->handleNewDirectory(root); });

  QAction *pasteAction = menu.addAction(tr("Paste"));
  pasteAction->setEnabled(fileModel->canPaste());
  connect(pasteAction, &QAction::triggered, this,
          [this, root]() { fileController->handlePaste(root); });

  menu.addSeparator();
  connect(menu.addAction(tr("Find in Folder…")), &QAction::triggered, this,
          [this, root]() { emit findInFolderRequested(root); });
  connect(menu.addAction(tr("Reveal in File Manager")), &QAction::triggered,
          this, [this, root]() { emit revealInFileManagerRequested(root); });
  connect(menu.addAction(tr("Open in Terminal")), &QAction::triggered, this,
          [this, root]() { emit openInTerminalRequested(root); });
  connect(menu.addAction(tr("Copy Path")), &QAction::triggered, this,
          [this, root]() {
            fileController->handleCopyAbsolutePath(QStringList{root});
          });

  menu.addSeparator();
  connect(menu.addAction(tr("Refresh")), &QAction::triggered, parentPage,
          &LightpadPage::updateModel);
  connect(menu.addAction(tr("Collapse All")), &QAction::triggered, this,
          &QTreeView::collapseAll);
}

void LightpadTreeView::buildEntryMenu(QMenu &menu, const QString &filePath,
                                      const QStringList &selection) {
  const QFileInfo fileInfo(filePath);
  const bool isDirectory = fileInfo.isDir();
  const bool multiple = selection.size() > 1;
  const QString name = fileInfo.fileName();
  const QString subject =
      multiple ? tr("%n items", nullptr, selection.size()) : name;
  const QString containingDir =
      isDirectory ? fileInfo.absoluteFilePath() : fileInfo.absolutePath();

  if (!multiple && !isDirectory) {
    connect(menu.addAction(tr("Open")), &QAction::triggered, this,
            [this, filePath]() {
              if (parentPage && parentPage->getMainWindow()) {
                parentPage->getMainWindow()->openFileAndAddToNewTab(filePath);
              }
            });
    menu.addSeparator();
  }

  if (!multiple) {
    connect(menu.addAction(tr("Reveal in Explorer")), &QAction::triggered, this,
            [this, filePath]() {
              if (parentPage)
                parentPage->revealPath(filePath);
            });
    connect(
        menu.addAction(tr("Find in Folder…")), &QAction::triggered, this,
        [this, containingDir]() { emit findInFolderRequested(containingDir); });
    menu.addSeparator();
  }

  if (!multiple) {
    connect(menu.addAction(tr("New File…")), &QAction::triggered, this,
            [this, containingDir]() {
              fileController->handleNewFile(containingDir);
            });
    connect(menu.addAction(tr("New Folder…")), &QAction::triggered, this,
            [this, containingDir]() {
              fileController->handleNewDirectory(containingDir);
            });
    menu.addSeparator();
  }

  connect(menu.addAction(tr("Cut")), &QAction::triggered, this,
          [this, selection]() { fileController->handleCut(selection); });
  connect(menu.addAction(tr("Copy")), &QAction::triggered, this,
          [this, selection]() { fileController->handleCopy(selection); });

  QAction *pasteAction = menu.addAction(tr("Paste"));
  pasteAction->setEnabled(fileModel->canPaste());
  connect(pasteAction, &QAction::triggered, this, [this, containingDir]() {
    fileController->handlePaste(containingDir);
  });

  menu.addSeparator();

  if (!multiple) {
    connect(menu.addAction(tr("Rename…")), &QAction::triggered, this,
            [this, filePath]() { fileController->handleRename(filePath); });
  }
  connect(menu.addAction(tr("Duplicate")), &QAction::triggered, this,
          [this, selection]() { fileController->handleDuplicate(selection); });
  connect(menu.addAction(tr("Move to Folder…")), &QAction::triggered, this,
          [this, selection, containingDir]() {
            fileController->handleTransferTo(selection, containingDir, false);
          });
  connect(menu.addAction(tr("Copy to Folder…")), &QAction::triggered, this,
          [this, selection, containingDir]() {
            fileController->handleTransferTo(selection, containingDir, true);
          });
  menu.addSeparator();

  connect(menu.addAction(tr("Move %1 to Trash").arg(subject)),
          &QAction::triggered, this, [this, selection]() {
            fileController->handleRemove(selection, DeleteMode::Trash);
          });
  connect(menu.addAction(tr("Delete Permanently…")), &QAction::triggered, this,
          [this, selection]() {
            fileController->handleRemove(selection, DeleteMode::Permanent);
          });

  menu.addSeparator();

  TestFileClassifier &classifier = TestFileClassifier::instance();
  const bool currentlyTest = isDirectory ? classifier.isTestDirectory(filePath)
                                         : classifier.isTestFile(filePath);
  if (!multiple) {
    QMenu *runMenu = menu.addMenu(tr("Run and Debug"));
    if (!isDirectory) {
      connect(runMenu->addAction(tr("Run %1").arg(name)), &QAction::triggered,
              this, [this, filePath]() { emit runFileRequested(filePath); });
      connect(runMenu->addAction(tr("Debug %1").arg(name)), &QAction::triggered,
              this, [this, filePath]() { emit debugFileRequested(filePath); });
    }
    connect(runMenu->addAction(isDirectory ? tr("Run Tests in Folder")
                                           : tr("Run as Test")),
            &QAction::triggered, this,
            [this, filePath]() { emit runTestsRequested(filePath); });

    if (!isDirectory) {
      connect(runMenu->addAction(currentlyTest ? tr("Unmark as Test File")
                                               : tr("Mark as Test File")),
              &QAction::triggered, this, [this, filePath, currentlyTest]() {
                emit toggleTestMarkerRequested(filePath, !currentlyTest);
              });
    }
    menu.addSeparator();
  }

  const QString root = rootPath();
  connect(menu.addAction(tr("Copy Path")), &QAction::triggered, this,
          [this, selection]() {
            fileController->handleCopyAbsolutePath(selection);
          });
  QAction *relativeAction = menu.addAction(tr("Copy Relative Path"));
  relativeAction->setEnabled(!root.isEmpty());
  connect(relativeAction, &QAction::triggered, this, [this, selection, root]() {
    fileController->handleCopyRelativePath(selection, root);
  });

  if (!multiple) {
    menu.addSeparator();
    connect(
        menu.addAction(tr("Reveal in File Manager")), &QAction::triggered, this,
        [this, filePath]() { emit revealInFileManagerRequested(filePath); });
    connect(menu.addAction(tr("Open in Terminal")), &QAction::triggered, this,
            [this, containingDir]() {
              emit openInTerminalRequested(containingDir);
            });
  }
}

QString LightpadTreeView::dropDirectoryAt(const QPoint &position) const {
  const QModelIndex index = indexAt(position);
  if (!index.isValid() || !parentPage) {

    return rootPath();
  }

  const QString path = parentPage->getFilePath(index);
  const QFileInfo info(path);
  return info.isDir() ? info.absoluteFilePath() : info.absolutePath();
}

bool LightpadTreeView::acceptsDropOf(const QStringList &sources,
                                     const QString &destination) const {
  const QFileInfo target(destination);
  if (destination.isEmpty() || sources.isEmpty() || !target.isDir() ||
      !target.isWritable()) {
    return false;
  }

  for (const QString &source : sources) {
    const QFileInfo info(source);
    if (!info.exists() && !info.isSymLink()) {
      return false;
    }

    if (info.isDir() && FileDirTreeModel::isInside(source, destination)) {
      return false;
    }
  }
  return true;
}

bool LightpadTreeView::performDrop(const QStringList &sources,
                                   const QString &destination, bool copy) {
  const QStringList created =
      fileController->handleTransfer(sources, destination, copy);
  if (parentPage && !created.isEmpty()) {
    parentPage->revealPath(created.last());
  }
  return created.size() == sources.size();
}

void LightpadTreeView::startDrag(Qt::DropActions) {
  const QStringList sources = selectedPaths();
  if (sources.isEmpty()) {
    return;
  }
  auto *mime = new QMimeData;
  QList<QUrl> urls;
  for (const QString &path : sources) {
    urls.append(QUrl::fromLocalFile(path));
  }
  mime->setUrls(urls);
  auto *drag = new QDrag(this);
  drag->setMimeData(mime);
  // Our destination owns the filesystem move; QAbstractItemView must not
  // remove source rows again when the drag returns MoveAction.
  drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::MoveAction);
  drag->deleteLater();
}

void LightpadTreeView::dragEnterEvent(QDragEnterEvent *event) {
  if (localDropPaths(event->mimeData()).isEmpty() ||
      dropAction(event) == Qt::IgnoreAction) {
    event->ignore();
    return;
  }
  event->setDropAction(dropAction(event));
  event->accept();
}

void LightpadTreeView::dragMoveEvent(QDragMoveEvent *event) {
  const QStringList sources = localDropPaths(event->mimeData());

  const QPoint position =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      event->position().toPoint();
#else
      event->pos();
#endif

  const QString destination = dropDirectoryAt(position);
  const Qt::DropAction action = dropAction(event);
  // Preserve Qt's autoscroll and hover-to-expand behaviour.
  QTreeView::dragMoveEvent(event);
  if (action != Qt::IgnoreAction && acceptsDropOf(sources, destination)) {
    m_dropDestination = destination;
    m_dropCopy = action == Qt::CopyAction;
    event->setDropAction(action);
    event->accept();
  } else {
    m_dropDestination.clear();
    event->ignore();
  }
  viewport()->update();
}

void LightpadTreeView::dropEvent(QDropEvent *event) {
  m_dropDestination.clear();
  viewport()->update();

  const QPoint dropPos =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      event->position().toPoint();
#else
      event->pos();
#endif

  const QString destination = dropDirectoryAt(dropPos);

  const QStringList sources = localDropPaths(event->mimeData());
  const Qt::DropAction action = dropAction(event);

  if (action == Qt::IgnoreAction || !acceptsDropOf(sources, destination)) {
    event->ignore();
    return;
  }

  if (performDrop(sources, destination, action == Qt::CopyAction)) {
    event->setDropAction(action);
    event->accept();
  } else {
    event->ignore();
  }
}

void LightpadTreeView::dragLeaveEvent(QDragLeaveEvent *event) {
  m_dropDestination.clear();
  viewport()->update();
  QTreeView::dragLeaveEvent(event);
}

void LightpadTreeView::paintEvent(QPaintEvent *event) {
  QTreeView::paintEvent(event);
  if (m_dropDestination.isEmpty()) {
    return;
  }
  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing);
  const QColor accent = palette().color(QPalette::Highlight);
  painter.setPen(QPen(accent, 2, Qt::DashLine));
  painter.drawRoundedRect(viewport()->rect().adjusted(2, 2, -3, -3), 5, 5);
  const QRect label = viewport()->rect().adjusted(8, 0, -8, -8);
  const QRect chip(label.left(), label.bottom() - 30, label.width(), 30);
  painter.setPen(Qt::NoPen);
  painter.setBrush(accent);
  painter.drawRoundedRect(chip, 5, 5);
  painter.setPen(palette().color(QPalette::HighlightedText));
  const QString target =
      m_dropDestination == rootPath()
          ? tr("project root")
          : QDir(rootPath()).relativeFilePath(m_dropDestination);
  const QString text = m_dropCopy ? tr("Copy into %1").arg(target)
                                  : tr("Move into %1").arg(target);
  painter.drawText(
      chip.adjusted(6, 0, -6, 0), Qt::AlignCenter,
      fontMetrics().elidedText(text, Qt::ElideMiddle, chip.width() - 12));
}
