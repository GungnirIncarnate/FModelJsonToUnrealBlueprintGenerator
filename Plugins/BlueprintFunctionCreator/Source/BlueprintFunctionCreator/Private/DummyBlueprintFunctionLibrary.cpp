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

bool UDummyBlueprintFunctionLibrary::AddFunctionStubToBlueprint(UBlueprint* Blueprint, FName FunctionName, bool bHasReturnValue)
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

	UE_LOG(LogTemp, Warning, TEXT("Creating graph for function: %s"), *FuncNameStr);

	// Create a new graph for the function
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FunctionName,
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
	EntryNode->AllocateDefaultPins();
	
	// CRITICAL: Set the function signature name on the entry node
	EntryNode->CustomGeneratedFunctionName = FunctionName;
	EntryNode->bIsEditable = true;
	
	NewGraph->AddNode(EntryNode);

	// Set node position
	EntryNode->NodePosX = -200;
	EntryNode->NodePosY = 0;

	// If the function has a return value, create a result node
	if (bHasReturnValue)
	{
		UK2Node_FunctionResult* ResultNode = NewObject<UK2Node_FunctionResult>(NewGraph);
		ResultNode->CreateNewGuid();
		ResultNode->PostPlacedNewNode();
		ResultNode->AllocateDefaultPins();
		NewGraph->AddNode(ResultNode);
		
		ResultNode->NodePosX = 200;
		ResultNode->NodePosY = 0;

		// Connect entry to result (execution pins)
		UEdGraphPin* EntryExecPin = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* ResultExecPin = ResultNode->FindPin(UEdGraphSchema_K2::PN_Execute);
		
	
	if (EntryExecPin && ResultExecPin)
	{
		EntryExecPin->MakeLinkTo(ResultExecPin);
	}
}

// Add the function graph to the blueprint
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, NewGraph, false, nullptr);

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);	UE_LOG(LogTemp, Log, TEXT("Successfully added function: %s"), *FunctionName.ToString());
	return true;
}

int32 UDummyBlueprintFunctionLibrary::AddMultipleFunctionStubsToBlueprint(UBlueprint* Blueprint, const TArray<FName>& FunctionNames)
{
	if (!Blueprint)
	{
		return 0;
	}

	int32 SuccessCount = 0;
	for (const FName& FuncName : FunctionNames)
	{
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
			if (AddFunctionStubToBlueprint(Blueprint, FuncName, false))
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

bool UDummyBlueprintFunctionLibrary::ParseFModelJSON(const FString& JsonFilePath, TArray<FName>& OutFunctionNames, TArray<FName>& OutComponentNames, TArray<FString>& OutComponentClasses, FString& OutParentClassPath)
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

			// Extract component names from ChildProperties array
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
									FString CompName;
									if ((*PropObj)->TryGetStringField(TEXT("Name"), CompName))
									{
									// Clean up class name: "Class'SceneComponent'" -> "SceneComponent"
									ClassName.RemoveFromStart(TEXT("Class'"));
									ClassName.RemoveFromEnd(TEXT("'"));
									
									// Use AddUnique to avoid duplicates
									OutComponentNames.AddUnique(FName(*CompName));
									OutComponentClasses.AddUnique(ClassName);
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

	return OutFunctionNames.Num() > 0 || OutComponentNames.Num() > 0;
}

UBlueprint* UDummyBlueprintFunctionLibrary::CreateBlueprintFromFModelJSON(const FString& JsonFilePath, const FString& DestinationPath, const FString& AssetName)
{
	// Parse JSON first
	TArray<FName> FunctionNames;
	TArray<FName> ComponentNames;
	TArray<FString> ComponentClasses;
	FString ParentClassPath;
	
	if (!ParseFModelJSON(JsonFilePath, FunctionNames, ComponentNames, ComponentClasses, ParentClassPath))
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

	// Add components
	int32 CompCount = AddComponentsToBlueprint(NewBlueprint, ComponentNames, ComponentClasses);
	UE_LOG(LogTemp, Log, TEXT("Added %d components"), CompCount);

	// Add functions
	int32 FuncCount = AddMultipleFunctionStubsToBlueprint(NewBlueprint, FunctionNames);
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

