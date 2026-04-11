#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFontComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QToolButton>

class MainWindow;

class Preferences : public QDialog {
  Q_OBJECT

public:
  Preferences(MainWindow *parent);
  ~Preferences();

private:
  void buildUi();
  void loadCurrentSettings();
  void connectSignals();
  void persistAll();

  QWidget *createSectionHeader(const QString &title);
  QWidget *createSeparator();
  QToolButton *createColorSwatch(const QColor &color);

  void onFontFamilyChanged(const QFont &font);
  void onFontSizeChanged(int size);
  void onColorSwatchClicked(QToolButton *button, const QString &role);

  MainWindow *m_mainWindow;

  // Font controls
  QFontComboBox *m_fontFamilyCombo;
  QSpinBox *m_fontSizeSpinner;
  QLabel *m_fontPreview;

  // Editor controls
  QComboBox *m_tabWidthCombo;
  QCheckBox *m_autoIndentCheck;
  QCheckBox *m_autoSaveCheck;
  QCheckBox *m_trimWhitespaceCheck;
  QCheckBox *m_finalNewlineCheck;

  // Display controls
  QCheckBox *m_lineNumbersCheck;
  QCheckBox *m_currentLineCheck;
  QCheckBox *m_bracketMatchCheck;

  // Color swatches (role name → button)
  QMap<QString, QToolButton *> m_colorSwatches;
};

#endif
