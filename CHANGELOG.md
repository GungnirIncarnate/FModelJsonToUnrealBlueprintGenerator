# Changelog

All notable changes to FModel Blueprint Generator will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-11-09

### Added
- Initial release of FModel Blueprint Generator
- C++ Editor plugin `BlueprintFunctionCreator`
- Python script for multi-pass Blueprint generation
- Support for Blueprint parent classes (`Super` field)
- Support for C++ parent classes (`SuperStruct` field)
- Function stub creation with entry and result nodes
- Component addition to Blueprint construction script
- Multi-pass dependency resolution (up to 10 passes)
- Automatic function deduplication from JSON
- Graph name setting to prevent "None" function names
- Function entry node signature configuration
- Parent Blueprint compilation before inheritance checks
- Space handling in function names (replaced with underscores)
- Comprehensive documentation (README, API, Installation, Examples)
- MIT License
- Example scripts for simple usage

### Features
- **JSON Parsing:**
  - Extracts functions from `Children` array
  - Extracts components from `ChildProperties` array
  - Detects parent class from `Super` or `SuperStruct`
  - Uses `AddUnique()` to prevent duplicates
  - Validates function names (no empty, no "None")

- **Blueprint Creation:**
  - Sets correct parent class (C++ or Blueprint)
  - Creates function graphs with proper names
  - Adds entry and result nodes
  - Connects execution pins
  - Adds components to Simple Construction Script
  - Compiles Blueprint after creation

- **Inheritance Handling:**
  - Loads parent Blueprint before creating child
  - Compiles parent if not up-to-date
  - Checks `FindFunctionByName()` to detect inherited functions
  - Skips inherited functions to avoid duplicates
  - Logs inheritance decisions for debugging

- **Multi-Pass Processing:**
  - Scans all JSON files to build dependency graph
  - Pass 1: Creates Blueprints with available parents
  - Pass 2-10: Retries skipped files as parents become available
  - Tracks progress and skipped files
  - Stops when no progress or all complete

### Technical Details
- **Plugin Dependencies:**
  - Core, CoreUObject, Engine
  - Slate, SlateCore
  - UnrealEd, BlueprintGraph, KismetCompiler, Kismet
  - Json, JsonUtilities

- **C++ API Functions:**
  - `ParseFModelJSON()` - Extract data from JSON
  - `CreateBlueprintFromFModelJSON()` - Full Blueprint creation
  - `AddFunctionStubToBlueprint()` - Single function creation
  - `AddMultipleFunctionStubsToBlueprint()` - Batch function creation
  - `AddComponentsToBlueprint()` - Component addition

- **Python Classes:**
  - `BlueprintCreator` - Main orchestrator
  - Methods: `is_blueprint_json()`, `check_parent_exists()`, `process_all_files()`

### Bug Fixes
- **Issue:** Functions showing as "None" in Blueprint Editor
  - **Fix:** Explicitly set graph name with `Rename()` and `CustomGeneratedFunctionName`
  
- **Issue:** Parent class defaulting to AActor for C++ parents
  - **Fix:** Added `SuperStruct` field parsing and `FindObject<UClass>()`
  
- **Issue:** Duplicate functions causing compilation errors
  - **Fix:** Use `AddUnique()` during JSON parsing
  
- **Issue:** Inherited functions being added to child Blueprints
  - **Fix:** Compile parent first, then check `FindFunctionByName()`

### Known Limitations
- Does not recreate function implementation (only stubs)
- Does not parse function parameters or return types
- Does not create variables from JSON properties
- Does not support AnimBlueprints or WidgetBlueprints
- Event graph (Ubergraph) is created but empty
- Component properties (transforms, etc.) not set

### Documentation
- README.md - Overview and features
- INSTALLATION.md - Detailed setup guide
- QUICKSTART.md - 5-minute quick start
- API.md - Complete API reference
- EXAMPLES.md - JSON structure examples
- STRUCTURE.md - Repository organization
- CONTRIBUTING.md - Contribution guidelines
- LICENSE - MIT License

### Testing
- Tested with 12,548+ Palworld FModel JSON exports
- Verified with Blueprint parent hierarchies up to 5 levels deep
- Confirmed C++ parent class resolution
- Validated multi-pass dependency resolution
- Tested function deduplication
- Verified component creation

## [Unreleased]

### Planned Features
- Function parameter parsing
- Return value type detection
- Variable creation from JSON
- AnimBlueprint support
- Widget Blueprint support
- Blueprint Interface support
- Performance optimizations
- Progress bar UI
- Better error messages

### Under Consideration
- Custom Blueprint templates
- Batch processing UI
- Undo/Redo support
- JSON validation tool
- Automated testing framework

---

## Version History Summary

- **v1.0.0** (2025-11-09) - Initial release with full functionality

---

## Migration Guides

### From Manual Blueprint Creation

If you were manually creating Blueprint dummies:

1. **Install the plugin** (replaces manual function creation)
2. **Export JSON from FModel** (same as before)
3. **Run the Python script** (automated vs manual)
4. **Verify results** (check parent classes and functions)

Benefits:
- ✅ 100x faster (automated vs manual)
- ✅ Consistent naming and structure
- ✅ Automatic dependency resolution
- ✅ No missing functions or components

### Breaking Changes

None - this is the initial release.

---

## Support

For issues, questions, or feature requests:
- GitHub Issues: Bug reports
- GitHub Discussions: Questions and ideas
- Documentation: Check relevant .md files

---

## Credits

Developed for the Palworld modding community.

Special thanks to:
- FModel developers for the JSON export capability
- Unreal Engine community for Blueprint API documentation
- Early testers and feedback providers

---

## License

This project is licensed under the MIT License - see LICENSE file for details.
