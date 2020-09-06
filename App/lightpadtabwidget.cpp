#include "lightpadtabwidget.h"
#include "lightpadpage.h"
#include "mainwindow.h"

#include <QDebug>
#include <QTextEdit>
#include <QTabBar>
#include <QFontMetrics>
#include <QApplication>

LightpadTabWidget:: LightpadTabWidget(QWidget* parent) :
    QTabWidget(parent)  {

        QWidget::connect(tabBar(), &QTabBar::tabCloseRequested, this, [this] (int index) {
            removeTab(index);
        });

        newTabButton = new QToolButton(parent);
        newTabButton->setIcon(QIcon(":/resources/icons/add_dark.png"));
        newTabButton->setIconSize(QSize(25, 25));
        newTabButton->setFixedSize(newTabButton->iconSize());
        newTabButton->setStyleSheet("border: none;");

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

    if (count() == 0) {
        newTabButton->hide();
    }
}

void LightpadTabWidget::correctTabButtonPosition()
{
    QRect rect = tabBar()->tabRect(count() - 1);
    newTabButton->setGeometry(rect.x() + rect.width() + 3, rect.y() + 3, newTabButton->width(), newTabButton->height());

    if (tabBar()->tabRect(count() - 1).x() + tabBar()->tabRect(count() - 1).width() + 3 + newTabButton->width() > width())
        parentWidget()->parentWidget()->resize(parentWidget()->parentWidget()->width() + 3 + newTabButton->width(), parentWidget()->parentWidget()->height());
}

void LightpadTabWidget::addNewTab()
{
    if (mainWindow) {
        LightpadPage* newPage = new LightpadPage(this);
        newPage->setMainWindow(mainWindow);
        insertTab(count(), newPage, "Unsaved Document");
        correctTabButtonPosition();
    }
}

void LightpadTabWidget::setMainWindow(MainWindow *window)
{
    mainWindow = window;
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

         "QTabBar::tab {"
            "height: " + QString::number(newTabButton->height() + 2) + ";"
            "color:" + foregroundColor + ";"
            "margin: 0 -2px;"
            "padding: 1px 5px;"
            "background-color: #262626;"
         "}"

            "QTabWidget#tabWidget {background-color:  " +  backgroundColor  +"; }");

}

