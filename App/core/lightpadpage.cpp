#include "lightpadpage.h"
#include "../filetree/explorersearchwidget.h"
#include "../git/gitintegration.h"
#include "../run_templates/runtemplatemanager.h"
#include "../theme/colorcontrast.h"
#include "../theme/themeengine.h"
#include "../ui/mainwindow.h"
#include "../ui/panels/minimap.h"
#include "../ui/uistylehelper.h"
#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGlobal>
#include <functional>

namespace {

class LabelResizeNotifier : public QObject {
public:
  LabelResizeNotifier(QWidget *target, std::function<void()> onResize)
      : QObject(target), m_target(target), m_onResize(std::move(onResize)) {}

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched == m_target && event->type() == QEvent::Resize && m_onResize) {
      m_onResize();
    }
    return QObject::eventFilter(watched, event);
  }

private:
  QWidget *m_target;
  std::function<void()> m_onResize;
};
} // namespace

LightpadPage::LightpadPage(QWidget *parent, bool treeViewHidden)
    : QWidget(parent), mainWindow(nullptr), treeContainer(nullptr),
      treeHeader(nullptr), treeTitleLabel(nullptr), treePathLabel(nullptr),
      treeSearch(nullptr), treeNewFileButton(nullptr),
      treeNewFolderButton(nullptr), treeMoreButton(nullptr), treeView(nullptr),
      editorContainer(nullptr), textArea(nullptr), minimap(nullptr),
      model(nullptr), m_ownsModel(true), m_gitIntegration(nullptr),
      m_treeFilterText(""), filePath(""), projectRootPath("") {

  auto *layoutHor = new QHBoxLayout(this);
  layoutHor->setContentsMargins(0, 0, 0, 0);

  treeContainer = new QWidget(this);
  treeContainer->setObjectName("treeContainer");
  treeContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  treeContainer->setMinimumWidth(240);
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
  treeTitleLabel->setTextFormat(Qt::PlainText);
  treeTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  treeHeaderLayout->addWidget(treeTitleLabel, 1);

  treeNewFileButton = new QToolButton(treeHeader);
  treeNewFileButton->setObjectName("treeToolButton");
  treeNewFileButton->setToolTip(tr("New file (Ctrl+Alt+N)"));
  treeNewFileButton->setAccessibleName(tr("New file"));
  treeNewFileButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
  treeNewFileButton->setIconSize(QSize(14, 14));
  treeHeaderLayout->addWidget(treeNewFileButton);

  treeNewFolderButton = new QToolButton(treeHeader);
  treeNewFolderButton->setObjectName("treeToolButton");
  treeNewFolderButton->setToolTip(tr("New folder (Ctrl+Shift+N)"));
  treeNewFolderButton->setAccessibleName(tr("New folder"));
  treeNewFolderButton->setIcon(
      style()->standardIcon(QStyle::SP_FileDialogNewFolder));
  treeNewFolderButton->setIconSize(QSize(14, 14));
  treeHeaderLayout->addWidget(treeNewFolderButton);

  treeMoreButton = new QToolButton(treeHeader);
  treeMoreButton->setObjectName("treeToolButton");
  treeMoreButton->setText(QString(QChar(0x22ef)));
  treeMoreButton->setToolTip(tr("Explorer actions"));
  treeMoreButton->setAccessibleName(tr("Explorer actions"));
  treeMoreButton->setPopupMode(QToolButton::InstantPopup);
  auto *treeMenu = new QMenu(treeMoreButton);
  treeMoreButton->setMenu(treeMenu);
  treeHeaderLayout->addWidget(treeMoreButton);

  treePathLabel = new QLabel(treeContainer);
  treePathLabel->setObjectName("treePathLabel");
  treePathLabel->setTextFormat(Qt::PlainText);
  treePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  treePathLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  treePathLabel->hide();
  treePathLabel->installEventFilter(new LabelResizeNotifier(
      treePathLabel, [this]() { updateTreePathLabel(); }));

  treeSearch = new ExplorerSearchWidget(treeContainer);

  treeView = new LightpadTreeView(this);
  treeView->setExpandsOnDoubleClick(false);
  treeView->setObjectName("fileTreeView");
  textArea = new TextArea(this);
  minimap = new Minimap(this);

  minimap->setSourceEditor(textArea);

  treeLayout->addWidget(treeHeader);
  treeLayout->addWidget(treePathLabel);
  treeLayout->addWidget(treeSearch);
  treeLayout->addWidget(treeView, 1);

  editorContainer = new QWidget(this);
  auto *editorLayout = new QHBoxLayout(editorContainer);
  editorLayout->setContentsMargins(0, 0, 0, 0);
  editorLayout->setSpacing(0);
  editorLayout->addWidget(textArea, 1);
  editorLayout->addWidget(minimap);
  auto *splitter = new QSplitter(Qt::Horizontal, this);
  m_explorerSplitter = splitter;
  splitter->setObjectName("explorerSplitter");
  splitter->setChildrenCollapsible(false);
  splitter->addWidget(treeContainer);
  splitter->addWidget(editorContainer);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({300, 900});
  layoutHor->addWidget(splitter);
  connect(splitter, &QSplitter::splitterMoved, this, [this]() {
    if (mainWindow && treeContainer->isVisible()) {
      mainWindow->setExplorerWidth(m_explorerSplitter->sizes().first());
    }
  });

  if (treeViewHidden) {
    treeContainer->hide();
  }

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
  connect(treeMenu->addAction(tr("Reveal Active File")), &QAction::triggered,
          this, [this]() { revealPath(filePath); });
  treeMenu->addSeparator();
  connect(treeMenu->addAction(tr("Refresh")), &QAction::triggered, this,
          [this]() {
            updateModel();
            refreshGitStatus();
            treeSearch->refresh();
          });
  connect(treeMenu->addAction(tr("Collapse All")), &QAction::triggered,
          treeView, &QTreeView::collapseAll);
  connect(treeMenu->addAction(tr("Expand One Level")), &QAction::triggered,
          this, [this]() { treeView->expandToDepth(1); });
  treeMenu->addSeparator();
  connect(
      treeMenu->addAction(tr("Find Files…")), &QAction::triggered, this,
      [this]() { treeSearch->focusSearch(ProjectSearch::Mode::FileNames); });
  connect(treeMenu->addAction(tr("Search Contents…")), &QAction::triggered,
          this,
          [this]() { treeSearch->focusSearch(ProjectSearch::Mode::Contents); });
  connect(treeSearch, &ExplorerSearchWidget::optionsChanged, this, [this]() {
    m_treeFilterText = treeSearch->options().query;
    if (mainWindow) {
      mainWindow->setTreeSearchOptions(treeSearch->options());
    }
  });
  connect(treeSearch, &ExplorerSearchWidget::searchActiveChanged, this,
          [this](bool active) {
            treeView->setVisible(!active);
            auto *layout = qobject_cast<QVBoxLayout *>(treeContainer->layout());
            layout->setStretchFactor(treeSearch, active ? 1 : 0);
          });
  connect(treeSearch, &ExplorerSearchWidget::revealRequested, this,
          &LightpadPage::revealPath);
  auto *view = qobject_cast<LightpadTreeView *>(treeView);
  connect(treeSearch, &ExplorerSearchWidget::entryContextMenuRequested, view,
          &LightpadTreeView::showPathContextMenu);
  connect(view, &LightpadTreeView::findInFolderRequested, treeSearch,
          &ExplorerSearchWidget::searchInFolder);
  connect(view->fileOperationsController(),
          &FileDirTreeController::actionCompleted, treeSearch,
          &ExplorerSearchWidget::refresh);
  connect(
      treeSearch, &ExplorerSearchWidget::navigateRequested, this,
      [this](const QString &path, int line, int column, int length) {
        if (!mainWindow)
          return;
        mainWindow->openFileAndAddToNewTab(path);
        TextArea *area = mainWindow->getCurrentTextArea();
        if (area && line > 0) {
          const QTextBlock block =
              area->document()->findBlockByNumber(line - 1);
          if (block.isValid()) {
            QTextCursor cursor(block);
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                qMin(qMax(0, column - 1), block.length() - 1));
            cursor.movePosition(
                QTextCursor::Right, QTextCursor::KeepAnchor,
                qMin(length, block.length() - 1 - cursor.positionInBlock()));
            area->setTextCursor(cursor);
            area->centerCursor();
          }
        }
        if (area)
          area->setFocus();
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
  treeSearch->setSearchText(text);
}

QString LightpadPage::getTreeFilterText() const { return m_treeFilterText; }

void LightpadPage::setTreeSearchOptions(const ProjectSearch::Options &options) {
  m_treeFilterText = options.query;
  auto localOptions = options;
  localOptions.rootPath = projectRootPath;
  treeSearch->setOptions(localOptions);
}

void LightpadPage::setExplorerWidth(int width) {
  m_explorerSplitter->setSizes(
      {width, qMax(240, m_explorerSplitter->width() - width -
                            m_explorerSplitter->handleWidth())});
}

void LightpadPage::setModelRootIndex(QString path) {
  treeView->setRootIndex(model->index(path));
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
  if (!projectRootPath.isEmpty() &&
      !FileDirTreeModel::isInside(projectRootPath, path)) {
    return;
  }
  if (!m_treeFilterText.isEmpty()) {
    if (mainWindow) {
      mainWindow->setTreeFilterText({});
    } else {
      setTreeFilterText({});
    }
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

  auto *layout = qobject_cast<QHBoxLayout *>(editorContainer->layout());
  if (!layout)
    return;

  layout->replaceWidget(textArea, widget);
  textArea->setVisible(false);
  widget->setParent(editorContainer);
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
    setTreeSearchOptions(mainWindow->getTreeSearchOptions());
    setExplorerWidth(mainWindow->getExplorerWidth());
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
  treeView->setColumnHidden(1, true);
  treeView->setColumnHidden(2, true);
  treeView->setColumnHidden(3, true);

  if (!projectRootPath.isEmpty()) {
    treeView->setRootIndex(model->index(projectRootPath));
  } else {
    treeView->setRootIndex(QModelIndex());
  }

  treeView->setHeaderHidden(true);
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

void LightpadPage::updateTreePathLabel() {
  if (!treePathLabel) {
    return;
  }

  if (projectRootPath.isEmpty()) {
    treePathLabel->clear();
    treePathLabel->hide();
    return;
  }

  const QString absolutePath =
      QDir::toNativeSeparators(QFileInfo(projectRootPath).absoluteFilePath());
  treePathLabel->setToolTip(absolutePath);

  const int available = qMax(1, treePathLabel->contentsRect().width());
  treePathLabel->setText(treePathLabel->fontMetrics().elidedText(
      absolutePath, Qt::ElideLeft, available));
  treePathLabel->show();
}

void LightpadPage::setProjectRootPath(const QString &path) {
  projectRootPath = path;
  treeSearch->setRootPath(path);
  if (treeTitleLabel) {
    const QString name = QFileInfo(path).fileName();
    treeTitleLabel->setText(tr("EXPLORER"));
    treeTitleLabel->setToolTip(name.isEmpty() ? path : name);
  }
  updateTreePathLabel();
  if (model) {
    model->setRootHeaderLabel(projectRootPath);
    treeView->setHeaderHidden(true);
    if (projectRootPath.isEmpty()) {
      treeView->setRootIndex(QModelIndex());
    } else {
      treeView->setRootIndex(model->index(projectRootPath));
    }
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
  applyTheme(ThemeDefinition::fromClassicTheme(theme, QStringLiteral("Page")));
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
                "  background: transparent;"
                "  color: %4;"
                "  font-size: 10px;"
                "  font-weight: 700;"
                "  letter-spacing: 1px;"
                "  padding: 2px 0;"
                "}"
                "QLabel#treePathLabel {"
                "  background: transparent;"
                "  color: %4;"
                "  font-size: 10px;"
                "  padding: 0 4px 4px 4px;"
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

  if (minimap) {
    minimap->setThemeColors(theme.colors.editorBg, theme.colors.editorFg,
                            theme.colors.accentPrimary);
  }

  if (model) {
    const ThemeColors &c = theme.colors;
    const QColor base = ColorContrast::flatten(c.surfaceRaised, c.surfaceBase);
    auto readable = [&](const QColor &color) {
      return ColorContrast::ensureOnAll(color, {base, c.surfaceBase},
                                        ColorContrast::SecondaryTextRatio);
    };
    GitFileSystemModel::StatusColors colors;
    colors.modified = readable(c.gitModified);
    colors.staged = readable(c.gitAdded);
    colors.untracked = readable(c.gitUntracked);
    colors.added = readable(c.gitAdded);
    colors.deleted = readable(c.gitDeleted);
    colors.conflict = readable(c.gitConflicted);
    model->setStatusColors(colors);
  }

  treeSearch->applyTheme(theme);
  if (treeMoreButton && treeMoreButton->menu()) {
    treeMoreButton->menu()->setStyleSheet(
        UIStyleHelper::contextMenuStyle(theme));
  }
  if (auto *view = qobject_cast<LightpadTreeView *>(treeView)) {
    view->applyTheme(theme);
  }
}

MainWindow *LightpadPage::getMainWindow() const { return mainWindow; }
