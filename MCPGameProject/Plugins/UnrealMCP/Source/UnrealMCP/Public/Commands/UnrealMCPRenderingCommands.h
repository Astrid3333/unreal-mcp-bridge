#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// =====================================================================
// FUnrealMCPRenderingCommands
//
// Control generico de render quality via consola de Unreal (cvars):
// r.ScreenPercentage, r.PostProcessAAQuality, r.RayTracing.Enable, y
// cualquier otra cvar r.*/sg.* registrada. No hardcodea propiedades
// especificas -- expone get/set generico sobre IConsoleVariable, mismo
// enfoque que FUnrealMCPMaterialNodeCommands para el grafo de material.
// =====================================================================
class UNREALMCP_API FUnrealMCPRenderingCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleGetCVar(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetCVar(const TSharedPtr<FJsonObject>& Params);
};
