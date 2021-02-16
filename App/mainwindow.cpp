#include <QDebug>
#include <QStackedWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QBoxLayout>
#include <QStringListModel>
#include <QPushButton>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "lightpadpage.h"
#include "shortcuts.h"
#include "textarea.h"
#include "findreplacepanel.h"
#include "popup.h"
#include "terminal.h"
#include "prefrences.h"
#include "runconfigurations.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    popupHighlightLanguage(nullptr),
    popupTabWidth(nullptr),
    prefrences(nullptr),
    highlightLanguage(""),
    findReplacePanel(nullptr),
    terminal(nullptr),
    font(QApplication::font()),
    fontSize(defaultFontSize),
    tabWidth(defaultTabWidth) {
        QApplication::instance()->installEventFilter(this);
        ui->setupUi(this);
        show();
        ui->tabWidget->setMainWindow(this);
        ui->magicButton->setIconSize(0.8*ui->magicButton->size());
        setupTextArea();
        setupTabWidget();
        setWindowTitle("LightPad");
        setTabWidth(tabWidth);
        setTheme(colors);
}

void MainWindow::setRowCol(int row, int col) {
    ui->rowCol->setText("Ln " + QString::number(row) + ", Col " +  QString::number(col));
}

void MainWindow::setTabWidthLabel(QString text) {
     ui->tabWidth->setText(text);

     if (prefrences)
         prefrences->setTabWidthLabel(text);
}

void MainWindow::setLanguageHighlightLabel(QString text) {
     ui->languageHighlight->setText(text);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::closeEvent( QCloseEvent* event ) {
    Q_UNUSED(event);

    if (prefrences) {
        prefrences->close();
     }
}

void MainWindow::keyPressEvent(QKeyEvent *keyEvent){

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

int MainWindow::getTabWidth() {
    return tabWidth;
}

int MainWindow::getFontSize() {
    return fontSize;
}

//use current page if empty
//else add new tab
void MainWindow::openFileAndAddToNewTab(QString filePath) {

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

void MainWindow::closeTabPage(QString filePath) {

    for (int i = 0; i < ui->tabWidget->count(); i++) {
        if (ui->tabWidget->getFilePath(i) == filePath)
            ui->tabWidget->removeTab(i);
    }
}

void MainWindow::on_actionToggle_Full_Screen_triggered() {

    if (isMaximized())
        showNormal();

    else
        showMaximized();
}

void MainWindow::on_actionQuit_triggered() {
    close();
}

void MainWindow::undo() {

    if (getCurrentTextArea())
        getCurrentTextArea()->undo();
}

void MainWindow::redo() {

    if (getCurrentTextArea())
        getCurrentTextArea()->redo();
}

TextArea *MainWindow::getCurrentTextArea() {

    if (ui->tabWidget->currentWidget()->findChild<LightpadPage*>("widget"))
        return ui->tabWidget->currentWidget()->findChild<LightpadPage*>("widget")->getTextArea();

    else if (ui->tabWidget->currentWidget()->findChild<TextArea*>(""))
        return ui->tabWidget->currentWidget()->findChild<TextArea*>("");

    return nullptr;
}

Theme MainWindow::getTheme() {
    return colors;
}

QFont MainWindow::getFont() {
    return font;
}

void MainWindow::setTabWidth(int width) {

    auto textAreas = ui->tabWidget->findChildren<TextArea*>();

    for (auto& textArea : textAreas)
       textArea->setTabWidth(width);

    tabWidth = width;
}

void MainWindow::on_actionToggle_Undo_triggered() {
    undo();
}

void MainWindow::on_actionToggle_Redo_triggered() {
    redo();
}

void MainWindow::on_actionIncrease_Font_Size_triggered()
{
    auto textAreas = ui->tabWidget->findChildren<TextArea*>();
    for (auto& textArea : textAreas)
       textArea->increaseFontSize();

    fontSize = getCurrentTextArea()->fontSize();

}

void MainWindow::on_actionDecrease_Font_Size_triggered() {

    auto textAreas = ui->tabWidget->findChildren<TextArea*>();
    for (auto& textArea : textAreas)
       textArea->decreaseFontSize();

    fontSize = getCurrentTextArea()->fontSize();
}

void MainWindow::on_actionReset_Font_Size_triggered() {

    auto textAreas = ui->tabWidget->findChildren<TextArea*>();
    for (auto& textArea : textAreas)
       textArea->setFontSize(defaultFontSize);

    fontSize = getCurrentTextArea()->fontSize();
}

void MainWindow::on_actionCut_triggered() {

    if (getCurrentTextArea())
        getCurrentTextArea()->cut();
}


void MainWindow::on_actionCopy_triggered() {

    if (getCurrentTextArea())
        getCurrentTextArea()->copy();
}

void MainWindow::on_actionPaste_triggered() {

    if (getCurrentTextArea())
        getCurrentTextArea()->paste();
}

void MainWindow::on_actionNew_Window_triggered() {

    new MainWindow();
}

void MainWindow::on_actionClose_Tab_triggered() {

    if (ui->tabWidget->currentIndex() > -1)
     ui->tabWidget->removeTab(ui->tabWidget->currentIndex());
}

void MainWindow::on_actionClose_All_Tabs_triggered()
{
     ui->tabWidget->closeAllTabs();
}

void MainWindow::on_actionFind_in_file_triggered() {

    showFindReplace(true);
}

void MainWindow::on_actionNew_File_triggered()
{
    ui->tabWidget->addNewTab();
}

void MainWindow::on_actionOpen_File_triggered() {

    auto filePath = QFileDialog::getOpenFileName(this, tr("Open Document"), QDir::homePath());

    openFileAndAddToNewTab(filePath);
}

void MainWindow::on_actionSave_triggered() {

    auto tabIndex = ui->tabWidget->currentIndex();
    auto filePath = ui->tabWidget->getFilePath(tabIndex);

    if (filePath.isEmpty()) {
        on_actionSave_as_triggered();
        return;
    }

    save(filePath);
}

void MainWindow::on_actionSave_as_triggered() {

    auto filePath = QFileDialog::getSaveFileName(this, tr("Save Document"), QDir::homePath());

    if (filePath.isEmpty())
        return;

    int tabIndex = ui->tabWidget->currentIndex();
    ui->tabWidget->setFilePath(tabIndex, filePath);

    save(filePath);
}

void MainWindow::open(const QString &filePath) {

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

void MainWindow::save(const QString &filePath) {

    QFile file(filePath);

    if (!file.open(QFile::WriteOnly|QFile::Truncate|QFile::Text))
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

void MainWindow::showFindReplace(bool onlyFind) {

    if (!findReplacePanel) {
        findReplacePanel = new FindReplacePanel(onlyFind);

        auto* layout = qobject_cast<QBoxLayout*>(ui->centralwidget->layout());

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
        case Dialog::runConfiguration:
            if (findChildren<RunConfigurations*>().isEmpty()) {
                auto configurationDialog = new RunConfigurations(this);

                connect(configurationDialog, &RunConfigurations::accepted, this, [&, configurationDialog]() {
                       auto scriptPath = configurationDialog->getScriptPath();
                       auto parameters = configurationDialog->getParameters();
                       qDebug() << scriptPath;
                       qDebug() << parameters;
                });
             }
            break;

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

void MainWindow::openShortcutsDialog()
{
    openDialog(Dialog::shortcuts);
}

void MainWindow::showTerminal() {

    auto page = ui->tabWidget->getCurrentPage();

    if (!page->scriptAssigned()) {
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
}

void MainWindow::setMainWindowTitle(QString title) {
    setWindowTitle(title + " - Lightpad");
}

void MainWindow::setTheme(Theme themeColors) {

    colors = themeColors;

    setStyleSheet(

        "QWidget { background-color: " + colors.backgroundColor.name() + ";}"

        "QMenu { color: " + colors.foregroundColor.name() + ";"
                 "selection-background-color: #404f4f;"
                 "border: 1px solid #404f4f;"
                 "border-radius: 3px 3px 3px 3px;}"

        "QMenuBar::item {color: " + colors.foregroundColor.name() + ";}"

        "QMessageBox QLabel {color: " + colors.foregroundColor.name() + ";}"

        "QAbstractButton { color: " + colors.foregroundColor.name() + ";"
                           "border: None;"
                           "padding: 5px;"
                           "background-color: " + colors.backgroundColor.name() + ";}"

        "QAbstractItemView {color: " + colors.foregroundColor.name() + "; outline: 0;}"

        "QAbstractItemView::item {color: " + colors.foregroundColor.name() + ";}"

        "QAbstractItemView::item:hover { background: #f3f3f3; color: #252424;}"

        "QAbstractItemView::item:selected { background: #bbdde6; }"

        "QAbstractButton:hover { background: rgb(85, 87, 83); border: 1; border-radius: 5;}"

        "QAbstractButton:pressed { background: rgb(46, 52, 54); border: 1; border-radius: 5;}"

        "QLineEdit {background: " + colors.foregroundColor.name() + ";}"

        "QLabel {color: " + colors.foregroundColor.name() + ";}"

        "QPlainTextEdit {color: " + colors.foregroundColor.name() + "; background-color: " + colors.backgroundColor.name() + "; }"

        "QRadioButton::indicator:checked { background-color: " + colors.foregroundColor.name() + ";"
                                           "border: 2px solid " + colors.foregroundColor.name() + ";"
                                           "border-radius: 6px; }"

        "QRadioButton::indicator:unchecked { background-color: " + colors.backgroundColor.name() + ";"
                                             "border: 2px solid " + colors.foregroundColor.name() + ";"
                                             "border-radius: 6px;}"
    );

    ui->tabWidget->setTheme(colors.backgroundColor.name(), colors.foregroundColor.name());
}

void MainWindow::setFont(QFont newFont) {

    font = newFont;

    auto textAreas = findChildren<TextArea*>();

    for (auto& textArea : textAreas)
        textArea->setFont(newFont);
}

void MainWindow::showLineNumbers(bool flag)
{
    auto textAreas = findChildren<TextArea*>();

    for (auto& textArea : textAreas)
        textArea->showLineNumbers(flag);
}

void MainWindow::highlihtCurrentLine(bool flag)
{
    auto textAreas = findChildren<TextArea*>();

    for (auto& textArea : textAreas)
        textArea->highlihtCurrentLine(flag);
}

void MainWindow::highlihtMatchingBracket(bool flag)
{
    auto textAreas = findChildren<TextArea*>();

    for (auto& textArea : textAreas)
        textArea->highlihtMatchingBracket(flag);
}

void MainWindow::runCurrentScript()
{
    on_actionSave_triggered();
    auto textArea = getCurrentTextArea();

    if (textArea && !textArea->changesUnsaved())
        showTerminal();
}

void MainWindow::setFilePathAsTabText(QString filePath) {

    auto fileName = QFileInfo(filePath).fileName();

    auto tabIndex = ui->tabWidget->currentIndex();
    auto tabText = ui->tabWidget->tabText(tabIndex);

    setMainWindowTitle(fileName);
    ui->tabWidget->setTabText(tabIndex, fileName);
}

void MainWindow::closeCurrentTab() {
    auto textArea = getCurrentTextArea();

    if (textArea && textArea->changesUnsaved())
        on_actionSave_triggered();

    ui->tabWidget->closeCurrentTab();
}

void MainWindow::setupTabWidget() {
    QObject::connect(ui->tabWidget, &QTabWidget::currentChanged, this, [&] (int index) {
        auto text = ui->tabWidget->tabText(index);
        setMainWindowTitle(text);

        if (!ui->menuRun->actions().empty())
            ui->menuRun->actions().front()->setText("Run " + text);
    });

    ui->tabWidget->currentChanged(0);
}

void MainWindow::setupTextArea() {

    if (getCurrentTextArea()) {
        getCurrentTextArea()->setMainWindow(this);
        getCurrentTextArea()->setFontSize(defaultFontSize);
        getCurrentTextArea()->setTabWidth(defaultTabWidth);
    }
}

void MainWindow::noScriptAssignedWarning()
{
    QMessageBox msgBox(this);
    msgBox.setText("No build script asociated with this file.");
    auto connectButton = msgBox.addButton(tr("Connect"), QMessageBox::ActionRole);

    msgBox.addButton(QMessageBox::Abort);
    msgBox.exec();

    if (msgBox.clickedButton() == connectButton)
        openConfigurationDialog();

    else
       msgBox.close();
}

void MainWindow::on_actionToggle_Menu_Bar_triggered()
{
    ui->menubar->setVisible(!ui->menubar->isVisible());
}

void MainWindow::on_languageHighlight_clicked() {

    if (!popupHighlightLanguage) {
        auto dir = QDir(":/resources/highlight").entryList(QStringList(), QDir::Dirs);
        auto popupHighlightLanguage = new  PopupLanguageHighlight(dir, this);
        auto point = mapToGlobal(ui->languageHighlight->pos());
        popupHighlightLanguage->setGeometry(point.x(), point.y() - 2*popupHighlightLanguage->height() + height(), popupHighlightLanguage->width(), popupHighlightLanguage->height());
    }

    else if (popupHighlightLanguage->isHidden())
        popupHighlightLanguage->show();

    else
        popupHighlightLanguage->hide();
}

void MainWindow::on_actionAbout_triggered() {

    QFile TextFile(":/resources/messages/About.txt");

    if (TextFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&TextFile);
        QString license = in.readAll();
        QMessageBox::information(this, tr("About Lightpad"), license, QMessageBox::Close);
        TextFile.close();
    }
}

void MainWindow::on_tabWidth_clicked() {

    if (!popupTabWidth) {
        auto* popupTabWidth = new  PopupTabWidth(QStringList({"2", "4", "8"}), this);
        auto point = mapToGlobal(ui->tabWidth->pos());
        popupTabWidth->setGeometry(point.x(), point.y() - 2*popupTabWidth->height() + height(), popupTabWidth->width(), popupTabWidth->height());
    }

    else if (popupTabWidth->isHidden())
        popupTabWidth->show();

    else
        popupTabWidth->hide();
}

void MainWindow::on_actionReplace_in_file_triggered() {
    showFindReplace(false);
}

void MainWindow::on_actionKeyboard_shortcuts_triggered() {
    openShortcutsDialog();
}

void MainWindow::on_actionPrefrences_triggered() {

    if (!prefrences) {
        prefrences = new Prefrences(this);

        connect(prefrences, &QObject::destroyed, this, [&] {
            prefrences = nullptr;
        });
    }
}

void MainWindow::on_runButton_clicked() {
    runCurrentScript();
}

void MainWindow::on_actionRun_file_name_triggered() {
    runCurrentScript();
}

void MainWindow::on_actionEdit_Configurations_triggered() {
    openConfigurationDialog();
}
