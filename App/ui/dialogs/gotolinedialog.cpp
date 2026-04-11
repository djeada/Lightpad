#include "gotolinedialog.h"
#include "../uistylehelper.h"

GoToLineDialog::GoToLineDialog(QWidget *parent, int maxLine)
    : StyledPopupDialog(parent), m_lineEdit(nullptr),
      m_infoLabel(nullptr), m_maxLine(maxLine) {
  setupUI();
}

GoToLineDialog::~GoToLineDialog() {}

void GoToLineDialog::setupUI() {
  setMinimumWidth(300);
  setFixedHeight(80);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(4);

  m_lineEdit = new QLineEdit(this);
  m_lineEdit->setPlaceholderText(tr("Go to line..."));
  layout->addWidget(m_lineEdit);

  m_infoLabel = new QLabel(this);
  m_infoLabel->setText(QString(tr("Enter line number (1-%1)")).arg(m_maxLine));
  layout->addWidget(m_infoLabel);

  connect(m_lineEdit, &QLineEdit::textChanged, this,
          &GoToLineDialog::onTextChanged);
  connect(m_lineEdit, &QLineEdit::returnPressed, this,
          &GoToLineDialog::onReturnPressed);
}

int GoToLineDialog::lineNumber() const {
  bool ok;
  int line = m_lineEdit->text().toInt(&ok);
  if (ok && line >= 1 && line <= m_maxLine) {
    return line;
  }
  return -1;
}

void GoToLineDialog::setMaxLine(int maxLine) {
  m_maxLine = maxLine;
  m_infoLabel->setText(QString(tr("Enter line number (1-%1)")).arg(m_maxLine));
}

void GoToLineDialog::showDialog() {
  m_lineEdit->clear();

  showCentered();
  m_lineEdit->setFocus();
}

void GoToLineDialog::keyPressEvent(QKeyEvent *event) {
  StyledPopupDialog::keyPressEvent(event);
}

void GoToLineDialog::onTextChanged(const QString &text) {
  bool ok;
  int line = text.toInt(&ok);

  if (text.isEmpty()) {
    m_infoLabel->setText(
        QString(tr("Enter line number (1-%1)")).arg(m_maxLine));
    m_infoLabel->setStyleSheet(UIStyleHelper::infoLabelStyle(m_theme));
  } else if (!ok || line < 1 || line > m_maxLine) {
    m_infoLabel->setText(tr("Invalid line number"));
    m_infoLabel->setStyleSheet(UIStyleHelper::errorInfoLabelStyle(m_theme));
  } else {
    m_infoLabel->setText(QString(tr("Go to line %1")).arg(line));
    m_infoLabel->setStyleSheet(UIStyleHelper::successInfoLabelStyle(m_theme));
  }
}

void GoToLineDialog::onReturnPressed() {
  int line = lineNumber();
  if (line > 0) {
    emit lineSelected(line);
    hide();
  }
}

void GoToLineDialog::applyTheme(const Theme &theme) {
  StyledPopupDialog::applyTheme(theme);
  m_infoLabel->setStyleSheet(UIStyleHelper::infoLabelStyle(theme));
}
