#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListView>
#include <QDebug>
#include <QDialog>

#include "textarea.h"
#include "findreplacepanel.h"
#include "prefrences.h"
#include "popup.h"

class Prefrences;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow();
        void keyPressEvent(QKeyEvent *event);
        void openFileAndAddToNewTab(QString path);
        void closeTabPage(QString filePath);
        void setRowCol(int row, int col);
        void setTabWidth(int width);
        void setTabWidthLabel(QString text);
        void setLanguageHighlightLabel(QString text);
        void setTheme(Theme theme);
        void setFont(QFont font);
        int getTabWidth();
        int getFontSize();
        TextArea* getCurrentTextArea();
        Theme getTheme();
        QFont getFont();

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
        void on_actionAbout_triggered();
        void on_tabWidth_clicked();
        void on_actionKeyboard_shortcuts_triggered();
        void on_actionPrefrences_triggered();

    private:
        Ui::MainWindow* ui;
        Popup* popupHighlightLanguage;
        Popup* popupTabWidth;
        Prefrences* prefrences;
        QString highlightLanguage;
        FindReplacePanel* findReplacePanel;
        QFont font;
        Theme colors;
        int fontSize;
        int tabWidth;
        void undo();
        void redo();
        void open(const QString& filePath);
        void save(const QString& filePath);
        void showFindReplace(bool onlyFind = true);
        void setMainWindowTitle(QString title);
        void setFilePathAsTabText(QString filePath);
        void closeEvent( QCloseEvent* event );
};

#endif // MAINWINDOW_H
