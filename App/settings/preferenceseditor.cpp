#include "preferenceseditor.h"
#include "../ui/mainwindow.h"
#include "../ui/popup.h"
#include "ui_preferenceseditor.h"

PreferencesEditor::PreferencesEditor(MainWindow *parent)
    : QWidget(nullptr), ui(new Ui::PreferencesEditor), parentWindow(parent),
      popupTabWidth(nullptr) {
  ui->setupUi(this);
  ui->tabWidth->setText("Tab width: " + QString::number(parent->getTabWidth()));
}

PreferencesEditor::~PreferencesEditor() { delete ui; }

void PreferencesEditor::setTabWidthLabel(const QString &text) {
  ui->tabWidth->setText(text);
}

void PreferencesEditor::on_tabWidth_clicked() {

  if (!popupTabWidth) {
    Popup *popupTabWidth =
        new PopupTabWidth(QStringList({"2", "4", "8"}), parentWindow);
    QPoint point = mapToGlobal(ui->tabWidth->pos());
    popupTabWidth->setGeometry(point.x(), point.y() + ui->tabWidth->height(),
                               popupTabWidth->width(), popupTabWidth->height());
  }

  else if (popupTabWidth->isHidden())
    popupTabWidth->show();

  else
    popupTabWidth->hide();
}
