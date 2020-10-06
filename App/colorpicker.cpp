#include "colorpicker.h"
#include "ui_colorpicker.h"

#include <QFontDialog>

const QString buttonStyleSheet = "border-radius: 13px;";

ColorPicker::ColorPicker(Theme theme, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ColorPicker)
{
    setWindowFlag(Qt::Popup);
    ui->setupUi(this);

    ui->buttonBackground->setStyleSheet(buttonStyleSheet + "background: " + theme.backgroundColor.name() + ";");
    ui->buttonClases->setStyleSheet(buttonStyleSheet + "background: " + theme.classFormat.name() + ";");
    ui->buttonComments->setStyleSheet(buttonStyleSheet + "background: " + theme.singleLineCommentFormat.name() + ";");
    ui->buttonQuotations->setStyleSheet(buttonStyleSheet + "background: " + theme.quotationFormat.name() + ";");
    ui->buttonFont->setStyleSheet(buttonStyleSheet + "background: " + theme.foregroundColor.name() + ";");
    ui->buttonFunctions->setStyleSheet(buttonStyleSheet + "background: " + theme.functionFormat.name() + ";");
    ui->buttonKeywords1->setStyleSheet(buttonStyleSheet + "background: " + theme.keywordFormat_0.name() + ";");
    ui->buttonKeywords2->setStyleSheet(buttonStyleSheet + "background: " + theme.keywordFormat_1.name() + ";");
    ui->buttonKeywords3->setStyleSheet(buttonStyleSheet + "background: " + theme.keywordFormat_2.name() + ";");
    ui->buttonKeywords3->setStyleSheet(buttonStyleSheet + "background: " + theme.numberFormat.name() + ";");

    show();
}

ColorPicker::~ColorPicker()
{
    delete ui;
}


void ColorPicker::on_buttonFontChooser_clicked()
{
    bool ok;
    QFont font = QFontDialog::getFont(
                    &ok, QFont("Helvetica [Cronyx]", 10), this);
    if (ok) {
        // the user clicked OK and font is set to the font the user selected
    } else {
        // the user canceled the dialog; font is set to the initial
        // value, in this case Helvetica [Cronyx], 10
    }
}
