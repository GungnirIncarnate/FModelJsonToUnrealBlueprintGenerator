# Contributing to FModel Blueprint Generator

Thank you for your interest in contributing! This document provides guidelines for contributing to the project.

## How to Contribute

### Reporting Bugs

Before creating a bug report:
1. Check existing issues to avoid duplicates
2. Collect relevant information:
   - Unreal Engine version
   - Error messages from Output Log
   - Sample JSON that causes the issue
   - Steps to reproduce

Create an issue with:
- **Title:** Clear, descriptive summary
- **Description:** Detailed explanation of the problem
- **Steps to Reproduce:** Numbered list
- **Expected Behavior:** What should happen
- **Actual Behavior:** What actually happens
- **Logs:** Relevant Output Log entries
- **Environment:** UE version, OS, etc.

### Suggesting Features

Feature requests should include:
- Use case explaining why the feature is needed
- Proposed implementation (if you have ideas)
- Examples of how it would work
- Impact on existing functionality

### Pull Requests

1. **Fork the repository**
   ```bash
   git clone https://github.com/yourusername/FModelBlueprintGenerator.git
   cd FModelBlueprintGenerator
   ```

2. **Create a feature branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

3. **Make your changes**
   - Follow the coding standards below
   - Add comments explaining complex logic
   - Update documentation if needed

4. **Test thoroughly**
   - Test with various JSON structures
   - Verify Blueprint parent classes work
   - Check function creation
   - Ensure no compilation errors

5. **Commit your changes**
   ```bash
   git add .
   git commit -m "Add feature: description"
   ```

6. **Push to your fork**
   ```bash
   git push origin feature/your-feature-name
   ```

7. **Create Pull Request**
   - Provide clear description of changes
   - Reference related issues
   - Include test results
   - Update documentation

## Coding Standards

### C++ Plugin Code

Follow [Unreal Engine Coding Standard](https://docs.unrealengine.com/5.0/en-US/epic-cplusplus-coding-standard-for-unreal-engine/):

```cpp
// Use descriptive names
bool bIsValid = true;  // Good
bool b = true;         // Bad

// Comment complex logic
// Extract function name from "Function'BP_Item_C:GetName'"
if (ObjectName.StartsWith(TEXT("Function'")))
{
    // ...
}

// Use UE_LOG for debugging
UE_LOG(LogTemp, Warning, TEXT("Processing: %s"), *AssetName);

// Check pointers before use
if (!Blueprint)
{
    UE_LOG(LogTemp, Error, TEXT("Blueprint is null"));
    return false;
}
```

### Python Code

Follow [PEP 8](https://pep8.org/):

```python
# Use descriptive variable names
blueprint_name = "BP_Example"  # Good
bp = "BP_Example"              # Bad

# Add docstrings
def process_file(json_path):
    """
    Process a single JSON file and create Blueprint.
    
    Args:
        json_path: Absolute path to JSON file
        
    Returns:
        True if successful, False otherwise
    """
    pass

# Use logging
import unreal
unreal.log(f"Processing: {asset_name}")
```

### Documentation

- Update README.md for major features
- Add examples to EXAMPLES.md for new JSON structures
- Update API.md for new functions
- Include inline comments for complex logic

## Development Setup

1. **Prerequisites**
   - Unreal Engine 5.1+
   - Visual Studio 2019/2022
   - Git
   - Test UE5 project

2. **Setup**
   ```bash
   git clone [your-fork]
   cd FModelBlueprintGenerator
   
   # Copy to test project
   cp -r Plugins/BlueprintFunctionCreator [TestProject]/Plugins/
   
   # Open test project
   # Plugin will compile automatically
   ```

3. **Testing**
   - Keep a set of test JSON files
   - Test each change with multiple Blueprint types
   - Verify parent class handling
   - Check for memory leaks
   - Test multi-pass processing

## Areas for Contribution

### High Priority

- [ ] Support for AnimBlueprints
- [ ] Function parameter parsing from JSON
- [ ] Return value type detection
- [ ] Variable creation from JSON properties
- [ ] Event node creation

### Medium Priority

- [ ] Widget Blueprint support
- [ ] Blueprint Interface support
- [ ] Macro creation
- [ ] Better error messages
- [ ] Progress bar UI

### Low Priority

- [ ] Performance optimizations
- [ ] Batch processing UI
- [ ] JSON validation before processing
- [ ] Undo/Redo support
- [ ] Custom templates

### Documentation

- [ ] Video tutorials
- [ ] More examples
- [ ] Troubleshooting guide expansion
- [ ] Multi-language support
- [ ] Wiki pages

## Testing Guidelines

### Unit Testing

Currently no automated tests. Contributors should:

1. **Test with minimal JSON**
   - Single function
   - No parent class
   - No components

2. **Test with complex JSON**
   - Multiple functions
   - Blueprint parent
   - C++ parent
   - Multiple components

3. **Test edge cases**
   - Function names with spaces
   - Duplicate functions
   - Missing parent
   - Invalid JSON

4. **Test multi-pass**
   - Parent created after child
   - Deep inheritance chains
   - Circular references (should fail gracefully)

### Integration Testing

Test with real FModel exports:
- Weapon Blueprints
- Character Blueprints
- Item Blueprints
- Action Blueprints

## Code Review Process

Pull requests will be reviewed for:
1. **Functionality** - Does it work as intended?
2. **Code Quality** - Is it clean and maintainable?
3. **Performance** - Any performance concerns?
4. **Documentation** - Is it properly documented?
5. **Testing** - Has it been thoroughly tested?

Reviews typically take 1-3 days.

## Community

- **GitHub Issues:** Bug reports and feature requests
- **GitHub Discussions:** General questions and ideas
- **Pull Requests:** Code contributions

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

## Recognition

Contributors will be:
- Listed in CONTRIBUTORS.md
- Mentioned in release notes
- Credited in README.md

## Questions?

Open a GitHub Discussion or issue if you have questions about contributing!
