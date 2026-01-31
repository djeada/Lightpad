#ifndef GOTOLINEDIALOG_H
#define GOTOLINEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QKeyEvent>

/**
 * @brief Go to Line dialog (Ctrl+G)
 * 
 * Provides a quick way to jump to a specific line number in the editor.
 */
class GoToLineDialog : public QDialog {
    Q_OBJECT

public:
    explicit GoToLineDialog(QWidget* parent = nullptr, int maxLine = 1);
    ~GoToLineDialog();

    /**
     * @brief Get the entered line number
     */
    int lineNumber() const;

    /**
     * @brief Set the maximum line number
     */
    void setMaxLine(int maxLine);

    /**
     * @brief Show the dialog
     */
    void showDialog();

signals:
    /**
     * @brief Emitted when user confirms the line number
     */
    void lineSelected(int lineNumber);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onTextChanged(const QString& text);
    void onReturnPressed();

private:
    void setupUI();

    QLineEdit* m_lineEdit;
    QLabel* m_infoLabel;
    int m_maxLine;
};

#endif // GOTOLINEDIALOG_H
