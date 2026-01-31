#ifndef TODOPANEL_H
#define TODOPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QMap>

struct TodoEntry {
    QString filePath;
    QString tag;
    QString message;
    int line;
};

/**
 * @brief TODO panel for displaying TODO/FIXME/NOTE items in the current document.
 */
class TodoPanel : public QWidget {
    Q_OBJECT

public:
    explicit TodoPanel(QWidget* parent = nullptr);
    ~TodoPanel();

    /**
     * @brief Update TODO items for the current file content
     */
    void setTodos(const QString& filePath, const QString& content);

    /**
     * @brief Clear all TODO items
     */
    void clearAll();

    /**
     * @brief Get total todo item count
     */
    int totalCount() const;
    int todoCount() const;
    int fixmeCount() const;
    int noteCount() const;

signals:
    /**
     * @brief Emitted when user clicks on a todo item
     */
    void todoClicked(const QString& filePath, int line);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onFilterChanged(int index);
    void onSearchChanged(const QString& text);

private:
    void setupUI();
    void updateCounts();
    void rebuildTree();
    bool passesFilter(const TodoEntry& entry) const;
    bool passesSearch(const TodoEntry& entry) const;
    QString tagIcon(const QString& tag) const;
    QString displayNameForPath(const QString& filePath) const;

    QTreeWidget* m_tree;
    QLabel* m_statusLabel;
    QComboBox* m_filterCombo;
    QLineEdit* m_searchEdit;

    QMap<QString, QList<TodoEntry>> m_entries;

    int m_todoCount;
    int m_fixmeCount;
    int m_noteCount;
    int m_currentFilter;  // 0=All, 1=TODO, 2=FIXME, 3=NOTE
    QString m_searchText;
};

#endif // TODOPANEL_H
