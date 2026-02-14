#ifndef RECENTFILESDIALOG_H
#define RECENTFILESDIALOG_H

#include "../../settings/theme.h"
#include <QDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

class RecentFilesManager;

class RecentFilesDialog : public QDialog {
  Q_OBJECT

public:
  explicit RecentFilesDialog(RecentFilesManager *manager,
                             QWidget *parent = nullptr);
  ~RecentFilesDialog();

  void showDialog();

  void refresh();

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
  void updateResults(const QString &query);
  int fuzzyMatch(const QString &pattern, const QString &text);
  void selectFile(int row);
  void selectNext();
  void selectPrevious();

  RecentFilesManager *m_manager;
  QLineEdit *m_searchBox;
  QListWidget *m_resultsList;
  QVBoxLayout *m_layout;
  QStringList m_recentFiles;
  QList<int> m_filteredIndices;
};

#endif
