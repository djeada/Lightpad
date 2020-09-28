#include "shortcuts.h"
#include "ui_shortcuts.h"

ShortcutsModel::ShortcutsModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}

int ShortcutsModel::rowCount(const QModelIndex& idx) const {
   Q_UNUSED(idx);
   return 2;
}

int ShortcutsModel::columnCount(const QModelIndex& idx) const {
    Q_UNUSED(idx);
    return 2;
}

QVariant ShortcutsModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::DisplayRole)
       return QString("Row%1, Column%2")
                   .arg(index.row() + 1)
                   .arg(index.column() +1);

    return QVariant();
}

ShortcutsDialog::ShortcutsDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::ShortcutsDialog)
{
    ui->setupUi(this);
    ui->tableView->setModel(&myModel);
    ui->tableView->horizontalHeader()->hide();
    ui->tableView->verticalHeader()->hide();
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    show();
}

ShortcutsDialog::~ShortcutsDialog() {
    delete ui;
}

void ShortcutsDialog::resizeEvent(QResizeEvent *event) {
    ui->tableView->setColumnWidth(0, width()/2);
    ui->tableView->setColumnWidth(1, width()/2);

    QDialog::resizeEvent(event);
}
