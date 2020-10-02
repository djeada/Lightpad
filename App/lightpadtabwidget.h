#ifndef LIGHTPADTABWIDGET_H
#define LIGHTPADTABWIDGET_H

#include <QTabWidget>
#include <QToolButton>

const QString unsavedDocumentLabel = "Unsaved Document";

class MainWindow;
class LightpadPage;

class LightpadTabWidget : public QTabWidget
{
    Q_OBJECT

    public:
        LightpadTabWidget(QWidget* parent = nullptr);
        void correctTabButtonPosition();
        void addNewTab();
        void setMainWindow(MainWindow* window);
        void ensureNewTabButtonVisible();
        void setTheme(QString backgroundColor, QString foregroundColor);
        void setFilePath(int index, QString filePath);
        LightpadPage* getPage(int index);
        QString getFilePath(int index);

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void tabRemoved(int index) override;

    private:
        MainWindow* mainWindow;
        QToolButton* newTabButton;
};

#endif // CODEEDITORTABS_H
