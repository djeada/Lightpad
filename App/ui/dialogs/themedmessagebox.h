#ifndef THEMEDMESSAGEBOX_H
#define THEMEDMESSAGEBOX_H

#include "../../settings/theme.h"
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>

class ThemedMessageBox : public QDialog {
  Q_OBJECT

public:
  enum Icon { NoIcon, Information, Warning, Critical, Question };
  enum StandardButton {
    NoButton = 0,
    Ok = 0x1,
    Cancel = 0x2,
    Yes = 0x4,
    No = 0x8,
  };

  static void setGlobalTheme(const Theme &theme);

  static int information(QWidget *parent, const QString &title,
                         const QString &text, int buttons = Ok);
  static int warning(QWidget *parent, const QString &title, const QString &text,
                     int buttons = Ok);
  static int critical(QWidget *parent, const QString &title,
                      const QString &text, int buttons = Ok);
  static int question(QWidget *parent, const QString &title,
                      const QString &text, int buttons = Yes | No,
                      int defaultButton = No);

  explicit ThemedMessageBox(QWidget *parent = nullptr);

  void setIcon(Icon icon);
  void setText(const QString &text);
  void setInformativeText(const QString &text);
  void setStandardButtons(int buttons);
  void setDefaultButton(int button);
  QPushButton *button(int which) const;
  int exec() override;

private:
  void buildUI();
  void applyStyle();
  QPushButton *addButton(const QString &text, int role);

  static Theme s_theme;
  static bool s_themeSet;

  Icon m_icon = NoIcon;
  QString m_text;
  QString m_informativeText;
  int m_buttons = Ok;
  int m_defaultButton = NoButton;
  int m_clickedButton = NoButton;
  bool m_built = false;

  QLabel *m_iconLabel = nullptr;
  QLabel *m_textLabel = nullptr;
  QLabel *m_infoLabel = nullptr;
  QHBoxLayout *m_buttonLayout = nullptr;
  QMap<int, QPushButton *> m_buttonMap;
};

#endif
