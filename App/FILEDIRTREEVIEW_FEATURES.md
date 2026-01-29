# FileDirTreeView (LightpadTreeView) - Feature Documentation

## Overview
The FileDirTreeView component has been enhanced with comprehensive file management capabilities, drag-and-drop support, and has been refactored to follow the Model-View-Controller (MVC) architectural pattern.

## Architecture

### MVC Implementation

The component is now structured following MVC principles:

#### 1. **Model** (`FileDirTreeModel`)
- **Location**: `filedirtreemodel.h` / `filedirtreemodel.cpp`
- **Responsibility**: Handles all file system operations and business logic
- **Key Features**:
  - Create new files and directories
  - Remove files and directories (including recursive deletion)
  - Rename files and directories
  - Duplicate files
  - Copy/Cut/Paste operations with clipboard management
  - Generate unique file names when conflicts occur
  - Emit signals for model updates and error notifications

#### 2. **View** (`LightpadTreeView`)
- **Location**: `lightpadpage.h` / `lightpadpage.cpp`
- **Responsibility**: Display the file tree and handle user interactions
- **Key Features**:
  - Display context menu on right-click
  - Handle drag-and-drop events
  - Visualize file system structure
  - Forward user actions to the controller

#### 3. **Controller** (`FileDirTreeController`)
- **Location**: `filedirtreecontroller.h` / `filedirtreecontroller.cpp`
- **Responsibility**: Mediate between Model and View, handle user input validation
- **Key Features**:
  - Display input dialogs for user input (file names, directory names)
  - Show confirmation dialogs for destructive operations
  - Display error messages from the model
  - Coordinate actions between view and model

## Features

### Context Menu Actions

Right-click on any file or directory in the tree view to access these actions:

#### File Creation
- **New File**: Creates a new empty file in the selected directory
  - Opens a dialog to enter the file name
  - Validates that the file doesn't already exist
  
- **New Directory**: Creates a new directory
  - Opens a dialog to enter the directory name
  - Validates that the directory doesn't already exist

#### File Operations
- **Duplicate**: Creates a copy of the selected file with a unique name
  - Automatically generates a name like "filename (1).ext"
  - Only works on files, not directories

- **Rename**: Renames the selected file or directory
  - Opens a dialog pre-filled with the current name
  - Updates all references in the application

#### Clipboard Operations
- **Copy**: Copies the selected file/directory to an internal clipboard
  - Can be pasted multiple times to different locations
  - Original file remains untouched
  - No modal confirmation dialog

- **Cut**: Cuts the selected file/directory to an internal clipboard
  - File will be moved when pasted
  - Original file will be removed after successful paste
  - Clipboard is cleared after paste operation
  - No modal confirmation dialog

- **Paste**: Pastes the copied/cut item to the selected location
  - Automatically handles name conflicts
  - Works with both files and directories

#### Deletion Operations
- **Remove**: Removes the selected file or directory
  - Shows confirmation dialog
  - Recursively deletes directories and their contents
  - Closes any open tabs for deleted files

#### Utility Operations
- **Copy Absolute Path**: Copies the full path of the selected item to system clipboard
  - Useful for sharing paths or referencing files
  - Works with both files and directories

### Drag and Drop Support

The tree view now supports drag-and-drop operations:

#### Drag Operations
- Click and drag any file or directory to move or copy it
- The drag cursor indicates the operation type (move or copy)

#### Drop Operations
- **Drop on Directory**: Moves/copies the dragged item into the directory
- **Drop on File**: Moves/copies the dragged item into the file's parent directory
- **Move Action** (default): Moves the file/directory to the new location
- **Copy Action** (hold Ctrl/Cmd): Copies the file/directory to the new location

#### Smart Conflict Resolution
- Automatically generates unique names if a file with the same name exists
- Format: "filename (1).ext", "filename (2).ext", etc.

## Technical Details

### Signal-Slot Connections

The component uses Qt's signal-slot mechanism for communication:

```cpp
// Model signals
void modelUpdated();          // Emitted when file system changes
void errorOccurred(QString);  // Emitted when an operation fails

// Controller signals
void actionCompleted();       // Emitted when an action succeeds
void fileRemoved(QString);    // Emitted when a file is deleted
```

### Clipboard Management

The internal clipboard system tracks:
- Path of the copied/cut item
- Operation type (Copy or Cut)
- Cleared automatically after Cut operation is completed

### Error Handling

All operations include proper error handling:
- User-friendly error messages via QMessageBox
- Validation of inputs (empty names, existing files, etc.)
- Graceful failure with informative messages
- Source file existence validation before paste operations
- Prevention of circular directory operations (copying directory into itself)
- Cross-filesystem directory move support via copy+delete

## Usage Examples

### Creating a New File
1. Right-click on a directory in the tree view
2. Select "New File" from the context menu
3. Enter the file name in the dialog
4. Click OK to create the file

### Moving Files via Drag-and-Drop
1. Click and drag a file from the tree view
2. Drag it over the target directory
3. Release the mouse button to move the file
4. The file is moved and the view updates automatically

### Copy and Paste
1. Right-click on a file or directory
2. Select "Copy" from the context menu
3. Navigate to the destination directory
4. Right-click on the destination
5. Select "Paste" from the context menu
6. The item is copied with automatic conflict resolution

## Benefits of MVC Architecture

### Separation of Concerns
- **Model**: Pure business logic, no UI dependencies
- **View**: Pure presentation, no business logic
- **Controller**: Mediator, no complex logic

### Testability
- Each component can be tested independently
- Model logic can be tested without UI
- Controller can be tested with mock objects

### Maintainability
- Easy to locate and fix bugs
- Changes to UI don't affect business logic
- Changes to file operations don't affect UI

### Extensibility
- Easy to add new operations (just add to model and controller)
- Easy to change UI behavior (just modify view)
- Easy to add new features without breaking existing code

## Future Enhancements

Potential improvements for future versions:
- Undo/Redo support for file operations
- Multi-selection support
- Keyboard shortcuts for common operations
- Progress indicators for long operations
- File preview in context menu
- Integration with system file manager
- Support for external drag-and-drop (from system file manager)
