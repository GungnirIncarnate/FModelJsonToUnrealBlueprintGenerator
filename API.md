# API Reference

## C++ Plugin API

### UDummyBlueprintFunctionLibrary

Blueprint-callable functions for programmatic Blueprint creation.

---

#### `ParseFModelJSON`

Parses an FModel JSON export file to extract Blueprint information.

```cpp
UFUNCTION(BlueprintCallable, Category = "Blueprint Creation")
static bool ParseFModelJSON(
    const FString& JsonFilePath,
    TArray<FName>& OutFunctionNames,
    TArray<FName>& OutComponentNames,
    TArray<FString>& OutComponentClasses,
    FString& OutParentClassPath
);
```

**Parameters:**
- `JsonFilePath` - Absolute path to the JSON file
- `OutFunctionNames` - Array to populate with function names
- `OutComponentNames` - Array to populate with component names
- `OutComponentClasses` - Array to populate with component class names
- `OutParentClassPath` - String to populate with parent class path

**Returns:** `true` if parsing succeeded, `false` otherwise

**Notes:**
- Automatically deduplicates function and component names
- Handles both `Super` (Blueprint parent) and `SuperStruct` (C++ parent)
- Replaces spaces in function names with underscores
- Filters out `ExecuteUbergraph_*` functions

---

#### `CreateBlueprintFromFModelJSON`

Creates a complete Blueprint from an FModel JSON export.

```cpp
UFUNCTION(BlueprintCallable, Category = "Blueprint Creation")
static UBlueprint* CreateBlueprintFromFModelJSON(
    const FString& JsonFilePath,
    const FString& DestinationPath,
    const FString& AssetName
);
```

**Parameters:**
- `JsonFilePath` - Absolute path to the JSON file
- `DestinationPath` - Content Browser path (e.g., "/Game/Blueprints")
- `AssetName` - Name for the new Blueprint asset

**Returns:** Pointer to the created Blueprint, or `nullptr` on failure

**Workflow:**
1. Parses JSON to extract functions, components, parent class
2. Determines parent class (C++ or Blueprint)
3. Compiles parent Blueprint if needed
4. Creates new Blueprint asset
5. Adds function stubs (skipping inherited functions)
6. Adds components to construction script
7. Compiles the new Blueprint

---

#### `AddFunctionStubToBlueprint`

Adds a single function stub to an existing Blueprint.

```cpp
UFUNCTION(BlueprintCallable, Category = "Blueprint Creation")
static bool AddFunctionStubToBlueprint(
    UBlueprint* Blueprint,
    FName FunctionName,
    bool bHasReturnValue = false
);
```

**Parameters:**
- `Blueprint` - Target Blueprint to modify
- `FunctionName` - Name of the function to create
- `bHasReturnValue` - Whether to create a result node

**Returns:** `true` if function was created successfully

**Creates:**
- New function graph
- Function entry node
- Function result node (if `bHasReturnValue` is true)
- Connection between entry and result

---

#### `AddMultipleFunctionStubsToBlueprint`

Batch adds multiple function stubs to a Blueprint.

```cpp
UFUNCTION(BlueprintCallable, Category = "Blueprint Creation")
static int32 AddMultipleFunctionStubsToBlueprint(
    UBlueprint* Blueprint,
    const TArray<FName>& FunctionNames
);
```

**Parameters:**
- `Blueprint` - Target Blueprint to modify
- `FunctionNames` - Array of function names to create

**Returns:** Number of functions successfully created

**Features:**
- Automatically skips functions that already exist in the Blueprint
- Checks parent class for inherited functions and skips those
- Compiles parent Blueprint before checking inheritance
- Skips `ExecuteUbergraph_*` functions

---

#### `AddComponentsToBlueprint`

Adds components to a Blueprint's Simple Construction Script.

```cpp
UFUNCTION(BlueprintCallable, Category = "Blueprint Creation")
static bool AddComponentsToBlueprint(
    UBlueprint* Blueprint,
    const TArray<FName>& ComponentNames,
    const TArray<FString>& ComponentClasses
);
```

**Parameters:**
- `Blueprint` - Target Blueprint to modify
- `ComponentNames` - Array of component instance names
- `ComponentClasses` - Array of component class names (must match length of ComponentNames)

**Returns:** `true` if components were added successfully

**Notes:**
- Creates Simple Construction Script if it doesn't exist
- Looks up component classes by name
- Skips components if class can't be found

---

## Python Script API

### BlueprintCreator Class

Main orchestrator for multi-pass Blueprint generation.

---

#### `__init__(json_root, destination_root)`

Initialize the Blueprint creator.

```python
creator = BlueprintCreator(
    json_root="d:/path/to/json/files",
    destination_root="/Game/MyBlueprints"
)
```

**Parameters:**
- `json_root` - Filesystem path to JSON files
- `destination_root` - Unreal Content Browser path

**Initialization:**
- Scans all JSON files to build available Blueprint set
- Prepares tracking for multi-pass processing

---

#### `is_blueprint_json(json_path)`

Checks if a JSON file represents a Blueprint asset.

```python
is_bp = creator.is_blueprint_json("d:/path/file.json")
```

**Returns:** `True` if file is a Blueprint, `False` otherwise

**Filters out:**
- Skeletons
- PhysicsAssets
- Materials
- AnimBlueprints
- WidgetBlueprints
- Animations
- Textures

---

#### `check_parent_exists(parent_path, parent_name)`

Checks if a parent Blueprint exists or should be waited for.

```python
exists, should_wait = creator.check_parent_exists(
    "/Game/Parent/BP_Base.0",
    "BlueprintGeneratedClass'BP_Base_C'"
)
```

**Returns:** Tuple of (exists: bool, should_wait: bool)

**Logic:**
- `exists=True, should_wait=False` - Parent exists, proceed
- `exists=False, should_wait=True` - Parent in JSON set but not created yet, wait
- `exists=False, should_wait=False` - Parent not in JSON set, proceed anyway

---

#### `process_all_files()`

Executes multi-pass Blueprint generation.

```python
creator.process_all_files()
```

**Workflow:**
1. **Pass 1:** Create Blueprints whose parents exist
2. **Pass 2-10:** Retry skipped files as parents become available
3. **Complete:** Stop when no progress or all files processed

**Logs:**
- Total files found
- Progress per pass
- Skipped files (with reasons)
- Final statistics

---

## Internal Implementation Details

### Graph Naming

Graphs are named using:
```cpp
NewGraph->Rename(*FuncNameStr, NewGraph->GetOuter(), 
    REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
```

This ensures the UObject has the correct internal name.

### Function Entry Node Setup

```cpp
EntryNode->CustomGeneratedFunctionName = FunctionName;
EntryNode->bIsEditable = true;
```

Sets the signature name for proper compilation.

### Parent Class Resolution

**For C++ classes:**
```cpp
UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
```

**For Blueprint classes:**
```cpp
UBlueprint* ParentBlueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
if (!ParentBlueprint->GeneratedClass || ParentBlueprint->Status != BS_UpToDate)
{
    FKismetEditorUtilities::CompileBlueprint(ParentBlueprint);
}
```

### Deduplication

Uses `TArray::AddUnique()` during JSON parsing:
```cpp
OutFunctionNames.AddUnique(FuncFName);
OutComponentNames.AddUnique(FName(*CompName));
```

---

## Error Handling

### Common Error Codes

**"Blueprint is null"** - Invalid Blueprint pointer passed
**"Invalid function name"** - FName is None or empty
**"Failed to create graph"** - Graph creation failed
**"Failed to get K2 schema"** - Schema cast failed
**"Parent Blueprint not found"** - Asset path is invalid
**"C++ class not found"** - Class doesn't exist in current project

### Logging Levels

- `LogTemp::Error` - Critical failures
- `LogTemp::Warning` - Non-critical issues, skipped items
- `LogTemp::Log` - Informational messages, success confirmations

---

## Performance Considerations

- **Multi-pass limit:** Maximum 10 passes to prevent infinite loops
- **Graph creation:** ~10ms per function on average
- **Blueprint compilation:** ~100-500ms depending on complexity
- **Batch size:** Recommended max 1000 Blueprints per run for stability

---

## Thread Safety

⚠️ **Not thread-safe** - All functions must be called from Game Thread

Use `AsyncTask(ENamedThreads::GameThread, [](){ ... })` if calling from other threads.
