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
class QLabel;
class QLineEdit;
class QToolButton;
struct Theme;
class ThemeDefinition;

class LightpadTreeView : public QTreeView {

  Q_OBJECT

public:
  LightpadTreeView(LightpadPage *parent = nullptr);
  ~LightpadTreeView();
  void renameFile(QString oldFilePath, QString newFilePath);

signals:
  void runTestsRequested(const QString &path);
  void runFileRequested(const QString &path);
  void debugFileRequested(const QString &path);
  void toggleTestMarkerRequested(const QString &path, bool markAsTest);

protected:
  void keyPressEvent(QKeyEvent *event) override;
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
  void setTreeFilterText(const QString &text);
  QString getTreeFilterText() const;
  void activateTreeIndex(const QModelIndex &index);
  void applyTheme(const Theme &theme);
  void applyTheme(const ThemeDefinition &theme);
  MainWindow *getMainWindow() const;

private:
  void applyTreeFilter();
  bool updateTreeVisibilityRecursive(const QModelIndex &parent,
                                     const QString &needle);

  MainWindow *mainWindow;
  QWidget *treeContainer;
  QWidget *treeHeader;
  QLabel *treeTitleLabel;
  QLineEdit *treeFilterEdit;
  QToolButton *treeRefreshButton;
  QToolButton *treeCollapseButton;
  QToolButton *treeExpandButton;
  QTreeView *treeView;
  TextArea *textArea;
  Minimap *minimap;
  GitFileSystemModel *model;
  bool m_ownsModel;
  GitIntegration *m_gitIntegration;
  QString m_treeFilterText;
  QString filePath;
  QString projectRootPath;
};

#endif
