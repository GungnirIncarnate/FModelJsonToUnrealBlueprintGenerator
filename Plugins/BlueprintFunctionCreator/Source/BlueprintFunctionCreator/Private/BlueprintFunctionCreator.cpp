// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintFunctionCreator.h"

#define LOCTEXT_NAMESPACE "FBlueprintFunctionCreatorModule"

void FBlueprintFunctionCreatorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory
}

void FBlueprintFunctionCreatorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlueprintFunctionCreatorModule, BlueprintFunctionCreator)
