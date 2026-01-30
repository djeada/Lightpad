#ifndef PROBLEMSPANEL_H
#define PROBLEMSPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QHBoxLayout>
#include "../../lsp/lspclient.h"

/**
 * @brief Problems Panel for displaying LSP diagnostics
 * 
 * Shows errors, warnings, and other diagnostics from language servers
 * grouped by file with navigation support.
 */
class ProblemsPanel : public QWidget {
    Q_OBJECT

public:
    explicit ProblemsPanel(QWidget* parent = nullptr);
    ~ProblemsPanel();

    /**
     * @brief Update diagnostics for a file
     */
    void setDiagnostics(const QString& uri, const QList<LspDiagnostic>& diagnostics);

    /**
     * @brief Clear all diagnostics
     */
    void clearAll();

    /**
     * @brief Clear diagnostics for a specific file
     */
    void clearFile(const QString& uri);

    /**
     * @brief Get total problem count
     */
    int totalCount() const;

    /**
     * @brief Get count by severity
     */
    int errorCount() const;
    int warningCount() const;
    int infoCount() const;

signals:
    /**
     * @brief Emitted when user clicks on a diagnostic
     */
    void problemClicked(const QString& filePath, int line, int column);

    /**
     * @brief Emitted when counts change
     */
    void countsChanged(int errors, int warnings, int infos);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onFilterChanged(int index);

private:
    void setupUI();
    void updateCounts();
    void rebuildTree();
    QString severityIcon(LspDiagnosticSeverity severity) const;
    QString severityText(LspDiagnosticSeverity severity) const;

    QTreeWidget* m_tree;
    QLabel* m_statusLabel;
    QComboBox* m_filterCombo;
    
    // File URI -> diagnostics
    QMap<QString, QList<LspDiagnostic>> m_diagnostics;
    
    int m_errorCount;
    int m_warningCount;
    int m_infoCount;
    int m_hintCount;
    
    int m_currentFilter;  // 0=All, 1=Errors, 2=Warnings, 3=Info
};

#endif // PROBLEMSPANEL_H
