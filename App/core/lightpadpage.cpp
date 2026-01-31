#include "lightpadpage.h"
#include "../ui/mainwindow.h"
#include "../ui/panels/minimap.h"
#include "../run_templates/runtemplatemanager.h"
#include "../git/gitintegration.h"
#include <QDebug>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>

class LineEdit : public QLineEdit {

    using QLineEdit::QLineEdit;

public:
    LineEdit(QRect rect, QString filePath, QWidget* parent)
        : QLineEdit(parent)
        , oldFilePath(filePath)
    {
        show();
        setGeometry(QRect(rect.x(), rect.y() + rect.height() + 1, 1.1 * rect.width(), 1.1 * rect.height()));
        setFocus(Qt::MouseFocusReason);
    }

protected:
    void focusOutEvent(QFocusEvent* event) override
    {
        Q_UNUSED(event);

        LightpadTreeView* treeView = qobject_cast<LightpadTreeView*>(parentWidget());

        if (treeView)
            treeView->renameFile(oldFilePath, QFileInfo(oldFilePath).absoluteDir().path() + QDir::separator() + text());

        close();
    }

    void keyPressEvent(QKeyEvent* event) override
    {

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

    void renameTreeViewEntry()
    {
        LightpadTreeView* treeView = qobject_cast<LightpadTreeView*>(parentWidget());

        if (treeView)
            treeView->renameFile(oldFilePath, QFileInfo(oldFilePath).absoluteDir().path() + QDir::separator() + text());
    }
};

LightpadTreeView::LightpadTreeView(LightpadPage* parent)
    : QTreeView(parent)
    , parentPage(parent)
    , fileModel(new FileDirTreeModel(this))
    , fileController(new FileDirTreeController(fileModel, this))
{
    // Enable drag and drop
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    
    // Connect signals
    connect(fileController, &FileDirTreeController::actionCompleted, 
            parentPage, &LightpadPage::updateModel);
    connect(fileController, &FileDirTreeController::fileRemoved,
            parentPage, &LightpadPage::closeTabPage);
}

LightpadTreeView::~LightpadTreeView()
{
    // Qt parent-child relationship will handle cleanup automatically
}

void LightpadTreeView::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::RightButton) {
        showContextMenu(e->pos());
    } else {
        QTreeView::mouseReleaseEvent(e);
    }
}

void LightpadTreeView::showContextMenu(const QPoint& pos)
{
    QModelIndex idx = indexAt(pos);
    if (!idx.isValid()) {
        return;
    }

    QString filePath = parentPage->getFilePath(idx);
    QFileInfo fileInfo(filePath);
    QString parentPath = fileInfo.isDir() ? filePath : fileInfo.absolutePath();

    QMenu menu;
    
    // Add context menu actions
    QAction* newFileAction = menu.addAction("New File");
    QAction* newDirAction = menu.addAction("New Directory");
    menu.addSeparator();
    QAction* duplicateAction = menu.addAction("Duplicate");
    QAction* renameAction = menu.addAction("Rename");
    menu.addSeparator();
    QAction* copyAction = menu.addAction("Copy");
    QAction* cutAction = menu.addAction("Cut");
    QAction* pasteAction = menu.addAction("Paste");
    menu.addSeparator();
    QAction* removeAction = menu.addAction("Remove");
    menu.addSeparator();
    QAction* copyPathAction = menu.addAction("Copy Absolute Path");

    QAction* selected = menu.exec(mapToGlobal(pos));

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
        } else if (selected == copyPathAction) {
            fileController->handleCopyAbsolutePath(filePath);
        }
    }
}

void LightpadTreeView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QTreeView::dragEnterEvent(event);
    }
}

void LightpadTreeView::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QTreeView::dragMoveEvent(event);
    }
}

void LightpadTreeView::dropEvent(QDropEvent* event)
{
    QModelIndex dropIndex = indexAt(event->pos());
    
    if (!dropIndex.isValid()) {
        event->ignore();
        return;
    }
    
    QString destPath = parentPage->getFilePath(dropIndex);
    QFileInfo destInfo(destPath);
    
    // If dropping on a file, use its parent directory
    if (destInfo.isFile()) {
        destPath = destInfo.absolutePath();
    }
    
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        bool anySuccess = false;
        
        for (const QUrl& url : urls) {
            QString srcPath = url.toLocalFile();
            QFileInfo srcInfo(srcPath);
            
            // Check if source and destination are the same
            if (srcInfo.absolutePath() == destPath) {
                continue;  // Skip if dropping in same directory
            }
            
            QString fileName = srcInfo.fileName();
            QString targetPath = destPath + QDir::separator() + fileName;
            
            // Use the model to handle the move with unique suffix
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
                    // For directory copy, we need to temporarily use the clipboard
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

void LightpadTreeView::renameFile(QString oldFilePath, QString newFilePath)
{
    if (QFileInfo(oldFilePath).isFile()) {
        QFile(oldFilePath).rename(newFilePath);
        parentPage->updateModel();
    }
}

void LightpadTreeView::duplicateFile(QString filePath)
{
    fileController->handleDuplicate(filePath);
}

void LightpadTreeView::removeFile(QString filePath)
{
    fileController->handleRemove(filePath);
}

LightpadPage::LightpadPage(QWidget* parent, bool treeViewHidden)
    : QWidget(parent)
    , mainWindow(nullptr)
    , treeView(nullptr)
    , textArea(nullptr)
    , minimap(nullptr)
    , model(nullptr)
    , m_gitIntegration(nullptr)
    , filePath("")
    , projectRootPath("")
{

    auto* layoutHor = new QHBoxLayout(this);

    treeView = new LightpadTreeView(this);
    textArea = new TextArea(this);
    minimap = new Minimap(this);

    // Connect minimap to text area
    minimap->setSourceEditor(textArea);

    layoutHor->addWidget(treeView);
    layoutHor->addWidget(textArea);
    layoutHor->addWidget(minimap);

    if (treeViewHidden)
        treeView->hide();

    layoutHor->setContentsMargins(0, 0, 0, 0);
    layoutHor->setStretch(0, 0);
    layoutHor->setStretch(1, 1);
    layoutHor->setStretch(2, 0);

    setLayout(layoutHor);
    updateModel();

    QObject::connect(treeView, &QAbstractItemView::clicked, this, [&](const QModelIndex& index) {
        if (!index.isValid() || !mainWindow) {
            return;
        }

        if (model->isDir(index)) {
            treeView->setExpanded(index, !treeView->isExpanded(index));
            treeView->setCurrentIndex(index);
            return;
        }

        QString path = model->filePath(index);
        mainWindow->openFileAndAddToNewTab(path);
        treeView->clearSelection();
        treeView->setCurrentIndex(index);
    });
}

QTreeView* LightpadPage::getTreeView()
{
    return treeView;
}

TextArea* LightpadPage::getTextArea()
{
    return textArea;
}

Minimap* LightpadPage::getMinimap()
{
    return minimap;
}

void LightpadPage::setTreeViewVisible(bool flag)
{
    treeView->setVisible(flag);
}

void LightpadPage::setMinimapVisible(bool flag)
{
    if (minimap) {
        minimap->setMinimapVisible(flag);
    }
}

bool LightpadPage::isMinimapVisible() const
{
    return minimap ? minimap->isMinimapVisible() : false;
}

void LightpadPage::setModelRootIndex(QString path)
{
    treeView->setRootIndex(model->index(path));
}

void LightpadPage::setMainWindow(MainWindow* window)
{

    mainWindow = window;

    if (textArea) {
        textArea->setMainWindow(mainWindow);
        textArea->setFontSize(mainWindow->getFontSize());
        textArea->setTabWidth(mainWindow->getTabWidth());
    }
}

void LightpadPage::setFilePath(QString path)
{
    filePath = path;
    
    // Visibility is controlled by project root path now
    // Only show treeview if project root is set
    if (!path.isEmpty() && !projectRootPath.isEmpty()) {
        setTreeViewVisible(true);
    }
}

void LightpadPage::closeTabPage(QString path)
{
    if (mainWindow)
        mainWindow->closeTabPage(path);
}

void LightpadPage::updateModel()
{
    // Preserve the current root path if one is set
    QString currentRootPath = projectRootPath.isEmpty() ? QDir::home().path() : projectRootPath;
    
    model = new GitFileSystemModel(this);
    model->setRootPath(currentRootPath);
    
    // Set git integration if available
    if (m_gitIntegration) {
        model->setGitIntegration(m_gitIntegration);
    }
    
    treeView->setModel(model);

    treeView->setColumnHidden(1, true);
    treeView->setColumnHidden(2, true);
    treeView->setColumnHidden(3, true);
    
    // Restore the root index if project root is set
    if (!projectRootPath.isEmpty()) {
        treeView->setRootIndex(model->index(projectRootPath));
    }

    treeView->setHeaderHidden(true);
}

QString LightpadPage::getFilePath()
{
    return filePath;
}

QString LightpadPage::getFilePath(const QModelIndex& index)
{
    return model->filePath(index);
}

bool LightpadPage::hasRunTemplate() const
{
    if (filePath.isEmpty()) {
        return false;
    }
    
    RunTemplateManager& manager = RunTemplateManager::instance();
    
    // Check if there's an explicit assignment
    FileTemplateAssignment assignment = manager.getAssignmentForFile(filePath);
    if (!assignment.templateId.isEmpty()) {
        return true;
    }
    
    // Check if there's a default template for the file extension
    QFileInfo fileInfo(filePath);
    QList<RunTemplate> templates = manager.getTemplatesForExtension(fileInfo.suffix());
    return !templates.isEmpty();
}

QString LightpadPage::getAssignedTemplateId() const
{
    if (filePath.isEmpty()) {
        return QString();
    }
    
    RunTemplateManager& manager = RunTemplateManager::instance();
    FileTemplateAssignment assignment = manager.getAssignmentForFile(filePath);
    
    if (!assignment.templateId.isEmpty()) {
        return assignment.templateId;
    }
    
    // Return default template ID based on extension
    QFileInfo fileInfo(filePath);
    QList<RunTemplate> templates = manager.getTemplatesForExtension(fileInfo.suffix());
    if (!templates.isEmpty()) {
        return templates.first().id;
    }
    
    return QString();
}

void LightpadPage::setProjectRootPath(const QString& path)
{
    projectRootPath = path;
}

QString LightpadPage::getProjectRootPath() const
{
    return projectRootPath;
}

void LightpadPage::setGitIntegration(GitIntegration* git)
{
    m_gitIntegration = git;
    if (model) {
        model->setGitIntegration(git);
    }
}

void LightpadPage::setGitStatusEnabled(bool enabled)
{
    if (model) {
        model->setGitStatusEnabled(enabled);
    }
}

void LightpadPage::refreshGitStatus()
{
    if (model) {
        model->refreshGitStatus();
    }
}
