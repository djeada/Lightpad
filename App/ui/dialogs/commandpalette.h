#ifndef COMMANDPALETTE_H
#define COMMANDPALETTE_H

#include "../../settings/theme.h"
#include <QAction>
#include <QDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QSettings>
#include <QVBoxLayout>

struct CommandItem {
  QString id;
  QString name;
  QString shortcut;
  QAction *action;
  int score;
};

class CommandPalette : public QDialog {
  Q_OBJECT

public:
  explicit CommandPalette(QWidget *parent = nullptr);
  ~CommandPalette();

  void registerAction(QAction *action, const QString &category = QString());

  void registerMenu(QMenu *menu, const QString &category = QString());

  void clearCommands();

  void showPalette();

  void applyTheme(const Theme &theme);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onSearchTextChanged(const QString &text);
  void onItemActivated(QListWidgetItem *item);
  void onItemClicked(QListWidgetItem *item);

private:
  void setupUI();
  void updateResults(const QString &query);
  int fuzzyMatch(const QString &pattern, const QString &text);
  void executeCommand(int index);
  void selectNext();
  void selectPrevious();
  void addToRecentCommands(const QString &commandId);
  void loadRecentCommands();
  void saveRecentCommands();
  int getRecentBonus(const QString &commandId) const;

  QLineEdit *m_searchBox;
  QListWidget *m_resultsList;
  QVBoxLayout *m_layout;
  QList<CommandItem> m_commands;
  QList<int> m_filteredIndices;
  QStringList m_recentCommands;
  static const int MAX_RECENT_COMMANDS = 10;
};

#endif
