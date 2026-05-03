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

class ThemeDefinition;

class StyledDialog : public QDialog {
  Q_OBJECT

public:
  explicit StyledDialog(QWidget *parent = nullptr,
                        Qt::WindowFlags flags = Qt::WindowFlags());

  virtual void applyTheme(const Theme &theme);
  void applyTheme(const ThemeDefinition &theme);

protected:
  struct SemanticStyleCache {
    QString formDialog;
    QString groupBox;
    QString lineEdit;
    QString comboBox;
    QString checkBox;
    QString resultList;
    QString secondaryButton;
    QString tableWidget;
    QString plainTextEdit;
    QString spinBox;
    QString tabWidget;
    QString primaryButton;
    QString dangerButton;
    QString titleLabel;
    QString subduedLabel;
  };

  Theme m_theme;
  SemanticStyleCache m_semanticStyles;
  bool m_hasSemanticStyles = false;

  void stylePrimaryButton(QPushButton *btn);
  void styleSecondaryButton(QPushButton *btn);
  void styleDangerButton(QPushButton *btn);
  void styleTitleLabel(QLabel *label);
  void styleSubduedLabel(QLabel *label);
};

#endif
