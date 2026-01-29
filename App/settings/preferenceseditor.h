#ifndef EDITORPREFERENCES_H
#define EDITORPREFERENCES_H

#include <QWidget>

class PopupTabWidth;
class MainWindow;

namespace Ui {
class PreferencesEditor;
}

class PreferencesEditor : public QWidget {
    Q_OBJECT

public:
    explicit PreferencesEditor(MainWindow* parent);
    ~PreferencesEditor();
    void setTabWidthLabel(const QString& text);

private slots:
    void on_tabWidth_clicked();

private:
    Ui::PreferencesEditor* ui;
    MainWindow* parentWindow;
    PopupTabWidth* popupTabWidth;
};

#endif // EDITORPREFERENCES_H
