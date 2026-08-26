#include "cmaketargetpickerdialog.h"

#include "build/cmakeproject.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

CMakeTargetPickerDialog::CMakeTargetPickerDialog(
    const QList<CMakeTargetInfo> &targets, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(tr("Select Debug Target"));
  setModal(true);
  resize(420, 320);

  auto *layout = new QVBoxLayout(this);

  auto *label = new QLabel(tr("This CMake project builds several executables.\n"
                              "Choose which one to build and debug:"),
                           this);
  layout->addWidget(label);

  m_targetList = new QListWidget(this);
  for (const CMakeTargetInfo &target : targets) {
    if (target.isExecutable) {
      m_targetList->addItem(target.name);
    }
  }
  m_targetList->setCurrentRow(0);
  connect(m_targetList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { accept(); });
  layout->addWidget(m_targetList);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this,
          &CMakeTargetPickerDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this,
          &CMakeTargetPickerDialog::reject);
  layout->addWidget(buttons);
}

QString CMakeTargetPickerDialog::selectedTargetName() const {
  QListWidgetItem *item = m_targetList->currentItem();
  return item ? item->text() : QString();
}
