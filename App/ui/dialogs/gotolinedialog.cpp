#include "gotolinedialog.h"
#include "../uistylehelper.h"

GoToLineDialog::GoToLineDialog(QWidget* parent, int maxLine)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_lineEdit(nullptr)
    , m_infoLabel(nullptr)
    , m_maxLine(maxLine)
{
    setupUI();
}

GoToLineDialog::~GoToLineDialog()
{
}

void GoToLineDialog::setupUI()
{
    setMinimumWidth(300);
    setFixedHeight(80);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // Line number input
    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setPlaceholderText(tr("Go to line..."));
    m_lineEdit->setStyleSheet(
        "QLineEdit {"
        "  padding: 8px;"
        "  font-size: 14px;"
        "  border: 1px solid #2a3241;"
        "  border-radius: 4px;"
        "  background: #1f2632;"
        "  color: #e6edf3;"
        "}"
    );
    layout->addWidget(m_lineEdit);

    // Info label
    m_infoLabel = new QLabel(this);
    m_infoLabel->setStyleSheet("color: #9aa4b2; font-size: 11px;");
    m_infoLabel->setText(QString(tr("Enter line number (1-%1)")).arg(m_maxLine));
    layout->addWidget(m_infoLabel);

    // Connections
    connect(m_lineEdit, &QLineEdit::textChanged, this, &GoToLineDialog::onTextChanged);
    connect(m_lineEdit, &QLineEdit::returnPressed, this, &GoToLineDialog::onReturnPressed);

    setStyleSheet("GoToLineDialog { background: #171c24; border: 1px solid #2a3241; border-radius: 8px; }");
}

int GoToLineDialog::lineNumber() const
{
    bool ok;
    int line = m_lineEdit->text().toInt(&ok);
    if (ok && line >= 1 && line <= m_maxLine) {
        return line;
    }
    return -1;
}

void GoToLineDialog::setMaxLine(int maxLine)
{
    m_maxLine = maxLine;
    m_infoLabel->setText(QString(tr("Enter line number (1-%1)")).arg(m_maxLine));
}

void GoToLineDialog::showDialog()
{
    m_lineEdit->clear();
    
    // Position at top center of parent
    if (parentWidget()) {
        QPoint parentCenter = parentWidget()->mapToGlobal(parentWidget()->rect().center());
        int x = parentCenter.x() - width() / 2;
        int y = parentWidget()->mapToGlobal(QPoint(0, 0)).y() + 50;
        move(x, y);
    }

    show();
    m_lineEdit->setFocus();
}

void GoToLineDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
    } else {
        QDialog::keyPressEvent(event);
    }
}

void GoToLineDialog::onTextChanged(const QString& text)
{
    bool ok;
    int line = text.toInt(&ok);
    
    if (text.isEmpty()) {
        m_infoLabel->setText(QString(tr("Enter line number (1-%1)")).arg(m_maxLine));
        m_infoLabel->setStyleSheet(UIStyleHelper::infoLabelStyle(m_theme));
    } else if (!ok || line < 1 || line > m_maxLine) {
        m_infoLabel->setText(tr("Invalid line number"));
        m_infoLabel->setStyleSheet(UIStyleHelper::errorInfoLabelStyle(m_theme));
    } else {
        m_infoLabel->setText(QString(tr("Go to line %1")).arg(line));
        m_infoLabel->setStyleSheet(UIStyleHelper::successInfoLabelStyle(m_theme));
    }
}

void GoToLineDialog::onReturnPressed()
{
    int line = lineNumber();
    if (line > 0) {
        emit lineSelected(line);
        hide();
    }
}

void GoToLineDialog::applyTheme(const Theme& theme)
{
    m_theme = theme;
    setStyleSheet("GoToLineDialog { " + UIStyleHelper::popupDialogStyle(theme) + " }");
    m_lineEdit->setStyleSheet(UIStyleHelper::searchBoxStyle(theme));
    m_infoLabel->setStyleSheet(UIStyleHelper::infoLabelStyle(theme));
}
