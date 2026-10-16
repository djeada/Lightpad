#ifndef LIGHTPADTREEVIEW_H
#define LIGHTPADTREEVIEW_H

#include "filedirtreemodel.h"
#include <QTreeView>

class LightpadPage;
class FileDirTreeController;
class ThemeDefinition;
class QMenu;

class LightpadTreeView : public QTreeView {

  Q_OBJECT

public:
  LightpadTreeView(LightpadPage *parent = nullptr);
  ~LightpadTreeView();

  QStringList selectedPaths() const;

  QString targetDirectory() const;

  QString rootPath() const;

  FileDirTreeController *fileOperationsController() const {
    return fileController;
  }

  void promptNewFile();
  void promptNewFolder();
  void applyTheme(const ThemeDefinition &theme);
  void showPathContextMenu(const QString &path, const QPoint &globalPos);

signals:
  void runTestsRequested(const QString &path);
  void runFileRequested(const QString &path);
  void debugFileRequested(const QString &path);
  void toggleTestMarkerRequested(const QString &path, bool markAsTest);
  void revealInFileManagerRequested(const QString &path);
  void openInTerminalRequested(const QString &directory);
  void findInFolderRequested(const QString &directory);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  void startDrag(Qt::DropActions supportedActions) override;
  void paintEvent(QPaintEvent *event) override;

private:
  LightpadPage *parentPage;
  FileDirTreeModel *fileModel;
  FileDirTreeController *fileController;
  QString m_dropDestination;
  bool m_dropCopy = true;

  void setupShortcuts();
  void showContextMenu(const QPoint &pos);
  void buildRootMenu(QMenu &menu);
  void buildEntryMenu(QMenu &menu, const QString &filePath,
                      const QStringList &selection);

  QString dropDirectoryAt(const QPoint &position) const;
  bool acceptsDropOf(const QStringList &sources,
                     const QString &destination) const;
  bool performDrop(const QStringList &sources, const QString &destination,
                   bool copy);
  void copySelection();
  void cutSelection();
  void pasteIntoTarget();
  void renameSelection();
  void duplicateSelection();
  void deleteSelection(DeleteMode mode);
};

#endif
