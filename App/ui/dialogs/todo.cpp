#include "todo.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QDialogButtonBox>

TodoDialog::TodoDialog(QWidget* parent)
    : QDialog(parent)
    , m_editor(new QPlainTextEdit(this))
{
    setWindowTitle("Todo");
    setMinimumSize(320, 240);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* label = new QLabel("Todo items", this);
    label->setStyleSheet("font-weight: 600;");
    layout->addWidget(label);

    m_editor->setPlaceholderText("Add your tasks here...");
    layout->addWidget(m_editor);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &TodoDialog::close);
    layout->addWidget(buttons);
}

QString TodoDialog::text() const
{
    return m_editor->toPlainText();
}

void TodoDialog::setText(const QString& content)
{
    m_editor->setPlainText(content);
}
