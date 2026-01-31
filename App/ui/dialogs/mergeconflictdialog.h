#ifndef MERGECONFLICTDIALOG_H
#define MERGECONFLICTDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include "../../git/gitintegration.h"

/**
 * @brief Dialog for resolving Git merge conflicts
 * 
 * Displays conflicted files and provides options to resolve
 * them by accepting ours, theirs, or manual editing.
 */
class MergeConflictDialog : public QDialog {
    Q_OBJECT

public:
    explicit MergeConflictDialog(GitIntegration* git, QWidget* parent = nullptr);
    ~MergeConflictDialog();

    /**
     * @brief Set the list of conflicted files
     */
    void setConflictedFiles(const QStringList& files);

    /**
     * @brief Refresh the conflict status
     */
    void refresh();

signals:
    /**
     * @brief Emitted when user wants to open a file for manual editing
     */
    void openFileRequested(const QString& filePath);

    /**
     * @brief Emitted when all conflicts are resolved
     */
    void allConflictsResolved();

private slots:
    void onFileSelected(QListWidgetItem* item);
    void onAcceptOursClicked();
    void onAcceptTheirsClicked();
    void onOpenInEditorClicked();
    void onMarkResolvedClicked();
    void onAbortMergeClicked();
    void onContinueMergeClicked();

private:
    void setupUI();
    void applyStyles();
    void updateConflictPreview(const QString& filePath);
    void updateButtons();

    GitIntegration* m_git;
    QListWidget* m_fileList;
    QTextEdit* m_oursPreview;
    QTextEdit* m_theirsPreview;
    QLabel* m_statusLabel;
    QLabel* m_conflictCountLabel;
    QPushButton* m_acceptOursButton;
    QPushButton* m_acceptTheirsButton;
    QPushButton* m_openEditorButton;
    QPushButton* m_markResolvedButton;
    QPushButton* m_abortButton;
    QPushButton* m_continueButton;
    QString m_currentFile;
};

#endif // MERGECONFLICTDIALOG_H
