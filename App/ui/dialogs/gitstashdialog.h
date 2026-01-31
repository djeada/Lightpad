#ifndef GITSTASHDIALOG_H
#define GITSTASHDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include "../../git/gitintegration.h"

/**
 * @brief Dialog for managing Git stash entries
 */
class GitStashDialog : public QDialog {
    Q_OBJECT

public:
    explicit GitStashDialog(GitIntegration* git, QWidget* parent = nullptr);
    ~GitStashDialog();

    /**
     * @brief Refresh the stash list
     */
    void refresh();

signals:
    /**
     * @brief Emitted when stash operation completes
     */
    void stashOperationCompleted(const QString& message);

private slots:
    void onStashClicked();
    void onPopClicked();
    void onApplyClicked();
    void onDropClicked();
    void onClearClicked();
    void onStashSelected(QListWidgetItem* item);
    void onCloseClicked();

private:
    void setupUI();
    void applyStyles();
    void updateStashList();

    GitIntegration* m_git;
    
    QListWidget* m_stashList;
    QLineEdit* m_messageEdit;
    QCheckBox* m_includeUntrackedCheckbox;
    QPushButton* m_stashButton;
    QPushButton* m_popButton;
    QPushButton* m_applyButton;
    QPushButton* m_dropButton;
    QPushButton* m_clearButton;
    QPushButton* m_closeButton;
    QLabel* m_statusLabel;
    QLabel* m_detailsLabel;
};

#endif // GITSTASHDIALOG_H
