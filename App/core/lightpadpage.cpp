#include "lightpadpage.h"
#include "../git/gitintegration.h"
#include "../run_templates/runtemplatemanager.h"
#include "../test_templates/testfileclassifier.h"
#include "../ui/mainwindow.h"
#include "../ui/panels/minimap.h"
#include "../ui/uistylehelper.h"
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
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGlobal>

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

class LineEdit : public QLineEdit {

  using QLineEdit::QLineEdit;

public:
  LineEdit(QRect rect, QString filePath, QWidget *parent)
      : QLineEdit(parent), oldFilePath(filePath) {
    show();
    setGeometry(QRect(rect.x(), rect.y() + rect.height() + 1,
                      1.1 * rect.width(), 1.1 * rect.height()));
    setFocus(Qt::MouseFocusReason);
  }

protected:
  void focusOutEvent(QFocusEvent *event) override {
    Q_UNUSED(event);

    LightpadTreeView *treeView =
        qobject_cast<LightpadTreeView *>(parentWidget());

    if (treeView)
      treeView->renameFile(oldFilePath,
                           QFileInfo(oldFilePath).absoluteDir().path() +
                               QDir::separator() + text());

    close();
  }

  void keyPressEvent(QKeyEvent *event) override {

    if ((event->key() == Qt::Key_Enter) || (event->key() == Qt::Key_Return)) {
      renameTreeViewEntry();
      close();
    }

    else if (event->key() == Qt::Key_Escape)
      close();

    else
      QLineEdit::keyPressEvent(event);
  }

private:
  QString oldFilePath;

  void renameTreeViewEntry() {
    LightpadTreeView *treeView =
        qobject_cast<LightpadTreeView *>(parentWidget());

    if (treeView)
      treeView->renameFile(oldFilePath,
                           QFileInfo(oldFilePath).absoluteDir().path() +
                               QDir::separator() + text());
  }
};

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
  setAllColumnsShowFocus(false);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  setItemDelegate(new ExplorerTreeDelegate(this));

  connect(fileController, &FileDirTreeController::actionCompleted, parentPage,
          &LightpadPage::updateModel);
  connect(fileController, &FileDirTreeController::fileRemoved, parentPage,
          &LightpadPage::closeTabPage);
}

LightpadTreeView::~LightpadTreeView() {}

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

void LightpadTreeView::mouseReleaseEvent(QMouseEvent *e) {
  if (e->button() == Qt::RightButton) {
    showContextMenu(e->pos());
  } else {
    QTreeView::mouseReleaseEvent(e);
  }
}

void LightpadTreeView::showContextMenu(const QPoint &pos) {
  QModelIndex idx = indexAt(pos);
  if (!idx.isValid()) {
    return;
  }

  QString filePath = parentPage->getFilePath(idx);
  QFileInfo fileInfo(filePath);
  QString parentPath = fileInfo.isDir() ? filePath : fileInfo.absolutePath();

  QMenu menu;
  if (parentPage && parentPage->getMainWindow()) {
    menu.setStyleSheet(UIStyleHelper::contextMenuStyle(
        parentPage->getMainWindow()->getTheme()));
  }

  QAction *newFileAction = menu.addAction("New File");
  QAction *newDirAction = menu.addAction("New Directory");
  menu.addSeparator();
  QAction *duplicateAction = menu.addAction("Duplicate");
  QAction *renameAction = menu.addAction("Rename");
  menu.addSeparator();
  QAction *copyAction = menu.addAction("Copy");
  QAction *cutAction = menu.addAction("Cut");
  QAction *pasteAction = menu.addAction("Paste");
  menu.addSeparator();
  QAction *removeAction = menu.addAction("Remove");
  menu.addSeparator();

  QAction *runFileAction = nullptr;
  QAction *debugFileAction = nullptr;
  if (!fileInfo.isDir()) {
    runFileAction = menu.addAction("Run File");
    debugFileAction = menu.addAction("Debug File");
    menu.addSeparator();
  }

  TestFileClassifier &classifier = TestFileClassifier::instance();
  bool currentlyTest = fileInfo.isDir() ? classifier.isTestDirectory(filePath)
                                        : classifier.isTestFile(filePath);

  QAction *runTestAction = menu.addAction(
      fileInfo.isDir() ? "Run Tests in Directory" : "Run as Test");

  QAction *markTestAction = nullptr;
  if (!fileInfo.isDir()) {
    QString markLabel =
        currentlyTest ? "Unmark as Test File" : "Mark as Test File";
    markTestAction = menu.addAction(markLabel);
  }

  menu.addSeparator();
  QAction *copyPathAction = menu.addAction("Copy Absolute Path");

  QAction *selected = menu.exec(mapToGlobal(pos));

  if (selected) {
    if (selected == newFileAction) {
      fileController->handleNewFile(parentPath);
    } else if (selected == newDirAction) {
      fileController->handleNewDirectory(parentPath);
    } else if (selected == duplicateAction) {
      fileController->handleDuplicate(filePath);
    } else if (selected == renameAction) {
      fileController->handleRename(filePath);
    } else if (selected == copyAction) {
      fileController->handleCopy(filePath);
    } else if (selected == cutAction) {
      fileController->handleCut(filePath);
    } else if (selected == pasteAction) {
      fileController->handlePaste(parentPath);
    } else if (selected == removeAction) {
      fileController->handleRemove(filePath);
    } else if (selected == runFileAction) {
      emit runFileRequested(filePath);
    } else if (selected == debugFileAction) {
      emit debugFileRequested(filePath);
    } else if (selected == runTestAction) {
      emit runTestsRequested(filePath);
    } else if (selected == markTestAction) {
      emit toggleTestMarkerRequested(filePath, !currentlyTest);
    } else if (selected == copyPathAction) {
      fileController->handleCopyAbsolutePath(filePath);
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
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  } else {
    QTreeView::dragMoveEvent(event);
  }
}

void LightpadTreeView::dropEvent(QDropEvent *event) {
  QPoint dropPos =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      event->position().toPoint();
#else
      event->pos();
#endif
  QModelIndex dropIndex = indexAt(dropPos);

  if (!dropIndex.isValid()) {
    event->ignore();
    return;
  }

  QString destPath = parentPage->getFilePath(dropIndex);
  QFileInfo destInfo(destPath);

  if (destInfo.isFile()) {
    destPath = destInfo.absolutePath();
  }

  if (event->mimeData()->hasUrls()) {
    QList<QUrl> urls = event->mimeData()->urls();
    bool anySuccess = false;

    for (const QUrl &url : urls) {
      QString srcPath = url.toLocalFile();
      QFileInfo srcInfo(srcPath);

      if (srcInfo.absolutePath() == destPath) {
        continue;
      }

      QString fileName = srcInfo.fileName();
      QString targetPath = destPath + QDir::separator() + fileName;

      targetPath = fileModel->addUniqueSuffix(targetPath);

      bool success = false;
      if (event->dropAction() == Qt::MoveAction) {
        success = fileModel->renameFileOrDirectory(srcPath, targetPath);
      } else if (event->dropAction() == Qt::CopyAction) {
        if (srcInfo.isFile()) {
          if (QFile::copy(srcPath, targetPath)) {
            success = true;
            emit fileModel->modelUpdated();
          }
        } else if (srcInfo.isDir()) {

          fileModel->copyToClipboard(srcPath);
          success = fileModel->pasteFromClipboard(destPath);
        }
      }

      if (success) {
        anySuccess = true;
      }
    }

    if (anySuccess) {
      parentPage->updateModel();
      event->acceptProposedAction();
    } else {
      event->ignore();
    }
  } else {
    QTreeView::dropEvent(event);
  }
}

void LightpadTreeView::renameFile(QString oldFilePath, QString newFilePath) {
  if (QFileInfo(oldFilePath).isFile()) {
    QFile(oldFilePath).rename(newFilePath);
    parentPage->updateModel();
  }
}

void LightpadTreeView::duplicateFile(QString filePath) {
  fileController->handleDuplicate(filePath);
}

void LightpadTreeView::removeFile(QString filePath) {
  fileController->handleRemove(filePath);
}

LightpadPage::LightpadPage(QWidget *parent, bool treeViewHidden)
    : QWidget(parent), mainWindow(nullptr), treeContainer(nullptr),
      treeHeader(nullptr), treeTitleLabel(nullptr), treeFilterEdit(nullptr),
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

  QObject::connect(treeView, &QAbstractItemView::clicked, this,
                   [this](const QModelIndex &index) {
                     if (!index.isValid() || !model) {
                       return;
                     }

                     treeView->setCurrentIndex(index);
                   });

  QObject::connect(
      treeView, &QAbstractItemView::doubleClicked, this,
      [this](const QModelIndex &index) { activateTreeIndex(index); });

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
    applyTheme(mainWindow->getTheme());
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

  treeView->setModel(model);
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

MainWindow *LightpadPage::getMainWindow() const { return mainWindow; }
