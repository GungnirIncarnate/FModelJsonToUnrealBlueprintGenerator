// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DummyBlueprintFunctionLibrary.generated.h"

/**
 * Function library for creating Blueprint functions programmatically
 */
UCLASS()
class BLUEPRINTFUNCTIONCREATOR_API UDummyBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Add a simple function stub to a Blueprint
	 * @param Blueprint - The Blueprint to add the function to
	 * @param FunctionName - Name of the function to create
	 * @param bHasReturnValue - Whether the function has a return value
	 * @param ReturnValueType - The type of return value (e.g., "ClassProperty", "BoolProperty", etc.) - empty for wildcard
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static bool AddFunctionStubToBlueprint(UBlueprint* Blueprint, FName FunctionName, bool bHasReturnValue = false, const FString& ReturnValueType = TEXT(""));

	/**
	 * Add multiple function stubs to a Blueprint from an array of names
	 * @param Blueprint - The Blueprint to add functions to
	 * @param FunctionNames - Array of function names to create
	 * @param ReturnTypes - Array of return types (parallel to FunctionNames) - empty strings for no return value
	 * @return Number of functions successfully created
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static int32 AddMultipleFunctionStubsToBlueprint(UBlueprint* Blueprint, const TArray<FName>& FunctionNames, const TArray<FString>& ReturnTypes);

	/**
	 * Add components to a Blueprint from component data
	 * @param Blueprint - The Blueprint to add components to
	 * @param ComponentNames - Array of component names
	 * @param ComponentClasses - Array of component class names (e.g., "SceneComponent", "StaticMeshComponent")
	 * @return Number of components successfully created
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static int32 AddComponentsToBlueprint(UBlueprint* Blueprint, const TArray<FName>& ComponentNames, const TArray<FString>& ComponentClasses);

	/**
	 * Add member variables to a Blueprint
	 * @param Blueprint - The Blueprint to add variables to
	 * @param VariableNames - Array of variable names
	 * @param VariableTypes - Array of variable types (e.g., "bool", "int32", "float")
	 * @return Number of variables successfully created
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static int32 AddVariablesToBlueprint(UBlueprint* Blueprint, const TArray<FName>& VariableNames, const TArray<FString>& VariableTypes);

	/**
	 * Parse FModel JSON file and extract function names
	 * @param JsonFilePath - Path to the JSON file
	 * @param OutFunctionNames - Output array of function names found
	 * @param OutComponentNames - Output array of component names found
	 * @param OutComponentClasses - Output array of component class names found
	 * @param OutVariableNames - Output array of variable names found
	 * @param OutVariableTypes - Output array of variable types found
	 * @param OutFunctionReturnTypes - Output array of return type info for functions (parallel to OutFunctionNames)
	 * @param OutParentClassPath - Output parent class path (e.g., "/Game/Pal/Blueprint/Weapon/BP_GatlingGun")
	 * @return True if parsing was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static bool ParseFModelJSON(const FString& JsonFilePath, TArray<FName>& OutFunctionNames, TArray<FName>& OutComponentNames, TArray<FString>& OutComponentClasses, TArray<FName>& OutVariableNames, TArray<FString>& OutVariableTypes, TArray<FString>& OutFunctionReturnTypes, FString& OutParentClassPath);

	/**
	 * Create a complete Blueprint from FModel JSON
	 * @param JsonFilePath - Path to the JSON file
	 * @param DestinationPath - Where to create the Blueprint in Unreal (e.g., "/Game/Pal/Blueprint/")
	 * @param AssetName - Name of the Blueprint asset to create
	 * @return The created Blueprint, or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static UBlueprint* CreateBlueprintFromFModelJSON(const FString& JsonFilePath, const FString& DestinationPath, const FString& AssetName);

	/**
	 * Create a UserDefinedStruct from FModel JSON
	 * @param JsonFilePath - Path to the JSON file containing UserDefinedStruct data
	 * @param DestinationPath - Where to create the struct in Unreal (e.g., "/Game/Pal/Blueprint/Spawner/Other/")
	 * @param StructName - Name of the struct asset to create
	 * @return The created UserDefinedStruct, or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static UUserDefinedStruct* CreateUserDefinedStructFromJSON(const FString& JsonFilePath, const FString& DestinationPath, const FString& StructName);
};
