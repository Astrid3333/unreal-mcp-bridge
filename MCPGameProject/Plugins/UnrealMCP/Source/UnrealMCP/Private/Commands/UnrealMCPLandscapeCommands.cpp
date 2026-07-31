#include "Commands/UnrealMCPLandscapeCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeLayerInfoObject.h"
#include "Math/UnrealMathUtility.h"

FUnrealMCPLandscapeCommands::FUnrealMCPLandscapeCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_landscape_info"))
    {
        return HandleGetLandscapeInfo(Params);
    }
    else if (CommandType == TEXT("sculpt_landscape_region"))
    {
        return HandleSculptLandscapeRegion(Params);
    }
    else if (CommandType == TEXT("paint_landscape_layer"))
    {
        return HandlePaintLandscapeLayer(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown landscape command: %s"), *CommandType));
}

// Shared helper: find a Landscape actor by name (actor label or object name),
// or the first Landscape found in the world if no name is given.
static ALandscape* FindLandscapeByName(UWorld* World, const FString& LandscapeName)
{
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        if (LandscapeName.IsEmpty() || It->GetActorLabel() == LandscapeName || It->GetName() == LandscapeName)
        {
            return *It;
        }
    }
    return nullptr;
}

// =====================================================================
// get_landscape_info
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandleGetLandscapeInfo(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString LandscapeName;
    Params->TryGetStringField(TEXT("landscape_name"), LandscapeName);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor world"));
    }

    ALandscape* FoundLandscape = FindLandscapeByName(World, LandscapeName);
    if (!FoundLandscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No matching Landscape actor found in the level"));
    }

    ULandscapeInfo* LandscapeInfo = FoundLandscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape has no LandscapeInfo (not registered?)"));
    }

    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    const bool bHasExtent = LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("landscape_name"), FoundLandscape->GetActorLabel());
    ResultObj->SetBoolField(TEXT("has_extent"), bHasExtent);
    if (bHasExtent)
    {
        // Coordenadas en "quads" (unidades de heightmap), no en cm de mundo.
        // Usar estos valores como rango válido para x/y en sculpt/paint.
        ResultObj->SetNumberField(TEXT("min_x"), MinX);
        ResultObj->SetNumberField(TEXT("min_y"), MinY);
        ResultObj->SetNumberField(TEXT("max_x"), MaxX);
        ResultObj->SetNumberField(TEXT("max_y"), MaxY);
    }
    ResultObj->SetNumberField(TEXT("component_size_quads"), FoundLandscape->ComponentSizeQuads);
    ResultObj->SetNumberField(TEXT("subsections_per_component"), FoundLandscape->NumSubsections);

    const FVector Scale = FoundLandscape->GetActorScale3D();
    TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
    ScaleObj->SetNumberField(TEXT("x"), Scale.X);
    ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
    ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
    ResultObj->SetObjectField(TEXT("scale"), ScaleObj);

    TArray<TSharedPtr<FJsonValue>> LayerNamesArr;
    for (const FLandscapeInfoLayerSettings& LayerSettings : LandscapeInfo->Layers)
    {
        if (LayerSettings.LayerInfoObj)
        {
            LayerNamesArr.Add(MakeShared<FJsonValueString>(LayerSettings.LayerInfoObj->LayerName.ToString()));
        }
        else if (!LayerSettings.LayerName.IsNone())
        {
            LayerNamesArr.Add(MakeShared<FJsonValueString>(
                FString::Printf(TEXT("%s (sin LayerInfo asset asignado)"), *LayerSettings.LayerName.ToString())));
        }
    }
    ResultObj->SetArrayField(TEXT("layers"), LayerNamesArr);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape editing requires WITH_EDITOR"));
#endif
}

// =====================================================================
// sculpt_landscape_region
//
// NOTE: LANDSCAPE_ZSCALE (1/128) es la constante fija de UE para convertir
// el valor crudo uint16 del heightmap a Z de mundo. Si SetHeightData /
// GetHeightData no compilan con la firma de abajo, grepear "LandscapeEdit.h"
// -- esta API cambió de firma entre versiones 5.x.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandleSculptLandscapeRegion(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString LandscapeName;
    Params->TryGetStringField(TEXT("landscape_name"), LandscapeName);

    double CenterX = 0.0, CenterY = 0.0;
    if (!Params->TryGetNumberField(TEXT("x"), CenterX) || !Params->TryGetNumberField(TEXT("y"), CenterY))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'x'/'y' (coordenadas de quad -- llamar a get_landscape_info para el rango valido)"));
    }

    double Radius = 20.0;
    Params->TryGetNumberField(TEXT("radius"), Radius);

    double StrengthCm = 100.0;
    Params->TryGetNumberField(TEXT("strength_cm"), StrengthCm);

    FString Mode = TEXT("raise");
    Params->TryGetStringField(TEXT("mode"), Mode);
    if (Mode != TEXT("raise") && Mode != TEXT("lower") && Mode != TEXT("flatten") && Mode != TEXT("noise"))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Modo desconocido '%s' -- usar raise, lower, flatten o noise"), *Mode));
    }

    double TargetHeightCm = 0.0;
    const bool bHasTarget = Params->TryGetNumberField(TEXT("target_height_cm"), TargetHeightCm);
    if (Mode == TEXT("flatten") && !bHasTarget)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("El modo 'flatten' requiere 'target_height_cm'"));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor world"));
    }

    ALandscape* FoundLandscape = FindLandscapeByName(World, LandscapeName);
    if (!FoundLandscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No matching Landscape actor found"));
    }

    ULandscapeInfo* LandscapeInfo = FoundLandscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape has no LandscapeInfo"));
    }

    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not determine landscape extent"));
    }

    const int32 IRadius = FMath::CeilToInt(Radius);
    int32 X1 = FMath::Clamp(FMath::FloorToInt(CenterX) - IRadius, MinX, MaxX);
    int32 Y1 = FMath::Clamp(FMath::FloorToInt(CenterY) - IRadius, MinY, MaxY);
    int32 X2 = FMath::Clamp(FMath::FloorToInt(CenterX) + IRadius, MinX, MaxX);
    int32 Y2 = FMath::Clamp(FMath::FloorToInt(CenterY) + IRadius, MinY, MaxY);

    if (X2 <= X1 || Y2 <= Y1)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("La region de sculpt cae fuera del extent del landscape"));
    }

    const int32 Width = X2 - X1 + 1;
    const int32 Height = Y2 - Y1 + 1;

    const double LandscapeZScale = 1.0 / 128.0;
    const FVector ActorScale = FoundLandscape->GetActorScale3D();
    const double CmPerRawUnit = LandscapeZScale * ActorScale.Z;
    const int32 StrengthRaw = (CmPerRawUnit != 0.0) ? FMath::RoundToInt(StrengthCm / CmPerRawUnit) : 0;
    const int32 TargetRaw = (bHasTarget && CmPerRawUnit != 0.0)
        ? (32768 + FMath::RoundToInt(TargetHeightCm / CmPerRawUnit))
        : 32768;

    FLandscapeEditDataInterface EditInterface(LandscapeInfo);

    TArray<uint16> HeightData;
    HeightData.AddZeroed(Width * Height);
    EditInterface.GetHeightData(X1, Y1, X2, Y2, HeightData.GetData(), 0);

    for (int32 Y = Y1; Y <= Y2; ++Y)
    {
        for (int32 X = X1; X <= X2; ++X)
        {
            const double Dist = FVector2D((double)(X - CenterX), (double)(Y - CenterY)).Size();
            if (Dist > Radius)
            {
                continue;
            }
            const double Falloff = 1.0 - FMath::SmoothStep(0.0, Radius, Dist);
            const int32 Index = (Y - Y1) * Width + (X - X1);
            int32 Value = HeightData[Index];

            if (Mode == TEXT("raise"))
            {
                Value += FMath::RoundToInt(StrengthRaw * Falloff);
            }
            else if (Mode == TEXT("lower"))
            {
                Value -= FMath::RoundToInt(StrengthRaw * Falloff);
            }
            else if (Mode == TEXT("flatten"))
            {
                Value = FMath::RoundToInt(FMath::Lerp((double)Value, (double)TargetRaw, Falloff));
            }
            else // "noise"
            {
                const double NoiseSample = FMath::PerlinNoise2D(FVector2D(X * 0.05, Y * 0.05));
                Value += FMath::RoundToInt(StrengthRaw * Falloff * NoiseSample);
            }

            HeightData[Index] = (uint16)FMath::Clamp(Value, 0, 65535);
        }
    }

    EditInterface.SetHeightData(X1, Y1, X2, Y2, HeightData.GetData(), 0, /*bCalcNormals=*/true);
    FoundLandscape->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("landscape_name"), FoundLandscape->GetActorLabel());
    ResultObj->SetStringField(TEXT("mode"), Mode);
    ResultObj->SetNumberField(TEXT("x"), CenterX);
    ResultObj->SetNumberField(TEXT("y"), CenterY);
    ResultObj->SetNumberField(TEXT("radius"), Radius);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape editing requires WITH_EDITOR"));
#endif
}

// =====================================================================
// paint_landscape_layer
//
// NOTE: la firma exacta de GetWeightData/SetAlphaData tambien ha cambiado
// entre versiones. Si no compila, grepear "LandscapeEdit.h" buscando
// "SetAlphaData" y "GetWeightData" y ajustar solo estas dos llamadas.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandlePaintLandscapeLayer(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString LandscapeName;
    Params->TryGetStringField(TEXT("landscape_name"), LandscapeName);

    FString LayerName;
    if (!Params->TryGetStringField(TEXT("layer_name"), LayerName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'layer_name' -- llamar a get_landscape_info para ver las capas disponibles"));
    }

    double CenterX = 0.0, CenterY = 0.0;
    if (!Params->TryGetNumberField(TEXT("x"), CenterX) || !Params->TryGetNumberField(TEXT("y"), CenterY))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'x'/'y' (coordenadas de quad)"));
    }

    double Radius = 20.0;
    Params->TryGetNumberField(TEXT("radius"), Radius);

    double Strength = 1.0; // 0.0 - 1.0, peso de pintado en el centro del pincel
    Params->TryGetNumberField(TEXT("strength"), Strength);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor world"));
    }

    ALandscape* FoundLandscape = FindLandscapeByName(World, LandscapeName);
    if (!FoundLandscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No matching Landscape actor found"));
    }

    ULandscapeInfo* LandscapeInfo = FoundLandscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape has no LandscapeInfo"));
    }

    ULandscapeLayerInfoObject* TargetLayerInfo = nullptr;
    for (const FLandscapeInfoLayerSettings& LayerSettings : LandscapeInfo->Layers)
    {
        if (LayerSettings.LayerInfoObj && LayerSettings.LayerInfoObj->LayerName.ToString() == LayerName)
        {
            TargetLayerInfo = LayerSettings.LayerInfoObj;
            break;
        }
    }
    if (!TargetLayerInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Capa '%s' no encontrada en este landscape (o no tiene un LayerInfo asset asignado en la lista de target layers del editor)"), *LayerName));
    }

    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not determine landscape extent"));
    }

    const int32 IRadius = FMath::CeilToInt(Radius);
    int32 X1 = FMath::Clamp(FMath::FloorToInt(CenterX) - IRadius, MinX, MaxX);
    int32 Y1 = FMath::Clamp(FMath::FloorToInt(CenterY) - IRadius, MinY, MaxY);
    int32 X2 = FMath::Clamp(FMath::FloorToInt(CenterX) + IRadius, MinX, MaxX);
    int32 Y2 = FMath::Clamp(FMath::FloorToInt(CenterY) + IRadius, MinY, MaxY);

    if (X2 <= X1 || Y2 <= Y1)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("La region de pintado cae fuera del extent del landscape"));
    }

    const int32 Width = X2 - X1 + 1;
    const int32 Height = Y2 - Y1 + 1;

    FLandscapeEditDataInterface EditInterface(LandscapeInfo);

    TArray<uint8> WeightData;
    WeightData.AddZeroed(Width * Height);
    EditInterface.GetWeightData(TargetLayerInfo, X1, Y1, X2, Y2, WeightData.GetData(), 0);

    const double ClampedStrength = FMath::Clamp(Strength, 0.0, 1.0);
    for (int32 Y = Y1; Y <= Y2; ++Y)
    {
        for (int32 X = X1; X <= X2; ++X)
        {
            const double Dist = FVector2D((double)(X - CenterX), (double)(Y - CenterY)).Size();
            if (Dist > Radius)
            {
                continue;
            }
            const double Falloff = 1.0 - FMath::SmoothStep(0.0, Radius, Dist);
            const int32 Index = (Y - Y1) * Width + (X - X1);
            const double NewWeight = FMath::Lerp((double)WeightData[Index], 255.0 * ClampedStrength, Falloff);
            WeightData[Index] = (uint8)FMath::Clamp(NewWeight, 0.0, 255.0);
        }
    }

    EditInterface.SetAlphaData(TargetLayerInfo, X1, Y1, X2, Y2, WeightData.GetData(), 0, ELandscapeLayerPaintingRestriction::None);
    FoundLandscape->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("landscape_name"), FoundLandscape->GetActorLabel());
    ResultObj->SetStringField(TEXT("layer_name"), LayerName);
    ResultObj->SetNumberField(TEXT("x"), CenterX);
    ResultObj->SetNumberField(TEXT("y"), CenterY);
    ResultObj->SetNumberField(TEXT("radius"), Radius);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape editing requires WITH_EDITOR"));
#endif
}
