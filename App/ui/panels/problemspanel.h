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
#include "../../settings/theme.h"

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
     * @brief Get problem count for a specific file
     * @param filePath The file path to query
     * @return Total number of problems (errors + warnings + infos) for the file
     */
    int problemCountForFile(const QString& filePath) const;

    /**
     * @brief Get error count for a specific file
     * @param filePath The file path to query
     * @return Number of errors for the file
     */
    int errorCountForFile(const QString& filePath) const;

    /**
     * @brief Get warning count for a specific file
     * @param filePath The file path to query
     * @return Number of warnings for the file
     */
    int warningCountForFile(const QString& filePath) const;

    /**
     * @brief Check if auto-refresh on save is enabled
     */
    bool isAutoRefreshEnabled() const;

    /**
     * @brief Enable or disable auto-refresh on save
     */
    void setAutoRefreshEnabled(bool enabled);

    /**
     * @brief Apply theme to the panel
     */
    void applyTheme(const Theme& theme);

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
     * @brief Emitted when file problem counts change
     * @param filePath The file whose problem count changed
     * @param errorCount Number of errors in the file
     * @param warningCount Number of warnings in the file
     * @param infoCount Number of info/hints in the file
     */
    void fileCountsChanged(const QString& filePath, int errorCount, int warningCount, int infoCount);

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
    const QList<LspDiagnostic>* findDiagnosticsForFile(const QString& filePath) const;

    QTreeWidget* m_tree;
    QWidget* m_header;
    QLabel* m_titleLabel;
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
    Theme m_theme;
};

#endif // PROBLEMSPANEL_H
