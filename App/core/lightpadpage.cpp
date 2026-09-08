#include "lightpadpage.h"
#include "../git/gitintegration.h"
#include "../run_templates/runtemplatemanager.h"
#include "../test_templates/testfileclassifier.h"
#include "../theme/themeengine.h"
#include "../ui/mainwindow.h"
#include "../ui/panels/minimap.h"
#include "../ui/uistylehelper.h"
#include <QAction>
#include <QContextMenuEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGlobal>
#include <functional>

namespace {
class ExplorerTreeDelegate : public QStyledItemDelegate {
public:
  explicit ExplorerTreeDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void setTheme(const Theme &theme) {
    m_textColor = theme.foregroundColor;
    m_selectedTextColor = theme.foregroundColor;
    m_hoverBackground = theme.hoverColor.lighter(112);
    m_selectedBackground = theme.accentSoftColor.lighter(115);
    m_selectedBorder = theme.accentColor;
  }

  void setTheme(const ThemeDefinition &theme) {
    m_textColor = theme.colors.textPrimary;
    m_selectedTextColor = theme.colors.textPrimary;
    m_hoverBackground = theme.colors.treeHoverBg.isValid()
                            ? theme.colors.treeHoverBg
                            : theme.colors.btnGhostHover;
    m_selectedBackground = theme.colors.treeSelectedBg.isValid()
                               ? theme.colors.treeSelectedBg
                               : theme.colors.accentSoft;
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
    opt.palette.setColor(QPalette::Highlight, Qt::transparent);
    opt.palette.setColor(QPalette::HighlightedText, m_selectedTextColor);
    opt.palette.setColor(QPalette::Text, m_textColor);

    QStyledItemDelegate::paint(painter, opt, index);

    if (!badge.isEmpty() && badgeColor.isValid()) {
      QFont badgeFont = opt.font;
      badgeFont.setPixelSize(10);
      badgeFont.setWeight(QFont::Bold);
      painter->setFont(badgeFont);
      painter->setPen(badgeColor);
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
  QColor m_textColor = QColor("#dce4ee");
  QColor m_selectedTextColor = QColor("#eef4ff");
  QColor m_hoverBackground = QColor("#232a33");
  QColor m_selectedBackground = QColor("#1f3554");
  QColor m_selectedBorder = QColor("#5fa8ff");
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

  connect(fileController, &FileDirTreeController::actionCompleted, parentPage,
          &LightpadPage::updateModel);
  connect(fileController, &FileDirTreeController::fileRemoved, parentPage,
          &LightpadPage::closeTabPage);
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
  return paths;
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
  const QStringList selection = selectedPaths();
  for (const QString &path : selection) {
    fileController->handleDuplicate(path);
  }
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
  showContextMenu(event->pos());
  event->accept();
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
    connect(menu.addAction(tr("Run %1").arg(name)), &QAction::triggered, this,
            [this, filePath]() { emit runFileRequested(filePath); });
    connect(menu.addAction(tr("Debug %1").arg(name)), &QAction::triggered, this,
            [this, filePath]() { emit debugFileRequested(filePath); });
    menu.addSeparator();
    connect(menu.addAction(tr("Open")), &QAction::triggered, this,
            [this, filePath]() {
              if (parentPage && parentPage->getMainWindow()) {
                parentPage->getMainWindow()->openFileAndAddToNewTab(filePath);
              }
            });
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
    connect(menu.addAction(tr("Duplicate")), &QAction::triggered, this,
            [this, filePath]() { fileController->handleDuplicate(filePath); });
  }

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
    connect(menu.addAction(isDirectory ? tr("Run Tests in Folder")
                                       : tr("Run as Test")),
            &QAction::triggered, this,
            [this, filePath]() { emit runTestsRequested(filePath); });

    if (!isDirectory) {
      connect(menu.addAction(currentlyTest ? tr("Unmark as Test File")
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
  if (destination.isEmpty() || sources.isEmpty()) {
    return false;
  }

  for (const QString &source : sources) {
    const QFileInfo info(source);
    if (!info.exists() && !info.isSymLink()) {
      continue;
    }

    if (info.isDir() && FileDirTreeModel::isInside(source, destination)) {
      return false;
    }
    return true;
  }
  return false;
}

void LightpadTreeView::performDrop(const QStringList &sources,
                                   const QString &destination, bool copy) {
  QString lastCreated;
  bool anySucceeded = false;

  for (const QString &source : sources) {
    QString created;
    const bool ok = copy ? fileModel->copyInto(source, destination, &created)
                         : fileModel->moveInto(source, destination, &created);
    if (ok) {
      anySucceeded = true;
      lastCreated = created;
    }
  }

  if (!anySucceeded) {
    return;
  }

  if (parentPage) {
    parentPage->updateModel();
    if (!lastCreated.isEmpty()) {
      parentPage->revealPath(lastCreated);
    }
  }
}

void LightpadTreeView::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  } else {
    QTreeView::dragEnterEvent(event);
  }
}

void LightpadTreeView::dragMoveEvent(QDragMoveEvent *event) {
  if (!event->mimeData()->hasUrls()) {
    QTreeView::dragMoveEvent(event);
    return;
  }

  QStringList sources;
  const QList<QUrl> urls = event->mimeData()->urls();
  for (const QUrl &url : urls) {
    const QString local = url.toLocalFile();
    if (!local.isEmpty()) {
      sources << local;
    }
  }

  const QPoint position =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      event->position().toPoint();
#else
      event->pos();
#endif

  if (acceptsDropOf(sources, dropDirectoryAt(position))) {
    event->acceptProposedAction();
  } else {
    event->ignore();
  }
}

void LightpadTreeView::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasUrls()) {
    QTreeView::dropEvent(event);
    return;
  }

  const QPoint dropPos =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      event->position().toPoint();
#else
      event->pos();
#endif

  const QString destination = dropDirectoryAt(dropPos);

  QStringList sources;
  const QList<QUrl> urls = event->mimeData()->urls();
  for (const QUrl &url : urls) {
    const QString local = url.toLocalFile();
    if (!local.isEmpty()) {
      sources << local;
    }
  }

  if (!acceptsDropOf(sources, destination)) {
    event->ignore();
    return;
  }

  const bool copy = event->modifiers().testFlag(Qt::ControlModifier) ||
                    event->dropAction() == Qt::CopyAction;
  performDrop(sources, destination, copy);
  event->acceptProposedAction();
}

LightpadPage::LightpadPage(QWidget *parent, bool treeViewHidden)
    : QWidget(parent), mainWindow(nullptr), treeContainer(nullptr),
      treeHeader(nullptr), treeTitleLabel(nullptr), treeFilterEdit(nullptr),
      treeNewFileButton(nullptr), treeNewFolderButton(nullptr),
      treeRefreshButton(nullptr), treeCollapseButton(nullptr),
      treeExpandButton(nullptr), treeView(nullptr), textArea(nullptr),
      minimap(nullptr), model(nullptr), m_ownsModel(true),
      m_gitIntegration(nullptr), m_treeFilterText(""), filePath(""),
      projectRootPath("") {

  auto *layoutHor = new QHBoxLayout(this);
  layoutHor->setContentsMargins(0, 0, 0, 0);

  treeContainer = new QWidget(this);
  treeContainer->setObjectName("treeContainer");
  treeContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  treeContainer->setMinimumWidth(240);
  treeContainer->setMaximumWidth(420);
  auto *treeLayout = new QVBoxLayout(treeContainer);
  treeLayout->setContentsMargins(8, 8, 8, 8);
  treeLayout->setSpacing(8);

  treeHeader = new QWidget(treeContainer);
  treeHeader->setObjectName("treeHeader");
  auto *treeHeaderLayout = new QHBoxLayout(treeHeader);
  treeHeaderLayout->setContentsMargins(4, 2, 4, 2);
  treeHeaderLayout->setSpacing(6);

  treeTitleLabel = new QLabel("EXPLORER", treeHeader);
  treeTitleLabel->setObjectName("treeTitleLabel");
  treeHeaderLayout->addWidget(treeTitleLabel);
  treeHeaderLayout->addStretch(1);

  treeNewFileButton = new QToolButton(treeHeader);
  treeNewFileButton->setObjectName("treeToolButton");
  treeNewFileButton->setToolTip(tr("New file (Ctrl+Alt+N)"));
  treeNewFileButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
  treeNewFileButton->setIconSize(QSize(14, 14));
  treeHeaderLayout->addWidget(treeNewFileButton);

  treeNewFolderButton = new QToolButton(treeHeader);
  treeNewFolderButton->setObjectName("treeToolButton");
  treeNewFolderButton->setToolTip(tr("New folder (Ctrl+Shift+N)"));
  treeNewFolderButton->setIcon(
      style()->standardIcon(QStyle::SP_FileDialogNewFolder));
  treeNewFolderButton->setIconSize(QSize(14, 14));
  treeHeaderLayout->addWidget(treeNewFolderButton);

  treeRefreshButton = new QToolButton(treeHeader);
  treeRefreshButton->setObjectName("treeToolButton");
  treeRefreshButton->setToolTip("Refresh file tree");
  treeRefreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
  treeRefreshButton->setIconSize(QSize(14, 14));
  treeHeaderLayout->addWidget(treeRefreshButton);

  treeCollapseButton = new QToolButton(treeHeader);
  treeCollapseButton->setObjectName("treeToolButton");
  treeCollapseButton->setToolTip("Collapse all");
  treeCollapseButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
  treeCollapseButton->setIconSize(QSize(14, 14));
  treeHeaderLayout->addWidget(treeCollapseButton);

  treeExpandButton = new QToolButton(treeHeader);
  treeExpandButton->setObjectName("treeToolButton");
  treeExpandButton->setToolTip("Expand one level");
  treeExpandButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
  treeExpandButton->setIconSize(QSize(14, 14));
  treeHeaderLayout->addWidget(treeExpandButton);

  treeFilterEdit = new QLineEdit(treeContainer);
  treeFilterEdit->setObjectName("treeFilterEdit");
  treeFilterEdit->setPlaceholderText("Filter files...");
  treeFilterEdit->setClearButtonEnabled(true);

  treeView = new LightpadTreeView(this);
  treeView->setExpandsOnDoubleClick(false);
  treeView->setObjectName("fileTreeView");
  textArea = new TextArea(this);
  minimap = new Minimap(this);

  minimap->setSourceEditor(textArea);

  treeLayout->addWidget(treeHeader);
  treeLayout->addWidget(treeFilterEdit);
  treeLayout->addWidget(treeView, 1);

  layoutHor->addWidget(treeContainer);
  layoutHor->addWidget(textArea);
  layoutHor->addWidget(minimap);

  if (treeViewHidden) {
    treeContainer->hide();
  }

  layoutHor->setStretch(0, 0);
  layoutHor->setStretch(1, 1);
  layoutHor->setStretch(2, 0);

  setLayout(layoutHor);

  QObject::connect(
      treeView, &QAbstractItemView::doubleClicked, this,
      [this](const QModelIndex &index) { activateTreeIndex(index); });

  connect(treeNewFileButton, &QToolButton::clicked, this, [this]() {
    if (auto *view = qobject_cast<LightpadTreeView *>(treeView)) {
      view->promptNewFile();
    }
  });
  connect(treeNewFolderButton, &QToolButton::clicked, this, [this]() {
    if (auto *view = qobject_cast<LightpadTreeView *>(treeView)) {
      view->promptNewFolder();
    }
  });
  connect(treeRefreshButton, &QToolButton::clicked, this, [this]() {
    updateModel();
    refreshGitStatus();
  });
  connect(treeCollapseButton, &QToolButton::clicked, treeView,
          &QTreeView::collapseAll);
  connect(treeExpandButton, &QToolButton::clicked, this, [this]() {
    if (!treeView) {
      return;
    }
    treeView->expandToDepth(1);
  });
  connect(treeFilterEdit, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (mainWindow) {
              mainWindow->setTreeFilterText(text);
              return;
            }
            setTreeFilterText(text);
          });
}

QTreeView *LightpadPage::getTreeView() { return treeView; }

TextArea *LightpadPage::getTextArea() { return textArea; }

Minimap *LightpadPage::getMinimap() { return minimap; }

void LightpadPage::setTreeViewVisible(bool flag) {
  if (treeContainer) {
    treeContainer->setVisible(flag);
  }
}

void LightpadPage::setMinimapVisible(bool flag) {
  if (minimap) {
    minimap->setMinimapVisible(flag);
  }
}

bool LightpadPage::isMinimapVisible() const {
  return minimap ? minimap->isMinimapVisible() : false;
}

void LightpadPage::setTreeFilterText(const QString &text) {
  m_treeFilterText = text;
  if (treeFilterEdit && treeFilterEdit->text() != text) {
    QSignalBlocker blocker(treeFilterEdit);
    treeFilterEdit->setText(text);
  }
  applyTreeFilter();
}

QString LightpadPage::getTreeFilterText() const { return m_treeFilterText; }

void LightpadPage::applyTreeFilter() {
  if (!treeView || !model) {
    return;
  }

  const QString needle = m_treeFilterText.trimmed();
  treeView->setUpdatesEnabled(false);

  const QModelIndex root = treeView->rootIndex();
  updateTreeVisibilityRecursive(root, needle);

  if (!needle.isEmpty()) {
    treeView->expandToDepth(2);
  }

  treeView->setUpdatesEnabled(true);
  treeView->viewport()->update();
}

bool LightpadPage::updateTreeVisibilityRecursive(const QModelIndex &parent,
                                                 const QString &needle) {
  if (!model || !treeView) {
    return false;
  }

  bool anyVisible = false;
  const int rowCount = model->rowCount(parent);
  for (int row = 0; row < rowCount; ++row) {
    const QModelIndex idx = model->index(row, 0, parent);
    if (!idx.isValid()) {
      continue;
    }

    if (!needle.isEmpty() && model->canFetchMore(idx)) {
      model->fetchMore(idx);
    }

    bool hasVisibleChild = false;
    if (model->hasChildren(idx)) {
      hasVisibleChild = updateTreeVisibilityRecursive(idx, needle);
    }

    const QString label = model->data(idx, Qt::DisplayRole).toString();
    const bool selfMatch =
        needle.isEmpty() || label.contains(needle, Qt::CaseInsensitive);
    const bool visible = selfMatch || hasVisibleChild;

    treeView->setRowHidden(row, parent, !visible);
    anyVisible = anyVisible || visible;
  }

  return anyVisible;
}

void LightpadPage::setModelRootIndex(QString path) {
  treeView->setRootIndex(model->index(path));
  applyTreeFilter();
  if (mainWindow) {
    auto *view = qobject_cast<LightpadTreeView *>(treeView);
    if (view) {
      mainWindow->registerTreeView(view);
    }
  }
}

void LightpadPage::revealPath(const QString &path) {
  if (path.isEmpty() || !model || !treeView) {
    return;
  }

  const QModelIndex index = model->index(path);
  if (!index.isValid()) {
    return;
  }

  QModelIndex ancestor = index.parent();
  const QModelIndex root = treeView->rootIndex();
  while (ancestor.isValid() && ancestor != root) {
    treeView->expand(ancestor);
    ancestor = ancestor.parent();
  }

  treeView->setCurrentIndex(index);
  if (QItemSelectionModel *selection = treeView->selectionModel()) {
    selection->select(index, QItemSelectionModel::ClearAndSelect |
                                 QItemSelectionModel::Rows);
  }
  treeView->scrollTo(index, QAbstractItemView::EnsureVisible);
}

void LightpadPage::activateTreeIndex(const QModelIndex &index) {
  if (!index.isValid() || !model || !treeView) {
    return;
  }

  if (model->isDir(index)) {
    if (treeView->isExpanded(index)) {
      treeView->collapse(index);
    } else {
      treeView->expand(index);
    }
    treeView->setCurrentIndex(index);
    return;
  }

  if (!mainWindow) {
    return;
  }

  QString path = model->filePath(index);
  mainWindow->openFileAndAddToNewTab(path);
  treeView->setCurrentIndex(index);
}

void LightpadPage::setCustomContentWidget(QWidget *widget) {
  if (!widget || !textArea)
    return;

  auto *layout = qobject_cast<QHBoxLayout *>(this->layout());
  if (!layout)
    return;

  layout->replaceWidget(textArea, widget);
  textArea->setVisible(false);
  widget->setParent(this);
  widget->setVisible(true);

  if (minimap) {
    minimap->setVisible(false);
  }
}

void LightpadPage::setMainWindow(MainWindow *window) {

  mainWindow = window;

  if (textArea) {
    textArea->setMainWindow(mainWindow);
    textArea->setFont(mainWindow->getFont());
    textArea->setTabWidth(mainWindow->getTabWidth());
    textArea->setVimModeEnabled(mainWindow->getSettings().vimModeEnabled);
  }

  if (mainWindow) {
    auto *sharedModel = mainWindow->getFileTreeModel();
    if (sharedModel) {
      setSharedFileSystemModel(sharedModel);
    } else {

      updateModel();
    }
    auto *view = qobject_cast<LightpadTreeView *>(treeView);
    if (view) {
      mainWindow->registerTreeView(view);
    }
    setTreeFilterText(mainWindow->getTreeFilterText());
    applyTheme(ThemeEngine::instance().activeTheme());
  }
}

void LightpadPage::setSharedFileSystemModel(GitFileSystemModel *sharedModel) {
  if (!sharedModel || model == sharedModel) {
    return;
  }
  model = sharedModel;
  m_ownsModel = false;
  updateModel();
}

void LightpadPage::setFilePath(QString path) {
  filePath = path;

  if (!path.isEmpty() && !projectRootPath.isEmpty()) {
    setTreeViewVisible(true);
  }
}

void LightpadPage::closeTabPage(QString path) {
  if (mainWindow)
    mainWindow->closeTabPage(path);
}

void LightpadPage::updateModel() {

  if (!model && mainWindow) {
    auto *sharedModel = mainWindow->getFileTreeModel();
    if (sharedModel) {
      model = sharedModel;
      m_ownsModel = false;
    }
  }

  if (!model) {
    model = new GitFileSystemModel(this);
    m_ownsModel = true;
  }

  if (m_ownsModel) {

    QString currentRootPath =
        projectRootPath.isEmpty() ? QDir::home().path() : projectRootPath;
    model->setRootPath(currentRootPath);
    model->setRootHeaderLabel(projectRootPath);
  }

  if (m_gitIntegration) {
    model->setGitIntegration(m_gitIntegration);
  }

  if (treeView->model() != model) {

    treeView->setModel(model);
  }
  model->sort(0, Qt::AscendingOrder);
  connect(
      model, &QFileSystemModel::directoryLoaded, this,
      [this](const QString &) {
        if (!m_treeFilterText.trimmed().isEmpty()) {
          applyTreeFilter();
        }
      },
      Qt::UniqueConnection);

  treeView->setColumnHidden(1, true);
  treeView->setColumnHidden(2, true);
  treeView->setColumnHidden(3, true);

  if (!projectRootPath.isEmpty()) {
    treeView->setRootIndex(model->index(projectRootPath));
  } else {
    treeView->setRootIndex(QModelIndex());
  }

  treeView->setHeaderHidden(true);
  applyTreeFilter();
}

QString LightpadPage::getFilePath() { return filePath; }

QString LightpadPage::getFilePath(const QModelIndex &index) {
  if (!model || !index.isValid()) {
    return QString();
  }
  return model->filePath(index);
}

bool LightpadPage::hasRunTemplate() const {
  if (filePath.isEmpty()) {
    return false;
  }

  RunTemplateManager &manager = RunTemplateManager::instance();

  FileTemplateAssignment assignment = manager.getAssignmentForFile(filePath);
  if (!assignment.templateId.isEmpty()) {
    return true;
  }

  QList<RunTemplate> templates = manager.getTemplatesForFilePath(filePath);
  return !templates.isEmpty();
}

QString LightpadPage::getAssignedTemplateId() const {
  if (filePath.isEmpty()) {
    return QString();
  }

  RunTemplateManager &manager = RunTemplateManager::instance();
  FileTemplateAssignment assignment = manager.getAssignmentForFile(filePath);

  if (!assignment.templateId.isEmpty()) {
    return assignment.templateId;
  }

  QList<RunTemplate> templates = manager.getTemplatesForFilePath(filePath);
  if (!templates.isEmpty()) {
    return templates.first().id;
  }

  return QString();
}

void LightpadPage::setProjectRootPath(const QString &path) {
  projectRootPath = path;
  if (treeTitleLabel) {
    const QString name = QFileInfo(path).fileName();
    treeTitleLabel->setText(name.isEmpty() ? tr("EXPLORER") : name.toUpper());
    treeTitleLabel->setToolTip(path);
  }
  if (model) {
    model->setRootHeaderLabel(projectRootPath);
    treeView->setHeaderHidden(true);
    if (projectRootPath.isEmpty()) {
      treeView->setRootIndex(QModelIndex());
    } else {
      treeView->setRootIndex(model->index(projectRootPath));
    }
    applyTreeFilter();
  }
}

QString LightpadPage::getProjectRootPath() const { return projectRootPath; }

void LightpadPage::setGitIntegration(GitIntegration *git) {
  m_gitIntegration = git;
  if (model) {
    model->setGitIntegration(git);
  }
}

void LightpadPage::setGitStatusEnabled(bool enabled) {
  if (model) {
    model->setGitStatusEnabled(enabled);
  }
}

void LightpadPage::refreshGitStatus() {
  if (model) {
    model->refreshGitStatus();
  }
}

void LightpadPage::applyTheme(const Theme &theme) {
  if (treeContainer) {
    const QString panelTop = theme.surfaceColor.lighter(108).name();
    const QString panelBottom = theme.surfaceColor.darker(102).name();
    const QString border = theme.borderColor.name();
    const QString muted = theme.singleLineCommentFormat.name();
    const QString fg = theme.foregroundColor.name();
    const QString filterBg = theme.surfaceAltColor.lighter(105).name();
    const QString filterFocusBg = theme.surfaceAltColor.lighter(112).name();
    const QString accent = theme.accentColor.name();
    const QString pressed = theme.pressedColor.name();
    treeContainer->setStyleSheet(
        QString("#treeContainer {"
                "  background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, "
                "stop: 0 %1, stop: 1 %2);"
                "  border-right: 1px solid %3;"
                "}"
                "#treeHeader {"
                "  background: transparent;"
                "  border-bottom: 1px solid %3;"
                "}"
                "QLabel#treeTitleLabel {"
                "  color: %4;"
                "  font-size: 10px;"
                "  font-weight: 700;"
                "  letter-spacing: 1px;"
                "  padding: 2px 0;"
                "}"
                "QLineEdit#treeFilterEdit {"
                "  background: %6;"
                "  color: %5;"
                "  border: 1px solid %3;"
                "  border-radius: 8px;"
                "  padding: 7px 10px;"
                "}"
                "QLineEdit#treeFilterEdit:focus {"
                "  background: %7;"
                "  border: 1px solid %8;"
                "}"
                "QToolButton#treeToolButton {"
                "  background: %6;"
                "  border: 1px solid %3;"
                "  border-radius: 7px;"
                "  padding: 4px;"
                "  color: %5;"
                "}"
                "QToolButton#treeToolButton:hover {"
                "  background: %7;"
                "  border-color: %8;"
                "}"
                "QToolButton#treeToolButton:pressed {"
                "  background: %9;"
                "}")
            .arg(panelTop, panelBottom, border, muted, fg, filterBg,
                 filterFocusBg, accent, pressed));
  }

  if (treeView) {
    treeView->setStyleSheet(UIStyleHelper::treeViewStyle(theme));
    if (auto *delegate =
            dynamic_cast<ExplorerTreeDelegate *>(treeView->itemDelegate())) {
      delegate->setTheme(theme);
    }
    treeView->viewport()->update();
  }
}

void LightpadPage::applyTheme(const ThemeDefinition &theme) {
  if (treeContainer) {
    const ThemeColors &c = theme.colors;
    QColor glowAccent =
        (c.accentPrimary.isValid() ? c.accentPrimary : c.borderFocus);
    glowAccent =
        glowAccent.lighter(100 + qRound(theme.ui.glowIntensity * 24.0));
    const QString panelTop =
        (c.surfaceRaised.isValid() ? c.surfaceRaised : c.surfaceBase).name();
    const QString panelBottom =
        (c.surfaceOverlay.isValid() ? c.surfaceOverlay : c.surfaceRaised)
            .name();
    const QString border =
        (c.borderDefault.isValid() ? c.borderDefault : c.borderSubtle).name();
    const QString panelBorder =
        theme.ui.panelBorders ? border : QStringLiteral("transparent");
    const QString muted = c.textMuted.name();
    const QString fg = c.textPrimary.name();
    const QString filterBg =
        (c.inputBg.isValid() ? c.inputBg : c.surfaceOverlay).name();
    const QString filterFocusBg =
        (c.surfacePopover.isValid() ? c.surfacePopover : c.inputBg).name();
    const QString accent = glowAccent.name();
    const QString pressed =
        (c.btnGhostActive.isValid() ? c.btnGhostActive : c.btnGhostHover)
            .name();
    treeContainer->setStyleSheet(
        QString("#treeContainer {"
                "  background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, "
                "stop: 0 %1, stop: 1 %2);"
                "  border-right: 1px solid %3;"
                "}"
                "#treeHeader {"
                "  background: transparent;"
                "  border-bottom: 1px solid %3;"
                "}"
                "QLabel#treeTitleLabel {"
                "  color: %4;"
                "  font-size: 10px;"
                "  font-weight: 700;"
                "  letter-spacing: 1px;"
                "  padding: 2px 0;"
                "}"
                "QLineEdit#treeFilterEdit {"
                "  background: %6;"
                "  color: %5;"
                "  border: 1px solid %3;"
                "  border-radius: 8px;"
                "  padding: 7px 10px;"
                "}"
                "QLineEdit#treeFilterEdit:focus {"
                "  background: %7;"
                "  border: 1px solid %8;"
                "}"
                "QToolButton#treeToolButton {"
                "  background: %6;"
                "  border: 1px solid %3;"
                "  border-radius: 7px;"
                "  padding: 4px;"
                "  color: %5;"
                "}"
                "QToolButton#treeToolButton:hover {"
                "  background: %7;"
                "  border-color: %8;"
                "}"
                "QToolButton#treeToolButton:pressed {"
                "  background: %9;"
                "}")
            .arg(panelTop, panelBottom, panelBorder, muted, fg, filterBg,
                 filterFocusBg, accent, pressed));
  }

  if (treeView) {
    treeView->setStyleSheet(
        UIStyleHelper::treeViewStyle(theme.toClassicTheme()));
    if (auto *delegate =
            dynamic_cast<ExplorerTreeDelegate *>(treeView->itemDelegate())) {
      delegate->setTheme(theme);
    }
    treeView->viewport()->update();
  }
}

MainWindow *LightpadPage::getMainWindow() const { return mainWindow; }
