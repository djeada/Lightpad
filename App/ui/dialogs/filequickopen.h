#ifndef FILEQUICKOPEN_H
#define FILEQUICKOPEN_H

#include "../../settings/theme.h"
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

class FileQuickOpen : public QDialog {
  Q_OBJECT

public:
  explicit FileQuickOpen(QWidget *parent = nullptr);
  ~FileQuickOpen();

  void setRootDirectory(const QString &path);

  void showDialog();

  void applyTheme(const Theme &theme);

signals:

  void fileSelected(const QString &filePath);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onSearchTextChanged(const QString &text);
  void onItemActivated(QListWidgetItem *item);
  void onItemClicked(QListWidgetItem *item);

private:
  void setupUI();
  void scanDirectory();
  void updateResults(const QString &query);
  int fuzzyMatch(const QString &pattern, const QString &text);
  void selectFile(int row);
  void selectNext();
  void selectPrevious();

  QLineEdit *m_searchBox;
  QListWidget *m_resultsList;
  QVBoxLayout *m_layout;
  QString m_rootPath;
  QStringList m_allFiles;
  QStringList m_filteredFiles;
};

#endif
