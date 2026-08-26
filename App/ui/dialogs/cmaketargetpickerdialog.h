#ifndef CMAKETARGETPICKERDIALOG_H
#define CMAKETARGETPICKERDIALOG_H

#include <QDialog>

class QListWidget;
struct CMakeTargetInfo;

class CMakeTargetPickerDialog : public QDialog {
  Q_OBJECT

public:
  explicit CMakeTargetPickerDialog(const QList<CMakeTargetInfo> &targets,
                                   QWidget *parent = nullptr);

  QString selectedTargetName() const;

private:
  QListWidget *m_targetList;
};

#endif
