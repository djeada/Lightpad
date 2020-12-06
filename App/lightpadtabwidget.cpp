#include "lightpadtabwidget.h"
#include "lightpadpage.h"
#include "mainwindow.h"

#include <QDebug>
#include <QTextEdit>
#include <QTabBar>
#include <QFontMetrics>
#include <QApplication>
#include <QSizePolicy>

class MyButton : public QToolButton
{
public:
    MyButton(QWidget* p) : QToolButton(p) { }

    void enterEvent(QEvent *event) {
        Q_UNUSED(event);

        move(x(), -1);

        setIconSize(1.2*QSize(buttonSize, buttonSize));
        setFixedSize(iconSize());
    }

    void leaveEvent(QEvent *event) {
        Q_UNUSED(event);

        move(x(), 3);

        setIconSize(QSize(buttonSize, buttonSize));
        setFixedSize(iconSize());
    }
};

LightpadTabWidget:: LightpadTabWidget(QWidget* parent) :
    QTabWidget(parent)  {

        QWidget::connect(tabBar(), &QTabBar::tabCloseRequested, this, [this] (int index) {
            removeTab(index);
        });

        newTabButton = new MyButton(parent);
        newTabButton->setIcon(QIcon(":/resources/icons/add_dark.png"));
        newTabButton->setIconSize(QSize(25, 25));
        newTabButton->setFixedSize(newTabButton->iconSize());

        QWidget::connect(newTabButton, &QToolButton::clicked, this, [this] {
            addNewTab();
        });

}

void LightpadTabWidget::resizeEvent(QResizeEvent *event) {

    QTabWidget::resizeEvent(event);

    if (tabBar()->tabRect(count() - 1).x() + tabBar()->tabRect(count() - 1).width() + 3 + newTabButton->width() > width())
     parentWidget()->parentWidget()->resize(parentWidget()->parentWidget()->width() + 3 + newTabButton->width(), parentWidget()->parentWidget()->height());
}

void LightpadTabWidget::tabRemoved(int index)
{
    Q_UNUSED(index);
    correctTabButtonPosition();

    //if (count() == 0)
    //    newTabButton->hide();
}

void LightpadTabWidget::correctTabButtonPosition()
{
    QRect rect = tabBar()->tabRect(count() - 1);
    newTabButton->setGeometry(rect.x() + rect.width() + 3, 3, newTabButton->width(), newTabButton->height());

    if (tabBar()->tabRect(count() - 1).x() + tabBar()->tabRect(count() - 1).width() + 3 + newTabButton->width() > width())
        parentWidget()->parentWidget()->resize(parentWidget()->parentWidget()->width() + 3 + newTabButton->width(), parentWidget()->parentWidget()->height());
}

void LightpadTabWidget::addNewTab()
{
    if (mainWindow) {
        LightpadPage* newPage = new LightpadPage(this);
        newPage->setMainWindow(mainWindow);
        insertTab(count(), newPage, unsavedDocumentLabel);
        correctTabButtonPosition();
    }
}

void LightpadTabWidget::setMainWindow(MainWindow *window)
{
    mainWindow = window;
    QList<LightpadPage*> pages = findChildren<LightpadPage*>();

    for (auto& page : pages)
        page->setMainWindow(window);
}

void LightpadTabWidget::ensureNewTabButtonVisible()
{
    newTabButton->show();
    correctTabButtonPosition();
}

void LightpadTabWidget::setTheme(QString backgroundColor, QString foregroundColor)
{

    setStyleSheet(
         "QScrollBar:vertical{background: " +  backgroundColor  +";}"

         "QScrollBar:horizontal{background: " +  backgroundColor  +";}"

         "QTabBar::tab:selected{ "
            "color: " + foregroundColor + ";"
            "border-bottom: 3px solid "  + foregroundColor + ";"
         "}"

         "QTabBar {background: " +  backgroundColor  +";}"

         "QTabBar::tab {"
            "height: " + QString::number(newTabButton->height() + 2) + ";"
            "color:" + foregroundColor + ";"
            "margin: 0 -2px;"
            "padding: 1px 5px;"
            "background-color: #262626;"
         "}"

        "QTabWidget#tabWidget {background-color:  " +  backgroundColor  +"; }");

}

void LightpadTabWidget::setFilePath(int index, QString filePath)
{
    if (index >= 0 && index < count()) {
        LightpadPage* page = qobject_cast<LightpadPage*>(widget(index));

        if (page)
            page->setFilePath(filePath);
    }
}

LightpadPage* LightpadTabWidget::getPage(int index)
{
    if (index < 0 || index >= count())
       return nullptr;

    return qobject_cast<LightpadPage*>(widget(index));
}

QString LightpadTabWidget::getFilePath(int index)
{
    if (index >= 0 && index < count()) {
        LightpadPage* page = qobject_cast<LightpadPage*>(widget(index));

        if (page)
           return page->getFilePath();
    }

    return "";
}

