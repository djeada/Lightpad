#ifndef POPUP_H
#define POPUP_H

#include <QDialog>
#include <QListView>
#include <QWidget>

class ListView : public QListView {
  Q_OBJECT

public:
  ListView(QWidget *parent = nullptr);
  QSize sizeHint() const;
};

class Popup : public QDialog {
  Q_OBJECT

public:
  Popup(QStringList list, QWidget *parent = nullptr);

protected:
  ListView *listView;

private:
  QStringList list;
};

class PopupLanguageHighlight : public Popup {
public:
  PopupLanguageHighlight(QStringList list, QWidget *parent = nullptr);
};

class PopupTabWidth : public Popup {
public:
  PopupTabWidth(QStringList list, QWidget *parent = nullptr);
};

void loadLanguageExtensions(QMap<QString, QString> &map);

const QString languageToExtensionPath =
    ":/resources/highlight/LanguageToExtension.txt";

#endif // POPUP_H
