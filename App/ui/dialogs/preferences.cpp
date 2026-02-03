#include "preferences.h"
#include "../../settings/preferenceseditor.h"
#include "../panels/preferencesview.h"
#include "colorpicker.h"
#include "ui_preferences.h"
#include <QCloseEvent>

Preferences::Preferences(MainWindow *parent)
    : QDialog(nullptr), ui(new Ui::Preferences), parentWindow(parent),
      colorPicker(nullptr), preferencesView(nullptr),
      preferencesEditor(nullptr) {

  ui->setupUi(this);

  setWindowTitle("Lightpad Preferences");
  setupParent();
  show();
}

Preferences::~Preferences() { delete ui; }

void Preferences::setTabWidthLabel(const QString &text) {
  if (preferencesEditor)
    preferencesEditor->setTabWidthLabel(text);
}

void Preferences::closeEvent(QCloseEvent *event) {
  emit destroyed();
  event->accept();
}

void Preferences::on_toolButton_clicked() { close(); }

void Preferences::setupParent() {
  if (parentWindow) {
    colorPicker = new ColorPicker(parentWindow->getTheme(), parentWindow);
    preferencesView = new PreferencesView(parentWindow);
    preferencesEditor = new PreferencesEditor(parentWindow);
    ui->tabWidget->addTab(preferencesView, "View");
    ui->tabWidget->addTab(preferencesEditor, "Editor");
    ui->tabWidget->addTab(colorPicker,
                          "Font " + QString(u8"\uFF06") + " Colors");
  }
}
