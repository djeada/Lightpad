#include "spliteditorcontainer.h"
#include "../../core/lightpadtabwidget.h"
#include "../mainwindow.h"
#include "../../core/logging/logger.h"

#include <QVBoxLayout>
#include <QFocusEvent>
#include <QApplication>
#include <algorithm>

SplitEditorContainer::SplitEditorContainer(QWidget* parent)
    : QWidget(parent)
    , m_mainWindow(nullptr)
    , m_rootSplitter(nullptr)
    , m_currentTabWidget(nullptr)
{
    setupUI();
}

SplitEditorContainer::~SplitEditorContainer()
{
}

void SplitEditorContainer::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_rootSplitter = new QSplitter(Qt::Horizontal, this);
    m_rootSplitter->setHandleWidth(2);
    m_rootSplitter->setChildrenCollapsible(false);
    layout->addWidget(m_rootSplitter);

    // Install application-level event filter once for focus tracking
    QApplication::instance()->installEventFilter(this);

    // Create initial tab widget
    LightpadTabWidget* initialTabWidget = createTabWidget();
    m_rootSplitter->addWidget(initialTabWidget);
    m_currentTabWidget = initialTabWidget;
}

void SplitEditorContainer::setMainWindow(MainWindow* window)
{
    m_mainWindow = window;
    
    // Update existing tab widgets
    for (LightpadTabWidget* tabWidget : m_tabWidgets) {
        if (tabWidget) {
            tabWidget->setMainWindow(m_mainWindow);
        }
    }
}

LightpadTabWidget* SplitEditorContainer::currentTabWidget() const
{
    return m_currentTabWidget;
}

QList<LightpadTabWidget*> SplitEditorContainer::allTabWidgets() const
{
    QList<LightpadTabWidget*> result;
    for (const QPointer<LightpadTabWidget>& ptr : m_tabWidgets) {
        if (ptr) {
            result.append(ptr);
        }
    }
    return result;
}

int SplitEditorContainer::groupCount() const
{
    int count = 0;
    for (const QPointer<LightpadTabWidget>& ptr : m_tabWidgets) {
        if (ptr) {
            count++;
        }
    }
    return count;
}

LightpadTabWidget* SplitEditorContainer::split(SplitOrientation orientation)
{
    if (!m_currentTabWidget) {
        LOG_WARNING("Cannot split: no current tab widget");
        return nullptr;
    }

    Qt::Orientation qtOrientation = (orientation == SplitOrientation::Horizontal) 
        ? Qt::Horizontal 
        : Qt::Vertical;

    // Find the parent splitter of the current tab widget
    QSplitter* parentSplitter = findParentSplitter(m_currentTabWidget);
    if (!parentSplitter) {
        LOG_ERROR("Cannot find parent splitter for current tab widget");
        return nullptr;
    }

    // Get the index of the current tab widget in its parent splitter
    int index = parentSplitter->indexOf(m_currentTabWidget);
    if (index == -1) {
        LOG_ERROR("Current tab widget not found in parent splitter");
        return nullptr;
    }

    // Create new tab widget
    LightpadTabWidget* newTabWidget = createTabWidget();

    // If parent splitter has same orientation, just insert the new widget
    if (parentSplitter->orientation() == qtOrientation) {
        parentSplitter->insertWidget(index + 1, newTabWidget);
    } else {
        // Need to create a new splitter to hold both widgets
        QSplitter* newSplitter = new QSplitter(qtOrientation, this);
        newSplitter->setHandleWidth(2);
        newSplitter->setChildrenCollapsible(false);

        // Get the sizes before removing
        QList<int> parentSizes = parentSplitter->sizes();

        // Remove current widget from parent and add to new splitter
        m_currentTabWidget->setParent(nullptr);
        newSplitter->addWidget(m_currentTabWidget);
        newSplitter->addWidget(newTabWidget);

        // Insert new splitter at the same position
        parentSplitter->insertWidget(index, newSplitter);

        // Restore parent sizes
        parentSplitter->setSizes(parentSizes);

        // Set equal sizes for the new splitter
        int size = (qtOrientation == Qt::Horizontal) ? newSplitter->width() / 2 : newSplitter->height() / 2;
        newSplitter->setSizes({size, size});
    }

    LOG_INFO(QString("Created new editor split (%1)")
        .arg(orientation == SplitOrientation::Horizontal ? "horizontal" : "vertical"));

    emit splitCountChanged(groupCount());

    // Focus the new tab widget
    updateFocus(newTabWidget);

    return newTabWidget;
}

LightpadTabWidget* SplitEditorContainer::splitHorizontal()
{
    return split(SplitOrientation::Horizontal);
}

LightpadTabWidget* SplitEditorContainer::splitVertical()
{
    return split(SplitOrientation::Vertical);
}

bool SplitEditorContainer::closeCurrentGroup()
{
    if (groupCount() <= 1) {
        LOG_DEBUG("Cannot close the last editor group");
        return false;
    }

    if (!m_currentTabWidget) {
        return false;
    }

    // Find next group to focus before removing
    int currentIndex = findTabWidgetIndex(m_currentTabWidget);
    LightpadTabWidget* nextFocus = nullptr;
    
    for (int i = currentIndex + 1; i < m_tabWidgets.size(); i++) {
        if (m_tabWidgets[i]) {
            nextFocus = m_tabWidgets[i];
            break;
        }
    }
    if (!nextFocus) {
        for (int i = currentIndex - 1; i >= 0; i--) {
            if (m_tabWidgets[i]) {
                nextFocus = m_tabWidgets[i];
                break;
            }
        }
    }

    // Close all tabs in current group
    m_currentTabWidget->closeAllTabs();

    // Remove from list and delete
    m_tabWidgets.removeAll(m_currentTabWidget);
    m_currentTabWidget->deleteLater();
    m_currentTabWidget = nullptr;

    // Cleanup empty splitters
    cleanupEmptySplitters();

    // Focus next group
    if (nextFocus) {
        updateFocus(nextFocus);
    }

    LOG_INFO("Closed editor group");
    emit splitCountChanged(groupCount());

    return true;
}

void SplitEditorContainer::focusNextGroup()
{
    QList<LightpadTabWidget*> widgets = allTabWidgets();
    if (widgets.size() <= 1) {
        return;
    }

    int currentIndex = widgets.indexOf(m_currentTabWidget);
    int nextIndex = (currentIndex + 1) % widgets.size();
    updateFocus(widgets[nextIndex]);
}

void SplitEditorContainer::focusPreviousGroup()
{
    QList<LightpadTabWidget*> widgets = allTabWidgets();
    if (widgets.size() <= 1) {
        return;
    }

    int currentIndex = widgets.indexOf(m_currentTabWidget);
    int prevIndex = (currentIndex - 1 + widgets.size()) % widgets.size();
    updateFocus(widgets[prevIndex]);
}

bool SplitEditorContainer::hasSplits() const
{
    return groupCount() > 1;
}

void SplitEditorContainer::unsplitAll()
{
    if (groupCount() <= 1) {
        return;
    }

    // Keep the first tab widget, close all others
    LightpadTabWidget* first = nullptr;
    for (QPointer<LightpadTabWidget>& ptr : m_tabWidgets) {
        if (ptr) {
            if (!first) {
                first = ptr;
            } else {
                ptr->closeAllTabs();
                ptr->deleteLater();
                ptr = nullptr;
            }
        }
    }

    // Remove null pointers using erase-remove idiom
    m_tabWidgets.erase(
        std::remove_if(m_tabWidgets.begin(), m_tabWidgets.end(), 
            [](const QPointer<LightpadTabWidget>& ptr) { return ptr.isNull(); }),
        m_tabWidgets.end());

    // Cleanup splitters and reset to single view
    cleanupEmptySplitters();

    if (first) {
        // Reparent to root splitter
        first->setParent(nullptr);
        
        // Clear root splitter
        while (m_rootSplitter->count() > 0) {
            QWidget* w = m_rootSplitter->widget(0);
            if (w && w != first) {
                w->deleteLater();
            }
            if (w) {
                w->setParent(nullptr);
            } else {
                break;  // Safety: exit if widget is null to prevent infinite loop
            }
        }
        
        m_rootSplitter->addWidget(first);
        updateFocus(first);
    }

    LOG_INFO("Reset to single editor view");
    emit splitCountChanged(groupCount());
}

bool SplitEditorContainer::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FocusIn) {
        // Check if a tab widget or its child received focus
        for (LightpadTabWidget* tabWidget : allTabWidgets()) {
            if (tabWidget && (watched == tabWidget || tabWidget->isAncestorOf(qobject_cast<QWidget*>(watched)))) {
                if (m_currentTabWidget != tabWidget) {
                    updateFocus(tabWidget);
                }
                break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SplitEditorContainer::onTabWidgetFocused()
{
    LightpadTabWidget* sender = qobject_cast<LightpadTabWidget*>(QObject::sender());
    if (sender && sender != m_currentTabWidget) {
        updateFocus(sender);
    }
}

LightpadTabWidget* SplitEditorContainer::createTabWidget()
{
    LightpadTabWidget* tabWidget = new LightpadTabWidget(this);
    
    if (m_mainWindow) {
        tabWidget->setMainWindow(m_mainWindow);
    }

    // Install event filter to track focus on this specific tab widget
    tabWidget->installEventFilter(this);

    m_tabWidgets.append(tabWidget);

    return tabWidget;
}

void SplitEditorContainer::updateFocus(LightpadTabWidget* tabWidget)
{
    if (m_currentTabWidget == tabWidget) {
        return;
    }

    m_currentTabWidget = tabWidget;
    
    if (tabWidget) {
        tabWidget->setFocus();
    }

    emit currentGroupChanged(tabWidget);
    LOG_DEBUG(QString("Editor group focus changed (total groups: %1)").arg(groupCount()));
}

void SplitEditorContainer::cleanupEmptySplitters()
{
    // Recursively find and remove empty splitters
    std::function<void(QSplitter*)> cleanup = [&](QSplitter* splitter) {
        if (!splitter || splitter == m_rootSplitter) {
            return;
        }

        // First, process child splitters
        for (int i = splitter->count() - 1; i >= 0; i--) {
            QSplitter* childSplitter = qobject_cast<QSplitter*>(splitter->widget(i));
            if (childSplitter) {
                cleanup(childSplitter);
            }
        }

        // If splitter is empty, remove it
        if (splitter->count() == 0) {
            splitter->deleteLater();
        }
        // If splitter has only one child, unwrap it
        else if (splitter->count() == 1) {
            QWidget* child = splitter->widget(0);
            QSplitter* parent = qobject_cast<QSplitter*>(splitter->parentWidget());
            if (parent) {
                int index = parent->indexOf(splitter);
                child->setParent(nullptr);
                parent->insertWidget(index, child);
                splitter->deleteLater();
            }
        }
    };

    // Process all child splitters of root
    for (int i = m_rootSplitter->count() - 1; i >= 0; i--) {
        QSplitter* childSplitter = qobject_cast<QSplitter*>(m_rootSplitter->widget(i));
        if (childSplitter) {
            cleanup(childSplitter);
        }
    }

    // Unwrap root if it has only one child splitter
    if (m_rootSplitter->count() == 1) {
        QSplitter* childSplitter = qobject_cast<QSplitter*>(m_rootSplitter->widget(0));
        if (childSplitter) {
            while (childSplitter->count() > 0) {
                QWidget* w = childSplitter->widget(0);
                w->setParent(nullptr);
                m_rootSplitter->addWidget(w);
            }
            childSplitter->deleteLater();
        }
    }
}

QSplitter* SplitEditorContainer::findParentSplitter(QWidget* widget) const
{
    if (!widget) {
        return nullptr;
    }

    QWidget* parent = widget->parentWidget();
    while (parent) {
        QSplitter* splitter = qobject_cast<QSplitter*>(parent);
        if (splitter) {
            return splitter;
        }
        parent = parent->parentWidget();
    }
    return nullptr;
}

int SplitEditorContainer::findTabWidgetIndex(LightpadTabWidget* tabWidget) const
{
    for (int i = 0; i < m_tabWidgets.size(); i++) {
        if (m_tabWidgets[i] == tabWidget) {
            return i;
        }
    }
    return -1;
}
