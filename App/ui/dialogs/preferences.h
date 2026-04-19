#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFontComboBox>
#include <QLabel>
#include <QListWidget>
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
  void persistCurrentTheme(const QString &activeThemeName);

  QWidget *createSectionHeader(const QString &title);
  QWidget *createSeparator();
  QToolButton *createColorSwatch(const QColor &color);
  QWidget *buildThemeSection();

  void onFontFamilyChanged(const QFont &font);
  void onFontSizeChanged(int size);
  void onColorSwatchClicked(QToolButton *button, const QString &role);
  void onThemePresetChanged(int row);
  void onScanlinesToggled(bool enabled);

  MainWindow *m_mainWindow;

  QFontComboBox *m_fontFamilyCombo;
  QSpinBox *m_fontSizeSpinner;
  QLabel *m_fontPreview;

  QComboBox *m_tabWidthCombo;
  QCheckBox *m_autoIndentCheck;
  QCheckBox *m_autoSaveCheck;
  QCheckBox *m_trimWhitespaceCheck;
  QCheckBox *m_finalNewlineCheck;

  QCheckBox *m_lineNumbersCheck;
  QCheckBox *m_currentLineCheck;
  QCheckBox *m_bracketMatchCheck;

  QListWidget *m_themeList;
  QWidget *m_themePreview;
  QCheckBox *m_scanlinesCheck = nullptr;

  QMap<QString, QToolButton *> m_colorSwatches;
};

#endif
