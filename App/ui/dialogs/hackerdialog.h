#ifndef HACKERDIALOG_H
#define HACKERDIALOG_H

#include "../../theme/themeengine.h"
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

class HackerDialog : public QDialog {
  Q_OBJECT

public:
  explicit HackerDialog(QWidget *parent = nullptr,
                        Qt::WindowFlags flags = Qt::WindowFlags());
  ~HackerDialog() override;

  virtual void applyTheme(const ThemeDefinition &theme);

  virtual void applyTheme(const Theme &theme);

protected:
  ThemeDefinition m_themeDef;
  const ThemeColors &colors() const { return m_themeDef.colors; }
  int borderRadius() const { return m_themeDef.ui.borderRadius; }

  void stylePrimaryButton(QPushButton *btn);
  void styleSecondaryButton(QPushButton *btn);
  void styleDangerButton(QPushButton *btn);
  void styleTitleLabel(QLabel *label);
  void styleSubduedLabel(QLabel *label);

  void paintEvent(QPaintEvent *event) override;

private:
  void connectToThemeEngine();
  void applyWidgetStyles();
  QString buttonStyle(const QColor &bg, const QColor &fg, const QColor &hover,
                      const QColor &border) const;
  QString inputStyle() const;
  QString comboStyle() const;
  QString checkBoxStyle() const;
  QString listWidgetStyle() const;
  QString tableWidgetStyle() const;
  QString tabWidgetStyle() const;
  QString groupBoxStyle() const;
  QString textEditStyle() const;
  QString spinBoxStyle() const;
  QString scrollBarStyle() const;
};

#endif
