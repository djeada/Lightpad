#ifndef SHORTCUTS_H
#define SHORTCUTS_H

#include <QDialog>
#include <QAbstractTableModel>

class ShortcutsModel: public QAbstractTableModel {
    Q_OBJECT
    public:
        ShortcutsModel(QObject *parent = nullptr);
        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        int columnCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};

namespace Ui {
    class ShortcutsDialog;
}

class ShortcutsDialog : public QDialog
{
    Q_OBJECT

    public:
        ShortcutsDialog(QWidget* parent = nullptr);
        ~ShortcutsDialog();

    protected:
        void resizeEvent(QResizeEvent *event) override;

    private:
        Ui::ShortcutsDialog *ui;
        ShortcutsModel myModel;
};

#endif // SHORTCUTS_H
