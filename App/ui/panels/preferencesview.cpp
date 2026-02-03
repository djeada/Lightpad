#include "preferencesview.h"
#include "../mainwindow.h"
#include "ui_preferencesview.h"

PreferencesView::PreferencesView(MainWindow *parent)
    : QWidget(nullptr), ui(new Ui::PreferencesView), parentWindow(parent) {
  ui->setupUi(this);

  if (parentWindow) {
    auto settings = parentWindow->getSettings();
    ui->checkBoxBracket->setChecked(settings.matchingBracketsHighlighted);
    ui->checkBoxCurrentLine->setChecked(settings.lineHighlighted);
    ui->checkBoxLineNumbers->setChecked(settings.showLineNumberArea);
  }
}

PreferencesView::~PreferencesView() { delete ui; }

void PreferencesView::on_checkBoxLineNumbers_clicked(bool checked) {
  if (parentWindow)
    parentWindow->showLineNumbers(checked);
}

void PreferencesView::on_checkBoxCurrentLine_clicked(bool checked) {
  if (parentWindow)
    parentWindow->highlihtCurrentLine(checked);
}

void PreferencesView::on_checkBoxBracket_clicked(bool checked) {
  if (parentWindow)
    parentWindow->highlihtMatchingBracket(checked);
}
