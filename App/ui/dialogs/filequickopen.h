#ifndef FILEQUICKOPEN_H
#define FILEQUICKOPEN_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QDir>
#include <QFileInfo>

/**
 * @brief File Quick Open dialog (Ctrl+P)
 * 
 * Provides fuzzy search for files in the current project/directory.
 */
class FileQuickOpen : public QDialog {
    Q_OBJECT

public:
    explicit FileQuickOpen(QWidget* parent = nullptr);
    ~FileQuickOpen();

    /**
     * @brief Set the root directory to search in
     */
    void setRootDirectory(const QString& path);

    /**
     * @brief Show and focus the dialog
     */
    void showDialog();

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
    void scanDirectory();
    void updateResults(const QString& query);
    int fuzzyMatch(const QString& pattern, const QString& text);
    void selectFile(int row);
    void selectNext();
    void selectPrevious();

    QLineEdit* m_searchBox;
    QListWidget* m_resultsList;
    QVBoxLayout* m_layout;
    QString m_rootPath;
    QStringList m_allFiles;
    QStringList m_filteredFiles;
};

#endif // FILEQUICKOPEN_H
