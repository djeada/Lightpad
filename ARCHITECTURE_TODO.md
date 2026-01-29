# Architecture Rework TODO List

This document outlines major architectural improvements and refactoring tasks needed to enhance the codebase quality, maintainability, and scalability of Lightpad.

## High Priority

### 1. Modularization and Code Organization
- [ ] **Separate concerns into logical modules**
  - Create separate directories for different concerns (editor, ui, syntax, config, io, etc.)
  - Move related classes into their respective modules
  - Current structure has all files in one directory making it hard to navigate
  
- [ ] **Extract business logic from UI components**
  - MainWindow class (19,426 bytes) is a god object mixing UI, file operations, and application logic
  - Create separate controllers/managers for file operations, editor state, and configuration
  - Implement proper separation between presentation and business logic

- [ ] **Create proper abstraction layers**
  - Define interfaces for key components (Editor, SyntaxHighlighter, FileManager, etc.)
  - Use dependency injection instead of tight coupling between components
  - Current: MainWindow directly creates and manages all components

### 2. Settings and Configuration Management
- [ ] **Implement proper settings architecture**
  - Current: Settings stored in root directory (settings.json)
  - Move user preferences to appropriate OS-specific locations (XDG_CONFIG_HOME on Linux, AppData on Windows)
  - Create a Settings Manager abstraction to handle loading/saving/migration
  - Separate user preferences from application state
  
- [ ] **Add settings validation and migration**
  - Validate settings on load with fallback to defaults
  - Implement version-based settings migration for future compatibility
  - Add schema validation for JSON settings

### 3. Error Handling and Logging
- [ ] **Implement comprehensive error handling**
  - Add proper error handling throughout the codebase (currently minimal)
  - Create custom exception types for different error categories
  - Implement error recovery strategies
  
- [ ] **Add logging infrastructure**
  - Replace scattered qDebug() calls with proper logging framework
  - Implement configurable log levels (DEBUG, INFO, WARN, ERROR)
  - Add log file rotation and management
  - Enable user-configurable logging preferences

### 4. Testing Infrastructure
- [ ] **Set up unit testing framework**
  - Add Qt Test framework or Google Test
  - Create test directory structure (tests/unit, tests/integration)
  - Write unit tests for core components (syntax highlighter, text area, file operations)
  - Aim for >70% code coverage
  
- [ ] **Add integration tests**
  - Test complete user workflows (open file, edit, save, syntax highlighting)
  - Test settings persistence
  - Test multi-tab operations
  
- [ ] **Set up continuous integration**
  - Configure CI pipeline to run tests on every commit
  - Add build verification for multiple platforms (Linux, Windows, macOS)
  - Implement automated code quality checks

### 5. Build System Improvements
- [ ] **Complete CMake migration**
  - Remove QMake files (lightpad.pro) as mentioned in README TODO
  - Ensure CMakeLists.txt includes all source files
  - Add proper install targets
  - Configure CPack for packaging
  
- [ ] **Improve build configuration**
  - Add separate Debug/Release configurations with appropriate flags
  - Implement precompiled headers for faster builds
  - Add option flags for optional features

## Medium Priority

### 6. Plugin Architecture
- [ ] **Design and implement plugin system**
  - Enable extensibility through plugins
  - Define plugin API for syntax highlighters, themes, tools
  - Create plugin manager for loading/unloading plugins
  - Allow custom language support via plugins
  
- [ ] **Refactor syntax highlighting as plugins**
  - Current: Hardcoded syntax rules in code
  - Make each language a loadable plugin
  - Support user-defined highlighting rules (as per README TODO)

### 7. Document Model Improvements
- [ ] **Implement proper document model**
  - Separate document data from view (TextArea currently mixes both)
  - Create Document class to manage file content, undo/redo stack, dirty state
  - Enable multiple views of same document
  - Implement proper undo/redo with command pattern
  
- [ ] **Add document lifecycle management**
  - Implement auto-save functionality
  - Add crash recovery with temporary file backups
  - Handle large files efficiently with streaming/lazy loading

### 8. Multi-threading and Performance
- [ ] **Move blocking operations to background threads**
  - File I/O operations currently block UI
  - Syntax highlighting for large files should be asynchronous
  - Implement QProcess for script execution (as per README TODO)
  
- [ ] **Optimize rendering performance**
  - Profile and optimize syntax highlighting for large files
  - Implement incremental/lazy syntax highlighting
  - Optimize line number rendering

### 9. Editor Features Enhancement
- [ ] **Implement Language Server Protocol (LSP) support**
  - Add LSP client for code intelligence features
  - Support autocompletion (mentioned in README TODO)
  - Add go-to-definition, find references
  - Implement inline error/warning display
  
- [ ] **Enhance VIM mode**
  - Current TODO mentions "Full VIM compatibility"
  - Create proper modal editing system
  - Support VIM commands and motions
  - Add VIM configuration options

### 10. Code Quality Improvements
- [ ] **Fix naming inconsistencies**
  - "prefrences" is misspelled (should be "preferences") in multiple files
  - Standardize naming conventions across codebase
  - Use consistent naming for similar patterns (e.g., get/set vs accessor methods)
  
- [ ] **Remove code duplication**
  - Consolidate similar highlighting rules across languages
  - Extract common UI patterns into reusable components
  - Create utility functions for repeated operations
  
- [ ] **Add code documentation**
  - Add Doxygen or similar documentation comments
  - Document class responsibilities and public APIs
  - Create architecture documentation with diagrams
  - Add inline comments for complex algorithms

### 11. UI/UX Improvements
- [ ] **Implement proper theming system**
  - Refactor Theme class to be more extensible
  - Support loading custom themes from files
  - Separate editor theme from UI theme
  - Add theme preview/selector
  
- [ ] **Enhance preferences dialog**
  - Current preferences implementation is fragmented
  - Create unified settings dialog with categories
  - Add search/filter for settings
  - Implement settings preview/apply/reset

### 12. Resource Management
- [ ] **Optimize resource loading**
  - Current: Resources embedded via QRC
  - Consider lazy loading for large resources
  - Implement resource caching strategy
  - Add resource compression
  
- [ ] **Memory management review**
  - Audit for memory leaks (check pointer ownership)
  - Use smart pointers (unique_ptr, shared_ptr) instead of raw pointers
  - Implement RAII patterns consistently

## Low Priority

### 13. Project/Workspace Support
- [ ] **Add project/workspace concept**
  - Support opening directories as projects
  - Remember open files per project
  - Add project-specific settings
  - Implement file tree/explorer view

### 14. Search and Replace Improvements
- [ ] **Enhance find/replace functionality**
  - Add search across all open files
  - Implement project-wide search
  - Support regex patterns with preview
  - Add search history

### 15. Internationalization
- [ ] **Add i18n support**
  - Extract all user-facing strings
  - Use Qt's translation system (tr())
  - Create translation files for multiple languages
  - Add language selector in preferences

### 16. Accessibility
- [ ] **Improve accessibility**
  - Add keyboard shortcuts documentation
  - Implement screen reader support
  - Add high contrast themes
  - Support font scaling for accessibility

### 17. Documentation
- [ ] **Create developer documentation**
  - Add ARCHITECTURE.md explaining design decisions
  - Create CONTRIBUTING.md with development setup (already exists, could be enhanced)
  - Add API documentation
  - Create plugin development guide
  
- [ ] **Improve user documentation**
  - Create user manual
  - Add in-app help system
  - Create keyboard shortcuts reference
  - Add tutorials for common tasks

## Technical Debt

### 18. Code Modernization
- [ ] **Update to modern C++ practices**
  - Current: C++14, could move to C++17/C++20
  - Use auto where appropriate
  - Replace typedef with using
  - Use nullptr consistently
  - Consider ranges and concepts (C++20)
  
- [ ] **Update Qt usage patterns**
  - Use modern Qt signal/slot syntax consistently
  - Leverage Qt 5 features more effectively
  - Consider Qt 6 migration path

### 19. Security Improvements
- [ ] **Add input validation**
  - Validate file paths and user inputs
  - Sanitize data before external process execution
  - Add sandboxing for script execution
  
- [ ] **Implement secure settings storage**
  - Don't store sensitive data in plain text
  - Add encryption for sensitive preferences if needed

### 20. Packaging and Distribution
- [ ] **Improve packaging**
  - Create proper installers for each platform
  - Add desktop integration (file associations, icons)
  - Implement auto-update mechanism
  - Create portable/AppImage versions

---

## Implementation Strategy

### Phase 1: Foundation (Weeks 1-4)
- Set up testing infrastructure
- Implement logging framework
- Fix critical code quality issues (naming, duplication)
- Complete CMake migration

### Phase 2: Core Refactoring (Weeks 5-12)
- Modularize codebase
- Separate business logic from UI
- Implement proper document model
- Refactor settings management

### Phase 3: Features and Performance (Weeks 13-20)
- Add plugin architecture
- Implement LSP support
- Optimize performance with multi-threading
- Enhance VIM mode

### Phase 4: Polish and Documentation (Weeks 21-24)
- Complete documentation
- Add remaining features (i18n, accessibility)
- Improve packaging and distribution
- Final testing and bug fixes

---

## Contributing

When working on any of these items:
1. Create a feature branch for your changes
2. Add tests for new functionality
3. Update documentation
4. Follow existing code style and conventions
5. Submit a pull request with clear description

## Notes

- This list is living document and should be updated as priorities change
- Some items may be split into smaller tasks when implementation begins
- Cross-reference with existing README.md TODO section
- Consider user feedback and feature requests when prioritizing
