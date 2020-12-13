#include "shortcuts.h"
#include "ui_shortcuts.h"
#include <QDebug>

ShortcutsModel::ShortcutsModel(QObject *parent)
    : QAbstractTableModel(parent),
      parentWindow(nullptr)
{

}

int ShortcutsModel::rowCount(const QModelIndex& idx) const {
   Q_UNUSED(idx);
   return 3;
}

int ShortcutsModel::columnCount(const QModelIndex& idx) const {
    Q_UNUSED(idx);
    return 3;
}

QVariant ShortcutsModel::data(const QModelIndex& index, int role) const
{
    qDebug() << parentWindow->getSelectedButton();

    if (role == Qt::DisplayRole)
       return QString("Row%1, Column%2")
                   .arg(index.row() + 1)
                   .arg(index.column() +1);

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
