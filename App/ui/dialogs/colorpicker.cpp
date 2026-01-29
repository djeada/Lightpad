#include "colorpicker.h"
#include "../mainwindow.h"
#include "ui_colorpicker.h"

#include <QColorDialog>
#include <QDebug>
#include <QFontDialog>
#include <QGraphicsDropShadowEffect>

const QString buttonStyleSheet = "border-radius: 12px;";

class DropShadowEffect : public QGraphicsDropShadowEffect {
    using QGraphicsDropShadowEffect::QGraphicsDropShadowEffect;

public:
    DropShadowEffect(QWidget* parent = nullptr)
        : QGraphicsDropShadowEffect(parent)
    {
        setBlurRadius(2);
        setOffset(2, 2);
        setColor(QColor("black"));
    }
};

static const QString getFontInfo(const QFont& font)
{
    const QString fontInfo = font.toString();
    return fontInfo.split(",")[0] + " " + fontInfo.split(",")[1];
}

ColorPicker::ColorPicker(Theme theme, MainWindow* parent)
    : QDialog(nullptr)
    , ui(new Ui::ColorPicker)
    , parentWindow(parent)
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
    ui->buttonNumbers->setStyleSheet(buttonStyleSheet + "background: " + theme.numberFormat.name() + ";");

    ui->buttonFontChooser->setGraphicsEffect(new DropShadowEffect());

    colorButtons = ui->colorButtonsContainer->findChildren<QToolButton*>();

    for (auto& button : colorButtons) {

        button->setGraphicsEffect(new DropShadowEffect());

        connect(button, &QToolButton::clicked, this, [&] {
            QColor color = QColorDialog::getColor(QColor(), this);

            if (color.isValid()) {
                button->setStyleSheet(buttonStyleSheet + "background: " + color.name() + ";");

                if (parentWindow) {
                    Theme colors = parentWindow->getTheme();

                    switch (colorButtons.indexOf(button)) {

                    case 0:
                        colors.backgroundColor = color;
                        break;

                    case 1:
                        colors.foregroundColor = color;
                        break;

                    case 2:
                        colors.keywordFormat_0 = color;
                        break;

                    case 3:
                        colors.keywordFormat_1 = color;
                        break;

                    case 4:
                        colors.keywordFormat_2 = color;
                        break;

                    case 5:
                        colors.singleLineCommentFormat = color;
                        break;

                    case 6:
                        colors.functionFormat = color;
                        break;

                    case 7:
                        colors.quotationFormat = color;
                        break;

                    case 8:
                        colors.classFormat = color;
                        break;

                    case 9:
                        colors.numberFormat = color;
                        break;
                    }

                    parentWindow->setTheme(colors);
                }
            }
        });
    }

    if (parentWindow) {
        QFont font = parentWindow->getFont();
        ui->buttonFontChooser->setText(getFontInfo(font));
    }
}

ColorPicker::~ColorPicker()
{
    delete ui;
}

void ColorPicker::setParentWindow(MainWindow* window)
{
    parentWindow = window;
}

void ColorPicker::on_buttonFontChooser_clicked()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, QFont("Helvetica [Cronyx]", 10), this);

    if (ok && parentWindow) {
        parentWindow->setFont(font);
        ui->buttonFontChooser->setText(getFontInfo(font));
    }
}
