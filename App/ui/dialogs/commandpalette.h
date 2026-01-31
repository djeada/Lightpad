#ifndef COMMANDPALETTE_H
#define COMMANDPALETTE_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QAction>
#include <QKeyEvent>
#include <QSettings>

/**
 * @brief Command item for the palette
 */
struct CommandItem {
    QString id;
    QString name;
    QString shortcut;
    QAction* action;
    int score;  // For fuzzy matching
};

/**
 * @brief Command Palette dialog (Ctrl+Shift+P)
 * 
 * Provides fuzzy search for all available commands/actions.
 */
class CommandPalette : public QDialog {
    Q_OBJECT

public:
    explicit CommandPalette(QWidget* parent = nullptr);
    ~CommandPalette();

    /**
     * @brief Register an action with the palette
     */
    void registerAction(QAction* action, const QString& category = QString());

    /**
     * @brief Register multiple actions from a menu
     */
    void registerMenu(QMenu* menu, const QString& category = QString());

    /**
     * @brief Clear all registered commands
     */
    void clearCommands();

    /**
     * @brief Show and focus the palette
     */
    void showPalette();

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
    void executeCommand(int index);
    void selectNext();
    void selectPrevious();
    void addToRecentCommands(const QString& commandId);
    void loadRecentCommands();
    void saveRecentCommands();
    int getRecentBonus(const QString& commandId) const;

    QLineEdit* m_searchBox;
    QListWidget* m_resultsList;
    QVBoxLayout* m_layout;
    QList<CommandItem> m_commands;
    QList<int> m_filteredIndices;
    QStringList m_recentCommands;
    static const int MAX_RECENT_COMMANDS = 10;
};

#endif // COMMANDPALETTE_H
