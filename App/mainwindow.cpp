#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "lightpadpage.h"

#include <QDebug>
#include <QStackedWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QBoxLayout>
#include <QStringListModel>


static void loadLanguageExtensions(QMap<QString, QString>& map) {
    QFile TextFile(":/highlight/LanguageToExtension.txt");

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

Popup::Popup(QWidget* parent) :
    QDialog(parent) {
        setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
        QStringListModel* model = new QStringListModel(this);
        listView = new ListView(this);

        QStringList List = QDir(":/highlight").entryList(QStringList(), QDir::Dirs);
        model->setStringList(List);

        listView->setModel(model);
        listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(listView);
        layout->setContentsMargins(0, 0, 0, 0);

        connect(listView,SIGNAL(clicked(QModelIndex)),this,SLOT(on_listView_clicked(QModelIndex)));

        show();
}

void Popup::on_listView_clicked(const QModelIndex &index){

    QString lang = index.data().toString();

    if (qobject_cast<MainWindow*>(parentWidget()) != 0)
        qobject_cast<MainWindow*>(parentWidget())->updateFileExtension(lang);

    close();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    popup(nullptr),
    highlightLanguage(""),
    findReplacePanel(nullptr) {
        QApplication::instance()->installEventFilter(this);
        ui->setupUi(this);
        show();
        ui->tabWidget->correctTabButtonPosition();
        setWindowTitle("LightPad");
        loadLanguageExtensions(langToExt);
}

void MainWindow::updateFileExtension(QString lang)
{
    QString ext = langToExt.value(lang);
    int tabIndex = ui->tabWidget->currentIndex();
    QString tabText = ui->tabWidget->tabText(tabIndex);

    if (windowTitle().contains(".")) {
        setWindowTitle(windowTitle().left(windowTitle().lastIndexOf(".")) + ext);
        ui->tabWidget->setTabText(tabIndex, tabText.left(tabText.lastIndexOf(".")) + ext);
    }

    else {
        setWindowTitle(windowTitle() + ext);
        ui->tabWidget->setTabText(tabIndex, tabText + ext);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object);

    switch (event->type()){
            case QEvent::KeyPress: {

                QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

                if (keyEvent->matches(QKeySequence::Undo)) {
                        undo();
                        return true;
                }

                else if (keyEvent->matches(QKeySequence::Redo)) {
                        redo();
                        return true;
                }

                else if (keyEvent->matches(QKeySequence::ZoomIn)) {
                    on_actionIncrease_Font_Size_triggered();
                    return true;
                }

                else if (keyEvent->matches(QKeySequence::ZoomOut)) {
                    on_actionDecrease_Font_Size_triggered();
                    return true;
                }

                else if (keyEvent->matches(QKeySequence::Save)) {
                    on_actionSave_triggered();
                    return true;
                }


                else if (keyEvent->matches(QKeySequence::SaveAs)) {
                    on_actionSave_as_triggered();
                    return true;
                }

                else if (keyEvent->key() == Qt::Key_Alt) {
                    on_actionToggle_Menu_Bar_triggered();
                    return true;
                }


                return false;
            }

             default:
                return false;
    }

    return false;
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
    if (getCurrentTextArea())
        getCurrentTextArea()->increaseFontSize();
}

void MainWindow::on_actionDecrease_Font_Size_triggered()
{
    if (getCurrentTextArea())
        getCurrentTextArea()->decreaseFontSize();
}

void MainWindow::on_actionReset_Font_Size_triggered()
{
    if (getCurrentTextArea())
        getCurrentTextArea()->changeFontSize(QFontMetrics(QApplication::font()).height());
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

}

void MainWindow::on_actionOpen_File_triggered()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open Document"), QDir::homePath());

    if (filePath.isEmpty())
        return;

    open(filePath);
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
        qobject_cast<QBoxLayout*>(ui->tabWidget->currentWidget()->layout())->addWidget(findReplacePanel, 0);
    }

    findReplacePanel->setVisible(!findReplacePanel->isVisible() || findReplacePanel->isOnlyFind() != onlyFind);
    findReplacePanel->setReplaceVisibility(!onlyFind);
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
    if (!popup) {
        Popup* popup = new Popup(this);
        QPoint point = mapToGlobal(ui->languageHighlight->pos());
        popup->setGeometry(point.x(), point.y() - popup->height(), popup->width(), popup->height());
    }

    else if (popup->isHidden())
        popup->show();

    else
        popup->hide();
}
