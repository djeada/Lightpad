#include "themedmessagebox.h"
#include "../uistylehelper.h"

Theme ThemedMessageBox::s_theme;
bool ThemedMessageBox::s_themeSet = false;

void ThemedMessageBox::setGlobalTheme(const Theme &theme) {
  s_theme = theme;
  s_themeSet = true;
}

int ThemedMessageBox::information(QWidget *parent, const QString &title,
                                  const QString &text, int buttons) {
  ThemedMessageBox box(parent);
  box.setWindowTitle(title);
  box.setIcon(Information);
  box.setText(text);
  box.setStandardButtons(buttons);
  box.setDefaultButton(Ok);
  return box.exec();
}

int ThemedMessageBox::warning(QWidget *parent, const QString &title,
                              const QString &text, int buttons) {
  ThemedMessageBox box(parent);
  box.setWindowTitle(title);
  box.setIcon(Warning);
  box.setText(text);
  box.setStandardButtons(buttons);
  return box.exec();
}

int ThemedMessageBox::critical(QWidget *parent, const QString &title,
                               const QString &text, int buttons) {
  ThemedMessageBox box(parent);
  box.setWindowTitle(title);
  box.setIcon(Critical);
  box.setText(text);
  box.setStandardButtons(buttons);
  return box.exec();
}

int ThemedMessageBox::question(QWidget *parent, const QString &title,
                               const QString &text, int buttons,
                               int defaultButton) {
  ThemedMessageBox box(parent);
  box.setWindowTitle(title);
  box.setIcon(Question);
  box.setText(text);
  box.setStandardButtons(buttons);
  box.setDefaultButton(defaultButton);
  return box.exec();
}

ThemedMessageBox::ThemedMessageBox(QWidget *parent) : QDialog(parent) {
  setModal(true);
}

void ThemedMessageBox::setIcon(Icon icon) { m_icon = icon; }

void ThemedMessageBox::setText(const QString &text) { m_text = text; }

void ThemedMessageBox::setInformativeText(const QString &text) {
  m_informativeText = text;
}

void ThemedMessageBox::setStandardButtons(int buttons) { m_buttons = buttons; }

void ThemedMessageBox::setDefaultButton(int button) {
  m_defaultButton = button;
}

void ThemedMessageBox::setButtonText(int button, const QString &text) {
  m_customButtonTexts[button] = text;
  if (m_buttonMap.contains(button)) {
    m_buttonMap[button]->setText(text);
  }
}

QPushButton *ThemedMessageBox::button(int which) const {
  return m_buttonMap.value(which, nullptr);
}

void ThemedMessageBox::buildUI() {
  if (m_built)
    return;
  m_built = true;

  setMinimumWidth(380);
  setMaximumWidth(560);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(16);
  mainLayout->setContentsMargins(24, 24, 24, 20);

  QHBoxLayout *contentLayout = new QHBoxLayout();
  contentLayout->setSpacing(16);

  m_iconLabel = new QLabel(this);
  m_iconLabel->setFixedSize(40, 40);
  m_iconLabel->setAlignment(Qt::AlignCenter);

  QString iconText;
  switch (m_icon) {
  case Information:
    iconText = "ℹ️";
    break;
  case Warning:
    iconText = "⚠️";
    break;
  case Critical:
    iconText = "❌";
    break;
  case Question:
    iconText = "❓";
    break;
  default:
    iconText = "";
    break;
  }
  m_iconLabel->setText(iconText);
  m_iconLabel->setStyleSheet("font-size: 28px; background: transparent;");

  if (m_icon != NoIcon) {
    contentLayout->addWidget(m_iconLabel, 0, Qt::AlignTop);
  }

  QVBoxLayout *textLayout = new QVBoxLayout();
  textLayout->setSpacing(8);

  m_textLabel = new QLabel(m_text, this);
  m_textLabel->setWordWrap(true);
  m_textLabel->setMinimumWidth(280);

  textLayout->addWidget(m_textLabel);

  if (!m_informativeText.isEmpty()) {
    m_infoLabel = new QLabel(m_informativeText, this);
    m_infoLabel->setWordWrap(true);
    textLayout->addWidget(m_infoLabel);
  }

  contentLayout->addLayout(textLayout, 1);
  mainLayout->addLayout(contentLayout);
  mainLayout->addSpacing(4);

  m_buttonLayout = new QHBoxLayout();
  m_buttonLayout->setSpacing(8);
  m_buttonLayout->addStretch();

  if (m_buttons & Cancel)
    addButton(tr("Cancel"), Cancel);
  if (m_buttons & No)
    addButton(tr("No"), No);
  if (m_buttons & Ok)
    addButton(tr("OK"), Ok);
  if (m_buttons & Yes)
    addButton(tr("Yes"), Yes);

  mainLayout->addLayout(m_buttonLayout);

  applyStyle();

  if (m_defaultButton != NoButton && m_buttonMap.contains(m_defaultButton)) {
    m_buttonMap[m_defaultButton]->setDefault(true);
    m_buttonMap[m_defaultButton]->setFocus();
  }
}

QPushButton *ThemedMessageBox::addButton(const QString &text, int role) {
  QPushButton *btn =
      new QPushButton(m_customButtonTexts.value(role, text), this);
  btn->setMinimumWidth(80);
  btn->setCursor(Qt::PointingHandCursor);
  m_buttonMap[role] = btn;
  m_buttonLayout->addWidget(btn);

  connect(btn, &QPushButton::clicked, [this, role]() {
    m_clickedButton = role;
    accept();
  });

  return btn;
}

void ThemedMessageBox::applyStyle() {
  Theme theme = s_themeSet ? s_theme : Theme();

  setStyleSheet(QString("ThemedMessageBox {"
                        "  background: %1;"
                        "}")
                    .arg(theme.surfaceColor.name()));

  if (m_textLabel) {
    m_textLabel->setStyleSheet(
        QString("font-size: 13px; font-weight: 500; color: %1; "
                "background: transparent;")
            .arg(theme.foregroundColor.name()));
  }

  if (m_infoLabel) {
    m_infoLabel->setStyleSheet(
        QString("font-size: 12px; color: %1; background: transparent;")
            .arg(theme.singleLineCommentFormat.name()));
  }

  for (auto it = m_buttonMap.begin(); it != m_buttonMap.end(); ++it) {
    int role = it.key();
    QPushButton *btn = it.value();

    bool isPrimary = false;
    bool isDanger = false;

    if (role == Ok || role == Yes) {
      isPrimary = true;
      if (m_icon == Critical || m_icon == Warning) {

        if (m_buttons & (Cancel | No)) {
          isDanger = (m_icon == Critical);
        }
      }
    }

    if (isDanger) {
      btn->setStyleSheet(UIStyleHelper::dangerButtonStyle(theme));
    } else if (isPrimary) {
      btn->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
    } else {
      btn->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
    }
  }
}

int ThemedMessageBox::exec() {
  buildUI();
  QDialog::exec();
  return m_clickedButton;
}
