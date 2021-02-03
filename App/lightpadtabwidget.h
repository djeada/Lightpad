#ifndef LIGHTPADTABWIDGET_H
#define LIGHTPADTABWIDGET_H

#include <QTabWidget>
#include <QToolButton>
#include <QTabBar>

const QString unsavedDocumentLabel = "Unsaved Document";
const int buttonSize = 25;

class MainWindow;
class LightpadPage;

class LightpadTabWidget : public QTabWidget
{
    Q_OBJECT

    public:
        LightpadTabWidget(QWidget* parent = nullptr);
        void addNewTab();
        void setMainWindow(MainWindow* window);
        void setTheme(QString backgroundColor, QString foregroundColor);
        void setFilePath(int index, QString filePath);
        void closeAllTabs();
        void closeCurrentTab();
        LightpadPage* getPage(int index);
        LightpadPage* getCurrentPage();
        QString getFilePath(int index);

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void tabRemoved(int index) override;

    private:
        MainWindow* mainWindow;
        QToolButton* newTabButton;
};

#endif // CODEEDITORTABS_H
