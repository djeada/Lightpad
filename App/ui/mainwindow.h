#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
#include <QListView>
#include <QMainWindow>

#include "../settings/textareasettings.h"
#include "../settings/theme.h"

class MainWindow;
class TerminalTabWidget;
class Popup;
class FindReplacePanel;
class TextArea;
class QCompleter;
class Preferences;
class CompletionEngine;

enum class Lang { cpp,
    js,
    py };
enum class Dialog { shortcuts,
    runConfiguration,
    formatConfiguration };

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    void keyPressEvent(QKeyEvent* event);
    void openFileAndAddToNewTab(QString path);
    void closeTabPage(QString filePath);
    void setRowCol(int row, int col);
    void setTabWidth(int width);
    void setTabWidthLabel(QString text);
    void setLanguageHighlightLabel(QString text);
    void setTheme(Theme theme);
    void setFont(QFont font);
    void showLineNumbers(bool flag);
    void highlihtCurrentLine(bool flag);
    void highlihtMatchingBracket(bool flag);
    void runCurrentScript();
    void formatCurrentDocument();
    int getTabWidth();
    int getFontSize();
    TextArea* getCurrentTextArea();
    Theme getTheme();
    QFont getFont();
    TextAreaSettings getSettings();

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
    void on_actionPreferences_triggered();
    void on_runButton_clicked();
    void on_actionRun_file_name_triggered();
    void on_actionEdit_Configurations_triggered();
    void on_magicButton_clicked();
    void on_actionFormat_Document_triggered();
    void on_actionEdit_Format_Configurations_triggered();
    void on_actionGo_to_Line_triggered();

private:
    Ui::MainWindow* ui;
    Popup* popupHighlightLanguage;
    Popup* popupTabWidth;
    Preferences* preferences;
    FindReplacePanel* findReplacePanel;
    TerminalTabWidget* terminalWidget;
    QCompleter* completer;  // Legacy - deprecated
    CompletionEngine* m_completionEngine;
    TextAreaSettings settings;
    QString highlightLanguage;
    QFont font;
    
    // Quick win features
    class CommandPalette* commandPalette;
    class ProblemsPanel* problemsPanel;
    class GoToLineDialog* goToLineDialog;
    class FileQuickOpen* fileQuickOpen;
    class QLabel* problemsStatusLabel;
    
    void undo();
    void redo();
    void open(const QString& filePath);
    void save(const QString& filePath);
    void showFindReplace(bool onlyFind = true);
    void openDialog(Dialog dialog);
    void openConfigurationDialog();
    void openFormatConfigurationDialog();
    void openShortcutsDialog();
    void showTerminal();
    void showProblemsPanel();
    void showCommandPalette();
    void showGoToLineDialog();
    void showFileQuickOpen();
    void setupCommandPalette();
    void setupGoToLineDialog();
    void setupFileQuickOpen();
    void updateProblemsStatusLabel(int errors, int warnings, int infos);
    void setMainWindowTitle(QString title);
    void setFilePathAsTabText(QString filePath);
    void closeCurrentTab();
    void setupTabWidget();
    void setupTextArea();
    void setupCompletionSystem();
    void noScriptAssignedWarning();
    void closeEvent(QCloseEvent* event);
    void loadSettings();
    void saveSettings();

    template <typename... Args>
    void updateAllTextAreas(void (TextArea::*f)(Args... args), Args... args);
};

const int defaultTabWidth = 4;
const int defaultFontSize = 12;
const QString settingsPath = "settings.json";

#endif // MAINWINDOW_H
