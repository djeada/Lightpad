#ifndef GOTOSYMBOLDIALOG_H
#define GOTOSYMBOLDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QKeyEvent>
#include "../../lsp/lspclient.h"
#include "../../settings/theme.h"

/**
 * @brief Symbol item for the dialog
 */
struct SymbolItem {
    QString name;
    QString detail;
    LspSymbolKind kind;
    int line;
    int column;
    int score;  // For fuzzy matching
};

/**
 * @brief Go to Symbol dialog (Ctrl+Shift+O)
 * 
 * Provides fuzzy search for document symbols from LSP.
 */
class GoToSymbolDialog : public QDialog {
    Q_OBJECT

public:
    explicit GoToSymbolDialog(QWidget* parent = nullptr);
    ~GoToSymbolDialog();

    /**
     * @brief Set symbols to display
     */
    void setSymbols(const QList<LspDocumentSymbol>& symbols);

    /**
     * @brief Clear all symbols
     */
    void clearSymbols();

    /**
     * @brief Show and focus the dialog
     */
    void showDialog();

    /**
     * @brief Apply theme to the dialog
     */
    void applyTheme(const Theme& theme);

signals:
    /**
     * @brief Emitted when user selects a symbol
     */
    void symbolSelected(int line, int column);

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
    void selectSymbol(int row);
    void selectNext();
    void selectPrevious();
    void flattenSymbols(const QList<LspDocumentSymbol>& symbols, const QString& prefix = QString());
    QString symbolKindIcon(LspSymbolKind kind) const;
    QString symbolKindName(LspSymbolKind kind) const;

    QLineEdit* m_searchBox;
    QListWidget* m_resultsList;
    QVBoxLayout* m_layout;
    QList<SymbolItem> m_symbols;
    QList<int> m_filteredIndices;
};

#endif // GOTOSYMBOLDIALOG_H
