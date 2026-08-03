#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FLevelEditorViewportClient;

// =====================================================================
// FUnrealMCPViewportCommands
//
// Control de la camara y el modo de visualizacion del viewport activo
// del editor (FLevelEditorViewportClient / GEditor->GetActiveViewport()).
// No confundir con foliage/actor-level focus_viewport ni con
// take_screenshot (ambos en UnrealMCPEditorCommands) -- este modulo
// mueve/orienta la camara y cambia el view mode (Lit/Unlit/Wireframe/etc),
// no toma capturas.
// =====================================================================
class UNREALMCP_API FUnrealMCPViewportCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleGetViewportCameraInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetViewportFOV(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetViewportViewMode(const TSharedPtr<FJsonObject>& Params);

    // Devuelve el FLevelEditorViewportClient activo, o nullptr + OutError
    // si no hay viewport activo (ej. editor headless).
    FLevelEditorViewportClient* GetActiveViewportClient(FString& OutError);
};
