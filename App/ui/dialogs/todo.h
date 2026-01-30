#ifndef TODO_H
#define TODO_H

#include <QDialog>
class QPlainTextEdit;

class TodoDialog : public QDialog {
    Q_OBJECT

public:
    explicit TodoDialog(QWidget* parent = nullptr);
    QString text() const;
    void setText(const QString& content);

private:
    QPlainTextEdit* m_editor;
};

#endif // TODO_H
