#ifndef STYLEDDIALOG_H
#define STYLEDDIALOG_H

#include "../../settings/theme.h"
#include "../uistylehelper.h"
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

  struct SemanticStyleCache {
    QString formDialog;
    QString groupBox;
    QString lineEdit;
    QString comboBox;
    QString checkBox;
    QString resultList;
    QString treeWidget;
    QString secondaryButton;
    QString tableWidget;
    QString plainTextEdit;
    QString spinBox;
    QString tabWidget;
    QString primaryButton;
    QString dangerButton;
    QString titleLabel;
    QString subduedLabel;
    QString sectionLabel;
    QString emptyState;
  };

protected:
  Theme m_theme;
  SemanticStyleCache m_semanticStyles;
  bool m_hasSemanticStyles = false;

  void setKeyboardDefault(QPushButton *button);

  void stylePrimaryButton(QPushButton *btn);
  void styleSecondaryButton(QPushButton *btn);
  void styleDangerButton(QPushButton *btn);
  void styleTitleLabel(QLabel *label);
  void styleSubduedLabel(QLabel *label);
  void styleSectionLabel(QLabel *label);
  void styleEmptyState(QLabel *label);
  void styleToneLabel(QLabel *label, UIStyleHelper::Tone tone);
  void styleBadge(QLabel *label, UIStyleHelper::Tone tone);

private:
  void applySemanticStyles();

  const ThemeDefinition *m_pendingDefinition = nullptr;
};

#endif
