#ifndef FILEQUICKOPEN_H
#define FILEQUICKOPEN_H

#include "styledpopupdialog.h"
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <atomic>
#include <memory>

class QLabel;
class QTimer;

class FileQuickOpen : public StyledPopupDialog {
  Q_OBJECT

public:
  explicit FileQuickOpen(QWidget *parent = nullptr);
  ~FileQuickOpen();
  using StyledPopupDialog::applyTheme;

  void setRootDirectory(const QString &path);

  void showDialog();

  void applyTheme(const Theme &theme) override;

signals:

  void fileSelected(const QString &filePath);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;
  void hideEvent(QHideEvent *event) override;

private slots:
  void onSearchTextChanged(const QString &text);
  void onItemActivated(QListWidgetItem *item);
  void onItemClicked(QListWidgetItem *item);

private:
  void setupUI();
  void updateResults(const QString &query);
  void runSearch();
  void selectFile(int row);
  void selectNext();
  void selectPrevious();

  QLineEdit *m_searchBox;
  QListWidget *m_resultsList;
  QVBoxLayout *m_layout;
  QString m_rootPath;
  QStringList m_filteredFiles;
  QLabel *m_status;
  QTimer *m_searchTimer;
  std::shared_ptr<std::atomic_bool> m_cancelled;
  int m_request = 0;
};

#endif
