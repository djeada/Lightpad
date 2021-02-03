#ifndef EDITORPREFRENCES_H
#define EDITORPREFRENCES_H

#include <QWidget>

class PopupTabWidth;
class MainWindow;

namespace Ui {
    class PrefrencesEditor;
}

class PrefrencesEditor : public QWidget {
    Q_OBJECT

public:
    explicit PrefrencesEditor(MainWindow* parent);
    ~PrefrencesEditor();
    void setTabWidthLabel(const QString& text);

private slots:
    void on_tabWidth_clicked();

private:
    Ui::PrefrencesEditor* ui;
    MainWindow* parentWindow;
    PopupTabWidth* popupTabWidth;
};

#endif // EDITORPREFRENCES_H
