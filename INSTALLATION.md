# Installation Guide

## Prerequisites

- Unreal Engine 5.1 or later
- Visual Studio 2019/2022 (for C++ compilation)
- Python 3.7+ (already included with Unreal Engine)
- Unreal Engine Python plugin enabled

## Step 1: Install the C++ Plugin

1. **Copy the plugin to your project:**
   ```
   Copy the Plugins/BlueprintFunctionCreator folder to:
   YourProject/Plugins/BlueprintFunctionCreator/
   ```

2. **Open your Unreal Engine project**
   - The project will detect the new plugin
   - You'll be prompted to rebuild the plugin - click "Yes"
   - Wait for compilation to complete

3. **Verify plugin is loaded:**
   - Go to Edit → Plugins
   - Search for "Blueprint Function Creator"
   - Ensure it's enabled with a checkmark

## Step 2: Set Up Python Environment

1. **Enable Python plugin** (if not already enabled):
   - Edit → Plugins
   - Search for "Python Editor Script Plugin"
   - Enable it and restart the editor

2. **Copy the Python script:**
   ```
   Copy PythonScript/create_complete_blueprints.py to your project:
   YourProject/Content/Python/create_complete_blueprints.py
   
   Or any location you prefer within your project.
   ```

## Step 3: Prepare Your JSON Files

1. **Export from FModel:**
   - Open your game in FModel
   - Navigate to Blueprint assets you want to export
   - Right-click → Export → JSON (.json)
   - Export to a dedicated folder

2. **Organize JSON files:**
   ```
   Recommended structure:
   YourProject/JSONExports/
   ├── Blueprint/
   │   ├── Action/
   │   ├── Weapon/
   │   ├── Character/
   │   └── ...
   ```

## Step 4: Configure the Script

Edit `create_complete_blueprints.py` and set your paths:

```python
# Line ~182 - Set your JSON source folder
json_root = "d:/YourPath/JSONExports"

# Line ~183 - Set your Blueprint destination folder
destination_root = "/Game/YourDestinationFolder"
```

## Step 5: Run the Generator

### Option A: Python Console in Editor

1. Open Unreal Editor
2. Window → Developer Tools → Output Log
3. Click the "Cmd" button at the bottom
4. Type:
   ```python
   import sys
   sys.path.append("d:/YourProject/Content/Python")
   import create_complete_blueprints
   ```

### Option B: Python Script Editor

1. Window → Developer Tools → Python Script Editor
2. Open your `create_complete_blueprints.py`
3. Click "Execute File" or press Ctrl+Enter

### Option C: Command Line

```bash
UnrealEditor-Cmd.exe "YourProject.uproject" -run=pythonscript -script="create_complete_blueprints.py"
```

## Step 6: Monitor Progress

Watch the Output Log for:
- ✅ "Creating Blueprint: BP_YourAsset"
- ✅ "Using C++ parent class: YourClass"
- ✅ "Successfully added function: YourFunction"
- ✅ "Pass 1 complete: X/Y files processed"

## Troubleshooting

### Plugin Won't Compile

**Error:** "Cannot find BlueprintFunctionCreator module"
- Ensure the plugin is in `YourProject/Plugins/`, not `Engine/Plugins/`
- Check that `BlueprintFunctionCreator.uplugin` exists
- Verify Visual Studio is installed

**Error:** "Missing dependencies"
- The plugin should automatically include all dependencies
- If issues persist, check `BlueprintFunctionCreator.Build.cs`

### Python Script Errors

**Error:** "No module named 'unreal'"
- Ensure Python Editor Script Plugin is enabled
- Restart Unreal Editor
- Run script from within the editor, not external Python

**Error:** "Path not found"
- Verify your `json_root` path uses forward slashes: `"d:/path/to/files"`
- Ensure the path exists and contains JSON files
- Check that destination folder exists in Content Browser

### No Blueprints Created

- Check Output Log for errors
- Verify JSON files are valid Blueprint exports (not Materials, Textures, etc.)
- Ensure JSON files have `"Type": "BlueprintGeneratedClass"`
- Try with a single JSON file first to isolate issues

### Functions Show as "None"

This should be fixed in the current version. If you still see this:
- Ensure you're using the latest version of the plugin
- Delete existing Blueprints and regenerate
- Check Output Log for "Created graph with FName:" messages

## Next Steps

After successful installation:
1. Start with a small batch of JSON files (~10-20)
2. Verify Blueprints are created correctly
3. Check parent classes are set properly
4. Verify functions and components appear
5. Run on full dataset once confident

## Getting Help

If you encounter issues:
1. Check the Output Log for detailed error messages
2. Verify your JSON structure matches the expected format
3. Try with a minimal test case
4. Report issues on GitHub with log output
