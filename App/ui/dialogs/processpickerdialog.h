#ifndef PROCESSPICKERDIALOG_H
#define PROCESSPICKERDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class ProcessPickerDialog : public QDialog {
  Q_OBJECT

public:
  struct ProcessEntry {
    qint64 pid = 0;
    QString name;
    QString commandLine;
  };

  explicit ProcessPickerDialog(QWidget *parent = nullptr);

  qint64 selectedPid() const { return m_selectedPid; }

  static QList<ProcessEntry> listRunningProcesses();

private slots:
  void refresh();
  void applyFilter(const QString &text);
  void onSelectionChanged();
  void onProcessDoubleClicked(QTreeWidgetItem *item, int column);

private:
  void setupUi();

  QTreeWidget *m_processTree;
  QLineEdit *m_filterInput;
  QLabel *m_countLabel;
  QList<ProcessEntry> m_processes;
  qint64 m_selectedPid;
};

#endif
