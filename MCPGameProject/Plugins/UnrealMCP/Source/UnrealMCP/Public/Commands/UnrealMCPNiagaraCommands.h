#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Niagara-related MCP commands.
 * Covers: spawning NiagaraSystem actors in the level, setting exposed
 * user parameters (float/vector/color), and activating/deactivating
 * the effect at runtime.
 */
class UNREALMCP_API FUnrealMCPNiagaraCommands
{
public:
    FUnrealMCPNiagaraCommands();

    // Dispatch entry point (matches the pattern of every other Commands class)
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Spawning
    TSharedPtr<FJsonObject> HandleSpawnNiagaraSystem(const TSharedPtr<FJsonObject>& Params);

    // Parameter setters
    TSharedPtr<FJsonObject> HandleSetNiagaraFloatParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetNiagaraVectorParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetNiagaraColorParameter(const TSharedPtr<FJsonObject>& Params);

    // Activation control
    TSharedPtr<FJsonObject> HandleActivateNiagaraComponent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeactivateNiagaraComponent(const TSharedPtr<FJsonObject>& Params);

    // User-exposed parameters on the NiagaraSystem ASSET (not a spawned instance)
    TSharedPtr<FJsonObject> HandleAddNiagaraUserParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListNiagaraUserParameters(const TSharedPtr<FJsonObject>& Params);
};
