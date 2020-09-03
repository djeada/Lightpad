#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListView>
#include "textarea.h"
#include "findreplacepanel.h"
#include <QDebug>
#include <QDialog>

namespace Ui {
    class MainWindow;
}

class ListView: public QListView {
    Q_OBJECT

    public:
        ListView(QWidget *parent = nullptr);
        QSize sizeHint() const;
};

class Popup: public QDialog {
    Q_OBJECT

    public:
        Popup(QWidget* parent = nullptr);

    private slots:
        void on_listView_clicked(const QModelIndex &index);

    private:
        ListView* listView;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        void updateFileExtension(QString ext);
        ~MainWindow();

    protected:
        bool eventFilter(QObject* object, QEvent *event);

    private slots:
        void on_actionQuit_triggered();
        void on_actionToggle_Full_Screen_triggered();
        void on_actionToggle_Undo_triggered();
        void on_actionToggle_Redo_triggered();
        void on_actionIncrease_Font_Size_triggered();
        void on_actionDecrease_Font_Size_triggered();
        void on_actionReset_Font_Size_triggered();
        void on_actionCut_triggered();
        void on_actionCopy_triggered();
        void on_actionPaste_triggered();
        void on_actionNew_Window_triggered();
        void on_actionClose_Tab_triggered();
        void on_actionClose_All_Tabs_triggered();
        void on_actionFind_in_file_triggered();
        void on_actionNew_File_triggered();
        void on_actionOpen_File_triggered();
        void on_actionSave_triggered();
        void on_actionSave_as_triggered();
        void on_actionToggle_Menu_Bar_triggered();
        void on_actionReplace_in_file_triggered();
        void on_languageHighlight_clicked();

    private:
        Ui::MainWindow *ui;
        void undo();
        void redo();
        void open(const QString &filePath);
        void save(const QString &filePath);
        void showFindReplace(bool onlyFind = true);
        void setMainWindowTitle(QString title);
        TextArea* getCurrentTextArea();
        Popup* popup;
        QString highlightLanguage;
        FindReplacePanel* findReplacePanel;
        QMap<QString, QString> langToExt = {};
};

#endif // MAINWINDOW_H
