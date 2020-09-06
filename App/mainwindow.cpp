#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "lightpadpage.h"

#include <QDebug>
#include <QStackedWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QBoxLayout>
#include <QStringListModel>

const int defaultTabWidth = 4;
const int defaultFontSize = 12;

static void loadLanguageExtensions(QMap<QString, QString>& map) {
    QFile TextFile(":/resources/highlight/LanguageToExtension.txt");

    if (TextFile.open(QIODevice::ReadOnly)) {
        while (!TextFile.atEnd()) {
                QString line = TextFile.readLine();
                QStringList words = line.split(" ");
                if (words.size() == 2)
                    map.insert(words[0], words[1]);
       }
    }

    TextFile.close();
}

ListView::ListView(QWidget *parent):
    QListView(parent) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

QSize ListView::sizeHint() const {

    if (model()->rowCount() == 0)
        return QSize(width(), 0);

    int nToShow = 10 < model()->rowCount() ? 10 : model()->rowCount();
    return QSize(width(), nToShow*sizeHintForRow(0));
}

Popup::Popup(QStringList list, QWidget* parent) :
    QDialog(parent),
    list(list){
        setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
        QStringListModel* model = new QStringListModel(this);
        listView = new ListView(this);

        model->setStringList(list);

        listView->setModel(model);
        listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(listView);
        layout->setContentsMargins(0, 0, 0, 0);

        show();
}

class PopupLanguageHighlight : public Popup
{

    public:
        PopupLanguageHighlight(QStringList list, QWidget* parent = nullptr) :
            Popup(list, parent) {

                QObject::connect(listView, &QListView::clicked, this, [&] (const QModelIndex &index) {

                    QString lang = index.data().toString();


                    MainWindow* mainWindow = qobject_cast<MainWindow*>(parentWidget());
                    if (mainWindow != 0) {
                        mainWindow->updateFileExtension(lang);
                        mainWindow->setLanguageHighlightLabel(lang);
                     }

                    close();

                });
        }
};


class PopupTabWidth : public Popup
{

    public:
        PopupTabWidth(QStringList list, QWidget* parent = nullptr) :
            Popup(list, parent) {

                QObject::connect(listView, &QListView::clicked, this, [&] (const QModelIndex &index) {


                    QString width = index.data().toString();

                    MainWindow* mainWindow = qobject_cast<MainWindow*>(parentWidget());
                    if (mainWindow != 0) {
                        mainWindow->setTabWidthLabel("Tab Width: " + width);
                        mainWindow->setTabWidth(width.toInt());
                     }

                    close();

                });
        }
};


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    popupHighlightLanguage(nullptr),
    popupTabWidth(nullptr),
    highlightLanguage(""),
    findReplacePanel(nullptr),
    fontSize(defaultFontSize),
    tabWidth(defaultTabWidth) {
        QApplication::instance()->installEventFilter(this);
        ui->setupUi(this);
        show();
        ui->tabWidget->correctTabButtonPosition();
        ui->tabWidget->setMainWindow(this);

        if (getCurrentTextArea())
            getCurrentTextArea()->setMainWindow(this);

        setWindowTitle("LightPad");
        loadLanguageExtensions(langToExt);

        QObject::connect(ui->tabWidget, &QTabWidget::currentChanged, this, [&] (int index) {
            setMainWindowTitle(ui->tabWidget->tabText(index));
        });

        ui->magicButton->setIconSize(0.8*ui->magicButton->size());
        setTabWidth(defaultTabWidth);

}

void MainWindow::updateFileExtension(QString lang)
{
    if (getCurrentTextArea())
        getCurrentTextArea()->updateSyntaxHighlightTags(":/resources/highlight/" + lang + "/0.txt");

    /*
    QString ext = langToExt.value(lang);
    int tabIndex = ui->tabWidget->currentIndex();
    QString tabText = ui->tabWidget->tabText(tabIndex);

    if (windowTitle().contains(".")) {
        setWindowTitle(windowTitle().left(windowTitle().lastIndexOf(".")) + ext);

        tabText =  tabText.left(tabText.lastIndexOf(".")) + ext;
        ui->tabWidget->insertTab(tabIndex, ui->tabWidget->widget(tabIndex), tabText);
    }

    else {
        setWindowTitle(windowTitle() + ext);
        ui->tabWidget->setTabText(tabIndex, tabText + ext);
    }*/
}

void MainWindow::setRowCol(int row, int col)
{
    ui->rowCol->setText("Ln " + QString::number(row) + ", Col " +  QString::number(col));
}

void MainWindow::setTabWidthLabel(QString text)
{
     ui->tabWidth->setText(text);
}

void MainWindow::setLanguageHighlightLabel(QString text)
{
     ui->languageHighlight->setText(text);
}

MainWindow::~MainWindow()
{
    delete ui;
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

}

int MainWindow::getTabWidth()
{
    return tabWidth;
}

int MainWindow::getFontSize()
{
    return fontSize;
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

TextArea *MainWindow::getCurrentTextArea()
{
    if (ui->tabWidget->currentWidget()->findChild<LightpadPage*>("widget"))
        return ui->tabWidget->currentWidget()->findChild<LightpadPage*>("widget")->getTextArea();

    else if (ui->tabWidget->currentWidget()->findChild<TextArea*>(""))
        return ui->tabWidget->currentWidget()->findChild<TextArea*>("");

    return nullptr;
}

void MainWindow::setTabWidth(int width) {
    QList<TextArea*> textAreas = ui->tabWidget->findChildren<TextArea*>();
    foreach (TextArea* textArea, textAreas)
       textArea->setTabWidth(width);

    tabWidth = width;
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
    QList<TextArea*> textAreas = ui->tabWidget->findChildren<TextArea*>();
    foreach (TextArea* textArea, textAreas)
       textArea->increaseFontSize();

    fontSize = getCurrentTextArea()->fontSize();

}

void MainWindow::on_actionDecrease_Font_Size_triggered()
{
    QList<TextArea*> textAreas = ui->tabWidget->findChildren<TextArea*>();
    foreach (TextArea* textArea, textAreas)
       textArea->decreaseFontSize();

    fontSize = getCurrentTextArea()->fontSize();
}

void MainWindow::on_actionReset_Font_Size_triggered()
{
    QList<TextArea*> textAreas = ui->tabWidget->findChildren<TextArea*>();
    foreach (TextArea* textArea, textAreas)
       textArea->setFontSize(defaultFontSize);

    fontSize = getCurrentTextArea()->fontSize();
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
    while (ui->tabWidget->currentIndex() > -1)
     ui->tabWidget->removeTab(ui->tabWidget->currentIndex());
}

void MainWindow::on_actionFind_in_file_triggered()
{
    showFindReplace();
}

void MainWindow::on_actionNew_File_triggered()
{
    ui->tabWidget->addNewTab();
}

void MainWindow::on_actionOpen_File_triggered()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open Document"), QDir::homePath());

    if (filePath.isEmpty() || !QFileInfo(filePath).exists())
        return;

    open(filePath);

    if (ui->tabWidget->count() == 0) {
        ui->tabWidget->addNewTab();
        ui->tabWidget->ensureNewTabButtonVisible();
    }


    QString fileName = QFileInfo(filePath).fileName();

    int tabIndex = ui->tabWidget->currentIndex();
    QString tabText = ui->tabWidget->tabText(tabIndex);

    setMainWindowTitle(fileName);
    ui->tabWidget->setTabText(tabIndex, fileName);
    ui->tabWidget->correctTabButtonPosition();

    LightpadPage* page = nullptr;

    if (qobject_cast<LightpadPage*>(ui->tabWidget->currentWidget()) != 0)
        page = qobject_cast<LightpadPage*>(ui->tabWidget->currentWidget());

    else if (ui->tabWidget->findChild<LightpadPage*>("widget"))
        page =ui->tabWidget->findChild<LightpadPage*>("widget");

    if (page) {
        page->setTreeViewVisible(true);
        page->setModelRootIndex(QFileInfo(filePath).absoluteDir().path());
    }
}

void MainWindow::on_actionSave_triggered()
{

    if (windowFilePath().isEmpty()) {
        on_actionSave_as_triggered();
        return;
    }

    save(windowFilePath());
}

void MainWindow::on_actionSave_as_triggered()
{
    //tr("JavaScript Documents (*.js)")
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Document"), QDir::homePath());

    if(filePath.isEmpty())
        return;

    setWindowFilePath(filePath);
    on_actionSave_triggered();
}

void MainWindow::open(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't open file."));
        return;
    }

    setWindowFilePath(filePath);

    if ( getCurrentTextArea())
      getCurrentTextArea()->setPlainText(QString::fromUtf8(file.readAll()));
}

void MainWindow::save(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QFile::WriteOnly|QFile::Truncate|QFile::Text))
        return;

    if (getCurrentTextArea()) {
        file.write(getCurrentTextArea()->toPlainText().toUtf8());
        getCurrentTextArea()->document()->setModified(false);
    }
}

void MainWindow::showFindReplace(bool onlyFind)
{
    if (!findReplacePanel) {
        findReplacePanel = new FindReplacePanel(onlyFind);
        QBoxLayout* layout = qobject_cast<QBoxLayout*>(ui->centralwidget->layout());

        if (layout != 0)
           layout->insertWidget(layout->count() - 1, findReplacePanel, 0);
    }

    findReplacePanel->setVisible(!findReplacePanel->isVisible() || findReplacePanel->isOnlyFind() != onlyFind);
    findReplacePanel->setOnlyFind(onlyFind);

    if (findReplacePanel->isVisible() && getCurrentTextArea()){
        findReplacePanel->setReplaceVisibility(!onlyFind);
        findReplacePanel->setDocument(getCurrentTextArea()->document());
    }
}

void MainWindow::setMainWindowTitle(QString title)
{
    setWindowTitle(title + " - Lightpad");
}

void MainWindow::on_actionToggle_Menu_Bar_triggered()
{
    ui->menubar->setVisible(!ui->menubar->isVisible());
}

void MainWindow::on_actionReplace_in_file_triggered()
{
    showFindReplace(false);
}

void MainWindow::on_languageHighlight_clicked()
{
    if (!popupHighlightLanguage) {
        Popup* popupHighlightLanguage = new  PopupLanguageHighlight(QDir(":/resources/highlight").entryList(QStringList(), QDir::Dirs), this);
        QPoint point = mapToGlobal(ui->languageHighlight->pos());
        popupHighlightLanguage->setGeometry(point.x(), point.y() - 2*popupHighlightLanguage->height() + height(), popupHighlightLanguage->width(), popupHighlightLanguage->height());
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
        QMessageBox::information(this, tr("About Geist"), license, QMessageBox::Close);
        TextFile.close();
    }

}

void MainWindow::on_tabWidth_clicked()
{


    if (!popupTabWidth) {
        Popup* popupTabWidth = new  PopupTabWidth(QStringList({"2", "4", "8"}), this);
        QPoint point = mapToGlobal(ui->tabWidth->pos());
        popupTabWidth->setGeometry(point.x(), point.y() - 2*popupTabWidth->height() + height(), popupTabWidth->width(), popupTabWidth->height());
    }

    else if (popupTabWidth->isHidden())
        popupTabWidth->show();

    else
        popupTabWidth->hide();
}
