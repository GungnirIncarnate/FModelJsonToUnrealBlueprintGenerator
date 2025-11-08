# Quick Start Guide

Get up and running in 5 minutes!

## 1. Install Plugin (2 minutes)

```bash
# Copy plugin to your project
Copy FModelBlueprintGenerator/Plugins/BlueprintFunctionCreator
To   YourProject/Plugins/BlueprintFunctionCreator

# Open your Unreal project
# Click "Yes" when prompted to rebuild
# Wait for compilation to complete
```

## 2. Export JSON from FModel (1 minute)

```
1. Open FModel
2. Navigate to Blueprints
3. Right-click → Export → JSON
4. Save to a folder (e.g., D:/JSONExports)
```

## 3. Configure Script (1 minute)

Edit `PythonScript/create_complete_blueprints.py`:

```python
# Line ~182-183
json_root = "d:/your/path/to/JSONExports"  # Change this
destination_root = "/Game/GeneratedBPs"     # Change this
```

## 4. Run Script (1 minute)

In Unreal Editor:

```
1. Window → Developer Tools → Output Log
2. Click "Cmd" at bottom
3. Type:
   import sys
   sys.path.append("d:/path/to/FModelBlueprintGenerator/PythonScript")
   import create_complete_blueprints
```

## 5. Verify Results

Check Content Browser:
- ✅ Blueprints created in `/Game/GeneratedBPs`
- ✅ Parent classes set correctly
- ✅ Functions show proper names
- ✅ Components added to Construction Script

## Common First-Run Issues

### "Module not found"
- Ensure Python Editor Script Plugin is enabled
- Restart Unreal Editor

### "Blueprint is null"
- Check JSON path is correct
- Verify JSON files are Blueprint exports (not Materials/Textures)

### Functions show as "None"
- Update to latest version (v1.0+)
- Delete old Blueprints and regenerate

## Next Steps

- Read INSTALLATION.md for detailed setup
- Check EXAMPLES.md for JSON structure
- Review API.md for advanced usage
- Join discussions on GitHub

## Need Help?

1. Check Output Log for detailed errors
2. Review troubleshooting in INSTALLATION.md
3. Open an issue on GitHub with log output
