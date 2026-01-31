#ifndef COMPLETIONWIDGET_H
#define COMPLETIONWIDGET_H

#include "completionitem.h"
#include "completionitemmodel.h"
#include "../settings/theme.h"
#include <QWidget>
#include <QListView>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @brief Popup widget for displaying completion suggestions
 * 
 * A custom popup widget that displays completion items with:
 * - Rich item rendering with icons
 * - Keyboard navigation
 * - Optional documentation panel
 */
class CompletionWidget : public QWidget {
    Q_OBJECT

public:
    explicit CompletionWidget(QWidget* parent = nullptr);
    void applyTheme(const Theme& theme);
    
    /**
     * @brief Set the completion items to display
     * @param items Items to show
     */
    void setItems(const QList<CompletionItem>& items);
    
    /**
     * @brief Show the popup at the specified position
     * @param position Global position for popup
     */
    void showAt(const QPoint& position);
    
    /**
     * @brief Hide the popup
     */
    void hide();
    
    /**
     * @brief Check if popup is visible
     */
    bool isVisible() const { return QWidget::isVisible(); }
    
    // Navigation
    void selectNext();
    void selectPrevious();
    void selectFirst();
    void selectLast();
    void selectPageDown();
    void selectPageUp();
    
    /**
     * @brief Get the currently selected item
     * @return Selected item, or invalid item if none selected
     */
    CompletionItem selectedItem() const;
    
    /**
     * @brief Get the selected index
     */
    int selectedIndex() const;
    
    /**
     * @brief Get number of items
     */
    int count() const { return m_model->count(); }
    
    // Configuration
    void setMaxVisibleItems(int count);
    void setShowDocumentation(bool show);
    void setShowIcons(bool show);

signals:
    /**
     * @brief Emitted when an item is selected (highlighted)
     */
    void itemSelected(const CompletionItem& item);
    
    /**
     * @brief Emitted when an item is accepted (Enter pressed)
     */
    void itemAccepted(const CompletionItem& item);
    
    /**
     * @brief Emitted when popup is cancelled (Escape pressed)
     */
    void cancelled();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onSelectionChanged();
    void onItemClicked(const QModelIndex& index);
    void onItemDoubleClicked(const QModelIndex& index);

private:
    void updateDocumentation();
    void adjustSize();
    
    CompletionItemModel* m_model;
    QListView* m_listView;
    QLabel* m_docLabel;
    QVBoxLayout* m_layout;
    
    int m_maxVisibleItems = 10;
    bool m_showDocumentation = true;
    bool m_showIcons = true;
};

#endif // COMPLETIONWIDGET_H
