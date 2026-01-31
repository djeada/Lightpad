#include "lightpadtabwidget.h"
#include "lightpadpage.h"
#include "../ui/mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QFontMetrics>
#include <QSizePolicy>
#include <QTabBar>
#include <QTextEdit>

LightpadTabWidget::LightpadTabWidget(QWidget* parent)
    : QTabWidget(parent)
{

    QWidget::connect(tabBar(), &QTabBar::tabCloseRequested, this, [this](int index) {
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

    QWidget::connect(tabBar(), &QTabBar::tabMoved, this, [this](int from, int to) {
        if (from == count() - 1)
            tabBar()->moveTab(to, from);
    });

    QWidget::connect(tabBar(), &QTabBar::currentChanged, this, [this](int index) {
        if (index == count() - 1)
            setCurrentIndex(0);
    });
}

void LightpadTabWidget::resizeEvent(QResizeEvent* event)
{

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

void LightpadTabWidget::setMainWindow(MainWindow* window)
{
    mainWindow = window;
    QList<LightpadPage*> pages = findChildren<LightpadPage*>();

    for (auto& page : pages)
        page->setMainWindow(window);

    if (count() <= 1)
        addNewTab();
}

void LightpadTabWidget::setTheme(const QString& backgroundColor,
    const QString& foregroundColor,
    const QString& surfaceColor,
    const QString& hoverColor,
    const QString& accentColor,
    const QString& borderColor)
{
    setStyleSheet(
        // Modern scrollbar styling
        "QScrollBar:vertical { background: transparent; }"
        "QScrollBar:horizontal { background: transparent; }"

        // Tab bar container with clean background
        "QTabBar { "
            "background: " + backgroundColor + "; "
            "qproperty-drawBase: 0; "
        "}"

        // Individual tabs - modern flat design with subtle indicators
        "QTabBar::tab { "
            "color: #8b949e; "
            "background-color: " + backgroundColor + "; "
            "padding: 10px 18px; "
            "margin: 4px 2px 0px 2px; "
            "border-top-left-radius: 8px; "
            "border-top-right-radius: 8px; "
            "border: 1px solid transparent; "
            "border-bottom: none; "
            "font-size: 13px; "
        "}"

        // Active/selected tab with accent indicator
        "QTabBar::tab:selected { "
            "color: " + foregroundColor + "; "
            "background-color: " + surfaceColor + "; "
            "border: 1px solid " + borderColor + "; "
            "border-bottom: 2px solid " + accentColor + "; "
        "}"

        // Hover state for tabs
        "QTabBar::tab:hover:!selected { "
            "color: " + foregroundColor + "; "
            "background-color: " + hoverColor + "; "
        "}"

        // Close button on tabs - subtle and minimal
        "QTabBar::close-button { "
            "image: url(:/resources/icons/close_dark.png); "
            "subcontrol-position: right; "
            "padding: 2px; "
            "border-radius: 4px; "
        "}"
        "QTabBar::close-button:hover { "
            "background-color: " + hoverColor + "; "
        "}"

        // Add tab button styling - clean and minimal
        "QToolButton#AddTabButton { "
            "background: " + backgroundColor + "; "
            "border-radius: 6px; "
            "padding: 4px; "
            "border: 1px solid transparent; "
        "}"
        "QToolButton#AddTabButton:hover { "
            "background: " + hoverColor + "; "
            "border: 1px solid " + borderColor + "; "
        "}"

        // Tab widget pane - seamless integration
        "QTabWidget::pane { "
            "border: none; "
            "background-color: " + backgroundColor + "; "
        "}"
        "QTabWidget#tabWidget { "
            "background-color: " + backgroundColor + "; "
        "}"
    );
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
