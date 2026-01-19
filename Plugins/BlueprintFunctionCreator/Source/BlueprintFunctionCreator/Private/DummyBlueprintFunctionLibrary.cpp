// Copyright Epic Games, Inc. All Rights Reserved.

#include "DummyBlueprintFunctionLibrary.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/SavePackage.h"
#include "AssetToolsModule.h"
#include "Factories/BlueprintFactory.h"
#include "Engine/UserDefinedStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/StructureEditorUtils.h"

bool UDummyBlueprintFunctionLibrary::AddFunctionStubToBlueprint(UBlueprint* Blueprint, FName FunctionName, bool bHasReturnValue, const FString& ReturnValueType)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionStubToBlueprint: Blueprint is null"));
		return false;
	}

	// Validate function name
	if (FunctionName.IsNone() || !FunctionName.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("AddFunctionStubToBlueprint: Invalid function name"));
		return false;
	}

	// Validate function name more thoroughly
	FString FuncNameStr = FunctionName.ToString();
	if (FuncNameStr.IsEmpty() || FuncNameStr == TEXT("None"))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid function name string: %s"), *FuncNameStr);
		return false;
	}
	
	// Auto-detect if function should have return value based on naming convention
	// Only use auto-detection if:
	// 1. bHasReturnValue is false (no return type specified)
	// 2. ReturnValueType is empty (not found in JSON at all)
	// 3. ReturnValueType is NOT "VOID" (function found but confirmed no return)
	// Functions starting with Get, Is, Can, Has, Should, Calc typically return values
	if (!bHasReturnValue && ReturnValueType.IsEmpty())
	{
		if (FuncNameStr.StartsWith(TEXT("Get")) || 
		    FuncNameStr.StartsWith(TEXT("Is")) ||
		    FuncNameStr.StartsWith(TEXT("Can")) ||
		    FuncNameStr.StartsWith(TEXT("Has")) ||
		    FuncNameStr.StartsWith(TEXT("Should")) ||
		    FuncNameStr.StartsWith(TEXT("Calc")) ||
		    FuncNameStr.StartsWith(TEXT("Gey"))) // Typo in original: "GeyEjectionPortTransform"
		{
			bHasReturnValue = true;
			UE_LOG(LogTemp, Log, TEXT("Auto-detected return value for function: %s (no explicit type info)"), *FuncNameStr);
		}
	}
	
	// If explicitly marked as VOID, ensure no return node is created
	if (ReturnValueType == TEXT("VOID"))
	{
		bHasReturnValue = false;
		UE_LOG(LogTemp, Log, TEXT("Function %s explicitly has no return value (VOID)"), *FuncNameStr);
	}

	UE_LOG(LogTemp, Warning, TEXT("Creating graph for function: %s (HasReturnValue: %s)"), *FuncNameStr, bHasReturnValue ? TEXT("true") : TEXT("false"));

	// Create a new graph for the function
	// Note: Use a temporary name, we'll rename it properly below
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FuncNameStr),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create graph for function: %s"), *FuncNameStr);
		return false;
	}
	
	// Ensure the graph has the correct name
	NewGraph->Rename(*FuncNameStr, NewGraph->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);

	UE_LOG(LogTemp, Warning, TEXT("Created graph with FName: %s, GetName: %s"), *NewGraph->GetFName().ToString(), *NewGraph->GetName());

	// Set up the graph as a function graph
	const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(NewGraph->GetSchema());
	if (!K2Schema)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get K2 schema"));
		return false;
	}

	// Create function entry node
	UK2Node_FunctionEntry* EntryNode = NewObject<UK2Node_FunctionEntry>(NewGraph);
	EntryNode->CreateNewGuid();
	EntryNode->PostPlacedNewNode();
	
	// CRITICAL: Set the function signature name BEFORE allocating pins
	EntryNode->CustomGeneratedFunctionName = FName(*FuncNameStr);
	EntryNode->bIsEditable = true;
	
	EntryNode->AllocateDefaultPins();
	NewGraph->AddNode(EntryNode);
	EntryNode->NodePosX = -200;
	EntryNode->NodePosY = 0;

	// Create result node if function has return value
	UK2Node_FunctionResult* ResultNode = nullptr;
	if (bHasReturnValue)
	{
		UE_LOG(LogTemp, Log, TEXT("Creating return node for '%s' with type: '%s'"), *FuncNameStr, *ReturnValueType);
		
		ResultNode = NewObject<UK2Node_FunctionResult>(NewGraph);
		ResultNode->CreateNewGuid();
		ResultNode->PostPlacedNewNode();
		
		// Determine the pin type based on ReturnValueType
		// Format can be:
		//   "PropertyType" - simple type
		//   "PropertyType|ClassName" - for native classes/structs
		//   "PropertyType|ClassName|ClassPath" - for Blueprint classes with full path
		//   "ArrayProperty|InnerType" - for arrays of simple types
		//   "ArrayProperty|InnerType|InnerClassName" - for arrays of objects/structs
		//   "ArrayProperty|InnerType|InnerClassName|InnerClassPath" - for arrays of Blueprint classes
		FString PropType = ReturnValueType;
		FString ClassName;
		FString ClassPath;
		FString InnerType;  // For arrays
		
		int32 SeparatorIdx;
		if (ReturnValueType.FindChar(TEXT('|'), SeparatorIdx))
		{
			PropType = ReturnValueType.Left(SeparatorIdx);
			FString Remainder = ReturnValueType.Mid(SeparatorIdx + 1);
			
			// Special handling for ArrayProperty which has format: ArrayProperty|InnerType|...
			if (PropType == TEXT("ArrayProperty"))
			{
				// Extract InnerType (second field)
				int32 SecondSepIdx;
				if (Remainder.FindChar(TEXT('|'), SecondSepIdx))
				{
					InnerType = Remainder.Left(SecondSepIdx);
					Remainder = Remainder.Mid(SecondSepIdx + 1);
					
					// Check for ClassName and ClassPath
					int32 ThirdSepIdx;
					if (Remainder.FindChar(TEXT('|'), ThirdSepIdx))
					{
						ClassName = Remainder.Left(ThirdSepIdx);
						ClassPath = Remainder.Mid(ThirdSepIdx + 1);
					}
					else
					{
						ClassName = Remainder;
					}
				}
				else
				{
					InnerType = Remainder;
				}
			}
			else
			{
				// Normal property: check if there's a second separator for ClassPath
				int32 SecondSeparatorIdx;
				if (Remainder.FindChar(TEXT('|'), SecondSeparatorIdx))
				{
					ClassName = Remainder.Left(SecondSeparatorIdx);
					ClassPath = Remainder.Mid(SecondSeparatorIdx + 1);
				}
				else
				{
					ClassName = Remainder;
				}
			}
		}
		
		FEdGraphPinType ReturnPinType;
		
		if (PropType.IsEmpty())
		{
			// No type specified, use wildcard
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		}
		else if (PropType == TEXT("BoolProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		}
		else if (PropType == TEXT("IntProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		}
		else if (PropType == TEXT("FloatProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			ReturnPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		}
		else if (PropType == TEXT("DoubleProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			ReturnPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		}
		else if (PropType == TEXT("ByteProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		}
		else if (PropType == TEXT("StrProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_String;
		}
		else if (PropType == TEXT("NameProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		}
		else if (PropType == TEXT("TextProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		}
		else if (PropType == TEXT("Int64Property"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		}
		else if (PropType == TEXT("EnumProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
			// TODO: Could extract enum type from JSON if needed
		}
		else if (PropType == TEXT("StructProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			
			// Try to find the specific struct if StructName was provided
			if (!ClassName.IsEmpty())
			{
				UScriptStruct* FoundStruct = nullptr;
				
				// If we have a StructPath from JSON, try it first (for UserDefinedStruct)
				if (!ClassPath.IsEmpty())
				{
					FString StructPath = ClassPath;
					
					// Remove the ".0" suffix if present
					if (StructPath.EndsWith(TEXT(".0")))
					{
						StructPath = StructPath.LeftChop(2);
					}
					
					// Construct full path: "/Game/Pal/Blueprint/Spawner/Other/F_NPC_PathWalkArray.F_NPC_PathWalkArray"
					FString FullPath = FString::Printf(TEXT("%s.%s"), *StructPath, *ClassName);
					
					UE_LOG(LogTemp, Log, TEXT("  Attempting to load UserDefinedStruct: %s"), *FullPath);
					
					FoundStruct = FindObject<UScriptStruct>(nullptr, *FullPath);
					if (!FoundStruct)
					{
						FoundStruct = LoadObject<UScriptStruct>(nullptr, *FullPath);
					}
					
					if (FoundStruct)
					{
						UE_LOG(LogTemp, Log, TEXT("  ✓ Found UserDefinedStruct %s at: %s"), *ClassName, *FullPath);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("  ✗ Could not load struct from path: %s"), *FullPath);
					}
				}
				
				// If no path or path failed, check if it's a UserDefinedStruct and try common content paths
				if (!FoundStruct && (ClassName.StartsWith(TEXT("F_")) || ClassName.Contains(TEXT("UserDefined"))))
				{
					TArray<FString> ContentPathsToTry = {
						FString::Printf(TEXT("/Game/Pal/DataTable/Struct/%s.%s"), *ClassName, *ClassName),
						FString::Printf(TEXT("/Game/Pal/Blueprint/Struct/%s.%s"), *ClassName, *ClassName),
						FString::Printf(TEXT("/Game/Pal/Struct/%s.%s"), *ClassName, *ClassName),
						FString::Printf(TEXT("/Game/Struct/%s.%s"), *ClassName, *ClassName)
					};
					
					for (const FString& ContentPath : ContentPathsToTry)
					{
						FoundStruct = FindObject<UScriptStruct>(nullptr, *ContentPath);
						if (!FoundStruct)
						{
							FoundStruct = LoadObject<UScriptStruct>(nullptr, *ContentPath);
						}
						if (FoundStruct)
						{
							UE_LOG(LogTemp, Log, TEXT("  Found UserDefinedStruct %s at: %s"), *ClassName, *ContentPath);
							break;
						}
					}
				}
				
				// If not found as user-defined, try native struct paths
				if (!FoundStruct)
				{
					TArray<FString> PathsToTry = {
						FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassName),
						FString::Printf(TEXT("/Script/Engine.%s"), *ClassName),
						FString::Printf(TEXT("/Script/Pal.%s"), *ClassName)
					};
					
					for (const FString& StructPath : PathsToTry)
					{
						FoundStruct = LoadObject<UScriptStruct>(nullptr, *StructPath);
						if (FoundStruct)
						{
							UE_LOG(LogTemp, Log, TEXT("  Found struct %s at path: %s"), *ClassName, *StructPath);
							break;
						}
					}
				}
				
				if (FoundStruct)
				{
					ReturnPinType.PinSubCategoryObject = FoundStruct;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("  Could not find struct '%s', using generic struct type"), *ClassName);
				}
			}
		}
		else if (PropType == TEXT("SoftObjectProperty") || PropType == TEXT("SoftClassProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
		}
		else if (PropType == TEXT("WeakObjectProperty"))
		{
			// Weak object references use PC_Object in UE5
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			ReturnPinType.bIsWeakPointer = true;
		}
		else if (PropType == TEXT("InterfaceProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
		}
		else if (PropType == TEXT("DelegateProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Delegate;
		}
		else if (PropType == TEXT("MulticastDelegateProperty") || PropType == TEXT("MulticastInlineDelegateProperty"))
		{
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
		}
		else if (PropType == TEXT("ClassProperty") || PropType == TEXT("ObjectProperty"))
		{
			// Object/Class reference
			if (PropType == TEXT("ClassProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
			}
			else
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			}
			
			// Try to find the specific class if ClassName was provided
			if (!ClassName.IsEmpty())
			{
				UClass* FoundClass = nullptr;
				
				// If we have a ClassPath from JSON, try it first
				if (!ClassPath.IsEmpty())
				{
					// ClassPath is like "/Game/Pal/Blueprint/Character/Base/BP_ShooterAnime_BowBase.0"
					// For Blueprint classes, construct the full path: "/Game/Path/ClassName.ClassName_C"
					FString BlueprintPath = ClassPath;
					
					// Remove the ".0" suffix if present
					if (BlueprintPath.EndsWith(TEXT(".0")))
					{
						BlueprintPath = BlueprintPath.LeftChop(2);
					}
					
					// Append the _C class name
					FString FullPath = FString::Printf(TEXT("%s.%s"), *BlueprintPath, *ClassName);
					UE_LOG(LogTemp, Warning, TEXT("  Attempting to load Blueprint class: %s"), *FullPath);
					UE_LOG(LogTemp, Warning, TEXT("    ClassName: %s, ClassPath: %s"), *ClassName, *ClassPath);
					
					FoundClass = LoadObject<UClass>(nullptr, *FullPath);
					
					if (FoundClass)
					{
						UE_LOG(LogTemp, Log, TEXT("  ✓ Found Blueprint class %s at path: %s"), *ClassName, *FullPath);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("  ✗ Failed to load Blueprint class from path: %s"), *FullPath);
					}
				}
				
				// If not found via ClassPath, try common native class paths
				if (!FoundClass)
				{
					TArray<FString> PathsToTry = {
						FString::Printf(TEXT("/Script/Pal.%s"), *ClassName),
						FString::Printf(TEXT("/Script/Engine.%s"), *ClassName),
						FString::Printf(TEXT("/Script/Niagara.%s"), *ClassName),
						FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassName)
					};
					
					for (const FString& NativeClassPath : PathsToTry)
					{
						FoundClass = LoadObject<UClass>(nullptr, *NativeClassPath);
						if (FoundClass)
						{
							UE_LOG(LogTemp, Log, TEXT("  Found native class %s at path: %s"), *ClassName, *NativeClassPath);
							break;
						}
					}
				}
				
				if (FoundClass)
				{
					ReturnPinType.PinSubCategoryObject = FoundClass;
				}
				else
				{
					// Class not found yet (might not be generated)
					// Try to use FindObject which doesn't trigger loading
					if (!ClassPath.IsEmpty())
					{
						FString BlueprintPath = ClassPath;
						if (BlueprintPath.EndsWith(TEXT(".0")))
						{
							BlueprintPath = BlueprintPath.LeftChop(2);
						}
						FString FullPath = FString::Printf(TEXT("%s.%s"), *BlueprintPath, *ClassName);
						
						// Use FindObject instead of LoadObject - won't load but will find if already in memory
						FoundClass = FindObject<UClass>(nullptr, *FullPath);
						
						if (FoundClass)
						{
							UE_LOG(LogTemp, Log, TEXT("  Found already-loaded class: %s"), *ClassName);
							ReturnPinType.PinSubCategoryObject = FoundClass;
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("  ⚠️ Class '%s' not found (may not be generated yet)"), *ClassName);
							UE_LOG(LogTemp, Warning, TEXT("  📝 TODO: Regenerate this Blueprint after '%s' is created for proper typing"), *ClassName);
							UE_LOG(LogTemp, Warning, TEXT("  📍 Missing dependency: %s"), *FullPath);
							
							// Use generic UObject as fallback with a note
							if (ReturnPinType.PinCategory == UEdGraphSchema_K2::PC_Object)
							{
								ReturnPinType.PinSubCategoryObject = UObject::StaticClass();
							}
							else if (ReturnPinType.PinCategory == UEdGraphSchema_K2::PC_Class)
							{
								ReturnPinType.PinSubCategoryObject = UClass::StaticClass();
							}
						}
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("  Could not find class '%s', using generic object/class type"), *ClassName);
						// Use a generic UObject class as fallback so the pin is valid
						if (ReturnPinType.PinCategory == UEdGraphSchema_K2::PC_Object)
						{
							ReturnPinType.PinSubCategoryObject = UObject::StaticClass();
						}
						else if (ReturnPinType.PinCategory == UEdGraphSchema_K2::PC_Class)
						{
							ReturnPinType.PinSubCategoryObject = UClass::StaticClass();
						}
					}
				}
			}
		}
		else if (PropType == TEXT("ArrayProperty"))
		{
			// Array type - set as array container
			ReturnPinType.ContainerType = EPinContainerType::Array;
			
			// Set the inner type based on InnerType
			if (InnerType == TEXT("ObjectProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
				
				// Try to find the specific class if ClassName is provided
				if (!ClassName.IsEmpty())
				{
					UClass* FoundClass = nullptr;
					
					// Try ClassPath first for Blueprint classes
					if (!ClassPath.IsEmpty())
					{
						FString BlueprintPath = ClassPath;
						if (BlueprintPath.EndsWith(TEXT(".0")))
						{
							BlueprintPath = BlueprintPath.LeftChop(2);
						}
						FString FullPath = FString::Printf(TEXT("%s.%s"), *BlueprintPath, *ClassName);
						FoundClass = LoadObject<UClass>(nullptr, *FullPath);
						if (FoundClass)
						{
							UE_LOG(LogTemp, Log, TEXT("  Found Blueprint class %s for array inner type"), *ClassName);
						}
					}
					
					// Fall back to native class paths
					if (!FoundClass)
					{
						TArray<FString> PathsToTry = {
							FString::Printf(TEXT("/Script/Pal.%s"), *ClassName),
							FString::Printf(TEXT("/Script/Engine.%s"), *ClassName),
							FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassName)
						};
						
						for (const FString& NativeClassPath : PathsToTry)
						{
							FoundClass = LoadObject<UClass>(nullptr, *NativeClassPath);
							if (FoundClass)
							{
								UE_LOG(LogTemp, Log, TEXT("  Found native class %s for array inner type"), *ClassName);
								break;
							}
						}
					}
					
					if (FoundClass)
					{
						ReturnPinType.PinSubCategoryObject = FoundClass;
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("  Could not find class '%s' for array inner type, using generic UObject"), *ClassName);
						// Use generic UObject as fallback so the pin is valid
						ReturnPinType.PinSubCategoryObject = UObject::StaticClass();
					}
				}
				else
				{
					// No class name specified, use generic UObject
					ReturnPinType.PinSubCategoryObject = UObject::StaticClass();
				}
			}
			else if (InnerType == TEXT("StructProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
				
				// Try to find the specific struct
				if (!ClassName.IsEmpty())
				{
					UScriptStruct* FoundStruct = nullptr;
					
					// Check if it's a UserDefinedStruct (Blueprint struct in /Game/)
					if (ClassName.StartsWith(TEXT("F_")) || ClassName.Contains(TEXT("UserDefined")))
					{
						TArray<FString> ContentPathsToTry = {
							FString::Printf(TEXT("/Game/Pal/DataTable/Struct/%s.%s"), *ClassName, *ClassName),
							FString::Printf(TEXT("/Game/Pal/Blueprint/Struct/%s.%s"), *ClassName, *ClassName),
							FString::Printf(TEXT("/Game/Pal/Struct/%s.%s"), *ClassName, *ClassName),
							FString::Printf(TEXT("/Game/Struct/%s.%s"), *ClassName, *ClassName)
						};
						
						for (const FString& ContentPath : ContentPathsToTry)
						{
							FoundStruct = FindObject<UScriptStruct>(nullptr, *ContentPath);
							if (!FoundStruct)
							{
								FoundStruct = LoadObject<UScriptStruct>(nullptr, *ContentPath);
							}
							if (FoundStruct)
							{
								UE_LOG(LogTemp, Log, TEXT("  Found UserDefinedStruct %s for array inner type"), *ClassName);
								break;
							}
						}
					}
					
					// If not found as user-defined, try native struct paths
					if (!FoundStruct)
					{
						TArray<FString> PathsToTry = {
							FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassName),
							FString::Printf(TEXT("/Script/Engine.%s"), *ClassName),
							FString::Printf(TEXT("/Script/Pal.%s"), *ClassName)
						};
						
						for (const FString& StructPath : PathsToTry)
						{
							FoundStruct = LoadObject<UScriptStruct>(nullptr, *StructPath);
							if (FoundStruct)
							{
								UE_LOG(LogTemp, Log, TEXT("  Found struct %s for array inner type"), *ClassName);
								break;
							}
						}
					}
					
					if (FoundStruct)
					{
						ReturnPinType.PinSubCategoryObject = FoundStruct;
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("  Could not find struct '%s' for array inner type"), *ClassName);
					}
				}
			}
			else if (InnerType == TEXT("IntProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
			}
			else if (InnerType == TEXT("Int64Property"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
			}
			else if (InnerType == TEXT("ByteProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
			}
			else if (InnerType == TEXT("BoolProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
			}
			else if (InnerType == TEXT("FloatProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
				ReturnPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
			}
			else if (InnerType == TEXT("DoubleProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
				ReturnPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
			}
			else if (InnerType == TEXT("StrProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_String;
			}
			else if (InnerType == TEXT("NameProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
			}
			else if (InnerType == TEXT("TextProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
			}
			else if (InnerType == TEXT("EnumProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
				// TODO: Could extract enum type from JSON if needed
			}
			else if (InnerType == TEXT("ClassProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
				
				// Try to find the specific class if ClassName is provided (similar to ObjectProperty handling)
				if (!ClassName.IsEmpty())
				{
					UClass* FoundClass = nullptr;
					
					// Try ClassPath first for Blueprint classes
					if (!ClassPath.IsEmpty())
					{
						FString BlueprintPath = ClassPath;
						if (BlueprintPath.EndsWith(TEXT(".0")))
						{
							BlueprintPath = BlueprintPath.LeftChop(2);
						}
						FString FullPath = FString::Printf(TEXT("%s.%s"), *BlueprintPath, *ClassName);
						FoundClass = LoadObject<UClass>(nullptr, *FullPath);
						if (!FoundClass)
						{
							FoundClass = FindObject<UClass>(nullptr, *FullPath);
						}
					}
					
					// Fall back to native class paths
					if (!FoundClass)
					{
						TArray<FString> PathsToTry = {
							FString::Printf(TEXT("/Script/Pal.%s"), *ClassName),
							FString::Printf(TEXT("/Script/Engine.%s"), *ClassName),
							FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassName)
						};
						
						for (const FString& NativeClassPath : PathsToTry)
						{
							FoundClass = LoadObject<UClass>(nullptr, *NativeClassPath);
							if (FoundClass)
							{
								break;
							}
						}
					}
					
					if (FoundClass)
					{
						ReturnPinType.PinSubCategoryObject = FoundClass;
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("  Could not find class '%s' for array inner ClassProperty, using generic UClass"), *ClassName);
						ReturnPinType.PinSubCategoryObject = UClass::StaticClass();
					}
				}
				else
				{
					// No class name specified, use generic UClass
					ReturnPinType.PinSubCategoryObject = UClass::StaticClass();
				}
			}
			else if (InnerType == TEXT("SoftObjectProperty") || InnerType == TEXT("SoftClassProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
				// TODO: Could extract specific soft object class if needed
			}
			else if (InnerType == TEXT("WeakObjectProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
				ReturnPinType.bIsWeakPointer = true;
			}
			else if (InnerType == TEXT("InterfaceProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
				// TODO: Could extract specific interface type if needed
			}
			else if (InnerType == TEXT("DelegateProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Delegate;
			}
			else if (InnerType == TEXT("MulticastDelegateProperty") || InnerType == TEXT("MulticastInlineDelegateProperty"))
			{
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
			}
			else
			{
				// Unknown inner type
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
				UE_LOG(LogTemp, Warning, TEXT("Unknown array inner type '%s' for function '%s'"), *InnerType, *FuncNameStr);
			}
		}
		else if (PropType == TEXT("MapProperty"))
		{
			// Map type - set as map container
			ReturnPinType.ContainerType = EPinContainerType::Map;
			
			// Parse "MapProperty|KeyType|ValueType|KeyClassName|ValueClassName"
			TArray<FString> MapParts;
			ReturnValueType.ParseIntoArray(MapParts, TEXT("|"));
			
			if (MapParts.Num() >= 3)
			{
				FString KeyType = MapParts[1];
				FString ValueType = MapParts[2];
				FString KeyClassName = (MapParts.Num() > 3) ? MapParts[3] : TEXT("");
				FString ValueClassName = (MapParts.Num() > 4) ? MapParts[4] : TEXT("");
				
				UE_LOG(LogTemp, Log, TEXT("  Processing Map return type: Key=%s, Value=%s"), *KeyType, *ValueType);
				
				// Set VALUE type (PinCategory)
				if (ValueType == TEXT("BoolProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
				}
				else if (ValueType == TEXT("IntProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
				}
				else if (ValueType == TEXT("Int64Property"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
				}
				else if (ValueType == TEXT("ByteProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
				}
				else if (ValueType == TEXT("FloatProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
					ReturnPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
				}
				else if (ValueType == TEXT("DoubleProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
					ReturnPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
				}
				else if (ValueType == TEXT("StrProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_String;
				}
				else if (ValueType == TEXT("NameProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
				}
				else if (ValueType == TEXT("TextProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
				}
				else if (ValueType == TEXT("EnumProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
				}
				else if (ValueType == TEXT("ObjectProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
					// Try to resolve value class
					if (!ValueClassName.IsEmpty())
					{
						UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Pal.%s"), *ValueClassName));
						if (!FoundClass)
						{
							FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ValueClassName));
						}
						if (FoundClass)
						{
							ReturnPinType.PinSubCategoryObject = FoundClass;
						}
						else
						{
							ReturnPinType.PinSubCategoryObject = UObject::StaticClass();
						}
					}
					else
					{
						ReturnPinType.PinSubCategoryObject = UObject::StaticClass();
					}
				}
				else if (ValueType == TEXT("ClassProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
					if (!ValueClassName.IsEmpty())
					{
						UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Pal.%s"), *ValueClassName));
						if (!FoundClass)
						{
							FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ValueClassName));
						}
						if (FoundClass)
						{
							ReturnPinType.PinSubCategoryObject = FoundClass;
						}
						else
						{
							ReturnPinType.PinSubCategoryObject = UClass::StaticClass();
						}
					}
					else
					{
						ReturnPinType.PinSubCategoryObject = UClass::StaticClass();
					}
				}
				else if (ValueType == TEXT("StructProperty"))
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
					if (!ValueClassName.IsEmpty())
					{
						UScriptStruct* FoundStruct = nullptr;
						
						// Check if it's a UserDefinedStruct
						if (ValueClassName.StartsWith(TEXT("F_")) || ValueClassName.Contains(TEXT("UserDefined")))
						{
							TArray<FString> ContentPathsToTry = {
								FString::Printf(TEXT("/Game/Pal/DataTable/Struct/%s.%s"), *ValueClassName, *ValueClassName),
								FString::Printf(TEXT("/Game/Pal/Blueprint/Struct/%s.%s"), *ValueClassName, *ValueClassName),
								FString::Printf(TEXT("/Game/Pal/Struct/%s.%s"), *ValueClassName, *ValueClassName),
								FString::Printf(TEXT("/Game/Struct/%s.%s"), *ValueClassName, *ValueClassName)
							};
							
							for (const FString& ContentPath : ContentPathsToTry)
							{
								FoundStruct = FindObject<UScriptStruct>(nullptr, *ContentPath);
								if (!FoundStruct)
								{
									FoundStruct = LoadObject<UScriptStruct>(nullptr, *ContentPath);
								}
								if (FoundStruct) break;
							}
						}
						
						// Try native struct paths
						if (!FoundStruct)
						{
							FoundStruct = FindObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("/Script/CoreUObject.%s"), *ValueClassName));
							if (!FoundStruct)
							{
								FoundStruct = FindObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ValueClassName));
							}
							if (!FoundStruct)
							{
								FoundStruct = FindObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("/Script/Pal.%s"), *ValueClassName));
							}
						}
						
						if (FoundStruct)
						{
							ReturnPinType.PinSubCategoryObject = FoundStruct;
						}
					}
				}
				else
				{
					ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
					UE_LOG(LogTemp, Warning, TEXT("Unknown map value type '%s'"), *ValueType);
				}
				
				// Set KEY type (PinValueType)
				if (KeyType == TEXT("BoolProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Boolean;
				}
				else if (KeyType == TEXT("IntProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Int;
				}
				else if (KeyType == TEXT("Int64Property"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Int64;
				}
				else if (KeyType == TEXT("ByteProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Byte;
				}
				else if (KeyType == TEXT("FloatProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Real;
					ReturnPinType.PinValueType.TerminalSubCategory = UEdGraphSchema_K2::PC_Float;
				}
				else if (KeyType == TEXT("DoubleProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Real;
					ReturnPinType.PinValueType.TerminalSubCategory = UEdGraphSchema_K2::PC_Double;
				}
				else if (KeyType == TEXT("StrProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_String;
				}
				else if (KeyType == TEXT("NameProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Name;
				}
				else if (KeyType == TEXT("TextProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Text;
				}
				else if (KeyType == TEXT("EnumProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Byte;
					// Try to find the enum type if KeyClassName is provided
					if (!KeyClassName.IsEmpty())
					{
						UEnum* FoundEnum = FindObject<UEnum>(nullptr, *FString::Printf(TEXT("/Script/Pal.%s"), *KeyClassName));
						if (!FoundEnum)
						{
							FoundEnum = FindObject<UEnum>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *KeyClassName));
						}
						if (FoundEnum)
						{
							ReturnPinType.PinValueType.TerminalSubCategoryObject = FoundEnum;
							UE_LOG(LogTemp, Log, TEXT("  Found enum type for map key: %s"), *KeyClassName);
						}
					}
				}
				else if (KeyType == TEXT("ObjectProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Object;
					if (!KeyClassName.IsEmpty())
					{
						UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Pal.%s"), *KeyClassName));
						if (!FoundClass)
						{
							FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *KeyClassName));
						}
						if (FoundClass)
						{
							ReturnPinType.PinValueType.TerminalSubCategoryObject = FoundClass;
						}
						else
						{
							ReturnPinType.PinValueType.TerminalSubCategoryObject = UObject::StaticClass();
						}
					}
					else
					{
						ReturnPinType.PinValueType.TerminalSubCategoryObject = UObject::StaticClass();
					}
				}
				else if (KeyType == TEXT("ClassProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Class;
					if (!KeyClassName.IsEmpty())
					{
						UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Pal.%s"), *KeyClassName));
						if (!FoundClass)
						{
							FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *KeyClassName));
						}
						if (FoundClass)
						{
							ReturnPinType.PinValueType.TerminalSubCategoryObject = FoundClass;
						}
						else
						{
							ReturnPinType.PinValueType.TerminalSubCategoryObject = UClass::StaticClass();
						}
					}
					else
					{
						ReturnPinType.PinValueType.TerminalSubCategoryObject = UClass::StaticClass();
					}
				}
				else if (KeyType == TEXT("StructProperty"))
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Struct;
					if (!KeyClassName.IsEmpty())
					{
						UScriptStruct* FoundStruct = nullptr;
						
						// Check if it's a UserDefinedStruct
						if (KeyClassName.StartsWith(TEXT("F_")) || KeyClassName.Contains(TEXT("UserDefined")))
						{
							TArray<FString> ContentPathsToTry = {
								FString::Printf(TEXT("/Game/Pal/DataTable/Struct/%s.%s"), *KeyClassName, *KeyClassName),
								FString::Printf(TEXT("/Game/Pal/Blueprint/Struct/%s.%s"), *KeyClassName, *KeyClassName),
								FString::Printf(TEXT("/Game/Pal/Struct/%s.%s"), *KeyClassName, *KeyClassName),
								FString::Printf(TEXT("/Game/Struct/%s.%s"), *KeyClassName, *KeyClassName)
							};
							
							for (const FString& ContentPath : ContentPathsToTry)
							{
								FoundStruct = FindObject<UScriptStruct>(nullptr, *ContentPath);
								if (!FoundStruct)
								{
									FoundStruct = LoadObject<UScriptStruct>(nullptr, *ContentPath);
								}
								if (FoundStruct) break;
							}
						}
						
						// Try native struct paths
						if (!FoundStruct)
						{
							FoundStruct = FindObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("/Script/CoreUObject.%s"), *KeyClassName));
							if (!FoundStruct)
							{
								FoundStruct = FindObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *KeyClassName));
							}
							if (!FoundStruct)
							{
								FoundStruct = FindObject<UScriptStruct>(nullptr, *FString::Printf(TEXT("/Script/Pal.%s"), *KeyClassName));
							}
						}
						
						if (FoundStruct)
						{
							ReturnPinType.PinValueType.TerminalSubCategoryObject = FoundStruct;
						}
					}
				}
				else
				{
					ReturnPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
					UE_LOG(LogTemp, Warning, TEXT("Unknown map key type '%s'"), *KeyType);
				}
				
				UE_LOG(LogTemp, Log, TEXT("  Created Map<%s, %s> return type"), *KeyType, *ValueType);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Invalid MapProperty format: %s"), *ReturnValueType);
				ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			}
		}
		else
		{
			// Unknown type, default to wildcard
			ReturnPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			UE_LOG(LogTemp, Warning, TEXT("Unknown return type '%s' for function '%s', using wildcard"), *PropType, *FuncNameStr);
		}
		
		// Add user-defined pin for return value
		TSharedPtr<FUserPinInfo> ReturnPin = MakeShareable(new FUserPinInfo());
		ReturnPin->PinName = FName(TEXT("ReturnValue"));
		ReturnPin->PinType = ReturnPinType;
		ReturnPin->DesiredPinDirection = EGPD_Input;
		ResultNode->UserDefinedPins.Add(ReturnPin);
		
		ResultNode->AllocateDefaultPins();
		NewGraph->AddNode(ResultNode);
		ResultNode->NodePosX = 400;
		ResultNode->NodePosY = 0;
		
		// Connect entry to result
		UEdGraphPin* EntryExecPin = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* ResultExecPin = ResultNode->FindPin(UEdGraphSchema_K2::PN_Execute);
		
		if (EntryExecPin && ResultExecPin)
		{
			EntryExecPin->MakeLinkTo(ResultExecPin);
		}
	}

	// Add the function graph to the blueprint
	Blueprint->FunctionGraphs.Add(NewGraph);

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("Successfully added function: %s"), *FuncNameStr);
	return true;
}

int32 UDummyBlueprintFunctionLibrary::AddMultipleFunctionStubsToBlueprint(UBlueprint* Blueprint, const TArray<FName>& FunctionNames, const TArray<FString>& ReturnTypes)
{
	if (!Blueprint)
	{
		return 0;
	}

	int32 SuccessCount = 0;
	for (int32 i = 0; i < FunctionNames.Num(); i++)
	{
		const FName& FuncName = FunctionNames[i];
		
		// Skip empty names and Ubergraph (that's the event graph)
		if (FuncName.IsNone() || FuncName.ToString().Contains(TEXT("ExecuteUbergraph")))
		{
			continue;
		}

		// Check if function already exists (either in this Blueprint or inherited from parent)
		bool bExists = false;
		
		// Check if it's already in this Blueprint's function graphs
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetFName() == FuncName)
			{
				bExists = true;
				UE_LOG(LogTemp, Warning, TEXT("Function %s already exists in this Blueprint, skipping"), *FuncName.ToString());
				break;
			}
		}
		
		// Check if it's inherited from parent class
		if (!bExists && Blueprint->ParentClass)
		{
			UFunction* InheritedFunc = Blueprint->ParentClass->FindFunctionByName(FuncName);
			if (InheritedFunc)
			{
				bExists = true;
				UE_LOG(LogTemp, Warning, TEXT("Function %s is inherited from parent, skipping"), *FuncName.ToString());
			}
		}

		if (!bExists)
		{
			// Get return type if available
			FString ReturnType = (i < ReturnTypes.Num()) ? ReturnTypes[i] : TEXT("");
			bool bHasReturn = !ReturnType.IsEmpty();
			
			if (AddFunctionStubToBlueprint(Blueprint, FuncName, bHasReturn, ReturnType))
			{
				SuccessCount++;
			}
		}
	}

	return SuccessCount;
}

int32 UDummyBlueprintFunctionLibrary::AddComponentsToBlueprint(UBlueprint* Blueprint, const TArray<FName>& ComponentNames, const TArray<FString>& ComponentClasses)
{
	if (!Blueprint || ComponentNames.Num() != ComponentClasses.Num())
	{
		return 0;
	}

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		return 0;
	}

	int32 SuccessCount = 0;

	for (int32 i = 0; i < ComponentNames.Num(); i++)
	{
		FName CompName = ComponentNames[i];
		FString CompClassName = ComponentClasses[i];

		// Get the component class
		UClass* ComponentClass = nullptr;
		
		if (CompClassName == TEXT("SceneComponent"))
		{
			ComponentClass = USceneComponent::StaticClass();
		}
		else if (CompClassName == TEXT("StaticMeshComponent"))
		{
			ComponentClass = UStaticMeshComponent::StaticClass();
		}
		else if (CompClassName == TEXT("SkeletalMeshComponent"))
		{
			ComponentClass = USkeletalMeshComponent::StaticClass();
		}
		else
		{
			// Try to load the class
			FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *CompClassName);
			ComponentClass = LoadClass<UActorComponent>(nullptr, *ClassPath);
		}

		if (ComponentClass)
		{
			// Create SCS node
			USCS_Node* NewNode = SCS->CreateNode(ComponentClass, CompName);
			if (NewNode)
			{
				SCS->AddNode(NewNode);
				SuccessCount++;
			}
		}
	}

	if (SuccessCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	return SuccessCount;
}

int32 UDummyBlueprintFunctionLibrary::AddVariablesToBlueprint(UBlueprint* Blueprint, const TArray<FName>& VariableNames, const TArray<FString>& VariableTypes)
{
	if (!Blueprint || VariableNames.Num() != VariableTypes.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("AddVariablesToBlueprint: Invalid input - Blueprint: %s, VarNames: %d, VarTypes: %d"), 
			Blueprint ? TEXT("Valid") : TEXT("NULL"), VariableNames.Num(), VariableTypes.Num());
		return 0;
	}

	int32 SuccessCount = 0;

	for (int32 i = 0; i < VariableNames.Num(); i++)
	{
		FName VarName = VariableNames[i];
		FString VarType = VariableTypes[i];

		UE_LOG(LogTemp, Log, TEXT("Processing variable %d: %s (%s)"), i, *VarName.ToString(), *VarType);

		// Get the property type
		FEdGraphPinType PinType;
		
		if (VarType == TEXT("bool"))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		}
		else if (VarType == TEXT("int32"))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		}
		else if (VarType == TEXT("float"))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		}
		else if (VarType == TEXT("double"))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		}
		else if (VarType == TEXT("uint8"))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		}
		else if (VarType == TEXT("FString"))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_String;
		}
		else if (VarType == TEXT("FName"))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		}
		else if (VarType == TEXT("FText"))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Unknown variable type: %s for variable %s"), *VarType, *VarName.ToString());
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("Attempting to add variable with PinCategory: %s"), *PinType.PinCategory.ToString());

		// Add the variable to the Blueprint
		if (FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, PinType))
		{
			SuccessCount++;
			UE_LOG(LogTemp, Log, TEXT("✅ Added variable: %s (%s)"), *VarName.ToString(), *VarType);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("❌ FBlueprintEditorUtils::AddMemberVariable FAILED for: %s (%s)"), *VarName.ToString(), *VarType);
		}
	}

	if (SuccessCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	return SuccessCount;
}

bool UDummyBlueprintFunctionLibrary::ParseFModelJSON(const FString& JsonFilePath, TArray<FName>& OutFunctionNames, TArray<FName>& OutComponentNames, TArray<FString>& OutComponentClasses, TArray<FName>& OutVariableNames, TArray<FString>& OutVariableTypes, TArray<FString>& OutFunctionReturnTypes, FString& OutParentClassPath)
{
	// Load JSON file
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load JSON file: %s"), *JsonFilePath);
		return false;
	}

	// Parse JSON
	TSharedPtr<FJsonValue> JsonValue;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON"));
		return false;
	}

	// JSON should be an array
	const TArray<TSharedPtr<FJsonValue>>* JsonArray;
	if (!JsonValue->TryGetArray(JsonArray))
	{
		return false;
	}

	// Find the BlueprintGeneratedClass entry
	for (const TSharedPtr<FJsonValue>& Entry : *JsonArray)
	{
		const TSharedPtr<FJsonObject>* EntryObj;
		if (!Entry->TryGetObject(EntryObj))
		{
			continue;
		}

		FString Type;
		if ((*EntryObj)->TryGetStringField(TEXT("Type"), Type) && Type == TEXT("BlueprintGeneratedClass"))
		{
			UE_LOG(LogTemp, Warning, TEXT("Found BlueprintGeneratedClass"));
			
			// Extract parent class from Super field (Blueprint parent)
			const TSharedPtr<FJsonObject>* SuperObj;
			if ((*EntryObj)->TryGetObjectField(TEXT("Super"), SuperObj))
			{
				FString SuperObjectPath;
				if ((*SuperObj)->TryGetStringField(TEXT("ObjectPath"), SuperObjectPath))
				{
					UE_LOG(LogTemp, Warning, TEXT("Found Super ObjectPath: %s"), *SuperObjectPath);
					OutParentClassPath = SuperObjectPath;
				}
			}
			// If no Super field, check for SuperStruct (C++ parent class)
			else
			{
				const TSharedPtr<FJsonObject>* SuperStructObj;
				if ((*EntryObj)->TryGetObjectField(TEXT("SuperStruct"), SuperStructObj))
				{
					FString SuperStructName;
					if ((*SuperStructObj)->TryGetStringField(TEXT("ObjectName"), SuperStructName))
					{
						// Extract class name from "Class'PalWeaponBase'" -> "PalWeaponBase"
						if (SuperStructName.StartsWith(TEXT("Class'")))
						{
							SuperStructName.RemoveFromStart(TEXT("Class'"));
							SuperStructName.RemoveFromEnd(TEXT("'"));
							UE_LOG(LogTemp, Warning, TEXT("Found SuperStruct class name: %s"), *SuperStructName);
							// Store with special prefix to indicate it's a C++ class
							OutParentClassPath = TEXT("CPP:") + SuperStructName;
						}
					}
				}
			}
			
			// Extract function names from Children array
			const TArray<TSharedPtr<FJsonValue>>* Children;
			if ((*EntryObj)->TryGetArrayField(TEXT("Children"), Children))
			{
				UE_LOG(LogTemp, Warning, TEXT("Found Children array with %d entries"), Children->Num());
				
				for (const TSharedPtr<FJsonValue>& Child : *Children)
				{
					const TSharedPtr<FJsonObject>* ChildObj;
					if (Child->TryGetObject(ChildObj))
					{
						FString ObjectName;
						if ((*ChildObj)->TryGetStringField(TEXT("ObjectName"), ObjectName))
						{
							UE_LOG(LogTemp, Warning, TEXT("Found ObjectName: %s"), *ObjectName);
							
							// Extract function name from "Function'BP_Item_C:GetName'"
							if (ObjectName.StartsWith(TEXT("Function'")))
							{
								int32 ColonIndex;
								if (ObjectName.FindChar(':', ColonIndex))
								{
									FString FuncName = ObjectName.Mid(ColonIndex + 1);
									FuncName.RemoveFromEnd(TEXT("'"));
									
									// Replace spaces with underscores (FName doesn't handle spaces well)
									FuncName = FuncName.Replace(TEXT(" "), TEXT("_"));
									
									// Validate the function name
									if (!FuncName.IsEmpty() && FuncName != TEXT("None"))
									{
										UE_LOG(LogTemp, Warning, TEXT("Extracted function name: %s"), *FuncName);
										
										// Create FName and verify it's valid
										FName FuncFName(*FuncName);
										if (FuncFName.IsValid() && !FuncFName.IsNone())
										{
											// Use AddUnique to avoid duplicate function names from JSON
											OutFunctionNames.AddUnique(FuncFName);
										}
										else
										{
											UE_LOG(LogTemp, Error, TEXT("Failed to create valid FName from: %s"), *FuncName);
										}
									}
								}
							}
						}
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Children array not found"));
			}

			// Extract component names and variable properties from ChildProperties array
			const TArray<TSharedPtr<FJsonValue>>* ChildProperties;
			if ((*EntryObj)->TryGetArrayField(TEXT("ChildProperties"), ChildProperties))
			{
				for (const TSharedPtr<FJsonValue>& Prop : *ChildProperties)
				{
					const TSharedPtr<FJsonObject>* PropObj;
					if (Prop->TryGetObject(PropObj))
					{
						FString PropType;
						(*PropObj)->TryGetStringField(TEXT("Type"), PropType);
						
						FString PropName;
						(*PropObj)->TryGetStringField(TEXT("Name"), PropName);
						
						// Skip certain system properties
						if (PropName == TEXT("UberGraphFrame"))
						{
							continue;
						}
						
						if (PropType == TEXT("ObjectProperty"))
						{
							const TSharedPtr<FJsonObject>* PropertyClass;
							if ((*PropObj)->TryGetObjectField(TEXT("PropertyClass"), PropertyClass))
							{
								FString ClassName;
								(*PropertyClass)->TryGetStringField(TEXT("ObjectName"), ClassName);
								
								// Check if it's a component class
								if (ClassName.Contains(TEXT("Component")))
								{
									if (!PropName.IsEmpty())
									{
										// Clean up class name: "Class'SceneComponent'" -> "SceneComponent"
										ClassName.RemoveFromStart(TEXT("Class'"));
										ClassName.RemoveFromEnd(TEXT("'"));
										
										// Use AddUnique to avoid duplicates
										OutComponentNames.AddUnique(FName(*PropName));
										OutComponentClasses.AddUnique(ClassName);
									}
								}
							}
						}
						// Handle Blueprint variables (non-component properties)
						else if (PropType == TEXT("BoolProperty") || 
						         PropType == TEXT("IntProperty") || 
						         PropType == TEXT("FloatProperty") || 
						         PropType == TEXT("DoubleProperty") ||
						         PropType == TEXT("ByteProperty") ||
						         PropType == TEXT("StrProperty") ||
						         PropType == TEXT("NameProperty") ||
						         PropType == TEXT("TextProperty"))
						{
							if (!PropName.IsEmpty())
							{
								// Check PropertyFlags to determine if it's a user-created variable
								FString PropertyFlags;
								(*PropObj)->TryGetStringField(TEXT("PropertyFlags"), PropertyFlags);
								
								// Only include properties that are Blueprint-visible/editable
								// Skip function-internal variables (CallFunc_, K2Node_, etc.)
								if (!PropName.StartsWith(TEXT("CallFunc_")) && 
								    !PropName.StartsWith(TEXT("K2Node_")) &&
								    !PropName.StartsWith(TEXT("Temp_")))
								{
									FString VarType;
									
									// Map FModel property types to Unreal variable types
									if (PropType == TEXT("BoolProperty"))
										VarType = TEXT("bool");
									else if (PropType == TEXT("IntProperty"))
										VarType = TEXT("int32");
									else if (PropType == TEXT("FloatProperty"))
										VarType = TEXT("float");
									else if (PropType == TEXT("DoubleProperty"))
										VarType = TEXT("double");
									else if (PropType == TEXT("ByteProperty"))
										VarType = TEXT("uint8");
									else if (PropType == TEXT("StrProperty"))
										VarType = TEXT("FString");
									else if (PropType == TEXT("NameProperty"))
										VarType = TEXT("FName");
									else if (PropType == TEXT("TextProperty"))
										VarType = TEXT("FText");
									
									if (!VarType.IsEmpty())
									{
										UE_LOG(LogTemp, Log, TEXT("Found variable: %s (%s)"), *PropName, *VarType);
										OutVariableNames.AddUnique(FName(*PropName));
										OutVariableTypes.Add(VarType);  // Use Add instead of AddUnique - types must match names array
									}
								}
							}
						}
					}
				}
			}

			break;
		}
	}

	// Parse Function objects to extract return types
	// Build a map of function name -> return type info (Type|ClassName format)
	TMap<FString, FString> FunctionReturnTypeMap;
	
	UE_LOG(LogTemp, Log, TEXT("Parsing Function objects for return types..."));
	
	for (const TSharedPtr<FJsonValue>& Entry : *JsonArray)
	{
		const TSharedPtr<FJsonObject>* EntryObj;
		if (!Entry->TryGetObject(EntryObj))
		{
			continue;
		}

		FString EntryType;
		(*EntryObj)->TryGetStringField(TEXT("Type"), EntryType);
		
		if (EntryType == TEXT("Function"))
		{
			FString FuncName;
			(*EntryObj)->TryGetStringField(TEXT("Name"), FuncName);
			
			if (FuncName.IsEmpty())
			{
				continue;
			}
			
			// Replace spaces with underscores to match how we process Children array
			FuncName = FuncName.Replace(TEXT(" "), TEXT("_"));
			
			// Track that we found this function (even if it has no return value)
			bool bFoundReturnParam = false;
			
			// Look for return parameter in ChildProperties
			const TArray<TSharedPtr<FJsonValue>>* ChildProps;
			if ((*EntryObj)->TryGetArrayField(TEXT("ChildProperties"), ChildProps))
			{
				for (const TSharedPtr<FJsonValue>& Prop : *ChildProps)
				{
					const TSharedPtr<FJsonObject>* PropObj;
					if (!Prop->TryGetObject(PropObj))
					{
						continue;
					}
					
					FString PropertyFlags;
					(*PropObj)->TryGetStringField(TEXT("PropertyFlags"), PropertyFlags);
					
					// Check if this is a return parameter
					// ReturnParm - explicit return value
					// OutParm without ReferenceParm - also a return value
					// OutParm WITH ReferenceParm - this is a reference parameter (like C# ref), NOT a return
					bool bIsReturnParam = PropertyFlags.Contains(TEXT("ReturnParm"));
					bool bIsOutParam = PropertyFlags.Contains(TEXT("OutParm")) && !PropertyFlags.Contains(TEXT("ReferenceParm"));
					
					if (PropertyFlags.Contains(TEXT("Parm")) && (bIsReturnParam || bIsOutParam))
					{
						FString PropType;
						(*PropObj)->TryGetStringField(TEXT("Type"), PropType);
						
						// Build return type info string (may include class/struct name)
						FString ReturnTypeInfo = PropType;
						
						// For Class/Object types, try to get the specific class name from MetaClass or PropertyClass
						if (PropType == TEXT("ClassProperty") || PropType == TEXT("ObjectProperty"))
						{
							FString ClassName;
							FString ClassPath;
							
							// Try MetaClass first (used by ClassProperty)
							const TSharedPtr<FJsonObject>* MetaClassObj;
							if ((*PropObj)->TryGetObjectField(TEXT("MetaClass"), MetaClassObj))
							{
								(*MetaClassObj)->TryGetStringField(TEXT("ObjectName"), ClassName);
								(*MetaClassObj)->TryGetStringField(TEXT("ObjectPath"), ClassPath);
							}
							// Try PropertyClass (used by ObjectProperty)
							else
							{
								const TSharedPtr<FJsonObject>* PropClassObj;
								if ((*PropObj)->TryGetObjectField(TEXT("PropertyClass"), PropClassObj))
								{
									(*PropClassObj)->TryGetStringField(TEXT("ObjectName"), ClassName);
									(*PropClassObj)->TryGetStringField(TEXT("ObjectPath"), ClassPath);
								}
							}
							
							if (!ClassName.IsEmpty())
							{
								// ObjectName is like "Class'PalBullet'" or "BlueprintGeneratedClass'BP_ShooterAnime_BowBase_C'" - extract just the name
								if (ClassName.Contains(TEXT("'")))
								{
									int32 StartIdx = ClassName.Find(TEXT("'"), ESearchCase::CaseSensitive) + 1;
									int32 EndIdx = ClassName.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
									if (StartIdx > 0 && EndIdx > StartIdx)
									{
										ClassName = ClassName.Mid(StartIdx, EndIdx - StartIdx);
									}
								}
								
								// Store as "Type|ClassName|ClassPath" format (path may be empty for native classes)
								ReturnTypeInfo = PropType + TEXT("|") + ClassName;
								if (!ClassPath.IsEmpty())
								{
									ReturnTypeInfo += TEXT("|") + ClassPath;
								}
								UE_LOG(LogTemp, Log, TEXT("  Function '%s' has return type: %s (Class: %s, Path: %s)"), *FuncName, *PropType, *ClassName, *ClassPath);
							}
						}
						// For Struct types, try to get the specific struct name
						else if (PropType == TEXT("StructProperty"))
						{
							const TSharedPtr<FJsonObject>* StructObj;
							if ((*PropObj)->TryGetObjectField(TEXT("Struct"), StructObj))
							{
								FString StructName;
								FString StructPath;
								
								if ((*StructObj)->TryGetStringField(TEXT("ObjectName"), StructName))
								{
									// ObjectName is like "Class'Transform'" or "UserDefinedStruct'F_NPC_PathWalkArray'" - extract just the name
									if (StructName.Contains(TEXT("'")))
									{
										int32 StartIdx = StructName.Find(TEXT("'"), ESearchCase::CaseSensitive) + 1;
										int32 EndIdx = StructName.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
										if (StartIdx > 0 && EndIdx > StartIdx)
										{
											StructName = StructName.Mid(StartIdx, EndIdx - StartIdx);
										}
									}
								}
								
								// Also get ObjectPath for user-defined structs
								(*StructObj)->TryGetStringField(TEXT("ObjectPath"), StructPath);
								
								// Store as "Type|StructName|StructPath" format (path may be empty for native structs)
								ReturnTypeInfo = PropType + TEXT("|") + StructName;
								if (!StructPath.IsEmpty())
								{
									ReturnTypeInfo += TEXT("|") + StructPath;
								}
								
								UE_LOG(LogTemp, Log, TEXT("  Function '%s' has return type: %s (Struct: %s, Path: %s)"), *FuncName, *PropType, *StructName, *StructPath);
							}
						}
						// For Array types, extract the inner type
						else if (PropType == TEXT("ArrayProperty"))
						{
							const TSharedPtr<FJsonObject>* InnerObj;
							if ((*PropObj)->TryGetObjectField(TEXT("Inner"), InnerObj))
							{
								FString InnerType;
								(*InnerObj)->TryGetStringField(TEXT("Type"), InnerType);
								
								// For arrays of objects/classes, get the specific class
								if (InnerType == TEXT("ObjectProperty") || InnerType == TEXT("ClassProperty"))
								{
									FString InnerClassName;
									FString InnerClassPath;
									
									const TSharedPtr<FJsonObject>* InnerPropClassObj;
									if ((*InnerObj)->TryGetObjectField(TEXT("PropertyClass"), InnerPropClassObj))
									{
										(*InnerPropClassObj)->TryGetStringField(TEXT("ObjectName"), InnerClassName);
										(*InnerPropClassObj)->TryGetStringField(TEXT("ObjectPath"), InnerClassPath);
									}
									
									if (!InnerClassName.IsEmpty())
									{
										// Extract class name from "Class'Actor'" format
										if (InnerClassName.Contains(TEXT("'")))
										{
											int32 StartIdx = InnerClassName.Find(TEXT("'"), ESearchCase::CaseSensitive) + 1;
											int32 EndIdx = InnerClassName.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
											if (StartIdx > 0 && EndIdx > StartIdx)
											{
												InnerClassName = InnerClassName.Mid(StartIdx, EndIdx - StartIdx);
											}
										}
										
										// Store as "ArrayProperty|InnerType|InnerClassName|InnerClassPath"
										ReturnTypeInfo = PropType + TEXT("|") + InnerType + TEXT("|") + InnerClassName;
										if (!InnerClassPath.IsEmpty())
										{
											ReturnTypeInfo += TEXT("|") + InnerClassPath;
										}
										UE_LOG(LogTemp, Log, TEXT("  Function '%s' has return type: Array<%s> (Class: %s)"), *FuncName, *InnerType, *InnerClassName);
									}
									else
									{
										// Array of objects but no class specified
										ReturnTypeInfo = PropType + TEXT("|") + InnerType;
										UE_LOG(LogTemp, Log, TEXT("  Function '%s' has return type: Array<%s>"), *FuncName, *InnerType);
									}
								}
								// For arrays of structs, get the struct name
								else if (InnerType == TEXT("StructProperty"))
								{
									const TSharedPtr<FJsonObject>* InnerStructObj;
									if ((*InnerObj)->TryGetObjectField(TEXT("Struct"), InnerStructObj))
									{
										FString InnerStructName;
										if ((*InnerStructObj)->TryGetStringField(TEXT("ObjectName"), InnerStructName))
										{
											// Extract struct name from "Class'Vector'" format
											if (InnerStructName.Contains(TEXT("'")))
											{
												int32 StartIdx = InnerStructName.Find(TEXT("'"), ESearchCase::CaseSensitive) + 1;
												int32 EndIdx = InnerStructName.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
												if (StartIdx > 0 && EndIdx > StartIdx)
												{
													InnerStructName = InnerStructName.Mid(StartIdx, EndIdx - StartIdx);
												}
											}
											
											// Store as "ArrayProperty|StructProperty|StructName"
											ReturnTypeInfo = PropType + TEXT("|") + InnerType + TEXT("|") + InnerStructName;
											UE_LOG(LogTemp, Log, TEXT("  Function '%s' has return type: Array<Struct:%s>"), *FuncName, *InnerStructName);
										}
									}
								}
								else
								{
									// Simple array (int, bool, etc.)
									ReturnTypeInfo = PropType + TEXT("|") + InnerType;
									UE_LOG(LogTemp, Log, TEXT("  Function '%s' has return type: Array<%s>"), *FuncName, *InnerType);
								}
							}
						}
						// For Map types, extract both key and value types
						else if (PropType == TEXT("MapProperty"))
						{
							FString KeyType, ValueType;
							FString KeyClassName, ValueClassName;
							
							// Extract KeyProp
							const TSharedPtr<FJsonObject>* KeyPropObj;
							if ((*PropObj)->TryGetObjectField(TEXT("KeyProp"), KeyPropObj))
							{
								(*KeyPropObj)->TryGetStringField(TEXT("Type"), KeyType);
								
								// If key is an object/class, get the class name
								if (KeyType == TEXT("ObjectProperty") || KeyType == TEXT("ClassProperty"))
								{
									const TSharedPtr<FJsonObject>* KeyClassObj;
									if ((*KeyPropObj)->TryGetObjectField(TEXT("PropertyClass"), KeyClassObj))
									{
										(*KeyClassObj)->TryGetStringField(TEXT("ObjectName"), KeyClassName);
										if (KeyClassName.Contains(TEXT("'")))
										{
											int32 StartIdx = KeyClassName.Find(TEXT("'"), ESearchCase::CaseSensitive) + 1;
											int32 EndIdx = KeyClassName.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
											if (StartIdx > 0 && EndIdx > StartIdx)
											{
												KeyClassName = KeyClassName.Mid(StartIdx, EndIdx - StartIdx);
											}
										}
									}
								}
								// If key is a struct, get the struct name
								else if (KeyType == TEXT("StructProperty"))
								{
									const TSharedPtr<FJsonObject>* KeyStructObj;
									if ((*KeyPropObj)->TryGetObjectField(TEXT("Struct"), KeyStructObj))
									{
										(*KeyStructObj)->TryGetStringField(TEXT("ObjectName"), KeyClassName);
										if (KeyClassName.Contains(TEXT("'")))
										{
											int32 StartIdx = KeyClassName.Find(TEXT("'"), ESearchCase::CaseSensitive) + 1;
											int32 EndIdx = KeyClassName.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
											if (StartIdx > 0 && EndIdx > StartIdx)
											{
												KeyClassName = KeyClassName.Mid(StartIdx, EndIdx - StartIdx);
											}
										}
									}
								}
								// If key is an enum, get the enum type
								else if (KeyType == TEXT("EnumProperty"))
								{
									const TSharedPtr<FJsonObject>* EnumObj;
									if ((*KeyPropObj)->TryGetObjectField(TEXT("Enum"), EnumObj))
									{
										(*EnumObj)->TryGetStringField(TEXT("ObjectName"), KeyClassName);
										if (KeyClassName.Contains(TEXT("'")))
										{
											int32 StartIdx = KeyClassName.Find(TEXT("'"), ESearchCase::CaseSensitive) + 1;
											int32 EndIdx = KeyClassName.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
											if (StartIdx > 0 && EndIdx > StartIdx)
											{
												KeyClassName = KeyClassName.Mid(StartIdx, EndIdx - StartIdx);
											}
										}
									}
								}
							}
							
							// Extract ValueProp
							const TSharedPtr<FJsonObject>* ValuePropObj;
							if ((*PropObj)->TryGetObjectField(TEXT("ValueProp"), ValuePropObj))
							{
								(*ValuePropObj)->TryGetStringField(TEXT("Type"), ValueType);
								
								// If value is an object/class, get the class name
								if (ValueType == TEXT("ObjectProperty") || ValueType == TEXT("ClassProperty"))
								{
									const TSharedPtr<FJsonObject>* ValueClassObj;
									if ((*ValuePropObj)->TryGetObjectField(TEXT("PropertyClass"), ValueClassObj))
									{
										(*ValueClassObj)->TryGetStringField(TEXT("ObjectName"), ValueClassName);
										if (ValueClassName.Contains(TEXT("'")))
										{
											int32 StartIdx = ValueClassName.Find(TEXT("'"), ESearchCase::CaseSensitive) + 1;
											int32 EndIdx = ValueClassName.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
											if (StartIdx > 0 && EndIdx > StartIdx)
											{
												ValueClassName = ValueClassName.Mid(StartIdx, EndIdx - StartIdx);
											}
										}
									}
								}
								// If value is a struct, get the struct name
								else if (ValueType == TEXT("StructProperty"))
								{
									const TSharedPtr<FJsonObject>* ValueStructObj;
									if ((*ValuePropObj)->TryGetObjectField(TEXT("Struct"), ValueStructObj))
									{
										(*ValueStructObj)->TryGetStringField(TEXT("ObjectName"), ValueClassName);
										if (ValueClassName.Contains(TEXT("'")))
										{
											int32 StartIdx = ValueClassName.Find(TEXT("'"), ESearchCase::CaseSensitive) + 1;
											int32 EndIdx = ValueClassName.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
											if (StartIdx > 0 && EndIdx > StartIdx)
											{
												ValueClassName = ValueClassName.Mid(StartIdx, EndIdx - StartIdx);
											}
										}
									}
								}
							}
							
							// Build MapProperty format: "MapProperty|KeyType|ValueType|KeyClassName|ValueClassName"
							// KeyClassName and ValueClassName may be empty for primitive types
							ReturnTypeInfo = PropType + TEXT("|") + KeyType + TEXT("|") + ValueType;
							if (!KeyClassName.IsEmpty())
							{
								ReturnTypeInfo += TEXT("|") + KeyClassName;
							}
							else
							{
								ReturnTypeInfo += TEXT("|");  // Empty slot
							}
							if (!ValueClassName.IsEmpty())
							{
								ReturnTypeInfo += TEXT("|") + ValueClassName;
							}
							
							UE_LOG(LogTemp, Log, TEXT("  Function '%s' has return type: Map<%s, %s>"), *FuncName, *KeyType, *ValueType);
							if (!KeyClassName.IsEmpty() || !ValueClassName.IsEmpty())
							{
								UE_LOG(LogTemp, Log, TEXT("    Key class: %s, Value class: %s"), 
									KeyClassName.IsEmpty() ? TEXT("(primitive)") : *KeyClassName,
									ValueClassName.IsEmpty() ? TEXT("(primitive)") : *ValueClassName);
							}
						}
						else
						{
							UE_LOG(LogTemp, Log, TEXT("  Function '%s' has return type: %s"), *FuncName, *PropType);
						}
						
						// Store the return type for this function
						FunctionReturnTypeMap.Add(FuncName, ReturnTypeInfo);
						bFoundReturnParam = true;
						break; // Only care about first return param
					}
				}
			}
			
			// If function was found but has no return parameter, mark it explicitly with "VOID"
			// This prevents auto-detection from kicking in
			if (!bFoundReturnParam)
			{
				FunctionReturnTypeMap.Add(FuncName, TEXT("VOID"));
				UE_LOG(LogTemp, Log, TEXT("  Function '%s' has no return value"), *FuncName);
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Found %d functions with return types"), FunctionReturnTypeMap.Num());
	
	// Build OutFunctionReturnTypes array parallel to OutFunctionNames
	UE_LOG(LogTemp, Log, TEXT("Building return types array for %d functions"), OutFunctionNames.Num());
	for (const FName& FuncName : OutFunctionNames)
	{
		FString* ReturnType = FunctionReturnTypeMap.Find(FuncName.ToString());
		if (ReturnType)
		{
			// If marked as VOID (function exists but has no return), convert to empty string
			// But mark it differently than "not found" so auto-detection doesn't kick in
			if (*ReturnType == TEXT("VOID"))
			{
				OutFunctionReturnTypes.Add(TEXT("VOID"));  // Keep VOID marker
				UE_LOG(LogTemp, Log, TEXT("  %s -> VOID (no return)"), *FuncName.ToString());
			}
			else
			{
				OutFunctionReturnTypes.Add(*ReturnType);
				UE_LOG(LogTemp, Log, TEXT("  %s -> %s"), *FuncName.ToString(), **ReturnType);
			}
		}
		else
		{
			// Function not found in JSON, add empty string (will trigger auto-detection)
			OutFunctionReturnTypes.Add(TEXT(""));
			UE_LOG(LogTemp, Log, TEXT("  %s -> (not in JSON, will auto-detect)"), *FuncName.ToString());
		}
	}

	// Even if no functions/components/variables found, still return true for valid Blueprint JSON
	// Simple Blueprints that just inherit from parents are valid and should be created
	return true;
}

UBlueprint* UDummyBlueprintFunctionLibrary::CreateBlueprintFromFModelJSON(const FString& JsonFilePath, const FString& DestinationPath, const FString& AssetName)
{
	// Parse JSON first
	TArray<FName> FunctionNames;
	TArray<FName> ComponentNames;
	TArray<FString> ComponentClasses;
	TArray<FName> VariableNames;
	TArray<FString> VariableTypes;
	TArray<FString> FunctionReturnTypes;
	FString ParentClassPath;
	
	if (!ParseFModelJSON(JsonFilePath, FunctionNames, ComponentNames, ComponentClasses, VariableNames, VariableTypes, FunctionReturnTypes, ParentClassPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON file: %s"), *JsonFilePath);
		return nullptr;
	}

	// Determine parent class
	UClass* ParentClass = AActor::StaticClass(); // Default to Actor
	
	if (!ParentClassPath.IsEmpty())
	{
		// Check if it's a C++ class (prefixed with "CPP:")
		if (ParentClassPath.StartsWith(TEXT("CPP:")))
		{
			FString ClassName = ParentClassPath.Mid(4); // Remove "CPP:" prefix
			UE_LOG(LogTemp, Log, TEXT("Looking for C++ parent class: %s"), *ClassName);
			
			// Try to find the C++ class
			UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
			if (FoundClass)
			{
				ParentClass = FoundClass;
				UE_LOG(LogTemp, Log, TEXT("✅ Using C++ parent class: %s"), *ParentClass->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("❌ C++ class '%s' not found, defaulting to AActor"), *ClassName);
			}
		}
		else
		{
			// It's a Blueprint parent class
			// Convert ObjectPath format to asset path
			// From: "/Game/Pal/Blueprint/Weapon/BP_GatlingGun.0"
			// To: "/Game/Pal/Blueprint/Weapon/BP_GatlingGun.BP_GatlingGun"
			FString AssetPath = ParentClassPath;
			
			// Remove the .0 or other numeric suffix
			int32 DotIndex;
			if (AssetPath.FindLastChar('.', DotIndex))
			{
				FString NumericPart = AssetPath.Mid(DotIndex + 1);
				if (NumericPart.IsNumeric())
				{
					AssetPath = AssetPath.Left(DotIndex);
				}
			}
			
			// Extract asset name from path
			FString AssetName;
			if (AssetPath.Split(TEXT("/"), nullptr, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
			{
				// Build proper asset reference: /Path/To/Asset.AssetName
				AssetPath = AssetPath + TEXT(".") + AssetName;
			}
			
			UE_LOG(LogTemp, Log, TEXT("Trying to load parent Blueprint: %s"), *AssetPath);
			
			// Try to load the parent Blueprint
			UBlueprint* ParentBlueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
			
			// If not found, try alternate path with /Content/Pal/ insertion
			if (!ParentBlueprint && AssetPath.StartsWith(TEXT("/Game/Pal/")))
			{
				FString AlternatePath = AssetPath.Replace(TEXT("/Game/Pal/"), TEXT("/Game/Pal/Content/Pal/"));
				UE_LOG(LogTemp, Log, TEXT("Trying alternate parent path: %s"), *AlternatePath);
				ParentBlueprint = LoadObject<UBlueprint>(nullptr, *AlternatePath);
			}
			
			if (ParentBlueprint)
			{
				// Ensure parent is compiled so we can check for inherited functions
				if (!ParentBlueprint->GeneratedClass || ParentBlueprint->Status != BS_UpToDate)
				{
					UE_LOG(LogTemp, Log, TEXT("Parent Blueprint needs compilation, compiling now..."));
					FKismetEditorUtilities::CompileBlueprint(ParentBlueprint);
			}
			
			if (ParentBlueprint->GeneratedClass)
			{
				ParentClass = ParentBlueprint->GeneratedClass;
				UE_LOG(LogTemp, Log, TEXT("✅ Using parent class: %s"), *ParentClass->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("❌ Parent Blueprint failed to compile, defaulting to AActor"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("❌ Parent Blueprint not found: %s, defaulting to AActor"), *AssetPath);
		}
		} // End of Blueprint parent handling
	}
	// Create Blueprint asset
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClass;
	
	UBlueprint* NewBlueprint = Cast<UBlueprint>(AssetTools.CreateAsset(
		AssetName,
		DestinationPath,
		UBlueprint::StaticClass(),
		Factory
	));

	if (!NewBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create Blueprint asset"));
		return nullptr;
	}

	// Log what we parsed
	UE_LOG(LogTemp, Log, TEXT("Parsed: %d functions, %d components, %d variables"), FunctionNames.Num(), ComponentNames.Num(), VariableNames.Num());
	
	// Add components
	int32 CompCount = AddComponentsToBlueprint(NewBlueprint, ComponentNames, ComponentClasses);
	UE_LOG(LogTemp, Log, TEXT("Added %d components"), CompCount);

	// Add variables
	UE_LOG(LogTemp, Log, TEXT("Attempting to add %d variables..."), VariableNames.Num());
	int32 VarCount = AddVariablesToBlueprint(NewBlueprint, VariableNames, VariableTypes);
	UE_LOG(LogTemp, Log, TEXT("Added %d variables"), VarCount);

	// Add functions with return type information
	int32 FuncCount = AddMultipleFunctionStubsToBlueprint(NewBlueprint, FunctionNames, FunctionReturnTypes);
	UE_LOG(LogTemp, Log, TEXT("Added %d functions"), FuncCount);

	// Compile blueprint
	FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

	// Save
	FString PackageName = DestinationPath + TEXT("/") + AssetName;
	UPackage* Package = NewBlueprint->GetOutermost();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(Package, NewBlueprint, *PackageFileName, SaveArgs);

	return NewBlueprint;
}

UUserDefinedStruct* UDummyBlueprintFunctionLibrary::CreateUserDefinedStructFromJSON(const FString& JsonFilePath, const FString& DestinationPath, const FString& StructName)
{
	UE_LOG(LogTemp, Log, TEXT("Creating UserDefinedStruct: %s at %s"), *StructName, *DestinationPath);

	// Create package
	FString PackageName = DestinationPath + TEXT("/") + StructName;
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create package for struct: %s"), *PackageName);
		return nullptr;
	}

	// Create the UserDefinedStruct
	UUserDefinedStruct* NewStruct = NewObject<UUserDefinedStruct>(Package, *StructName, RF_Public | RF_Standalone);
	if (!NewStruct)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create UserDefinedStruct: %s"), *StructName);
		return nullptr;
	}

	// Initialize the struct
	NewStruct->EditorData = NewObject<UUserDefinedStructEditorData>(NewStruct, NAME_None, RF_Transactional);
	NewStruct->Guid = FGuid::NewGuid();
	NewStruct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
	NewStruct->Status = UDSS_UpToDate;
	
	// Add a single dummy member variable to make the struct valid
	// Unreal requires at least one member for a struct to be considered "non-empty"
	// Use FStructureEditorUtils for proper variable addition
	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	PinType.PinSubCategory = NAME_None;
	PinType.PinSubCategoryObject = nullptr;
	
	FStructureEditorUtils::AddVariable(NewStruct, PinType);
	
	// Rename the variable to something meaningful
	if (NewStruct->EditorData)
	{
		UUserDefinedStructEditorData* StructEditorData = Cast<UUserDefinedStructEditorData>(NewStruct->EditorData);
		if (StructEditorData && StructEditorData->VariablesDescriptions.Num() > 0)
		{
			StructEditorData->VariablesDescriptions[0].VarName = FName(TEXT("DummyValue"));
			StructEditorData->VariablesDescriptions[0].FriendlyName = TEXT("Dummy Value");
			StructEditorData->VariablesDescriptions[0].DefaultValue = TEXT("false");
			UE_LOG(LogTemp, Log, TEXT("  Added dummy boolean member 'DummyValue' to struct"));
		}
	}
	
	// Compile the struct
	FStructureEditorUtils::CompileStructure(NewStruct);

	// Mark package as dirty and save
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewStruct);

	// Save the package
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(Package, NewStruct, *PackageFileName, SaveArgs);

	UE_LOG(LogTemp, Log, TEXT("✅ Successfully created UserDefinedStruct: %s"), *StructName);
	return NewStruct;
}
