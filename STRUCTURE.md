# FModel Blueprint Generator - Repository Structure

```
FModelBlueprintGenerator/
├── README.md                       # Main documentation
├── INSTALLATION.md                 # Step-by-step installation guide
├── API.md                         # Complete API reference
├── EXAMPLES.md                    # JSON structure examples
├── LICENSE                        # MIT License
├── .gitignore                     # Git ignore rules
│
├── Plugins/
│   └── BlueprintFunctionCreator/          # C++ Editor Plugin
│       ├── BlueprintFunctionCreator.uplugin
│       ├── Resources/
│       │   └── Icon128.png
│       └── Source/
│           └── BlueprintFunctionCreator/
│               ├── BlueprintFunctionCreator.Build.cs
│               ├── Private/
│               │   ├── BlueprintFunctionCreator.cpp
│               │   └── DummyBlueprintFunctionLibrary.cpp
│               └── Public/
│                   ├── BlueprintFunctionCreator.h
│                   └── DummyBlueprintFunctionLibrary.h
│
└── PythonScript/
    ├── create_complete_blueprints.py      # Full production script
    └── simple_examples.py                 # Simple usage examples
```

## Component Description

### Documentation Files

- **README.md** - Overview, features, quick start guide
- **INSTALLATION.md** - Detailed installation steps and troubleshooting
- **API.md** - Complete C++ and Python API documentation
- **EXAMPLES.md** - JSON structure examples and edge cases
- **LICENSE** - MIT License
- **.gitignore** - Excludes build artifacts and temporary files

### Plugin (C++)

**BlueprintFunctionCreator** - Unreal Engine 5 Editor plugin

Key files:
- `.uplugin` - Plugin descriptor
- `Build.cs` - Build configuration with dependencies
- `BlueprintFunctionCreator.cpp` - Module startup/shutdown
- `DummyBlueprintFunctionLibrary.cpp` - Core implementation
- `DummyBlueprintFunctionLibrary.h` - Public API declarations

Features:
- Blueprint creation from JSON
- Function stub generation
- Component addition
- Parent class resolution (C++ and Blueprint)
- Multi-pass compilation support

### Python Scripts

**create_complete_blueprints.py** - Production-ready batch processor
- Multi-pass dependency resolution
- Progress tracking
- Error handling
- Asset registry management

**simple_examples.py** - Educational examples
- Single Blueprint creation
- Batch processing
- Manual function addition
- JSON parsing only

## Usage Workflow

```
1. Export JSON from FModel
   └── Place in project folder

2. Install C++ Plugin
   └── Copy to YourProject/Plugins/
   └── Compile on project open

3. Configure Python Script
   └── Set json_root path
   └── Set destination_root path

4. Run Script
   └── Execute in Unreal Editor
   └── Monitor Output Log

5. Verify Blueprints
   └── Check parent classes
   └── Verify function names
   └── Confirm components
```

## File Size Reference

- Plugin source: ~50 KB
- Python scripts: ~15 KB
- Documentation: ~100 KB
- Total repository: ~165 KB (excluding build artifacts)

## Git Repository Structure

When pushing to GitHub:

```
git init
git add .
git commit -m "Initial commit: FModel Blueprint Generator v1.0"
git branch -M main
git remote add origin https://github.com/yourusername/FModelBlueprintGenerator.git
git push -u origin main
```

## Development Setup

For contributors:

1. Clone repository
2. Copy to UE5 project
3. Build plugin
4. Make changes
5. Test with sample JSON files
6. Submit PR with:
   - Code changes
   - Updated documentation
   - Test results

## Dependencies

### Required
- Unreal Engine 5.1+
- Visual Studio 2019/2022
- Python 3.7+ (included with UE5)

### Plugin Dependencies
- Core
- CoreUObject
- Engine
- Slate
- SlateCore
- UnrealEd
- BlueprintGraph
- KismetCompiler
- Kismet
- Json
- JsonUtilities

All dependencies are standard UE5 modules.

## Platform Support

- ✅ Windows (tested)
- ✅ Linux (should work, untested)
- ✅ Mac (should work, untested)

The plugin uses standard Unreal APIs and should be cross-platform compatible.

## Version Information

**Current Version:** 1.0.0

**Changelog:**
- Initial release with full functionality
- Supports Blueprint and C++ parent classes
- Multi-pass dependency resolution
- Function deduplication
- Graph name fixes for "None" issue

**Compatibility:**
- Unreal Engine: 5.1, 5.2, 5.3, 5.4
- FModel: Any version that exports JSON
