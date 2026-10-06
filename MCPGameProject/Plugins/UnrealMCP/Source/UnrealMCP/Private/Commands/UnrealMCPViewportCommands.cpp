#include "Commands/UnrealMCPViewportCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Engine/EngineBaseTypes.h"
#include "Dom/JsonValue.h"

// =====================================================================
// Mapa string <-> EViewModeIndex. Solo cubrimos los modos mas usados
// para trabajo de iluminacion/atmosfera (no los de debug de streaming,
// LOD, etc -- se pueden sumar despues si hacen falta).
// =====================================================================
static FString ViewModeToString(EViewModeIndex Mode)
{
    switch (Mode)
    {
        case VMI_Lit:                   return TEXT("Lit");
        case VMI_Unlit:                 return TEXT("Unlit");
        case VMI_Wireframe:             return TEXT("Wireframe");
        case VMI_BrushWireframe:        return TEXT("BrushWireframe");
        case VMI_LightingOnly:          return TEXT("LightingOnly");
        case VMI_Lit_DetailLighting:    return TEXT("DetailLighting");
        case VMI_ShaderComplexity:      return TEXT("ShaderComplexity");
        case VMI_LightComplexity:       return TEXT("LightComplexity");
        default:                        return TEXT("Other");
    }
}

static bool StringToViewMode(const FString& Str, EViewModeIndex& OutMode)
{
    if (Str == TEXT("Lit"))              { OutMode = VMI_Lit; return true; }
    if (Str == TEXT("Unlit"))            { OutMode = VMI_Unlit; return true; }
    if (Str == TEXT("Wireframe"))        { OutMode = VMI_Wireframe; return true; }
    if (Str == TEXT("BrushWireframe"))   { OutMode = VMI_BrushWireframe; return true; }
    if (Str == TEXT("LightingOnly"))     { OutMode = VMI_LightingOnly; return true; }
    if (Str == TEXT("DetailLighting"))   { OutMode = VMI_Lit_DetailLighting; return true; }
    if (Str == TEXT("ShaderComplexity")) { OutMode = VMI_ShaderComplexity; return true; }
    if (Str == TEXT("LightComplexity"))  { OutMode = VMI_LightComplexity; return true; }
    return false;
}

FLevelEditorViewportClient* FUnrealMCPViewportCommands::GetActiveViewportClient(FString& OutError)
{
    // Crash #6: GetActiveViewport() puede ser nullptr en instancias frescas
    // (tab del LevelEditor sin activar). Helper con guard + fallback + apertura
    // del tab via FGlobalTabmanager si hace falta.
    FLevelEditorViewportClient* ViewportClient =
        FUnrealMCPCommonUtils::FindAnyLevelEditorViewportClient();
    if (!ViewportClient)
    {
        OutError = TEXT("No active viewport");
        return nullptr;
    }
    return ViewportClient;
}

TSharedPtr<FJsonObject> FUnrealMCPViewportCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_viewport_camera_info")) { return HandleGetViewportCameraInfo(Params); }
    if (CommandType == TEXT("set_viewport_camera"))      { return HandleSetViewportCamera(Params); }
    if (CommandType == TEXT("set_viewport_fov"))         { return HandleSetViewportFOV(Params); }
    if (CommandType == TEXT("set_viewport_view_mode"))   { return HandleSetViewportViewMode(Params); }

    TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
    Error->SetBoolField(TEXT("success"), false);
    Error->SetStringField(TEXT("message"), FString::Printf(TEXT("Unknown viewport command: %s"), *CommandType));
    return Error;
}

TSharedPtr<FJsonObject> FUnrealMCPViewportCommands::HandleGetViewportCameraInfo(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    FString Err;
    FLevelEditorViewportClient* VC = GetActiveViewportClient(Err);
    if (!VC)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), Err);
        return Result;
    }

    const FVector Loc = VC->GetViewLocation();
    const FRotator Rot = VC->GetViewRotation();

    TArray<TSharedPtr<FJsonValue>> LocArr, RotArr;
    LocArr.Add(MakeShared<FJsonValueNumber>(Loc.X));
    LocArr.Add(MakeShared<FJsonValueNumber>(Loc.Y));
    LocArr.Add(MakeShared<FJsonValueNumber>(Loc.Z));
    RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Pitch));
    RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Yaw));
    RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Roll));

    Result->SetArrayField(TEXT("location"), LocArr);
    Result->SetArrayField(TEXT("rotation"), RotArr);
    Result->SetNumberField(TEXT("fov"), VC->ViewFOV);
    Result->SetStringField(TEXT("view_mode"), ViewModeToString(VC->GetViewMode()));
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPViewportCommands::HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    FString Err;
    FLevelEditorViewportClient* VC = GetActiveViewportClient(Err);
    if (!VC)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), Err);
        return Result;
    }

    bool bChangedAnything = false;

    const TArray<TSharedPtr<FJsonValue>>* LocArr = nullptr;
    if (Params->TryGetArrayField(TEXT("location"), LocArr) && LocArr && LocArr->Num() == 3)
    {
        FVector NewLoc((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(), (*LocArr)[2]->AsNumber());
        VC->SetViewLocation(NewLoc);
        bChangedAnything = true;
    }

    const TArray<TSharedPtr<FJsonValue>>* RotArr = nullptr;
    if (Params->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr && RotArr->Num() == 3)
    {
        FRotator NewRot((*RotArr)[0]->AsNumber(), (*RotArr)[1]->AsNumber(), (*RotArr)[2]->AsNumber());
        VC->SetViewRotation(NewRot);
        bChangedAnything = true;
    }

    if (!bChangedAnything)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), TEXT("Provide at least one of 'location' [x,y,z] or 'rotation' [pitch,yaw,roll]"));
        return Result;
    }

    VC->Invalidate();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPViewportCommands::HandleSetViewportFOV(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    FString Err;
    FLevelEditorViewportClient* VC = GetActiveViewportClient(Err);
    if (!VC)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), Err);
        return Result;
    }

    double Fov = 0.0;
    if (!Params->TryGetNumberField(TEXT("fov"), Fov))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), TEXT("Missing 'fov' parameter"));
        return Result;
    }

    VC->ViewFOV = Fov;
    VC->FOVAngle = Fov;
    VC->Invalidate();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPViewportCommands::HandleSetViewportViewMode(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    FString Err;
    FLevelEditorViewportClient* VC = GetActiveViewportClient(Err);
    if (!VC)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), Err);
        return Result;
    }

    FString ModeStr;
    if (!Params->TryGetStringField(TEXT("view_mode"), ModeStr))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), TEXT("Missing 'view_mode' parameter"));
        return Result;
    }

    EViewModeIndex Mode;
    if (!StringToViewMode(ModeStr, Mode))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), FString::Printf(
            TEXT("Unknown view_mode '%s'. Valid: Lit, Unlit, Wireframe, BrushWireframe, LightingOnly, DetailLighting, ShaderComplexity, LightComplexity"),
            *ModeStr));
        return Result;
    }

    VC->SetViewMode(Mode);
    VC->Invalidate();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}
