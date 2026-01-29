#ifndef SHORTCUTS_H
#define SHORTCUTS_H

#include <QAbstractTableModel>
#include <QButtonGroup>
#include <QDialog>

class ShortcutsDialog;

class ShortcutsModel : public QAbstractTableModel {
    Q_OBJECT
public:
    ShortcutsModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    void setParentWindow(ShortcutsDialog* window);

private:
    ShortcutsDialog* parentWindow;
};

namespace Ui {
class ShortcutsDialog;
}

class ShortcutsDialog : public QDialog {
    Q_OBJECT

public:
    ShortcutsDialog(QWidget* parent = nullptr);
    ~ShortcutsDialog();
    int getSelectedButton();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    Ui::ShortcutsDialog* ui;
    ShortcutsModel myModel;
    QButtonGroup radioButtonsGroup;
};

#endif // SHORTCUTS_H
