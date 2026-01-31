#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
#include <QListView>
#include <QMainWindow>
#include <QMap>
#include <QSet>

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
class SplitEditorContainer;

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
    void applyLanguageOverride(const QString& extension, const QString& displayName);

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
    void on_actionOpen_Project_triggered();
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
    void on_actionToggle_Minimap_triggered();
    void on_actionSplit_Horizontally_triggered();
    void on_actionSplit_Vertically_triggered();
    void on_actionClose_Editor_Group_triggered();
    void on_actionFocus_Next_Group_triggered();
    void on_actionFocus_Previous_Group_triggered();
    void on_actionUnsplit_All_triggered();
    void on_actionToggle_Terminal_triggered();

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
    
    // Split editor views
    SplitEditorContainer* m_splitEditorContainer;
    
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
    void applyHighlightForFile(const QString& filePath);
    QString detectLanguageIdForExtension(const QString& extension) const;
    QString detectLanguageIdForFile(const QString& filePath) const;
    QString displayNameForLanguage(const QString& languageId, const QString& extension) const;
    void loadHighlightOverridesForDir(const QString& dirPath);
    bool saveHighlightOverridesForDir(const QString& dirPath) const;
    QString highlightOverrideForFile(const QString& filePath);
    void setHighlightOverrideForFile(const QString& filePath, const QString& languageId);

    template <typename... Args>
    void updateAllTextAreas(void (TextArea::*f)(Args... args), Args... args);

    QMap<QString, QString> m_highlightOverrides;
    QSet<QString> m_loadedHighlightOverrideDirs;
    
    // Project root path for persistent treeview across all tabs
    QString m_projectRootPath;

public:
    void setProjectRootPath(const QString& path);
    QString getProjectRootPath() const;
};

const int defaultTabWidth = 4;
const int defaultFontSize = 12;
const QString settingsPath = "settings.json";

#endif // MAINWINDOW_H
