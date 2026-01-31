#ifndef LIGHTPADPAGE_H
#define LIGHTPADPAGE_H

#include "textarea.h"
#include "../filetree/filedirtreemodel.h"
#include "../filetree/filedirtreecontroller.h"
#include "../filetree/gitfilesystemmodel.h"

#include <QFileSystemModel>
#include <QTreeView>
#include <QWidget>

class LightpadPage;
class Minimap;
class GitIntegration;

class LightpadTreeView : public QTreeView {

    Q_OBJECT

public:
    LightpadTreeView(LightpadPage* parent = nullptr);
    ~LightpadTreeView();
    void renameFile(QString oldFilePath, QString newFilePath);

protected:
    void mouseReleaseEvent(QMouseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    LightpadPage* parentPage;
    FileDirTreeModel* fileModel;
    FileDirTreeController* fileController;
    
    void duplicateFile(QString filePath);
    void removeFile(QString filePath);
    void showContextMenu(const QPoint& pos);
};

class LightpadPage : public QWidget {

    Q_OBJECT

public:
    LightpadPage(QWidget* parent = nullptr, bool treeViewHidden = true);
    ~LightpadPage(){};
    QTreeView* getTreeView();
    TextArea* getTextArea();
    Minimap* getMinimap();
    void setTreeViewVisible(bool flag);
    void setMinimapVisible(bool flag);
    bool isMinimapVisible() const;
    void setModelRootIndex(QString path);
    void setMainWindow(MainWindow* window);
    void setFilePath(QString path);
    void closeTabPage(QString path);
    void updateModel();
    QString getFilePath();
    QString getFilePath(const QModelIndex& index);
    
    /**
     * @brief Check if a run template is assigned to the current file
     * @return true if a template is assigned or can be auto-detected
     */
    bool hasRunTemplate() const;
    
    /**
     * @brief Get the assigned template ID for the current file
     * @return Template ID or empty string if none
     */
    QString getAssignedTemplateId() const;
    
    /**
     * @brief Set the project root path for persistent treeview
     * @param path The project root directory path
     */
    void setProjectRootPath(const QString& path);
    
    /**
     * @brief Get the project root path
     * @return The project root path or empty string if not set
     */
    QString getProjectRootPath() const;

    /**
     * @brief Set the git integration instance for displaying git status
     */
    void setGitIntegration(GitIntegration* git);

    /**
     * @brief Enable or disable git status display in file tree
     */
    void setGitStatusEnabled(bool enabled);

    /**
     * @brief Refresh git status display
     */
    void refreshGitStatus();

private:
    MainWindow* mainWindow;
    QTreeView* treeView;
    TextArea* textArea;
    Minimap* minimap;
    GitFileSystemModel* model;
    GitIntegration* m_gitIntegration;
    QString filePath;
    QString projectRootPath;
};

#endif // LIGHTPADPAGE_H
