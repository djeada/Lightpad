#ifndef SPLITEDITORCONTAINER_H
#define SPLITEDITORCONTAINER_H

#include <QWidget>
#include <QSplitter>
#include <QList>
#include <QPointer>

class LightpadTabWidget;
class MainWindow;

/**
 * @brief Orientation for splitting editors
 */
enum class SplitOrientation {
    Horizontal,
    Vertical
};

/**
 * @brief Container for managing split editor views
 * 
 * This class manages multiple editor groups that can be split horizontally
 * or vertically. Each group contains a LightpadTabWidget for managing tabs.
 */
class SplitEditorContainer : public QWidget {
    Q_OBJECT

public:
    explicit SplitEditorContainer(QWidget* parent = nullptr);
    ~SplitEditorContainer();
    void adoptTabWidget(LightpadTabWidget* tabWidget);

    /**
     * @brief Set the main window reference
     */
    void setMainWindow(MainWindow* window);

    /**
     * @brief Get the currently focused tab widget
     */
    LightpadTabWidget* currentTabWidget() const;

    /**
     * @brief Get all tab widgets in the container
     */
    QList<LightpadTabWidget*> allTabWidgets() const;

    /**
     * @brief Get the number of editor groups
     */
    int groupCount() const;

    /**
     * @brief Split the current editor view
     * @param orientation The direction to split (horizontal or vertical)
     * @return The newly created tab widget, or nullptr on failure
     */
    LightpadTabWidget* split(SplitOrientation orientation);

    /**
     * @brief Split horizontally (side by side)
     */
    LightpadTabWidget* splitHorizontal();

    /**
     * @brief Split vertically (one above the other)
     */
    LightpadTabWidget* splitVertical();

    /**
     * @brief Close the current editor group
     * @return true if closed successfully
     */
    bool closeCurrentGroup();

    /**
     * @brief Move focus to the next editor group
     */
    void focusNextGroup();

    /**
     * @brief Move focus to the previous editor group
     */
    void focusPreviousGroup();

    /**
     * @brief Check if there are multiple groups
     */
    bool hasSplits() const;

    /**
     * @brief Reset to single editor view
     */
    void unsplitAll();

signals:
    /**
     * @brief Emitted when the focused editor group changes
     */
    void currentGroupChanged(LightpadTabWidget* tabWidget);

    /**
     * @brief Emitted when a split is created or removed
     */
    void splitCountChanged(int count);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onTabWidgetFocused();

private:
    void setupUI();
    LightpadTabWidget* createTabWidget();
    void updateFocus(LightpadTabWidget* tabWidget);
    void cleanupEmptySplitters();
    QSplitter* findParentSplitter(QWidget* widget) const;
    int findTabWidgetIndex(LightpadTabWidget* tabWidget) const;

    MainWindow* m_mainWindow;
    QSplitter* m_rootSplitter;
    QList<QPointer<LightpadTabWidget>> m_tabWidgets;
    QPointer<LightpadTabWidget> m_currentTabWidget;
};

#endif // SPLITEDITORCONTAINER_H
