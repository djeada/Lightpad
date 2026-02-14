#ifndef GOTOSYMBOLDIALOG_H
#define GOTOSYMBOLDIALOG_H

#include "../../lsp/lspclient.h"
#include "../../settings/theme.h"
#include <QDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

struct SymbolItem {
  QString name;
  QString detail;
  LspSymbolKind kind;
  int line;
  int column;
  int score;
};

class GoToSymbolDialog : public QDialog {
  Q_OBJECT

public:
  explicit GoToSymbolDialog(QWidget *parent = nullptr);
  ~GoToSymbolDialog();

  void setSymbols(const QList<LspDocumentSymbol> &symbols);

  void clearSymbols();

  void showDialog();

  void applyTheme(const Theme &theme);

signals:

  void symbolSelected(int line, int column);

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
  void selectSymbol(int row);
  void selectNext();
  void selectPrevious();
  void flattenSymbols(const QList<LspDocumentSymbol> &symbols,
                      const QString &prefix = QString());
  QString symbolKindIcon(LspSymbolKind kind) const;
  QString symbolKindName(LspSymbolKind kind) const;

  QLineEdit *m_searchBox;
  QListWidget *m_resultsList;
  QVBoxLayout *m_layout;
  QList<SymbolItem> m_symbols;
  QList<int> m_filteredIndices;
};

#endif
