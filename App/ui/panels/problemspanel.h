#ifndef PROBLEMSPANEL_H
#define PROBLEMSPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QCheckBox>
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

    /**
     * @brief Check if auto-refresh on save is enabled
     */
    bool isAutoRefreshEnabled() const;

    /**
     * @brief Enable or disable auto-refresh on save
     */
    void setAutoRefreshEnabled(bool enabled);

    /**
     * @brief Notify the panel that a file was saved (for auto-refresh)
     * @param filePath The path of the saved file
     */
    void onFileSaved(const QString& filePath);

signals:
    /**
     * @brief Emitted when user clicks on a diagnostic
     */
    void problemClicked(const QString& filePath, int line, int column);

    /**
     * @brief Emitted when counts change
     */
    void countsChanged(int errors, int warnings, int infos);

    /**
     * @brief Emitted when a refresh is requested for a file
     */
    void refreshRequested(const QString& filePath);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onFilterChanged(int index);
    void onAutoRefreshToggled(bool checked);

private:
    void setupUI();
    void updateCounts();
    void rebuildTree();
    QString severityIcon(LspDiagnosticSeverity severity) const;
    QString severityText(LspDiagnosticSeverity severity) const;

    QTreeWidget* m_tree;
    QLabel* m_statusLabel;
    QComboBox* m_filterCombo;
    QCheckBox* m_autoRefreshCheckBox;
    
    // File URI -> diagnostics
    QMap<QString, QList<LspDiagnostic>> m_diagnostics;
    
    int m_errorCount;
    int m_warningCount;
    int m_infoCount;
    int m_hintCount;
    
    int m_currentFilter;  // 0=All, 1=Errors, 2=Warnings, 3=Info
    bool m_autoRefreshEnabled;
};

#endif // PROBLEMSPANEL_H
