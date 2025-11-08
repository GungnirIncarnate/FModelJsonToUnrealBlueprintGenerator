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
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static bool AddFunctionStubToBlueprint(UBlueprint* Blueprint, FName FunctionName, bool bHasReturnValue = false);

	/**
	 * Add multiple function stubs to a Blueprint from an array of names
	 * @param Blueprint - The Blueprint to add functions to
	 * @param FunctionNames - Array of function names to create
	 * @return Number of functions successfully created
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static int32 AddMultipleFunctionStubsToBlueprint(UBlueprint* Blueprint, const TArray<FName>& FunctionNames);

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
	 * Parse FModel JSON file and extract function names
	 * @param JsonFilePath - Path to the JSON file
	 * @param OutFunctionNames - Output array of function names found
	 * @param OutComponentNames - Output array of component names found
	 * @param OutComponentClasses - Output array of component class names found
	 * @param OutParentClassPath - Output parent class path (e.g., "/Game/Pal/Blueprint/Weapon/BP_GatlingGun")
	 * @return True if parsing was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static bool ParseFModelJSON(const FString& JsonFilePath, TArray<FName>& OutFunctionNames, TArray<FName>& OutComponentNames, TArray<FString>& OutComponentClasses, FString& OutParentClassPath);

	/**
	 * Create a complete Blueprint from FModel JSON
	 * @param JsonFilePath - Path to the JSON file
	 * @param DestinationPath - Where to create the Blueprint in Unreal (e.g., "/Game/Pal/Blueprint/")
	 * @param AssetName - Name of the Blueprint asset to create
	 * @return The created Blueprint, or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Function Creator")
	static UBlueprint* CreateBlueprintFromFModelJSON(const FString& JsonFilePath, const FString& DestinationPath, const FString& AssetName);
};
