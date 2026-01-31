set(CMAKE_INCLUDE_CURRENT_DIR ON)
 
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)

if(WIN32)
  set(CMAKE_WIN32_EXECUTABLE ON)
endif()

# Subprocess library is available in libs/ but not currently used by the main application
# add_subdirectory(libs)

set(project_ui 
    ui/dialogs/colorpicker.ui
    ui/panels/findreplacepanel.ui
    ui/mainwindow.ui
    ui/dialogs/preferences.ui
    settings/preferenceseditor.ui
    ui/panels/preferencesview.ui
    ui/dialogs/runconfigurations.ui
    ui/dialogs/shortcuts.ui
    ui/panels/terminal.ui)

set(project_headers 
    accessibility/accessibilitymanager.h
    completion/completionitem.h
    completion/completioncontext.h
    completion/icompletionprovider.h
    completion/completionproviderregistry.h
    completion/languagekeywordsregistry.h
    completion/completionengine.h
    completion/completionitemmodel.h
    completion/completionwidget.h
    completion/snippetregistry.h
    completion/providers/keywordcompletionprovider.h
    completion/providers/lspcompletionprovider.h
    completion/providers/plugincompletionprovider.h
    completion/providers/snippetcompletionprovider.h
    git/gitintegration.h
    ui/dialogs/colorpicker.h
    ui/dialogs/commandpalette.h
    ui/panels/findreplacepanel.h
    ui/panels/problemspanel.h
    ui/panels/sourcecontrolpanel.h
    core/async/asyncworker.h
    core/document.h
    core/formatter.h
    core/io/filemanager.h
    core/lightpadpage.h
    core/logging/logger.h
    editor/vimmode.h
    i18n/i18n.h
    lsp/lspclient.h
    plugins/iplugin.h
    plugins/isyntaxplugin.h
    plugins/pluginmanager.h
    syntax/lightpadsyntaxhighlighter.h
    syntax/pluginbasedsyntaxhighlighter.h
    syntax/syntaxpluginregistry.h
    syntax/basesyntaxplugin.h
    syntax/cppsyntaxplugin.h
    syntax/javascriptsyntaxplugin.h
    syntax/pythonsyntaxplugin.h
    core/lightpadtabwidget.h
    ui/mainwindow.h
    ui/popup.h
    ui/dialogs/preferences.h
    settings/preferenceseditor.h
    settings/settingsmanager.h
    ui/panels/preferencesview.h
    ui/dialogs/runconfigurations.h
    ui/dialogs/runtemplateselector.h
    ui/dialogs/formattemplateselector.h
    ui/dialogs/shortcuts.h
    ui/panels/terminal.h
    ui/panels/terminaltabwidget.h
    core/textarea.h
    settings/textareasettings.h
    filetree/filedirtreemodel.h
    filetree/filedirtreecontroller.h
    run_templates/runtemplatemanager.h
    format_templates/formattemplatemanager.h
    settings/theme.h)

set(project_sources 
    main.cpp
    accessibility/accessibilitymanager.cpp
    completion/completionproviderregistry.cpp
    completion/languagekeywordsregistry.cpp
    completion/completionengine.cpp
    completion/completionitemmodel.cpp
    completion/completionwidget.cpp
    completion/snippetregistry.cpp
    completion/providers/keywordcompletionprovider.cpp
    completion/providers/lspcompletionprovider.cpp
    completion/providers/plugincompletionprovider.cpp
    completion/providers/snippetcompletionprovider.cpp
    git/gitintegration.cpp
    ui/dialogs/colorpicker.cpp
    ui/dialogs/commandpalette.cpp
    ui/panels/findreplacepanel.cpp
    ui/panels/problemspanel.cpp
    ui/panels/sourcecontrolpanel.cpp
    core/async/asyncworker.cpp
    core/document.cpp
    core/formatter.cpp
    core/io/filemanager.cpp
    core/lightpadpage.cpp
    core/logging/logger.cpp
    editor/vimmode.cpp
    i18n/i18n.cpp
    lsp/lspclient.cpp
    plugins/pluginmanager.cpp
    syntax/lightpadsyntaxhighlighter.cpp
    syntax/pluginbasedsyntaxhighlighter.cpp
    syntax/syntaxpluginregistry.cpp
    syntax/cppsyntaxplugin.cpp
    syntax/javascriptsyntaxplugin.cpp
    syntax/pythonsyntaxplugin.cpp
    core/lightpadtabwidget.cpp
    ui/mainwindow.cpp
    ui/popup.cpp
    ui/dialogs/preferences.cpp
    settings/preferenceseditor.cpp
    settings/settingsmanager.cpp
    ui/panels/preferencesview.cpp
    ui/dialogs/runconfigurations.cpp
    ui/dialogs/runtemplateselector.cpp
    ui/dialogs/formattemplateselector.cpp
    ui/dialogs/shortcuts.cpp
    ui/panels/terminal.cpp
    ui/panels/terminaltabwidget.cpp
    core/textarea.cpp
    settings/textareasettings.cpp
    filetree/filedirtreemodel.cpp
    filetree/filedirtreecontroller.cpp
    run_templates/runtemplatemanager.cpp
    format_templates/formattemplatemanager.cpp
    settings/theme.cpp)

set(project_resources
    highlight.qrc
    icons.qrc
    messages.qrc
    run_templates/run_templates.qrc
    format_templates/format_templates.qrc)

add_executable(Lightpad 
    ${project_headers} 
    ${project_sources}
    ${project_resources})

target_link_libraries(Lightpad 
    PRIVATE 
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets)

# Add QT_DEPRECATED_WARNINGS definition
target_compile_definitions(Lightpad PRIVATE QT_DEPRECATED_WARNINGS)
