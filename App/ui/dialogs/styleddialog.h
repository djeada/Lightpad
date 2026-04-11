#ifndef STYLEDDIALOG_H
#define STYLEDDIALOG_H

#include "../../settings/theme.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>

class StyledDialog : public QDialog {
  Q_OBJECT

public:
  explicit StyledDialog(QWidget *parent = nullptr,
                        Qt::WindowFlags flags = Qt::WindowFlags());

  virtual void applyTheme(const Theme &theme);

protected:
  Theme m_theme;

  void stylePrimaryButton(QPushButton *btn);
  void styleSecondaryButton(QPushButton *btn);
  void styleDangerButton(QPushButton *btn);
  void styleTitleLabel(QLabel *label);
  void styleSubduedLabel(QLabel *label);
};

#endif
