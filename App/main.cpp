#include <QApplication>

#include "syntax/cppsyntaxplugin.h"
#include "syntax/csssyntaxplugin.h"
#include "syntax/gosyntaxplugin.h"
#include "syntax/htmlsyntaxplugin.h"
#include "syntax/javascriptsyntaxplugin.h"
#include "syntax/javasyntaxplugin.h"
#include "syntax/jsonsyntaxplugin.h"
#include "syntax/markdownsyntaxplugin.h"
#include "syntax/pythonsyntaxplugin.h"
#include "syntax/rustsyntaxplugin.h"
#include "syntax/shellsyntaxplugin.h"
#include "syntax/syntaxpluginregistry.h"
#include "syntax/typescriptsyntaxplugin.h"
#include "syntax/yamlsyntaxplugin.h"
#include "ui/mainwindow.h"
#include <memory>

void registerBuiltInSyntaxPlugins() {
  auto &registry = SyntaxPluginRegistry::instance();

  // Register built-in language plugins
  registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<CssSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<GoSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<HtmlSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<JavaScriptSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<JavaSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<JsonSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<MarkdownSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<PythonSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<RustSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<ShellSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<TypeScriptSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<YamlSyntaxPlugin>());
}

int main(int argv, char **args) {
  QApplication app(argv, args);
  app.setApplicationName("Lightpad");
  app.setOrganizationName("Lightpad");
  app.setWindowIcon(QIcon(":/resources/icons/app.png"));

  // Initialize syntax highlighting plugins
  registerBuiltInSyntaxPlugins();

  MainWindow w;

  return app.exec();
}
