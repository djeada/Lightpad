#include "lightpadtabwidget.h"
#include "lightpadpage.h"

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
                 "QTabBar::tab { background: gray; color: white; padding: 10px; } "
                 "QTabBar::tab:selected { background: lightgray; } "
                 "QTabWidget::pane { border: 0; } "
                 "QWidget { background: lightgray; } ");

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
    LightpadPage* newPage = new LightpadPage();
    newPage->setMainWindow(mainWindow);

     //work in progress fonts
    //if(count() > 3)
    //    newPage->getTextArea()->setFontSize(qobject_cast<LightpadPage*>(widget(0))->getTextArea()->fontSize());

    insertTab(count(), newPage, "Unsaved Document");
    correctTabButtonPosition();
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

