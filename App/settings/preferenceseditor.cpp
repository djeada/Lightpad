#include "preferenceseditor.h"
#include "../ui/mainwindow.h"
#include "../ui/popup.h"
#include "ui_preferenceseditor.h"
#include <QSignalBlocker>

PreferencesEditor::PreferencesEditor(MainWindow *parent)
    : QWidget(nullptr), ui(new Ui::PreferencesEditor), parentWindow(parent),
      popupTabWidth(nullptr) {
  ui->setupUi(this);
  ui->tabWidth->setText("Tab width: " + QString::number(parent->getTabWidth()));
  {
    QSignalBlocker blocker(ui->checkBoxAutoSave);
    ui->checkBoxAutoSave->setChecked(
        parentWindow ? parentWindow->isAutoSaveEnabled() : true);
  }
  connect(ui->checkBoxAutoSave, &QCheckBox::toggled, this,
          [this](bool checked) {
            if (parentWindow) {
              parentWindow->setAutoSaveEnabled(checked);
            }
          });
}

PreferencesEditor::~PreferencesEditor() { delete ui; }

void PreferencesEditor::setTabWidthLabel(const QString &text) {
  ui->tabWidth->setText(text);
}

void PreferencesEditor::on_tabWidth_clicked() {

  if (!popupTabWidth) {
    popupTabWidth =
        new PopupTabWidth(QStringList({"2", "4", "8"}), parentWindow);
    QPoint point = mapToGlobal(ui->tabWidth->pos());
    const QSize popupSize = popupTabWidth->sizeHint();
    popupTabWidth->setGeometry(point.x(), point.y() + ui->tabWidth->height(),
                               popupSize.width(), popupSize.height());
  }

  else if (popupTabWidth->isHidden())
    popupTabWidth->show();

  else
    popupTabWidth->hide();
}
