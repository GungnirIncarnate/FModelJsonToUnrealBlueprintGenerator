# FModel Blueprint Generator

Automated Blueprint creation tool for Unreal Engine 5 that generates functional Blueprint dummies from FModel JSON exports.

## Overview

This tool consists of two main components:
1. **BlueprintFunctionCreator** - C++ Editor plugin that provides Blueprint API access
2. **Python Script** - Orchestrates multi-pass Blueprint generation with dependency resolution

## Features

- ✅ Creates Blueprint classes with proper C++ and Blueprint parent inheritance
- ✅ Generates function stubs with entry/result nodes
- ✅ Detects and skips inherited functions to avoid duplicates
- ✅ Adds components from JSON definitions
- ✅ Multi-pass processing for dependency resolution
- ✅ Handles spaces in function names
- ✅ Deduplicates functions that appear multiple times in JSON

## Requirements

- Unreal Engine 5.1+
- Python 3.x (with Unreal Engine Python plugin enabled)
- FModel for exporting JSON files

## Installation

### 1. Install the C++ Plugin

Copy the `Plugins/BlueprintFunctionCreator` folder to your Unreal Engine project's `Plugins` directory:

```
YourProject/
└── Plugins/
    └── BlueprintFunctionCreator/
```

The plugin will automatically compile when you open your project.

### 2. Set Up the Python Script

Copy `create_complete_blueprints.py` to a location in your project (e.g., `DummyAssetGenerator/Processing/`).

## Usage

### 1. Export JSON from FModel

1. Open your game in FModel
2. Navigate to the assets you want to export
3. Right-click → Export → JSON (.json)
4. Export all desired Blueprint assets

### 2. Organize JSON Files

Place your exported JSON files in a folder structure like:
```
Pal (Dummies) New/
├── Blueprint/
│   ├── Action/
│   ├── Weapon/
│   └── ...
```

### 3. Run the Python Script

In Unreal Engine's Python console or via command line:

```python
import unreal

# Set your paths
json_root = "d:/path/to/your/Pal (Dummies) New"
destination_root = "/Game/YourFolder"

# Run the generator
# (See script for full implementation)
```

The script will:
- Scan all JSON files
- Create base classes first
- Create child classes in subsequent passes
- Skip inherited functions
- Add components and function stubs

### 4. Review Generated Blueprints

Open the generated Blueprints in Unreal Editor to verify:
- Parent class is set correctly
- Functions are named properly (not "None")
- Override functions are marked correctly
- Components are present

## How It Works

### C++ Plugin: BlueprintFunctionCreator

Provides Blueprint-callable functions:

- `ParseFModelJSON()` - Extracts functions, components, and parent class from JSON
- `CreateBlueprintFromFModelJSON()` - Creates a complete Blueprint with all elements
- `AddFunctionStubToBlueprint()` - Creates individual function graphs
- `AddMultipleFunctionStubsToBlueprint()` - Batch creates functions with inheritance checking
- `AddComponentsToBlueprint()` - Adds components to Blueprint's construction script

**Key Features:**
- Handles both C++ parent classes (`SuperStruct`) and Blueprint parents (`Super`)
- Explicitly sets graph names and function entry node signatures
- Compiles parent Blueprints before checking for inherited functions
- Uses `AddUnique()` to deduplicate JSON entries

### Python Script: Multi-Pass Processing

The Python script handles dependency resolution:

1. **Scan Phase** - Builds a set of all available Blueprint names
2. **Pass 1** - Creates Blueprints whose parents exist or aren't in the JSON set
3. **Pass 2-10** - Retries skipped files as parent Blueprints become available
4. **Completion** - Stops when no progress is made or all files are processed

**Key Features:**
- Filters out non-Blueprint JSON files (Skeletons, Materials, etc.)
- Checks if parent exists before creating child
- Explicit asset saving for asset registry refresh
- Progress tracking and logging

## JSON Structure

The tool expects FModel JSON exports with this structure:

```json
[
  {
    "Type": "BlueprintGeneratedClass",
    "Name": "BP_YourBlueprint_C",
    "Super": {
      "ObjectName": "BlueprintGeneratedClass'BP_ParentBlueprint_C'",
      "ObjectPath": "/Game/Path/To/BP_ParentBlueprint.0"
    },
    "SuperStruct": {
      "ObjectName": "Class'YourCppClass'",
      "ObjectPath": "/Script/YourModule"
    },
    "Children": [
      {
        "ObjectName": "Function'BP_YourBlueprint_C:YourFunction'",
        "ObjectPath": "/Game/Path/To/BP_YourBlueprint.1"
      }
    ],
    "ChildProperties": [
      {
        "Type": "ObjectProperty",
        "Name": "YourComponent",
        "PropertyType": "Class'YourComponentClass'"
      }
    ]
  }
]
```

- `Super` - Blueprint parent class (optional)
- `SuperStruct` - C++ parent class (optional)
- `Children` - Array of functions
- `ChildProperties` - Array of components

## Troubleshooting

### Functions Show as "None"

**Cause:** Graph names not set properly during creation.

**Solution:** Already fixed in latest version - graphs are explicitly renamed after creation.

### Parent Class Not Set

**Cause:** C++ parent class not being parsed from `SuperStruct` field.

**Solution:** Already fixed - now parses both `Super` (Blueprint) and `SuperStruct` (C++ class).

### Duplicate Functions

**Cause:** Same function appears multiple times in JSON.

**Solution:** Already fixed - uses `AddUnique()` during JSON parsing.

### Parent Blueprint Not Found

**Cause:** Parent hasn't been created yet due to processing order.

**Solution:** Multi-pass processing automatically retries in subsequent passes.

### Plugin Won't Compile

**Common issues:**
- Missing dependencies - ensure `Json` and `JsonUtilities` are in Build.cs
- Class name conflicts - plugin uses `UDummyBlueprintFunctionLibrary` to avoid UE5 built-in conflicts

## Contributing

Contributions welcome! Please:
1. Test changes with a variety of Blueprint types
2. Ensure C++ code follows Unreal coding standards
3. Update documentation for new features

## License

[Add your license here]

## Credits

Developed for Palworld modding community.

## Version History

### v1.0.0
- Initial release
- Support for function stub creation
- Component addition
- Multi-pass dependency resolution
- C++ and Blueprint parent class support
- Function name deduplication
