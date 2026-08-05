#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Niagara-related MCP commands.
 * Covers: spawning NiagaraSystem actors in the level, setting exposed
 * user parameters (float/vector/color), activating/deactivating the
 * effect at runtime, and pulsing a User-exposed bool to signal a
 * custom event to a running system's scripts.
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

    // Asset creation
    TSharedPtr<FJsonObject> HandleCreateNiagaraEmitter(const TSharedPtr<FJsonObject>& Params);

    // Trigger a custom event on a running Niagara component (implemented as a
    // pulse of a User-exposed boolean parameter User.<event_name>, since there
    // is no simple external "Custom Event" invocation API for a running
    // system). The parameter must already exist on the system as User Exposed
    // Bool for this to have any effect.
    TSharedPtr<FJsonObject> HandleTriggerNiagaraEvent(const TSharedPtr<FJsonObject>& Params);
};
