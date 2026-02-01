set(CMAKE_INCLUDE_CURRENT_DIR ON)
 
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
find_package(Qt6 QUIET COMPONENTS Pdf PdfWidgets)

option(ENABLE_PDF_SUPPORT "Enable PDF viewer support (requires Qt6Pdf)" ON)

if(ENABLE_PDF_SUPPORT AND Qt6Pdf_FOUND AND Qt6PdfWidgets_FOUND)
    set(HAVE_PDF_SUPPORT TRUE)
    message(STATUS "PDF support: enabled")
else()
    set(HAVE_PDF_SUPPORT FALSE)
    if(ENABLE_PDF_SUPPORT)
        message(STATUS "PDF support: disabled (Qt6Pdf/Qt6PdfWidgets not found)")
    else()
        message(STATUS "PDF support: disabled (by option)")
    endif()
endif()

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
    core/editor/texttransforms.h
    core/editor/linenumberarea.h
    core/editor/multicursor.h
    core/editor/codefolding.h
    dap/dapclient.h
    dap/idebugadapter.h
    dap/debugadapterregistry.h
    dap/breakpointmanager.h
    dap/debugconfiguration.h
    dap/watchmanager.h
    dap/debugsession.h
    dap/debugsettings.h
    git/gitintegration.h
    ui/dialogs/colorpicker.h
    ui/dialogs/commandpalette.h
    ui/dialogs/gotolinedialog.h
    ui/dialogs/gotosymboldialog.h
    ui/dialogs/recentfilesdialog.h
    ui/dialogs/filequickopen.h
    ui/dialogs/gitinitdialog.h
    ui/dialogs/mergeconflictdialog.h
    ui/dialogs/gitremotedialog.h
    ui/dialogs/gitstashdialog.h
    ui/panels/findreplacepanel.h
    ui/panels/problemspanel.h
    ui/panels/breadcrumbwidget.h
    ui/panels/debugpanel.h
    ui/panels/sourcecontrolpanel.h
    ui/panels/minimap.h
    ui/panels/spliteditorcontainer.h
    core/async/asyncworker.h
    core/document.h
    core/formatter.h
    core/io/filemanager.h
    core/lightpadpage.h
    core/logging/logger.h
    core/recentfilesmanager.h
    core/navigationhistory.h
    core/autosavemanager.h
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
    syntax/csssyntaxplugin.h
    syntax/gosyntaxplugin.h
    syntax/htmlsyntaxplugin.h
    syntax/javascriptsyntaxplugin.h
    syntax/javasyntaxplugin.h
    syntax/jsonsyntaxplugin.h
    syntax/markdownsyntaxplugin.h
    syntax/pythonsyntaxplugin.h
    syntax/rustsyntaxplugin.h
    syntax/shellsyntaxplugin.h
    syntax/typescriptsyntaxplugin.h
    syntax/yamlsyntaxplugin.h
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
    ui/panels/shellprofile.h
    core/textarea.h
    settings/textareasettings.h
    filetree/filedirtreemodel.h
    filetree/filedirtreecontroller.h
    filetree/gitfilesystemmodel.h
    run_templates/runtemplatemanager.h
    format_templates/formattemplatemanager.h
    settings/theme.h
    ui/viewers/imageviewer.h)

if(HAVE_PDF_SUPPORT)
    list(APPEND project_headers ui/viewers/pdfviewer.h)
endif()

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
    core/editor/texttransforms.cpp
    core/editor/linenumberarea.cpp
    core/editor/multicursor.cpp
    core/editor/codefolding.cpp
    dap/dapclient.cpp
    dap/debugadapterregistry.cpp
    dap/breakpointmanager.cpp
    dap/debugconfiguration.cpp
    dap/watchmanager.cpp
    dap/debugsession.cpp
    dap/debugsettings.cpp
    git/gitintegration.cpp
    ui/dialogs/colorpicker.cpp
    ui/dialogs/commandpalette.cpp
    ui/dialogs/gotolinedialog.cpp
    ui/dialogs/gotosymboldialog.cpp
    ui/dialogs/recentfilesdialog.cpp
    ui/dialogs/filequickopen.cpp
    ui/dialogs/gitinitdialog.cpp
    ui/dialogs/mergeconflictdialog.cpp
    ui/dialogs/gitremotedialog.cpp
    ui/dialogs/gitstashdialog.cpp
    ui/panels/findreplacepanel.cpp
    ui/panels/problemspanel.cpp
    ui/panels/breadcrumbwidget.cpp
    ui/panels/debugpanel.cpp
    ui/panels/sourcecontrolpanel.cpp
    ui/panels/minimap.cpp
    ui/panels/spliteditorcontainer.cpp
    core/async/asyncworker.cpp
    core/document.cpp
    core/formatter.cpp
    core/io/filemanager.cpp
    core/lightpadpage.cpp
    core/logging/logger.cpp
    core/recentfilesmanager.cpp
    core/navigationhistory.cpp
    core/autosavemanager.cpp
    editor/vimmode.cpp
    i18n/i18n.cpp
    lsp/lspclient.cpp
    plugins/pluginmanager.cpp
    syntax/lightpadsyntaxhighlighter.cpp
    syntax/pluginbasedsyntaxhighlighter.cpp
    syntax/syntaxpluginregistry.cpp
    syntax/cppsyntaxplugin.cpp
    syntax/csssyntaxplugin.cpp
    syntax/gosyntaxplugin.cpp
    syntax/htmlsyntaxplugin.cpp
    syntax/javascriptsyntaxplugin.cpp
    syntax/javasyntaxplugin.cpp
    syntax/jsonsyntaxplugin.cpp
    syntax/markdownsyntaxplugin.cpp
    syntax/pythonsyntaxplugin.cpp
    syntax/rustsyntaxplugin.cpp
    syntax/shellsyntaxplugin.cpp
    syntax/typescriptsyntaxplugin.cpp
    syntax/yamlsyntaxplugin.cpp
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
    ui/panels/shellprofile.cpp
    core/textarea.cpp
    settings/textareasettings.cpp
    filetree/filedirtreemodel.cpp
    filetree/filedirtreecontroller.cpp
    filetree/gitfilesystemmodel.cpp
    run_templates/runtemplatemanager.cpp
    format_templates/formattemplatemanager.cpp
    settings/theme.cpp
    ui/viewers/imageviewer.cpp)

if(HAVE_PDF_SUPPORT)
    list(APPEND project_sources ui/viewers/pdfviewer.cpp)
endif()

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

if(APPLE)
  set_target_properties(Lightpad PROPERTIES MACOSX_BUNDLE TRUE)
endif()

target_link_libraries(Lightpad 
    PRIVATE 
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets)

if(HAVE_PDF_SUPPORT)
    target_link_libraries(Lightpad PRIVATE Qt6::Pdf Qt6::PdfWidgets)
    target_compile_definitions(Lightpad PRIVATE HAVE_PDF_SUPPORT)
endif()

# Add QT_DEPRECATED_WARNINGS definition
target_compile_definitions(Lightpad PRIVATE QT_DEPRECATED_WARNINGS)
