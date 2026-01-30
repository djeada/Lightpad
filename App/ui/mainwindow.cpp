#include <QBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringListModel>
#include <QCompleter>
#include <QProcess>
#include <cstdio>

#include "panels/findreplacepanel.h"
#include "../core/lightpadpage.h"
#include "../core/logging/logger.h"
#include "mainwindow.h"
#include "popup.h"
#include "dialogs/preferences.h"
#include "dialogs/runconfigurations.h"
#include "dialogs/runtemplateselector.h"
#include "dialogs/formattemplateselector.h"
#include "dialogs/shortcuts.h"
#include "panels/terminal.h"
#include "../core/textarea.h"
#include "../run_templates/runtemplatemanager.h"
#include "../format_templates/formattemplatemanager.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , popupHighlightLanguage(nullptr)
    , popupTabWidth(nullptr)
    , preferences(nullptr)
    , findReplacePanel(nullptr)
    , terminal(nullptr)
    , completer(nullptr)
    , highlightLanguage("")
    , font(QApplication::font())
{
    QApplication::instance()->installEventFilter(this);
    ui->setupUi(this);
    show();
    ui->tabWidget->setMainWindow(this);
    ui->magicButton->setIconSize(0.8 * ui->magicButton->size());
    
    // Initialize shared completer
    QStringList wordList;
    
    // Common keywords across languages
    wordList << "break" << "case" << "continue" << "default" << "do" << "else" 
             << "for" << "if" << "return" << "switch" << "while";
    
    // C/C++ specific keywords
    wordList << "auto" << "char" << "const" << "double" << "enum" << "extern" << "float"
             << "goto" << "int" << "long" << "register" << "short" << "signed" << "sizeof" 
             << "static" << "struct" << "typedef" << "union" << "unsigned" << "void" << "volatile"
             << "class" << "namespace" << "template" << "public" << "private" << "protected"
             << "virtual" << "override" << "final" << "explicit" << "inline" << "constexpr"
             << "nullptr" << "delete" << "new" << "this" << "try" << "catch" << "throw"
             << "bool" << "true" << "false";
    
    // C++ STL types and functions
    wordList << "std" << "string" << "vector" << "map" << "set" << "list" << "queue" 
             << "stack" << "pair" << "cout" << "cin" << "endl" << "include" << "define" 
             << "ifdef" << "ifndef" << "endif";
    
    // Python specific keywords
    wordList << "and" << "as" << "assert" << "async" << "await" << "class" << "def" 
             << "del" << "elif" << "except" << "finally" << "from" << "global" << "import" 
             << "in" << "is" << "lambda" << "nonlocal" << "not" << "or" << "pass" << "raise"
             << "with" << "yield" << "True" << "False" << "None" << "self";
    
    // Python built-ins
    wordList << "print" << "range" << "len" << "str" << "int" << "float" << "list" << "dict"
             << "tuple" << "set" << "open" << "file" << "read" << "write" << "append";
    
    // JavaScript specific keywords
    wordList << "abstract" << "arguments" << "boolean" << "byte" << "debugger" 
             << "eval" << "export" << "extends" << "final" << "function" << "implements"
             << "instanceof" << "interface" << "let" << "native" << "package" 
             << "super" << "synchronized" << "throws" << "transient" << "typeof" << "var"
             << "const";
    
    // JavaScript DOM/Browser
    wordList << "console" << "log" << "document" << "window" << "alert" << "prompt" 
             << "confirm" << "getElementById" << "querySelector" << "addEventListener" 
             << "setTimeout" << "setInterval";
    
    wordList.sort();
    wordList.removeDuplicates();
    
    completer = new QCompleter(wordList, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    
    setupTextArea();
    setupTabWidget();
    loadSettings();
    setWindowTitle("LightPad");
}

void MainWindow::setRowCol(int row, int col)
{
    ui->rowCol->setText("Ln " + QString::number(row) + ", Col " + QString::number(col));
}

void MainWindow::setTabWidthLabel(QString text)
{
    ui->tabWidth->setText(text);

    if (preferences)
        preferences->setTabWidthLabel(text);
}

void MainWindow::setLanguageHighlightLabel(QString text)
{
    ui->languageHighlight->setText(text);
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    Q_UNUSED(event);

    if (preferences) {
        preferences->close();
    }
}

void MainWindow::loadSettings()
{
    if (QFileInfo(settingsPath).exists())
        settings.loadSettings(settingsPath);

    else
        setTabWidth(defaultTabWidth);

    updateAllTextAreas(&TextArea::loadSettings, settings);
    setTheme(settings.theme);
}

void MainWindow::saveSettings()
{
    LOG_DEBUG(QString("Saving settings, showLineNumberArea: %1").arg(settings.showLineNumberArea));
    settings.saveSettings(settingsPath);
}

template <typename... Args>
void MainWindow::updateAllTextAreas(void (TextArea::*f)(Args... args), Args... args)
{
    auto textAreas = ui->tabWidget->findChildren<TextArea*>();
    for (auto& textArea : textAreas)
        (textArea->*f)(args...);
}

void MainWindow::keyPressEvent(QKeyEvent* keyEvent)
{

    if (keyEvent->matches(QKeySequence::Undo))
        undo();

    else if (keyEvent->matches(QKeySequence::Redo))
        redo();

    else if (keyEvent->matches(QKeySequence::ZoomIn))
        on_actionIncrease_Font_Size_triggered();

    else if (keyEvent->matches(QKeySequence::ZoomOut))
        on_actionDecrease_Font_Size_triggered();

    else if (keyEvent->matches(QKeySequence::Save))
        on_actionSave_triggered();

    else if (keyEvent->matches(QKeySequence::SaveAs))
        on_actionSave_as_triggered();

    else if (keyEvent->key() == Qt::Key_Alt)
        on_actionToggle_Menu_Bar_triggered();

    else if (keyEvent->matches(QKeySequence::Find))
        showFindReplace();

    else if (keyEvent->matches(QKeySequence::Replace))
        showFindReplace(false);

    else if (keyEvent->matches(QKeySequence::Close))
        closeCurrentTab();

    else if (keyEvent->matches(QKeySequence::AddTab))
        ui->tabWidget->addNewTab();
}

int MainWindow::getTabWidth()
{
    return settings.tabWidth;
}

int MainWindow::getFontSize()
{
    return settings.mainFont.pointSize();
}

//use current page if empty
//else add new tab
void MainWindow::openFileAndAddToNewTab(QString filePath)
{

    if (filePath.isEmpty() || !QFileInfo(filePath).exists())
        return;

    //check if file not already edited
    for (int i = 0; i < ui->tabWidget->count(); i++) {
        auto page = ui->tabWidget->getPage(i);

        if (page && page->getFilePath() == filePath) {
            ui->tabWidget->setCurrentIndex(i);
            return;
        }
    }

    if (ui->tabWidget->count() == 0 || !getCurrentTextArea()->toPlainText().isEmpty()) {
        ui->tabWidget->addNewTab();
        ui->tabWidget->setCurrentIndex(ui->tabWidget->count() - 1);
    }

    open(filePath);
    setFilePathAsTabText(filePath);

    auto page = ui->tabWidget->getCurrentPage();

    if (page) {
        page->setTreeViewVisible(true);
        page->setModelRootIndex(QFileInfo(filePath).absoluteDir().path());
        page->setFilePath(filePath);
    }

    if (getCurrentTextArea())
        getCurrentTextArea()->updateSyntaxHighlightTags("", QFileInfo(filePath).completeSuffix());

    ui->tabWidget->currentChanged(ui->tabWidget->currentIndex());
}

void MainWindow::closeTabPage(QString filePath)
{

    for (int i = 0; i < ui->tabWidget->count(); i++) {
        if (ui->tabWidget->getFilePath(i) == filePath)
            ui->tabWidget->removeTab(i);
    }
}

void MainWindow::on_actionToggle_Full_Screen_triggered()
{
    if (isMaximized())
        showNormal();

    else
        showMaximized();
}

void MainWindow::on_actionQuit_triggered()
{
    close();
}

void MainWindow::undo()
{
    if (getCurrentTextArea())
        getCurrentTextArea()->undo();
}

void MainWindow::redo()
{
    if (getCurrentTextArea())
        getCurrentTextArea()->redo();
}

TextArea* MainWindow::getCurrentTextArea()
{
    if (ui->tabWidget->currentWidget()->findChild<LightpadPage*>("widget"))
        return ui->tabWidget->currentWidget()->findChild<LightpadPage*>("widget")->getTextArea();

    else if (ui->tabWidget->currentWidget()->findChild<TextArea*>(""))
        return ui->tabWidget->currentWidget()->findChild<TextArea*>("");

    return nullptr;
}

Theme MainWindow::getTheme()
{
    return settings.theme;
}

QFont MainWindow::getFont()
{
    return settings.mainFont;
}

TextAreaSettings MainWindow::getSettings()
{
    return settings;
}

void MainWindow::setTabWidth(int width)
{
    updateAllTextAreas(&TextArea::setTabWidth, width);
    settings.tabWidth = width;
}

void MainWindow::on_actionToggle_Undo_triggered()
{
    undo();
}

void MainWindow::on_actionToggle_Redo_triggered()
{
    redo();
}

void MainWindow::on_actionIncrease_Font_Size_triggered()
{
    updateAllTextAreas(&TextArea::increaseFontSize);
    settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
}

void MainWindow::on_actionDecrease_Font_Size_triggered()
{
    updateAllTextAreas(&TextArea::decreaseFontSize);
    settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
}

void MainWindow::on_actionReset_Font_Size_triggered()
{
    updateAllTextAreas(&TextArea::setFontSize, defaultFontSize);
    settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
}

void MainWindow::on_actionCut_triggered()
{

    if (getCurrentTextArea())
        getCurrentTextArea()->cut();
}

void MainWindow::on_actionCopy_triggered()
{

    if (getCurrentTextArea())
        getCurrentTextArea()->copy();
}

void MainWindow::on_actionPaste_triggered()
{

    if (getCurrentTextArea())
        getCurrentTextArea()->paste();
}

void MainWindow::on_actionNew_Window_triggered()
{
    new MainWindow();
}

void MainWindow::on_actionClose_Tab_triggered()
{
    if (ui->tabWidget->currentIndex() > -1)
        ui->tabWidget->removeTab(ui->tabWidget->currentIndex());
}

void MainWindow::on_actionClose_All_Tabs_triggered()
{
    ui->tabWidget->closeAllTabs();
}

void MainWindow::on_actionFind_in_file_triggered()
{
    showFindReplace(true);
}

void MainWindow::on_actionNew_File_triggered()
{
    ui->tabWidget->addNewTab();
}

void MainWindow::on_actionOpen_File_triggered()
{
    auto filePath = QFileDialog::getOpenFileName(this, tr("Open Document"), QDir::homePath());

    openFileAndAddToNewTab(filePath);
}

void MainWindow::on_actionSave_triggered()
{

    auto tabIndex = ui->tabWidget->currentIndex();
    auto filePath = ui->tabWidget->getFilePath(tabIndex);

    if (filePath.isEmpty()) {
        on_actionSave_as_triggered();
        return;
    }

    save(filePath);
}

void MainWindow::on_actionSave_as_triggered()
{

    auto filePath = QFileDialog::getSaveFileName(this, tr("Save Document"), QDir::homePath());

    if (filePath.isEmpty())
        return;

    int tabIndex = ui->tabWidget->currentIndex();
    ui->tabWidget->setFilePath(tabIndex, filePath);

    save(filePath);
}

void MainWindow::open(const QString& filePath)
{

    QFile file(filePath);

    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't open file."));
        return;
    }

    int tabIndex = ui->tabWidget->currentIndex();
    ui->tabWidget->setFilePath(tabIndex, filePath);

    if (getCurrentTextArea())
        getCurrentTextArea()->setPlainText(QString::fromUtf8(file.readAll()));
}

void MainWindow::save(const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text))
        return;

    if (getCurrentTextArea()) {
        auto tabIndex = ui->tabWidget->currentIndex();
        ui->tabWidget->setFilePath(tabIndex, filePath);

        file.write(getCurrentTextArea()->toPlainText().toUtf8());
        getCurrentTextArea()->document()->setModified(false);
        getCurrentTextArea()->removeIconUnsaved();
        setFilePathAsTabText(filePath);
    }
}

void MainWindow::showFindReplace(bool onlyFind)
{
    if (!findReplacePanel) {
        findReplacePanel = new FindReplacePanel(onlyFind);

        auto layout = qobject_cast<QBoxLayout*>(ui->centralwidget->layout());

        if (layout != 0)
            layout->insertWidget(layout->count() - 1, findReplacePanel, 0);
    }

    findReplacePanel->setVisible(!findReplacePanel->isVisible() || findReplacePanel->isOnlyFind() != onlyFind);
    findReplacePanel->setOnlyFind(onlyFind);

    if (findReplacePanel->isVisible() && getCurrentTextArea())
        findReplacePanel->setReplaceVisibility(!onlyFind);

    if (findReplacePanel->isVisible()) {
        findReplacePanel->setTextArea(getCurrentTextArea());
        findReplacePanel->setFocusOnSearchBox();
    }
}

void MainWindow::openDialog(Dialog dialog)
{
    switch (dialog) {
    case Dialog::runConfiguration: {
        // Use new template selector
        auto page = ui->tabWidget->getCurrentPage();
        QString filePath = page ? page->getFilePath() : QString();
        
        if (filePath.isEmpty()) {
            QMessageBox::information(this, "Run Configuration", 
                "Please open a file first to configure run settings.");
            return;
        }
        
        if (findChildren<RunTemplateSelector*>().isEmpty()) {
            auto selector = new RunTemplateSelector(filePath, this);
            selector->setAttribute(Qt::WA_DeleteOnClose);
            selector->show();
        }
        break;
    }
    
    case Dialog::formatConfiguration: {
        // Use new format template selector
        auto page = ui->tabWidget->getCurrentPage();
        QString filePath = page ? page->getFilePath() : QString();
        
        if (filePath.isEmpty()) {
            QMessageBox::information(this, "Format Configuration", 
                "Please open a file first to configure format settings.");
            return;
        }
        
        if (findChildren<FormatTemplateSelector*>().isEmpty()) {
            auto selector = new FormatTemplateSelector(filePath, this);
            selector->setAttribute(Qt::WA_DeleteOnClose);
            selector->show();
        }
        break;
    }

    case Dialog::shortcuts:
        if (findChildren<ShortcutsDialog*>().isEmpty())
            new ShortcutsDialog(this);
        break;

    default:
        return;
    }
}

void MainWindow::openConfigurationDialog()
{
    openDialog(Dialog::runConfiguration);
}

void MainWindow::openFormatConfigurationDialog()
{
    openDialog(Dialog::formatConfiguration);
}

void MainWindow::openShortcutsDialog()
{
    openDialog(Dialog::shortcuts);
}

void MainWindow::showTerminal()
{
    auto page = ui->tabWidget->getCurrentPage();
    QString filePath = page ? page->getFilePath() : QString();

    if (filePath.isEmpty()) {
        noScriptAssignedWarning();
        return;
    }

    if (!terminal) {
        terminal = new Terminal();

        connect(terminal, &QObject::destroyed, this, [&]() {
            terminal = nullptr;
        });

        auto layout = qobject_cast<QBoxLayout*>(ui->centralwidget->layout());

        if (layout != 0)
            layout->insertWidget(layout->count() - 1, terminal, 0);
    }
    
    // Run the file using the template system
    terminal->runFile(filePath);
}

void MainWindow::setMainWindowTitle(QString title)
{
    setWindowTitle(title + " - Lightpad");
}

void MainWindow::setFont(QFont newFont)
{
    updateAllTextAreas(&TextArea::setFont, newFont);
    font = newFont;
}

void MainWindow::showLineNumbers(bool flag)
{
    updateAllTextAreas(&TextArea::showLineNumbers, flag);
    settings.showLineNumberArea = flag;
}

void MainWindow::highlihtCurrentLine(bool flag)
{
    updateAllTextAreas(&TextArea::highlihtCurrentLine, flag);
    settings.lineHighlighted = flag;
}

void MainWindow::highlihtMatchingBracket(bool flag)
{
    updateAllTextAreas(&TextArea::highlihtMatchingBracket, flag);
    settings.matchingBracketsHighlighted = flag;
}

void MainWindow::runCurrentScript()
{
    on_actionSave_triggered();
    auto textArea = getCurrentTextArea();

    if (textArea && !textArea->changesUnsaved())
        showTerminal();
}

void MainWindow::formatCurrentDocument()
{
    auto page = ui->tabWidget->getCurrentPage();
    QString filePath = page ? page->getFilePath() : QString();
    
    if (filePath.isEmpty()) {
        QMessageBox::information(this, "Format Document", 
            "Please save the file first before formatting.");
        return;
    }
    
    // Check for unsaved changes before formatting
    auto textArea = getCurrentTextArea();
    bool hadUnsavedChanges = textArea && textArea->changesUnsaved();
    
    // Save the file first
    on_actionSave_triggered();
    
    // Verify the file was saved (check if still has unsaved changes)
    if (textArea && textArea->changesUnsaved()) {
        QMessageBox::warning(this, "Format Document", 
            "Could not save the file. Formatting cancelled.");
        return;
    }
    
    FormatTemplateManager& manager = FormatTemplateManager::instance();
    
    // Ensure templates are loaded
    if (manager.getAllTemplates().isEmpty()) {
        manager.loadTemplates();
    }
    
    QPair<QString, QStringList> command = manager.buildCommand(filePath);
    
    if (command.first.isEmpty()) {
        QMessageBox::information(this, "Format Document", 
            "No formatter found for this file type.\nUse Format > Edit Format Configurations to assign one.");
        return;
    }
    
    // Execute the formatter
    QProcess process;
    process.setWorkingDirectory(QFileInfo(filePath).absoluteDir().path());
    process.start(command.first, command.second);
    
    if (!process.waitForFinished(60000)) {
        QMessageBox::warning(this, "Format Document", 
            "Formatting timed out or failed to start.\nMake sure the formatter is installed and in PATH.");
        return;
    }
    
    if (process.exitCode() != 0) {
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());
        QString stdOut = QString::fromUtf8(process.readAllStandardOutput());
        LOG_WARNING(QString("Formatter exited with code %1: %2").arg(process.exitCode()).arg(errorOutput));
        
        // Show error to user
        QString errorMsg = QString("Formatter exited with error code %1.").arg(process.exitCode());
        if (!errorOutput.isEmpty()) {
            errorMsg += QString("\n\nError output:\n%1").arg(errorOutput.left(500));
        }
        QMessageBox::warning(this, "Format Document", errorMsg);
    }
    
    // Reload the file if it was modified in place
    if (textArea) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString newContent = QString::fromUtf8(file.readAll());
            file.close();
            
            // Only update if content changed
            if (textArea->toPlainText() != newContent) {
                // Save cursor position
                int cursorPos = textArea->textCursor().position();
                
                textArea->setPlainText(newContent);
                // Mark as unmodified since we just formatted and the file matches the disk
                textArea->document()->setModified(false);
                
                // Restore cursor position as close as possible
                QTextCursor cursor = textArea->textCursor();
                cursor.setPosition(qMin(cursorPos, textArea->toPlainText().length()));
                textArea->setTextCursor(cursor);
            }
        } else {
            LOG_WARNING(QString("Failed to reload file after formatting: %1").arg(filePath));
        }
    }
}

void MainWindow::setFilePathAsTabText(QString filePath)
{

    auto fileName = QFileInfo(filePath).fileName();

    auto tabIndex = ui->tabWidget->currentIndex();
    auto tabText = ui->tabWidget->tabText(tabIndex);

    setMainWindowTitle(fileName);
    ui->tabWidget->setTabText(tabIndex, fileName);
}

void MainWindow::closeCurrentTab()
{
    auto textArea = getCurrentTextArea();

    if (textArea && textArea->changesUnsaved())
        on_actionSave_triggered();

    ui->tabWidget->closeCurrentTab();
}

void MainWindow::setupTabWidget()
{
    QObject::connect(ui->tabWidget, &QTabWidget::currentChanged, this, [&](int index) {
        auto text = ui->tabWidget->tabText(index);
        setMainWindowTitle(text);

        if (!ui->menuRun->actions().empty())
            ui->menuRun->actions().front()->setText("Run " + text);
    });

    ui->tabWidget->currentChanged(0);
}

void MainWindow::setupTextArea()
{

    if (getCurrentTextArea()) {
        getCurrentTextArea()->setMainWindow(this);
        getCurrentTextArea()->setFontSize(settings.mainFont.pointSize());
        getCurrentTextArea()->setTabWidth(settings.tabWidth);
        
        // Setup autocompletion with shared completer
        if (completer)
            getCurrentTextArea()->setCompleter(completer);
    }
}

void MainWindow::noScriptAssignedWarning()
{
    QMessageBox msgBox(this);
    msgBox.setText("No file is currently open.");
    msgBox.setInformativeText("Open a file first, then you can run it or configure a run template.");
    auto openButton = msgBox.addButton(tr("Open File"), QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.exec();

    if (msgBox.clickedButton() == openButton)
        on_actionOpen_File_triggered();
}

void MainWindow::on_actionToggle_Menu_Bar_triggered()
{
    ui->menubar->setVisible(!ui->menubar->isVisible());
}

void MainWindow::on_languageHighlight_clicked()
{

    if (!popupHighlightLanguage) {
        auto dir = QDir(":/resources/highlight").entryList(QStringList(), QDir::Dirs);
        auto popupHighlightLanguage = new PopupLanguageHighlight(dir, this);
        auto point = mapToGlobal(ui->languageHighlight->pos());
        popupHighlightLanguage->setGeometry(point.x(), point.y() - 2 * popupHighlightLanguage->height() + height(), popupHighlightLanguage->width(), popupHighlightLanguage->height());
    }

    else if (popupHighlightLanguage->isHidden())
        popupHighlightLanguage->show();

    else
        popupHighlightLanguage->hide();
}

void MainWindow::on_actionAbout_triggered()
{

    QFile TextFile(":/resources/messages/About.txt");

    if (TextFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&TextFile);
        QString license = in.readAll();
        QMessageBox::information(this, tr("About Lightpad"), license, QMessageBox::Close);
        TextFile.close();
    }
}

void MainWindow::on_tabWidth_clicked()
{

    if (!popupTabWidth) {
        auto popupTabWidth = new PopupTabWidth(QStringList({ "2", "4", "8" }), this);
        auto point = mapToGlobal(ui->tabWidth->pos());
        QRect rect(point.x(), point.y() - 2 * popupTabWidth->height() + height(), popupTabWidth->width(), popupTabWidth->height());
        popupTabWidth->setGeometry(rect);
    }

    else if (popupTabWidth->isHidden())
        popupTabWidth->show();

    else
        popupTabWidth->hide();
}

void MainWindow::on_actionReplace_in_file_triggered()
{
    showFindReplace(false);
}

void MainWindow::on_actionKeyboard_shortcuts_triggered()
{
    openShortcutsDialog();
}

void MainWindow::on_actionPreferences_triggered()
{
    if (!preferences) {
        preferences = new Preferences(this);

        connect(preferences, &QObject::destroyed, this, [&] {
            preferences = nullptr;
        });
    }
}

void MainWindow::on_runButton_clicked()
{
    runCurrentScript();
}

void MainWindow::on_actionRun_file_name_triggered()
{
    runCurrentScript();
}

void MainWindow::on_actionEdit_Configurations_triggered()
{
    openConfigurationDialog();
}

void MainWindow::on_magicButton_clicked()
{
    formatCurrentDocument();
}

void MainWindow::on_actionFormat_Document_triggered()
{
    formatCurrentDocument();
}

void MainWindow::on_actionEdit_Format_Configurations_triggered()
{
    openFormatConfigurationDialog();
}

void MainWindow::setTheme(Theme theme)
{
    settings.theme = theme;

    QString bgColor = settings.theme.backgroundColor.name();
    QString fgColor = settings.theme.foregroundColor.name();
    QString surfaceColor = "#313244";  // Slightly lighter surface
    QString accentColor = "#89b4fa";   // Soft blue accent
    QString hoverColor = "#45475a";    // Subtle hover
    QString pressedColor = "#585b70";  // Pressed state

    setStyleSheet(
        // Base widget styling
        "QWidget { background-color: " + bgColor + "; }"

        // Modern menu styling
        "QMenu { "
            "color: " + fgColor + "; "
            "background-color: " + surfaceColor + "; "
            "selection-background-color: " + hoverColor + "; "
            "border: 1px solid " + hoverColor + "; "
            "border-radius: 8px; "
            "padding: 4px; "
        "}"
        "QMenu::item { "
            "padding: 6px 24px 6px 12px; "
            "border-radius: 4px; "
            "margin: 2px 4px; "
        "}"
        "QMenu::item:selected { "
            "background-color: " + hoverColor + "; "
        "}"
        "QMenu::separator { "
            "height: 1px; "
            "background: " + hoverColor + "; "
            "margin: 4px 8px; "
        "}"

        // Menu bar styling
        "QMenuBar { "
            "background-color: " + bgColor + "; "
            "spacing: 4px; "
        "}"
        "QMenuBar::item { "
            "color: " + fgColor + "; "
            "padding: 6px 12px; "
            "border-radius: 6px; "
        "}"
        "QMenuBar::item:selected { "
            "background-color: " + hoverColor + "; "
        "}"

        // Message box styling
        "QMessageBox { background-color: " + surfaceColor + "; }"
        "QMessageBox QLabel { color: " + fgColor + "; }"

        // Modern button styling
        "QAbstractButton { "
            "color: " + fgColor + "; "
            "border: none; "
            "padding: 6px 12px; "
            "background-color: " + surfaceColor + "; "
            "border-radius: 6px; "
        "}"
        "QAbstractButton:hover { "
            "background-color: " + hoverColor + "; "
        "}"
        "QAbstractButton:pressed { "
            "background-color: " + pressedColor + "; "
        "}"

        // Tree view and list styling
        "QAbstractItemView { "
            "color: " + fgColor + "; "
            "background-color: " + bgColor + "; "
            "outline: 0; "
            "border: none; "
        "}"
        "QAbstractItemView::item { "
            "color: " + fgColor + "; "
            "padding: 4px 8px; "
            "border-radius: 4px; "
        "}"
        "QAbstractItemView::item:hover { "
            "background-color: " + hoverColor + "; "
            "color: " + fgColor + "; "
        "}"
        "QAbstractItemView::item:selected { "
            "background-color: " + accentColor + "; "
            "color: " + bgColor + "; "
        "}"

        // Modern text input styling
        "QLineEdit { "
            "background-color: " + surfaceColor + "; "
            "color: " + fgColor + "; "
            "border: 1px solid " + hoverColor + "; "
            "border-radius: 6px; "
            "padding: 6px 10px; "
            "selection-background-color: " + accentColor + "; "
        "}"
        "QLineEdit:focus { "
            "border: 1px solid " + accentColor + "; "
        "}"

        // Label styling
        "QLabel { color: " + fgColor + "; }"

        // Text editor styling
        "QPlainTextEdit { "
            "color: " + fgColor + "; "
            "background-color: " + bgColor + "; "
            "selection-background-color: " + hoverColor + "; "
            "border: none; "
        "}"

        // Modern radio button styling
        "QRadioButton { color: " + fgColor + "; }"
        "QRadioButton::indicator { "
            "width: 14px; "
            "height: 14px; "
            "border-radius: 8px; "
        "}"
        "QRadioButton::indicator:checked { "
            "background-color: " + accentColor + "; "
            "border: 2px solid " + accentColor + "; "
        "}"
        "QRadioButton::indicator:unchecked { "
            "background-color: " + bgColor + "; "
            "border: 2px solid " + fgColor + "; "
        "}"

        // Checkbox styling
        "QCheckBox { color: " + fgColor + "; }"
        "QCheckBox::indicator { "
            "width: 16px; "
            "height: 16px; "
            "border-radius: 4px; "
            "border: 2px solid " + fgColor + "; "
            "background-color: " + bgColor + "; "
        "}"
        "QCheckBox::indicator:checked { "
            "background-color: " + accentColor + "; "
            "border: 2px solid " + accentColor + "; "
        "}"

        // Modern scrollbar styling
        "QScrollBar:vertical { "
            "background-color: " + bgColor + "; "
            "width: 12px; "
            "margin: 0; "
            "border-radius: 6px; "
        "}"
        "QScrollBar::handle:vertical { "
            "background-color: " + hoverColor + "; "
            "min-height: 30px; "
            "border-radius: 6px; "
            "margin: 2px; "
        "}"
        "QScrollBar::handle:vertical:hover { "
            "background-color: " + pressedColor + "; "
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
            "height: 0; "
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
            "background: none; "
        "}"
        "QScrollBar:horizontal { "
            "background-color: " + bgColor + "; "
            "height: 12px; "
            "margin: 0; "
            "border-radius: 6px; "
        "}"
        "QScrollBar::handle:horizontal { "
            "background-color: " + hoverColor + "; "
            "min-width: 30px; "
            "border-radius: 6px; "
            "margin: 2px; "
        "}"
        "QScrollBar::handle:horizontal:hover { "
            "background-color: " + pressedColor + "; "
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
            "width: 0; "
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { "
            "background: none; "
        "}"

        // Tooltip styling
        "QToolTip { "
            "background-color: " + surfaceColor + "; "
            "color: " + fgColor + "; "
            "border: 1px solid " + hoverColor + "; "
            "border-radius: 6px; "
            "padding: 4px 8px; "
        "}"

        // Splitter styling
        "QSplitter::handle { "
            "background-color: " + hoverColor + "; "
        "}"
        "QSplitter::handle:horizontal { width: 2px; }"
        "QSplitter::handle:vertical { height: 2px; }"

        // Status bar styling
        "QStatusBar { "
            "background-color: " + surfaceColor + "; "
            "color: " + fgColor + "; "
        "}"

        // Bottom toolbar styling
        "QWidget#backgroundBottom { "
            "background-color: " + surfaceColor + "; "
        "}"
    );

    ui->tabWidget->setTheme(bgColor, fgColor);
}
