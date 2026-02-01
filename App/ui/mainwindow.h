#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
#include <QListView>
#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QTimer>

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
class QTreeView;
class QModelIndex;
class SplitEditorContainer;
class LightpadTabWidget;
class ImageViewer;
class GitIntegration;
class GitFileSystemModel;
class SourceControlPanel;
class QDockWidget;
class VimMode;
class LightpadTreeView;
#ifdef HAVE_PDF_SUPPORT
class PdfViewer;
#endif

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
    void on_actionAbout_Qt_triggered();
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
    void on_actionToggle_Source_Control_triggered();
    
    // Text transformation actions
    void on_actionTransform_Uppercase_triggered();
    void on_actionTransform_Lowercase_triggered();
    void on_actionTransform_Title_Case_triggered();
    void on_actionSort_Lines_Ascending_triggered();
    void on_actionSort_Lines_Descending_triggered();
    
    // View actions
    void on_actionToggle_Word_Wrap_triggered();
    void on_actionToggle_Vim_Mode_triggered();
    void on_actionFold_Current_triggered();
    void on_actionUnfold_Current_triggered();
    void on_actionFold_All_triggered();
    void on_actionUnfold_All_triggered();
    void on_actionFold_Comments_triggered();
    void on_actionUnfold_Comments_triggered();

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
    class GoToSymbolDialog* goToSymbolDialog;
    class FileQuickOpen* fileQuickOpen;
    class RecentFilesDialog* recentFilesDialog;
    class QLabel* problemsStatusLabel;
    class QLabel* vimStatusLabel;
    bool m_vimCommandPanelActive;
    VimMode* m_connectedVimMode;
    class BreadcrumbWidget* breadcrumbWidget;
    class RecentFilesManager* recentFilesManager;
    
    // Navigation history
    class NavigationHistory* navigationHistory;
    
    // Auto-save
    class AutoSaveManager* autoSaveManager;
    GitIntegration* m_gitIntegration;
    SourceControlPanel* sourceControlPanel;
    QDockWidget* sourceControlDock;
    
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
    void showGoToSymbolDialog();
    void showFileQuickOpen();
    void setupCommandPalette();
    void setupGoToLineDialog();
    void setupGoToSymbolDialog();
    void setupFileQuickOpen();
    void showRecentFilesDialog();
    void setupRecentFilesDialog();
    void setupBreadcrumb();
    void updateBreadcrumb(const QString& filePath);
    void toggleShowWhitespace();
    void toggleShowIndentGuides();
    void navigateBack();
    void navigateForward();
    void recordNavigationLocation();
    void setupNavigationHistory();
    void setupAutoSave();
    void setupGitIntegration();
    void updateGitIntegrationForPath(const QString& path);
    void applyGitIntegrationToAllPages();
    void ensureFileTreeModel();
    void trackTreeExpandedState(const QModelIndex& index, bool expanded);
    void applyTreeStateToView(QTreeView* treeView);
    void applyTreeExpandedStateToViews();
    void loadTreeStateFromSettings(const QString& rootPath);
    void persistTreeStateToSettings();
    QList<LightpadTreeView*> allTreeViews() const;
    void expandIndexInView(QTreeView* treeView, const QModelIndex& index);
    void ensureSourceControlPanel();
    void ensureStatusLabels();
    void updateProblemsStatusLabel(int errors, int warnings, int infos);
    void updateVimStatusLabel(const QString& text);
    void showVimStatusMessage(const QString& message);
    void setMainWindowTitle(QString title);
    void setFilePathAsTabText(QString filePath);
    void closeCurrentTab();
    void setupTabWidget();
    void setupTabWidgetConnections(LightpadTabWidget* tabWidget);
    void updateTabWidgetContext(LightpadTabWidget* tabWidget, int index);
    void applyTabWidgetTheme(LightpadTabWidget* tabWidget);
    void setupTextArea();
    void connectVimMode(TextArea* textArea);
    void disconnectVimMode();
    void showVimCommandPanel(const QString& prefix, const QString& buffer);
    void hideVimCommandPanel();
    void setupCompletionSystem();
    void noScriptAssignedWarning();
    LightpadTabWidget* currentTabWidget() const;
    QList<LightpadTabWidget*> allTabWidgets() const;
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
    void ensureProjectSettings(const QString& path);

    template <typename... Args>
    void updateAllTextAreas(void (TextArea::*f)(Args... args), Args... args);
    void updateAllTextAreas(void (TextArea::*f)(const Theme&), const Theme& theme);

    QMap<QString, QString> m_highlightOverrides;
    QSet<QString> m_loadedHighlightOverrideDirs;
    
    // Project root path for persistent treeview across all tabs
    QString m_projectRootPath;
    GitFileSystemModel* m_fileTreeModel;
    QSet<QString> m_treeExpandedPaths;

public:
    void setProjectRootPath(const QString& path);
    QString getProjectRootPath() const;
    GitIntegration* getGitIntegration() const;
    GitFileSystemModel* getFileTreeModel() const;
    void registerTreeView(LightpadTreeView* treeView);
};

const int defaultTabWidth = 4;
const int defaultFontSize = 12;
const QString settingsPath = "settings.json";

#endif // MAINWINDOW_H
