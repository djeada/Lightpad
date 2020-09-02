#include "lightpadtabwidget.h"
#include "lightpadpage.h"

#include <QDebug>
#include <QTextEdit>
#include <QTabBar>

LightpadTabWidget:: LightpadTabWidget(QWidget* parent) :
    QTabWidget(parent)  {

        QWidget::connect(tabBar(), &QTabBar::tabCloseRequested, this, [this] (int index) {
            removeTab(index);
        });

        newTabButton = new QToolButton(parent);
        newTabButton->setIcon(QIcon(":/icons/add.svg"));
        newTabButton->setIconSize(QSize(25, 25));
        newTabButton->setFixedSize(newTabButton->iconSize());
        newTabButton->setStyleSheet("border: none;");

        QWidget::connect(newTabButton, &QToolButton::clicked, this, [this] {
            insertTab(count(), new LightpadPage(), "Unsaved Document");
            correctTabButtonPosition();
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
}

void LightpadTabWidget::correctTabButtonPosition()
{
    QRect rect = tabBar()->tabRect(count() - 1);
    newTabButton->setGeometry(rect.x() + rect.width() + 3, rect.y() + 3, newTabButton->width(), newTabButton->height());

    if (tabBar()->tabRect(count() - 1).x() + tabBar()->tabRect(count() - 1).width() + 3 + newTabButton->width() > width())
        parentWidget()->parentWidget()->resize(parentWidget()->parentWidget()->width() + 3 + newTabButton->width(), parentWidget()->parentWidget()->height());

  //  else if (tabBar()->tabRect(count() - 1).x() + tabBar()->tabRect(count() - 1).width()*2 < newTabButton->x())
      //  newTabButton->setGeometry(tabBar()->tabRect(count() - 1).x() + tabBar()->tabRect(count() - 1).width() + 3, newTabButton->y(), newTabButton->width(), newTabButton->height());
}

