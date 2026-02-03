#ifndef GOTOLINEDIALOG_H
#define GOTOLINEDIALOG_H

#include "../../settings/theme.h"
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

/**
 * @brief Go to Line dialog (Ctrl+G)
 *
 * Provides a quick way to jump to a specific line number in the editor.
 */
class GoToLineDialog : public QDialog {
  Q_OBJECT

public:
  explicit GoToLineDialog(QWidget *parent = nullptr, int maxLine = 1);
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

  /**
   * @brief Apply theme to the dialog
   */
  void applyTheme(const Theme &theme);

signals:
  /**
   * @brief Emitted when user confirms the line number
   */
  void lineSelected(int lineNumber);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void onTextChanged(const QString &text);
  void onReturnPressed();

private:
  void setupUI();

  QLineEdit *m_lineEdit;
  QLabel *m_infoLabel;
  int m_maxLine;
  Theme m_theme;
};

#endif // GOTOLINEDIALOG_H
