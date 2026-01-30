#include <QApplication>

#include "ui/mainwindow.h"
#include "syntax/syntaxpluginregistry.h"
#include "syntax/cppsyntaxplugin.h"
#include "syntax/javascriptsyntaxplugin.h"
#include "syntax/pythonsyntaxplugin.h"
#include <memory>

void registerBuiltInSyntaxPlugins()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Register built-in language plugins
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<JavaScriptSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<PythonSyntaxPlugin>());
}

int main(int argv, char** args)
{
    QApplication app(argv, args);
    app.setApplicationName("Lightpad");
    app.setOrganizationName("Lightpad");
    app.setWindowIcon(QIcon(":/resources/icons/app.png"));

    // Initialize syntax highlighting plugins
    registerBuiltInSyntaxPlugins();

    MainWindow w;

    return app.exec();
}
