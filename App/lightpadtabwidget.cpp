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

        setStyleSheet(
            "QTabBar::tab {background-color: rgb(111,0,0); border: 2px solid #C4C4C3; padding: 2px;} \
            QTabBar::tab:hover { background-color: rgb(255, 255, 255);}    \
            QTabBar::tab:selected {background-color: rgb(255, 255, 255); } \
            QTabBar::tab:!selected { margin-top: 2px; } \
            QTabWidget::pane { border: 0; } \
            QWidget { background-color: rgb(5, 0, 17); } ");

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

