#include "popup.h"
#include "lightpadsyntaxhighlighter.h"
#include "mainwindow.h"
#include "textarea.h"
#include <QStringListModel>
#include <QVBoxLayout>

ListView::ListView(QWidget* parent)
    : QListView(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

QSize ListView::sizeHint() const
{

    if (model()->rowCount() == 0)
        return QSize(width(), 0);

    int nToShow = 10 < model()->rowCount() ? 10 : model()->rowCount();
    return QSize(width(), nToShow * sizeHintForRow(0));
}

Popup::Popup(QStringList list, QWidget* parent)
    : QDialog(parent)
    , list(list)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    QStringListModel* model = new QStringListModel(this);
    listView = new ListView(this);

    model->setStringList(list);

    listView->setModel(model);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(listView);
    layout->setContentsMargins(0, 0, 0, 0);

    show();
}

PopupLanguageHighlight ::PopupLanguageHighlight(QStringList list, QWidget* parent)
    : Popup(list, parent)
{

    QObject::connect(listView, &QListView::clicked, this, [&](const QModelIndex& index) {
        QString lang = index.data().toString();

        MainWindow* mainWindow = qobject_cast<MainWindow*>(parentWidget());
        if (mainWindow != 0 && mainWindow->getCurrentTextArea()) {
            QMap<QString, QString> langToExt = {};
            loadLanguageExtensions(langToExt);
            mainWindow->getCurrentTextArea()->updateSyntaxHighlightTags("", langToExt[lang]);
            mainWindow->setLanguageHighlightLabel(lang);
        }

        close();
    });
}

PopupTabWidth::PopupTabWidth(QStringList list, QWidget* parent)
    : Popup(list, parent)
{

    QObject::connect(listView, &QListView::clicked, this, [&](const QModelIndex& index) {
        QString width = index.data().toString();

        MainWindow* mainWindow = qobject_cast<MainWindow*>(parentWidget());
        if (mainWindow != 0) {
            mainWindow->setTabWidthLabel("Tab Width: " + width);
            mainWindow->setTabWidth(width.toInt());
        }

        close();
    });
}

void loadLanguageExtensions(QMap<QString, QString>& map)
{
    QFile TextFile(languageToExtensionPath);

    if (TextFile.open(QIODevice::ReadOnly)) {
        while (!TextFile.atEnd()) {
            QString line = TextFile.readLine();
            QStringList words = line.split(" ");
            if (words.size() == 2)
                map.insert(words[0], cutEndOfLine(words[1]));
        }
    }

    TextFile.close();
}
