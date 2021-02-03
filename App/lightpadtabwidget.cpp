#include "lightpadtabwidget.h"
#include "lightpadpage.h"
#include "mainwindow.h"

#include <QDebug>
#include <QTextEdit>
#include <QTabBar>
#include <QFontMetrics>
#include <QApplication>
#include <QSizePolicy>

LightpadTabWidget:: LightpadTabWidget(QWidget* parent) :
    QTabWidget(parent)  {

        QWidget::connect(tabBar(), &QTabBar::tabCloseRequested, this, [this] (int index) {
            removeTab(index);
        });

        setTabsClosable(true);
        setMovable(true);

        newTabButton = new QToolButton(this);
        newTabButton->setObjectName("AddTabButton");
        newTabButton->setIcon(QIcon(":/resources/icons/add_dark.png"));
        newTabButton->setIconSize(QSize(25, 25));
        newTabButton->setFixedSize(newTabButton->iconSize());

        addTab(new QWidget(), QString());
        setTabEnabled(0, false);
        tabBar()->setTabButton(0, QTabBar::RightSide, newTabButton);

        QWidget::connect(newTabButton, &QToolButton::clicked, this, [this] {
            addNewTab();
        });

        QWidget::connect(tabBar(), &QTabBar::tabMoved, this, [this](int from, int to){
            if (from == count() - 1)
                tabBar()->moveTab(to, from);
        });


        QWidget::connect(tabBar(), &QTabBar::currentChanged, this, [this](int index){
            if (index == count() - 1)
                setCurrentIndex(0);
        });

}

void LightpadTabWidget::resizeEvent(QResizeEvent *event) {

    QTabWidget::resizeEvent(event);
}

void LightpadTabWidget::tabRemoved(int index)
{
    Q_UNUSED(index);


    if (count() <= 1)
        addNewTab();
}

void LightpadTabWidget::addNewTab()
{
    if (mainWindow) {
        LightpadPage* newPage = new LightpadPage(this);
        newPage->setMainWindow(mainWindow);
        insertTab(count() - 1, newPage, unsavedDocumentLabel);
        setCurrentIndex(count() - 2);
    }
}

void LightpadTabWidget::setMainWindow(MainWindow *window)
{
    mainWindow = window;
    QList<LightpadPage*> pages = findChildren<LightpadPage*>();

    for (auto& page : pages)
        page->setMainWindow(window);

    if (count() <= 1)
        addNewTab();
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

        "QToolButton#AddTabButton {background: #262626;}"

        "QToolButton#AddTabButton:hover {background: #505050;}"

         "QTabBar::tab {"
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

void LightpadTabWidget::closeAllTabs()
{
    if (count() == 1)
          return;

    for (int i = count() - 2; i >= 0; i--)
        removeTab(i);
}

void LightpadTabWidget::closeCurrentTab()
{
    if (count() == 1)
        return;

    removeTab(currentIndex());
}

LightpadPage* LightpadTabWidget::getPage(int index)
{
    if (index < 0 || index >= count())
       return nullptr;

    return qobject_cast<LightpadPage*>(widget(index));
}

LightpadPage* LightpadTabWidget::getCurrentPage()
{
    LightpadPage* page = nullptr;

    if (qobject_cast<LightpadPage*>(currentWidget()) != 0)
        page = qobject_cast<LightpadPage*>(currentWidget());

    else if (findChild<LightpadPage*>("widget"))
        page = findChild<LightpadPage*>("widget");

    return page;
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

