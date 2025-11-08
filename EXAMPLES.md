# Example JSON Structure

This document shows examples of the JSON structures that the tool expects from FModel exports.

## Blueprint with Blueprint Parent

```json
[
  {
    "Type": "BlueprintGeneratedClass",
    "Name": "BP_GatlingGun_C",
    "Class": "UScriptClass'BlueprintGeneratedClass'",
    "Super": {
      "ObjectName": "BlueprintGeneratedClass'BP_AssaultRifleBase_C'",
      "ObjectPath": "/Game/Pal/Blueprint/Weapon/BP_AssaultRifleBase.0"
    },
    "Flags": "RF_Public | RF_Transactional | RF_WasLoaded | RF_LoadCompleted",
    "Properties": {
      "SimpleConstructionScript": {
        "ObjectName": "SimpleConstructionScript'BP_GatlingGun_C:SimpleConstructionScript_0'",
        "ObjectPath": "/Game/Pal/Blueprint/Weapon/BP_GatlingGun.26"
      }
    },
    "Children": [
      {
        "ObjectName": "Function'BP_GatlingGun_C:GetAmmoClass'",
        "ObjectPath": "/Game/Pal/Blueprint/Weapon/BP_GatlingGun.2"
      },
      {
        "ObjectName": "Function'BP_GatlingGun_C:GetMuzzleEffect'",
        "ObjectPath": "/Game/Pal/Blueprint/Weapon/BP_GatlingGun.3"
      },
      {
        "ObjectName": "Function'BP_GatlingGun_C:StopFireLoopSound'",
        "ObjectPath": "/Game/Pal/Blueprint/Weapon/BP_GatlingGun.4"
      }
    ],
    "ChildProperties": [
      {
        "Type": "ObjectProperty",
        "Name": "Mesh",
        "PropertyType": "Class'SkeletalMeshComponent'"
      },
      {
        "Type": "ObjectProperty",
        "Name": "AudioComponent",
        "PropertyType": "Class'AudioComponent'"
      }
    ]
  }
]
```

**Key Points:**
- Has `Super` field pointing to another Blueprint
- Will wait for `BP_AssaultRifleBase` to be created first
- Functions listed in `Children` array
- Components listed in `ChildProperties` array

---

## Blueprint with C++ Parent

```json
[
  {
    "Type": "BlueprintGeneratedClass",
    "Name": "BP_AssaultRifleBase_C",
    "Class": "UScriptClass'BlueprintGeneratedClass'",
    "Flags": "RF_Public | RF_Transactional | RF_WasLoaded | RF_LoadCompleted",
    "Properties": {
      "SimpleConstructionScript": {
        "ObjectName": "SimpleConstructionScript'BP_AssaultRifleBase_C:SimpleConstructionScript_0'",
        "ObjectPath": "/Game/Pal/Blueprint/Weapon/BP_AssaultRifleBase.40"
      }
    },
    "SuperStruct": {
      "ObjectName": "Class'PalWeaponBase'",
      "ObjectPath": "/Script/Pal"
    },
    "Children": [
      {
        "ObjectName": "Function'BP_AssaultRifleBase_C:GetAmmoClass'",
        "ObjectPath": "/Game/Pal/Blueprint/Weapon/BP_AssaultRifleBase.43"
      },
      {
        "ObjectName": "Function'BP_AssaultRifleBase_C:OnShoot'",
        "ObjectPath": "/Game/Pal/Blueprint/Weapon/BP_AssaultRifleBase.75"
      },
      {
        "ObjectName": "Function'BP_AssaultRifleBase_C:CalcDPS'",
        "ObjectPath": "/Game/Pal/Blueprint/Weapon/BP_AssaultRifleBase.39"
      }
    ],
    "ChildProperties": [
      {
        "Type": "ObjectProperty",
        "Name": "WeaponMesh",
        "PropertyType": "Class'SkeletalMeshComponent'"
      }
    ]
  }
]
```

**Key Points:**
- Has `SuperStruct` field pointing to C++ class
- No `Super` field (not inheriting from another Blueprint)
- Will be created in first pass (no Blueprint dependency)
- Functions are overrides of C++ base class methods

---

## Blueprint with No Parent (Defaults to Actor)

```json
[
  {
    "Type": "BlueprintGeneratedClass",
    "Name": "BP_CustomActor_C",
    "Class": "UScriptClass'BlueprintGeneratedClass'",
    "Flags": "RF_Public | RF_Transactional | RF_WasLoaded | RF_LoadCompleted",
    "Children": [
      {
        "ObjectName": "Function'BP_CustomActor_C:CustomFunction'",
        "ObjectPath": "/Game/Custom/BP_CustomActor.2"
      }
    ],
    "ChildProperties": []
  }
]
```

**Key Points:**
- No `Super` or `SuperStruct` field
- Will default to `AActor` as parent class
- Created in first pass

---

## Function Names with Spaces

```json
{
  "ObjectName": "Function'BP_AssaultRifleBase_C:Get Right Hand Location'",
  "ObjectPath": "/Game/Pal/Blueprint/Weapon/BP_AssaultRifleBase.55"
}
```

**Handling:**
- Spaces replaced with underscores: `Get_Right_Hand_Location`
- Original: `"Get Right Hand Location"`
- Created as: `Get_Right_Hand_Location`

---

## Duplicate Functions in JSON

Some JSON files contain the same function multiple times:

```json
"Children": [
  {
    "ObjectName": "Function'BP_Example_C:MyFunction'",
    "ObjectPath": "/Game/Example/BP_Example.2"
  },
  // ... other functions ...
  {
    "ObjectName": "Function'BP_Example_C:MyFunction'",
    "ObjectPath": "/Game/Example/BP_Example.150"
  }
]
```

**Handling:**
- Uses `AddUnique()` to automatically deduplicate
- Only creates the function once
- No "None" naming conflicts

---

## Component Examples

### Skeletal Mesh Component

```json
{
  "Type": "ObjectProperty",
  "Name": "WeaponMesh",
  "PropertyType": "Class'SkeletalMeshComponent'"
}
```

### Audio Component

```json
{
  "Type": "ObjectProperty",
  "Name": "FireSound",
  "PropertyType": "Class'AudioComponent'"
}
```

### Scene Component

```json
{
  "Type": "ObjectProperty",
  "Name": "MuzzleLocation",
  "PropertyType": "Class'SceneComponent'"
}
```

**Handling:**
- Extracts component name: `WeaponMesh`, `FireSound`, `MuzzleLocation`
- Extracts class name: `SkeletalMeshComponent`, `AudioComponent`, `SceneComponent`
- Creates component in Simple Construction Script
- Attaches to Blueprint's root component

---

## Ignored Elements

The following JSON elements are currently NOT processed:

### Event Graphs (Ubergraph)

```json
{
  "ObjectName": "Function'BP_Example_C:ExecuteUbergraph_BP_Example'",
  "ObjectPath": "/Game/Example/BP_Example.6"
}
```

**Reason:** Event graphs have complex node structures that can't be easily recreated. Function stubs are created, but internal logic is not.

### Non-Component Properties

```json
{
  "Type": "FloatProperty",
  "Name": "DamageMultiplier",
  "Value": "1.5"
}
```

**Reason:** Currently only processes `ObjectProperty` entries with `Component` in the class name.

### Inner Class Definitions

```json
{
  "Type": "Class",
  "Name": "BP_Example_InnerClass",
  ...
}
```

**Reason:** Only processes the main `BlueprintGeneratedClass` entry.

---

## Non-Blueprint JSON Files (Filtered Out)

The script automatically skips these file types:

### Skeleton Asset

```json
{
  "Type": "Skeleton",
  "Name": "SK_Character_Skeleton",
  ...
}
```

### Material

```json
{
  "Type": "Material",
  "Name": "M_Character_Material",
  ...
}
```

### Animation Blueprint

```json
{
  "Type": "AnimBlueprintGeneratedClass",
  "Name": "ABP_Character_C",
  ...
}
```

These are detected by `is_blueprint_json()` and skipped automatically.
