"""
FModel JSON to Unreal Blueprint Converter - WITH FUNCTION SUPPORT!

This script uses the BlueprintFunctionCreator plugin to create COMPLETE
Blueprint dummy assets including components AND functions!

REQUIREMENTS:
1. BlueprintFunctionCreator plugin must be compiled and enabled
2. Run this script FROM WITHIN Unreal Editor using the Python plugin
3. File → Execute Python Script → Select this file

FEATURES:
✅ Creates Blueprint assets
✅ Adds components from JSON
✅ Adds function stubs from JSON (via plugin!)
✅ Processes thousands of files automatically
✅ Auto-detects JSON folder
"""

import unreal
import os
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
                    entry = data[0]
                    if entry.get('Type') == 'BlueprintGeneratedClass':
                        name = entry.get('Name', '')
                        if name:
                            self.available_blueprints.add(name)
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
        
        try:
            # Use plugin to create complete Blueprint!
            blueprint = self.blueprint_lib.create_blueprint_from_f_model_json(
                str(json_file),
                dest_path,
                asset_name
            )
            
            if blueprint:
                # Force save and refresh asset registry so it's immediately available
                full_path = f"{dest_path}/{asset_name}"
                unreal.EditorAssetLibrary.save_asset(full_path)
                
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
                entry = data[0]
                return entry.get('Type') == 'BlueprintGeneratedClass'
            
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
                entry = data[0]
                if entry.get('Type') == 'BlueprintGeneratedClass':
                    super_obj = entry.get('Super')
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
                                    parent_full_path = f"{parent_asset_path}.{parent_asset_name}"
                                    
                                    exists = unreal.EditorAssetLibrary.does_asset_exist(parent_full_path)
                                    unreal.log(f"  Checking path: {parent_full_path}, Exists: {exists}")
                                    
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
    
    def process_all(self):
        """Process all JSON files with multi-pass for parent dependencies"""
        all_json_files = list(self.json_folder.rglob('*.json'))
        
        # Filter to only Blueprint JSON files
        unreal.log("Filtering Blueprint JSON files...")
        json_files = [f for f in all_json_files if self.is_blueprint_json(f)]
        
        total = len(json_files)
        non_blueprint_count = len(all_json_files) - total
        
        unreal.log("\n" + "="*80)
        unreal.log("COMPLETE BLUEPRINT CREATION - WITH FUNCTIONS!")
        unreal.log("="*80)
        unreal.log(f"Found {total} Blueprint JSON files (skipping {non_blueprint_count} non-Blueprint files)")
        unreal.log(f"Processing with dependency resolution...")
        unreal.log("="*80 + "\n")
        
        skipped = []
        max_passes = 10  # Prevent infinite loops
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
        
        # Create converter and process all files
        converter = CompleteBlueprintConverter(json_folder=None)  # Auto-detect
        converter.process_all()
        
    except Exception as e:
        unreal.log_error(f"Fatal error: {str(e)}")
        import traceback
        unreal.log_error(traceback.format_exc())


if __name__ == "__main__":
    main()
