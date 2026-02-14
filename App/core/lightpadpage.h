#ifndef LIGHTPADPAGE_H
#define LIGHTPADPAGE_H

#include "../filetree/filedirtreecontroller.h"
#include "../filetree/filedirtreemodel.h"
#include "../filetree/gitfilesystemmodel.h"
#include "textarea.h"

#include <QFileSystemModel>
#include <QTreeView>
#include <QWidget>

class LightpadPage;
class Minimap;
class GitIntegration;

class LightpadTreeView : public QTreeView {

  Q_OBJECT

public:
  LightpadTreeView(LightpadPage *parent = nullptr);
  ~LightpadTreeView();
  void renameFile(QString oldFilePath, QString newFilePath);

protected:
  void mouseReleaseEvent(QMouseEvent *e) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  LightpadPage *parentPage;
  FileDirTreeModel *fileModel;
  FileDirTreeController *fileController;

  void duplicateFile(QString filePath);
  void removeFile(QString filePath);
  void showContextMenu(const QPoint &pos);
};

class LightpadPage : public QWidget {

  Q_OBJECT

public:
  LightpadPage(QWidget *parent = nullptr, bool treeViewHidden = true);
  ~LightpadPage(){};
  QTreeView *getTreeView();
  TextArea *getTextArea();
  Minimap *getMinimap();
  void setTreeViewVisible(bool flag);
  void setMinimapVisible(bool flag);
  bool isMinimapVisible() const;
  void setModelRootIndex(QString path);
  void setCustomContentWidget(QWidget *widget);
  void setMainWindow(MainWindow *window);
  void setSharedFileSystemModel(GitFileSystemModel *sharedModel);
  void setFilePath(QString path);
  void closeTabPage(QString path);
  void updateModel();
  QString getFilePath();
  QString getFilePath(const QModelIndex &index);

  bool hasRunTemplate() const;

  QString getAssignedTemplateId() const;

  void setProjectRootPath(const QString &path);

  QString getProjectRootPath() const;

  void setGitIntegration(GitIntegration *git);

  void setGitStatusEnabled(bool enabled);

  void refreshGitStatus();

private:
  MainWindow *mainWindow;
  QTreeView *treeView;
  TextArea *textArea;
  Minimap *minimap;
  GitFileSystemModel *model;
  bool m_ownsModel;
  GitIntegration *m_gitIntegration;
  QString filePath;
  QString projectRootPath;
};

#endif
