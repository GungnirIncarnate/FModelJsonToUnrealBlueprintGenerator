"""
FModel JSON to Unreal Blueprint Converter - WITH FUNCTION SUPPORT!

This script uses the BlueprintFunctionCreator plugin to create COMPLETE
Blueprint dummy assets including components AND functions!

REQUIREMENTS:
1. BlueprintFunctionCreator plugin must be compiled and enabled
2. Run this script FROM WITHIN Unreal Editor using the Python plugin
3. File → Execute Python Script → Select this file

FEATURES:
✅ Creates UserDefinedStruct assets first (dependencies)
✅ Creates Blueprint assets
✅ Adds components from JSON
✅ Adds function stubs from JSON (via plugin!)
✅ Processes thousands of files automatically
✅ Auto-detects JSON folder
"""

import unreal
import os
import json
import sys
import importlib
from pathlib import Path


class CompleteBlueprintConverter:
    """Creates COMPLETE Blueprint dummies with functions using the C++ plugin"""
    
    def __init__(self, json_folder=None):
        """Initialize converter with auto-detection"""
        if json_folder is None:
            json_folder = self.find_json_folder()
            if json_folder is None:
                raise ValueError("Could not find JSON folder!")
        
        self.json_folder = Path(json_folder)
        self.blueprint_lib = unreal.DummyBlueprintFunctionLibrary
        
        # Build a set of all Blueprint names we're going to create
        self.available_blueprints = set()
        self._scan_available_blueprints()
        
        self.stats = {
            'total': 0,
            'created': 0,
            'failed': 0,
            'errors': []
        }
    
    def _scan_available_blueprints(self):
        """Scan all JSON files to build a list of available Blueprints"""
        import json
        for json_file in self.json_folder.rglob('*.json'):
            try:
                with open(json_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                if isinstance(data, list) and len(data) > 0:
                    # Scan ALL entries to find the BlueprintGeneratedClass (it might not be first)
                    for entry in data:
                        if entry.get('Type') == 'BlueprintGeneratedClass':
                            name = entry.get('Name', '')
                            if name:
                                self.available_blueprints.add(name)
                                break  # Only need one BlueprintGeneratedClass per file
            except Exception:
                pass
        
        unreal.log(f"Found {len(self.available_blueprints)} Blueprints to create")
        # Debug: show first 10
        sample = list(self.available_blueprints)[:10]
        unreal.log(f"Sample Blueprint names: {sample}")
    
    def find_json_folder(self):
        """Auto-detect JSON folder in script directory"""
        script_dir = Path(__file__).parent
        unreal.log(f"Searching for JSON folder in: {script_dir}")
        
        json_folders = []
        for item in script_dir.iterdir():
            if item.is_dir():
                json_files = list(item.rglob('*.json'))
                if json_files:
                    json_folders.append((item, len(json_files)))
                    unreal.log(f"Found {len(json_files)} JSON files in: {item.name}")
        
        if not json_folders:
            unreal.log_error("No folders with JSON files found!")
            return None
        
        best_folder, file_count = max(json_folders, key=lambda x: x[1])
        unreal.log(f"\n✅ Auto-selected: {best_folder.name} ({file_count} JSON files)")
        return str(best_folder)
    
    def create_user_defined_struct(self, json_file):
        """Create a UserDefinedStruct asset from JSON using C++ plugin"""
        try:
            unreal.log(f"  📄 Reading JSON: {json_file.name}")
            
            # Parse JSON to validate
            with open(json_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            if not isinstance(data, list) or len(data) == 0:
                unreal.log_warning(f"  ⚠️ Invalid JSON format (not a list or empty)")
                return False
            
            entry = data[0]
            if entry.get('Type') != 'UserDefinedStruct':
                unreal.log_warning(f"  ⚠️ Not a UserDefinedStruct (type: {entry.get('Type')})")
                return False
            
            struct_name = entry.get('Name', '')
            if not struct_name:
                unreal.log_warning(f"  ⚠️ No struct name found in JSON")
                return False
            
            unreal.log(f"  📦 Struct name: {struct_name}")
            
            # Get destination path
            dest_path, _ = self.get_destination_path(json_file)
            if not dest_path:
                unreal.log_error(f"  ❌ Could not determine destination path")
                return False
            
            full_path = f"{dest_path}/{struct_name}"
            unreal.log(f"  📍 Target path: {full_path}")
            
            # Check if already exists
            if unreal.EditorAssetLibrary.does_asset_exist(full_path):
                unreal.log(f"  ✅ Struct already exists: {struct_name}")
                return True
            
            # Use C++ plugin to create the struct
            unreal.log(f"  🔨 Creating struct asset via C++ plugin...")
            new_struct = self.blueprint_lib.create_user_defined_struct_from_json(
                str(json_file),
                dest_path,
                struct_name
            )
            
            if new_struct:
                unreal.log(f"  ✅ Created struct: {struct_name} at {full_path}")
                return True
            else:
                unreal.log_error(f"  ❌ Failed to create struct (C++ function returned None)")
                return False
                
        except Exception as e:
            unreal.log_error(f"  ❌ Exception creating struct {json_file.name}: {str(e)}")
            import traceback
            unreal.log_error(traceback.format_exc())
            return False
    
    def process_all_structs(self):
        """Process all UserDefinedStruct JSON files first"""
        unreal.log("\n" + "="*80)
        unreal.log("📦 CREATING USER-DEFINED STRUCTS (Phase 1)")
        unreal.log("="*80 + "\n")
        
        struct_files = []
        for json_file in self.json_folder.rglob('F_*.json'):
            try:
                with open(json_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                if isinstance(data, list) and len(data) > 0:
                    if data[0].get('Type') == 'UserDefinedStruct':
                        struct_files.append(json_file)
            except Exception:
                pass
        
        unreal.log(f"Found {len(struct_files)} UserDefinedStruct files\n")
        
        created = 0
        failed = 0
        
        for i, struct_file in enumerate(struct_files, 1):
            unreal.log(f"[{i}/{len(struct_files)}] Processing {struct_file.name}")
            if self.create_user_defined_struct(struct_file):
                created += 1
            else:
                failed += 1
        
        unreal.log("\n" + "="*80)
        unreal.log(f"✅ Struct creation complete: {created} created, {failed} failed")
        unreal.log("="*80 + "\n")
    
    def get_destination_path(self, json_file_path):
        """Convert JSON file path to Unreal destination path"""
        try:
            rel_path = json_file_path.relative_to(self.json_folder)
        except ValueError:
            parts = json_file_path.parts
            if 'Game' in parts:
                game_index = parts.index('Game')
                rel_path = Path(*parts[game_index:])
            else:
                return None, None
        
        # Get folder path (without filename)
        path_parts = list(rel_path.parts[:-1])
        asset_name = rel_path.stem
        
        # Build Unreal path
        if path_parts and path_parts[0] == 'Game':
            dest_path = '/' + '/'.join(path_parts)
        else:
            dest_path = '/Game/Pal/' + '/'.join(path_parts) if path_parts else '/Game/Pal'
        
        return dest_path, asset_name
    
    def process_json_file(self, json_file):
        """Process a single JSON file using the plugin"""
        self.stats['total'] += 1
        
        # Get destination path
        dest_path, asset_name = self.get_destination_path(json_file)
        if not dest_path or not asset_name:
            self.stats['errors'].append(f"Could not determine path for: {json_file.name}")
            self.stats['failed'] += 1
            return False
        
        # Check if already exists
        full_path = f"{dest_path}/{asset_name}"
        if unreal.EditorAssetLibrary.does_asset_exist(full_path):
            unreal.log(f"⏭️ Skipping existing: {asset_name}")
            return True
        
        # Also check with /Content/Pal/ prefix in case Unreal remapped the path
        if dest_path.startswith('/Game/Pal/') and not dest_path.startswith('/Game/Pal/Content/'):
            alt_path = dest_path.replace('/Game/Pal/', '/Game/Pal/Content/Pal/')
            alt_full_path = f"{alt_path}/{asset_name}"
            if unreal.EditorAssetLibrary.does_asset_exist(alt_full_path):
                unreal.log(f"⏭️ Skipping existing (alt path): {asset_name}")
                return True
        
        try:
            # Use plugin to create complete Blueprint!
            blueprint = self.blueprint_lib.create_blueprint_from_f_model_json(
                str(json_file),
                dest_path,
                asset_name
            )
            
            if blueprint:
                # Force save the Blueprint
                full_path = f"{dest_path}/{asset_name}"
                unreal.EditorAssetLibrary.save_asset(full_path)
                
                # Add to our available list immediately for dependency resolution
                self.available_blueprints.add(asset_name)
                
                unreal.log(f"✅ Created with functions: {asset_name}")
                self.stats['created'] += 1
                return True
            else:
                unreal.log_warning(f"❌ Failed to create: {asset_name}")
                self.stats['failed'] += 1
                return False
                
        except Exception as e:
            error_msg = f"Error processing {asset_name}: {str(e)}"
            self.stats['errors'].append(error_msg)
            unreal.log_error(error_msg)
            self.stats['failed'] += 1
            return False
    
    def is_blueprint_json(self, json_file):
        """Check if JSON file is a BlueprintGeneratedClass"""
        import json
        
        try:
            with open(json_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            if isinstance(data, list) and len(data) > 0:
                # Scan ALL entries to find the BlueprintGeneratedClass (it might not be first)
                for entry in data:
                    if entry.get('Type') == 'BlueprintGeneratedClass':
                        return True
                return False
            
            return False
            
        except Exception:
            return False
    
    def check_parent_exists(self, json_file):
        """Check if the parent Blueprint exists for this JSON file"""
        import json
        
        try:
            with open(json_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            # Look for BlueprintGeneratedClass with Super field
            if isinstance(data, list) and len(data) > 0:
                # Scan ALL entries to find the BlueprintGeneratedClass (it might not be first)
                blueprint_entry = None
                for entry in data:
                    if entry.get('Type') == 'BlueprintGeneratedClass':
                        blueprint_entry = entry
                        break
                
                if blueprint_entry:
                    super_obj = blueprint_entry.get('Super')
                    if super_obj and isinstance(super_obj, dict):
                        parent_name = super_obj.get('ObjectName', '')
                        parent_path = super_obj.get('ObjectPath', '')
                        
                        if parent_path and parent_path.startswith('/Game/'):
                            # Extract the parent class name from ObjectName (e.g., "BlueprintGeneratedClass'BP_GatlingGun_C'")
                            if parent_name.startswith('BlueprintGeneratedClass'):
                                # Extract class name: "BlueprintGeneratedClass'BP_GatlingGun_C'" -> "BP_GatlingGun_C"
                                parent_class_name = parent_name.split("'")[1] if "'" in parent_name else ""
                                
                                unreal.log(f"  Checking parent: {parent_class_name}, In available list: {parent_class_name in self.available_blueprints}")
                                
                                # Check if this parent is in our list of Blueprints to create
                                if parent_class_name in self.available_blueprints:
                                    # Parent is a Blueprint we're creating, check if it exists yet
                                    # Remove the _C suffix to get the asset name
                                    parent_asset_name = parent_class_name.replace('_C', '')
                                    # Remove the .0 or other suffixes from the path and build proper path
                                    parent_asset_path = parent_path.rsplit('.', 1)[0]  # Remove .0
                                    # Use asset name (remove _C suffix only from end) for existence check
                                    if parent_class_name.endswith('_C'):
                                        parent_asset_name = parent_class_name[:-2]  # Remove last 2 characters
                                    else:
                                        parent_asset_name = parent_class_name
                                    parent_full_path = f"{parent_asset_path}.{parent_asset_name}"
                                    
                                    exists = unreal.EditorAssetLibrary.does_asset_exist(parent_full_path)
                                    unreal.log(f"  Checking path: {parent_full_path}, Exists: {exists}")
                                    
                                    # If not found, try with /Content/Pal/ prefix (Unreal may remap paths)
                                    if not exists and parent_asset_path.startswith('/Game/Pal/') and not parent_asset_path.startswith('/Game/Pal/Content/'):
                                        # The actual structure is /Game/Pal/Content/Pal/...
                                        alt_path = parent_asset_path.replace('/Game/Pal/', '/Game/Pal/Content/Pal/')
                                        alt_full_path = f"{alt_path}.{parent_asset_name}"
                                        exists = unreal.EditorAssetLibrary.does_asset_exist(alt_full_path)
                                        if exists:
                                            unreal.log(f"  ✓ Found at alternate path: {alt_full_path}")
                                        else:
                                            unreal.log(f"  ✗ Also checked: {alt_full_path}, Exists: {exists}")
                                    
                                    if not exists:
                                        unreal.log(f"  ⏸️ Waiting for parent: {parent_class_name}")
                                    else:
                                        unreal.log(f"  ✅ Parent exists: {parent_class_name}")
                                    return exists
                                else:
                                    # Parent is not in our JSON files, it's from the original game
                                    # We can't create it, so just use AActor as parent
                                    unreal.log(f"  ⚠️ Parent not in JSON files (will use AActor): {parent_class_name}")
                                    return True
                            else:
                                # It's a native class, can proceed
                                return True
            
            # No parent specified, can proceed
            return True
            
        except Exception as e:
            unreal.log_warning(f"Error checking parent for {json_file.name}: {str(e)}")
            return True  # Proceed anyway if we can't check
    
    def sort_by_dependencies(self, json_files):
        """Sort JSON files by dependency order - parents before children"""
        import json
        
        # Build dependency map: child -> parent_file
        file_to_class = {}  # json_file -> class_name
        class_to_file = {}  # class_name -> json_file
        dependencies = {}   # child_class -> parent_class
        
        # First pass: map files to class names
        for json_file in json_files:
            try:
                with open(json_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                
                if isinstance(data, list) and len(data) > 0:
                    for entry in data:
                        if entry.get('Type') == 'BlueprintGeneratedClass':
                            class_name = entry.get('Name', '')
                            if class_name:
                                file_to_class[json_file] = class_name
                                class_to_file[class_name] = json_file
                                
                                # Extract parent dependency
                                super_obj = entry.get('Super')
                                if super_obj and isinstance(super_obj, dict):
                                    parent_name = super_obj.get('ObjectName', '')
                                    if parent_name.startswith('BlueprintGeneratedClass'):
                                        if "'" in parent_name:
                                            parent_class = parent_name.split("'")[1]
                                            dependencies[class_name] = parent_class
                                break
            except Exception:
                continue
        
        # Topological sort - parents before children
        sorted_files = []
        processed = set()
        in_progress = set()
        
        def visit(json_file):
            if json_file in processed:
                return
            if json_file in in_progress:
                return  # Circular dependency, skip
                
            class_name = file_to_class.get(json_file)
            if not class_name:
                sorted_files.append(json_file)
                processed.add(json_file)
                return
                
            in_progress.add(json_file)
            
            # Process parent first
            parent_class = dependencies.get(class_name)
            if parent_class and parent_class in class_to_file:
                parent_file = class_to_file[parent_class]
                if parent_file not in processed:
                    visit(parent_file)
            
            # Now process this file
            sorted_files.append(json_file)
            processed.add(json_file)
            in_progress.remove(json_file)
        
        # Visit all files
        for json_file in json_files:
            visit(json_file)
            
        unreal.log(f"  Sorted {len(sorted_files)} files by dependency order")
        return sorted_files
    
    def process_all(self):
        """Process all JSON files with multi-pass for parent dependencies"""
        all_json_files = list(self.json_folder.rglob('*.json'))
        
        # Filter to only Blueprint JSON files
        unreal.log("Filtering Blueprint JSON files...")
        json_files = [f for f in all_json_files if self.is_blueprint_json(f)]
        
        # Sort by dependency order - parents first
        unreal.log("Sorting by dependency order...")
        json_files = self.sort_by_dependencies(json_files)
        
        total = len(json_files)
        non_blueprint_count = len(all_json_files) - total
        
        unreal.log("\n" + "="*80)
        unreal.log("COMPLETE BLUEPRINT CREATION - WITH FUNCTIONS!")
        unreal.log("="*80)
        unreal.log(f"Found {total} Blueprint JSON files (skipping {non_blueprint_count} non-Blueprint files)")
        unreal.log(f"Processing with dependency resolution...")
        unreal.log("="*80 + "\n")
        
        skipped = []
        max_passes = 15  # Prevent infinite loops - increased for better dependency resolution
        current_pass = 1
        
        while json_files and current_pass <= max_passes:
            if current_pass > 1:
                unreal.log(f"\n{'='*60}")
                unreal.log(f"PASS {current_pass}: Processing {len(json_files)} remaining files...")
                unreal.log(f"{'='*60}\n")
            
            still_skipped = []
            
            for i, json_file in enumerate(json_files, 1):
                # Check if parent exists
                if not self.check_parent_exists(json_file):
                    unreal.log(f"[{i}/{len(json_files)}] ⏸️ Skipping (parent missing): {json_file.name}")
                    still_skipped.append(json_file)
                    continue
                
                unreal.log(f"\n[{i}/{len(json_files)}] {json_file.name}")
                self.process_json_file(json_file)
                
                # Progress update every 50 files
                if (self.stats['total']) % 50 == 0:
                    self.print_progress(self.stats['total'], total)
            
            # Check if we made progress
            if len(still_skipped) == len(json_files):
                # No progress made, parents are missing and won't be created
                unreal.log_warning(f"\n⚠️ Could not resolve {len(still_skipped)} files due to missing parents")
                for sf in still_skipped:
                    self.stats['failed'] += 1
                    self.stats['total'] += 1
                break
            
            json_files = still_skipped
            current_pass += 1
            
            # Force asset registry refresh between passes to pick up newly created assets
            if json_files and current_pass <= max_passes:
                unreal.log(f"🔄 Refreshing asset registry for pass {current_pass}...")
                # Give UE5 a moment to fully process the assets
                import time
                time.sleep(0.1)
        
        if current_pass > max_passes:
            unreal.log_warning(f"⚠️ Reached maximum passes ({max_passes})")
        
        self.print_summary()
    
    def print_progress(self, current, total):
        """Print progress update"""
        percentage = (current * 100) // total
        unreal.log("\n" + "="*60)
        unreal.log(f"PROGRESS: {current}/{total} ({percentage}%)")
        unreal.log(f"Created: {self.stats['created']} | Failed: {self.stats['failed']}")
        unreal.log("="*60 + "\n")
    
    def print_summary(self):
        """Print final summary"""
        unreal.log("\n" + "="*80)
        unreal.log("✨ BLUEPRINT CREATION COMPLETE! ✨")
        unreal.log("="*80)
        unreal.log(f"Total processed: {self.stats['total']}")
        unreal.log(f"✅ Successfully created: {self.stats['created']}")
        unreal.log(f"❌ Failed: {self.stats['failed']}")
        
        if self.stats['errors']:
            unreal.log(f"\n⚠️ Errors ({len(self.stats['errors'])}):")
            for error in self.stats['errors'][:10]:
                unreal.log(f"  - {error}")
            if len(self.stats['errors']) > 10:
                unreal.log(f"  ... and {len(self.stats['errors']) - 10} more")
        
        unreal.log("\n" + "="*80)
        unreal.log("🎉 ALL DUMMIES NOW HAVE FUNCTIONS AND COMPONENTS!")
        unreal.log("="*80 + "\n")


def main():
    """Main entry point"""
    try:
        # Check if plugin is available
        if not hasattr(unreal, 'BlueprintFunctionLibrary'):
            unreal.log_error("❌ BlueprintFunctionCreator plugin not found!")
            unreal.log_error("Please compile and enable the plugin first.")
            unreal.log_error("See PLUGIN_USAGE_GUIDE.md for instructions.")
            return
        
        unreal.log("✅ BlueprintFunctionCreator plugin detected!")
        
        # Create converter
        converter = CompleteBlueprintConverter(json_folder=None)  # Auto-detect
        
        # PHASE 1: Create UserDefinedStruct assets first (dependencies for Blueprints)
        converter.process_all_structs()
        
        # PHASE 2: Create Blueprint assets
        converter.process_all()
        
    except Exception as e:
        unreal.log_error(f"Fatal error: {str(e)}")
        import traceback
        unreal.log_error(traceback.format_exc())


if __name__ == "__main__":
    main()
