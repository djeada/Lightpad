#include "shortcuts.h"
#include "ui_shortcuts.h"
#include <QDebug>

QList<QList<QString>> shortcuts {
    {"Close", ""},
    {"Save", ""},
    {"SaveAs", ""},
    {"Delete", ""},
    {"Cut", ""},
    {"Copy", ""},
    {"Paste", ""},
    {"Undo", ""},
    {"Redo", ""},
    {"ZoomIn", ""},
    {"ZoomOut", ""},
    {"AddTab", ""},
    {"Find", ""},
    {"FindPrevious", ""},
    {"Replace", ""},
    {"MoveToNextChar", ""},
    {"MoveToPreviousChar", ""},
    {"MoveToNextWord", ""},
    {"MoveToPreviousWord", ""},
    {"MoveToNextLine", ""},
    {"MoveToPreviousLine", ""},
    {"MoveToStartOfLine", ""},
    {"MoveToEndOfLine", ""},
    {"MoveToStartOfDocument", ""},
    {"MoveToEndOfDocument", ""},
    {"SelectNextChar", ""},
    {"SelectPreviousChar", ""},
    {"SelectNextWord", ""},
    {"SelectPreviousWord", ""},
    {"SelectNextLine", ""},
    {"SelectPreviousLine", ""},
    {"SelectStartOfLine", ""},
    {"SelectEndOfLine", ""},
};

ShortcutsModel::ShortcutsModel(QObject *parent)
    : QAbstractTableModel(parent),
      parentWindow(nullptr)
{

}

int ShortcutsModel::rowCount(const QModelIndex& idx) const {
   Q_UNUSED(idx);
   return 10;
}

int ShortcutsModel::columnCount(const QModelIndex& idx) const {
    Q_UNUSED(idx);
    return 2;
}

QVariant ShortcutsModel::data(const QModelIndex& index, int role) const
{

    if (role == Qt::DisplayRole) {
        int i = index.row() + 10*(index.column() + parentWindow->getSelectedButton());
        if (i < shortcuts.size())
            return QString(shortcuts[i][0] + " : " + shortcuts[i][1])
                    .arg(index.row() + 1)
                    .arg(index.column() +1);
    }

    return QVariant();
}

void ShortcutsModel::setParentWindow(ShortcutsDialog *window)
{
    parentWindow = window;
}

ShortcutsDialog::ShortcutsDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::ShortcutsDialog)
{
    ui->setupUi(this);
    setWindowTitle("Shortcuts");
    myModel.setParentWindow(this);
    ui->tableView->setModel(&myModel);
    ui->tableView->horizontalHeader()->hide();
    ui->tableView->verticalHeader()->hide();
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setFocusPolicy(Qt::NoFocus);
    ui->radioButton->setChecked(true);

    auto allButtons = findChildren<QRadioButton *>();
    for(int i = 0; i < allButtons.size(); ++i)
        radioButtonsGroup.addButton(allButtons[i],i);

    show();

}

ShortcutsDialog::~ShortcutsDialog() {
    delete ui;
}

int ShortcutsDialog::getSelectedButton()
{
    return radioButtonsGroup.checkedId();
}

void ShortcutsDialog::resizeEvent(QResizeEvent *event) {
    ui->tableView->setColumnWidth(0, width()/2);
    ui->tableView->setColumnWidth(1, width()/2);

    QDialog::resizeEvent(event);
}
