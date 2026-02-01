#ifndef RECENTFILESDIALOG_H
#define RECENTFILESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QKeyEvent>

class RecentFilesManager;

/**
 * @brief Recent Files dialog for quick access to recently opened files
 * 
 * Provides fuzzy search for recently opened files.
 */
class RecentFilesDialog : public QDialog {
    Q_OBJECT

public:
    explicit RecentFilesDialog(RecentFilesManager* manager, QWidget* parent = nullptr);
    ~RecentFilesDialog();

    /**
     * @brief Show and focus the dialog
     */
    void showDialog();

    /**
     * @brief Refresh the list of recent files
     */
    void refresh();

signals:
    /**
     * @brief Emitted when user selects a file
     */
    void fileSelected(const QString& filePath);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);
    void onItemClicked(QListWidgetItem* item);

private:
    void setupUI();
    void updateResults(const QString& query);
    int fuzzyMatch(const QString& pattern, const QString& text);
    void selectFile(int row);
    void selectNext();
    void selectPrevious();

    RecentFilesManager* m_manager;
    QLineEdit* m_searchBox;
    QListWidget* m_resultsList;
    QVBoxLayout* m_layout;
    QStringList m_recentFiles;
    QList<int> m_filteredIndices;
};

#endif // RECENTFILESDIALOG_H
