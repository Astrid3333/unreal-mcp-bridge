#include "Commands/UnrealMCPEditorCommands.h"
#include "Components/BrushComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/PostProcessVolume.h"
#include "UObject/UObjectIterator.h"
#include "LandscapeProxy.h"
#include "InstancedFoliageActor.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "PackageTools.h"
#include "EngineUtils.h"
#include "GameFramework/Volume.h"
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"

FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor") || CommandType == TEXT("create_actor"))
    {
        if (CommandType == TEXT("create_actor"))
        {
            UE_LOG(LogTemp, Warning, TEXT("'create_actor' command is deprecated and will be removed in a future version. Please use 'spawn_actor' instead."));
        }
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        return HandleGetActorProperties(Params);
    }
    else if (CommandType == TEXT("set_actor_property"))
    {
        return HandleSetActorProperty(Params);
    }
    else if (CommandType == TEXT("set_actor_material"))
    {
        return HandleSetActorMaterial(Params);
    }
    else if (CommandType == TEXT("get_actor_material"))
    {
        return HandleGetActorMaterial(Params);
    }
    else if (CommandType == TEXT("create_dynamic_material_instance"))
    {
        return HandleCreateDynamicMaterialInstance(Params);
    }
    else if (CommandType == TEXT("set_material_scalar_parameter"))
    {
        return HandleSetMaterialScalarParameter(Params);
    }
    else if (CommandType == TEXT("set_material_vector_parameter"))
    {
        return HandleSetMaterialVectorParameter(Params);
    }
    else if (CommandType == TEXT("create_material"))
    {
        return HandleCreateMaterial(Params);
    }
    else if (CommandType == TEXT("import_texture"))
    {
        return HandleImportTexture(Params);
    }
    else if (CommandType == TEXT("create_pbr_material"))
    {
        return HandleCreatePBRMaterial(Params);
    }
    else if (CommandType == TEXT("get_material_properties"))
    {
        return HandleGetMaterialProperties(Params);
    }
    else if (CommandType == TEXT("set_material_blend_mode"))
    {
        return HandleSetMaterialBlendMode(Params);
    }
    else if (CommandType == TEXT("create_moss_stone_material"))
    {
        return HandleCreateMossStoneMaterial(Params);
    }
    else if (CommandType == TEXT("duplicate_actor"))
    {
        return HandleDuplicateActor(Params);
    }
    else if (CommandType == TEXT("get_actor_bounds"))
    {
        return HandleGetActorBounds(Params);
    }
    else if (CommandType == TEXT("attach_actor_to_actor"))
    {
        return HandleAttachActorToActor(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Foliage commands
    else if (CommandType == TEXT("spawn_foliage_instances"))
    {
        return HandleSpawnFoliageInstances(Params);
    }
    // Editor viewport commands
    else if (CommandType == TEXT("focus_viewport"))
    {
        return HandleFocusViewport(Params);
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        return HandleTakeScreenshot(Params);
    }
    else if (CommandType == TEXT("line_trace"))
    {
        return HandleLineTrace(Params);
    }
    else if (CommandType == TEXT("get_gravity"))
    {
        return HandleGetGravity(Params);
    }
    else if (CommandType == TEXT("set_gravity"))
    {
        return HandleSetGravity(Params);
    }
    else if (CommandType == TEXT("apply_force"))
    {
        return HandleApplyForce(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName().Contains(Pattern) || Actor->GetActorLabel().Contains(Pattern)))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("PostProcessVolume"))
    {
        NewActor = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        // Fallback generico: buscar cualquier UClass nativa de Unreal por nombre exacto
        UClass* FoundClass = nullptr;
        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->IsChildOf(AActor::StaticClass()) && It->GetName() == ActorType)
            {
                FoundClass = *It;
                break;
            }
        }

        if (FoundClass)
        {
            NewActor = World->SpawnActor<AActor>(FoundClass, Location, Rotation, SpawnParams);

            // Los actores AVolume (NavMeshBoundsVolume, BlockingVolume, etc.) necesitan
            // un Brush real generado con un BrushBuilder. Sin esto quedan con bounds
            // (0,0,0)-(0,0,0), y GetNavigableBounds() no devuelve tiles aunque
            // build_navigation reporte exito.
            if (AVolume* NewVolume = Cast<AVolume>(NewActor))
            {
                UCubeBuilder* CubeBuilder = NewObject<UCubeBuilder>();
                CubeBuilder->X = 200.0f;
                CubeBuilder->Y = 200.0f;
                CubeBuilder->Z = 200.0f;
                UActorFactory::CreateBrushForVolumeActor(NewVolume, CubeBuilder);
            }
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
        }
    }
    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);


        // Los AVolume con Brush recien creado necesitan que se les avise
        // del cambio de geometria/transform para que GetActorBounds()
        // (y por lo tanto get_navmesh_info) no siga viendo (0,0,0).
        if (AVolume* SpawnedVolume = Cast<AVolume>(NewActor))
        {
            SpawnedVolume->PostEditChange();
            if (UBrushComponent* BrushComp = SpawnedVolume->GetBrushComponent())
            {
                BrushComp->UpdateBounds();
                BrushComp->MarkRenderStateDirty();
            }
        }
        // Return the created actor's details
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Always return detailed properties for this command
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Get property value
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }
    
    TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));
    
    // Set the property using our utility function
    FString ErrorMessage;
    if (FUnrealMCPCommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
    {
        // Property set successfully
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);
        
        // Also include the full actor details
        ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
        return ResultObj;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the blueprint
    if (BlueprintName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint name is empty"));
    }

    FString Root      = TEXT("/Game/Blueprints/");
    FString AssetPath = Root + BlueprintName;

    if (!FPackageName::DoesPackageExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found – it must reside under /Game/Blueprints"), *BlueprintName));
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get target actor name if provided
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName);

    // Get location if provided
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // Get distance
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // Get orientation if provided
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    // Get the active viewport
    FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    // Determine the focus target location (either the actor's location or the explicit location)
    FVector FocusTarget;
    if (HasTargetActor)
    {
        // Find the actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }
        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }
        FocusTarget = TargetActor->GetActorLocation();
    }
    else if (HasLocation)
    {
        FocusTarget = Location;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // Preserve the current viewing direction (camera->target), scaled to the requested Distance,
    // instead of always offsetting by a fixed (-Distance, 0, 0) which ignores the target's actual position.
    FVector CurrentCamPos = ViewportClient->GetViewLocation();
    FVector DirCamFromTarget = CurrentCamPos - FocusTarget;
    if (DirCamFromTarget.IsNearlyZero())
    {
        // Camera is already at (or extremely close to) the target; fall back to a default offset direction.
        DirCamFromTarget = FVector(-1.0f, 0.0f, 0.0f);
    }
    DirCamFromTarget.Normalize();
    FVector NewCamPos = FocusTarget + DirCamFromTarget * Distance;
    ViewportClient->SetViewLocation(NewCamPos);

    // Set orientation: explicit value if provided, otherwise auto-compute a look-at rotation
    // toward the focus target so the camera actually faces what it's focusing on.
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }
    else
    {
        FVector LookDir = (FocusTarget - NewCamPos).GetSafeNormal();
        FRotator LookAtRotation = LookDir.Rotation();
        ViewportClient->SetViewRotation(LookAtRotation);
    }

    // Force viewport to redraw
    ViewportClient->Invalidate();
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get file path parameter
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' parameter"));
    }
    
    // Ensure the file path has a proper extension
    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    // Get the active viewport
    if (GEditor && GEditor->GetActiveViewport())
    {
        FViewport* Viewport = GEditor->GetActiveViewport();
        TArray<FColor> Bitmap;
        FIntRect ViewportRect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
        
        if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
        {
            TArray<uint8> CompressedBitmap;
            FImageUtils::CompressImageArray(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, Bitmap, CompressedBitmap);
            
            if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("filepath"), FilePath);
                return ResultObj;
            }
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to take screenshot"));
} 

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnFoliageInstances(const TSharedPtr<FJsonObject>& Params)
{
    FString MeshPath;
    if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
    }
    FString LandscapeName;
    if (!Params->TryGetStringField(TEXT("landscape_name"), LandscapeName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'landscape_name' parameter"));
    }
    int32 Count = 100;
    Params->TryGetNumberField(TEXT("count"), Count);
    double RegionCenterX = 0.0, RegionCenterY = 0.0;
    const TArray<TSharedPtr<FJsonValue>>* CenterArray;
    if (Params->TryGetArrayField(TEXT("region_center"), CenterArray) && CenterArray->Num() >= 2)
    {
        RegionCenterX = (*CenterArray)[0]->AsNumber();
        RegionCenterY = (*CenterArray)[1]->AsNumber();
    }
    double RegionRadius = 5000.0;
    Params->TryGetNumberField(TEXT("region_radius"), RegionRadius);
    double MinScale = 0.8, MaxScale = 1.5;
    Params->TryGetNumberField(TEXT("min_scale"), MinScale);
    Params->TryGetNumberField(TEXT("max_scale"), MaxScale);

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == LandscapeName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Landscape not found: %s"), *LandscapeName));
    }
    ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(TargetActor);
    if (!Landscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' is not a Landscape"), *LandscapeName));
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!Mesh)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath));
    }
    UWorld* World = GWorld;

    UFoliageType_InstancedStaticMesh* FoliageType = NewObject<UFoliageType_InstancedStaticMesh>(GetTransientPackage());
    FoliageType->SetStaticMesh(Mesh);
    AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(World->GetCurrentLevel(), /*bCreateIfNone=*/true);
    if (!IFA)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get or create InstancedFoliageActor for level"));
    }
    FFoliageInfo* FoliageInfo = IFA->FindOrAddMesh(FoliageType);
    if (!FoliageInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add foliage type"));
    }
    int32 SpawnedCount = 0;
    for (int32 i = 0; i < Count; i++)
    {
        FVector2D RandomOffset = FMath::RandPointInCircle(RegionRadius);
        double SampleX = RegionCenterX + RandomOffset.X;
        double SampleY = RegionCenterY + RandomOffset.Y;
        FVector TraceStart(SampleX, SampleY, 100000.0);
        FVector TraceEnd(SampleX, SampleY, -100000.0);
        FHitResult Hit;
        FCollisionQueryParams QueryParams;
        QueryParams.bTraceComplex = true;
        if (World->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd,
            FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams))
        {
            FFoliageInstance Instance;
            Instance.Location = Hit.Location;
            Instance.Rotation = FRotator(0, FMath::RandRange(0.f, 360.f), 0).Quaternion().Rotator();
            float RandomScale = FMath::RandRange((float)MinScale, (float)MaxScale);
            Instance.DrawScale3D = FVector3f(RandomScale);
            FoliageInfo->AddInstance(FoliageType, Instance);
            SpawnedCount++;
        }
    }
    FoliageInfo->Refresh(true, false);
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("landscape"), LandscapeName);
    ResultObj->SetStringField(TEXT("mesh"), MeshPath);
    ResultObj->SetNumberField(TEXT("requested_count"), Count);
    ResultObj->SetNumberField(TEXT("spawned_count"), SpawnedCount);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }
    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UStaticMeshComponent* MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName));
    }

    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    MeshComp->SetMaterial(SlotIndex, Material);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("material"), MaterialPath);
    ResultObj->SetNumberField(TEXT("slot_index"), SlotIndex);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// Alta prioridad — Materiales
// =====================================================================

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UStaticMeshComponent* MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName));
    }

    UMaterialInterface* Material = MeshComp->GetMaterial(SlotIndex);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetNumberField(TEXT("slot_index"), SlotIndex);
    ResultObj->SetStringField(TEXT("material_path"), Material ? Material->GetPathName() : TEXT(""));
    ResultObj->SetBoolField(TEXT("is_dynamic_instance"), Material ? Material->IsA<UMaterialInstanceDynamic>() : false);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCreateDynamicMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UStaticMeshComponent* MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName));
    }

    UMaterialInstanceDynamic* DynMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(SlotIndex);
    if (!DynMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create dynamic material instance (check the slot has a valid parent material)"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetNumberField(TEXT("slot_index"), SlotIndex);
    ResultObj->SetStringField(TEXT("dynamic_instance_name"), DynMaterial->GetName());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetMaterialScalarParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString ParamName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParamName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }
    double Value = 0.0;
    if (!Params->TryGetNumberField(TEXT("value"), Value))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }
    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UStaticMeshComponent* MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName));
    }

    // Si el slot todavía no es una instancia dinámica, la promovemos automáticamente
    // asi el tool funciona directo sobre un actor recien spawneado sin pasos previos.
    UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(MeshComp->GetMaterial(SlotIndex));
    if (!DynMaterial)
    {
        DynMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(SlotIndex);
    }
    if (!DynMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get or create a dynamic material instance for this slot"));
    }

    DynMaterial->SetScalarParameterValue(FName(*ParamName), static_cast<float>(Value));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("parameter"), ParamName);
    ResultObj->SetNumberField(TEXT("value"), Value);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetMaterialVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString ParamName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParamName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }
    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    const TSharedPtr<FJsonObject>* ValueObj;
    if (!Params->TryGetObjectField(TEXT("value"), ValueObj))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'value' must be an object like {\"r\":1.0,\"g\":0.0,\"b\":0.0,\"a\":1.0}"));
    }
    double R = 0.0, G = 0.0, B = 0.0, A = 1.0;
    (*ValueObj)->TryGetNumberField(TEXT("r"), R);
    (*ValueObj)->TryGetNumberField(TEXT("g"), G);
    (*ValueObj)->TryGetNumberField(TEXT("b"), B);
    (*ValueObj)->TryGetNumberField(TEXT("a"), A);
    FLinearColor ColorValue(R, G, B, A);

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UStaticMeshComponent* MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName));
    }

    UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(MeshComp->GetMaterial(SlotIndex));
    if (!DynMaterial)
    {
        DynMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(SlotIndex);
    }
    if (!DynMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get or create a dynamic material instance for this slot"));
    }

    DynMaterial->SetVectorParameterValue(FName(*ParamName), ColorValue);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("parameter"), ParamName);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// Alta prioridad — Actores
// =====================================================================

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
    FString SourceActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), SourceActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString NewActorName;
    Params->TryGetStringField(TEXT("new_name"), NewActorName);

    AActor* SourceActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == SourceActorName)
        {
            SourceActor = Actor;
            break;
        }
    }
    if (!SourceActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *SourceActorName));
    }

    FVector Offset = FVector::ZeroVector;
    if (Params->HasField(TEXT("location_offset")))
    {
        Offset = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location_offset"));
    }

    FTransform NewTransform = SourceActor->GetActorTransform();
    NewTransform.SetLocation(NewTransform.GetLocation() + Offset);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* NewActor = GWorld->SpawnActor<AActor>(SourceActor->GetClass(), NewTransform, SpawnParams);
    if (!NewActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn duplicated actor"));
    }

    // Copia mesh, materiales y demas propiedades del actor original (mismo mecanismo
    // que usa el editor internamente al duplicar objetos no relacionados por herencia directa)
    UEngine::CopyPropertiesForUnrelatedObjects(SourceActor, NewActor);
    NewActor->SetActorTransform(NewTransform);

    FString FinalLabel = NewActorName.IsEmpty() ? (SourceActorName + TEXT("_Copy")) : NewActorName;
    NewActor->SetActorLabel(FinalLabel);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("source_actor"), SourceActorName);
    ResultObj->SetStringField(TEXT("new_actor"), NewActor->GetActorLabel());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorBounds(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    FVector Origin, BoxExtent;
    TargetActor->GetActorBounds(false, Origin, BoxExtent);

    TSharedPtr<FJsonObject> OriginObj = MakeShared<FJsonObject>();
    OriginObj->SetNumberField(TEXT("x"), Origin.X);
    OriginObj->SetNumberField(TEXT("y"), Origin.Y);
    OriginObj->SetNumberField(TEXT("z"), Origin.Z);

    TSharedPtr<FJsonObject> ExtentObj = MakeShared<FJsonObject>();
    ExtentObj->SetNumberField(TEXT("x"), BoxExtent.X);
    ExtentObj->SetNumberField(TEXT("y"), BoxExtent.Y);
    ExtentObj->SetNumberField(TEXT("z"), BoxExtent.Z);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetObjectField(TEXT("origin"), OriginObj);
    ResultObj->SetObjectField(TEXT("box_extent"), ExtentObj);
    ResultObj->SetNumberField(TEXT("min_z"), Origin.Z - BoxExtent.Z);
    ResultObj->SetNumberField(TEXT("max_z"), Origin.Z + BoxExtent.Z);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleAttachActorToActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ChildName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ChildName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter (the actor to attach)"));
    }
    FString ParentName;
    if (!Params->TryGetStringField(TEXT("parent_actor_name"), ParentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_actor_name' parameter"));
    }
    FString SocketName;
    Params->TryGetStringField(TEXT("socket_name"), SocketName);

    // KeepRelative | KeepWorld | SnapToTarget — default KeepRelative
    FString RuleString = TEXT("KeepRelative");
    Params->TryGetStringField(TEXT("attachment_rule"), RuleString);

    EAttachmentRule Rule = EAttachmentRule::KeepRelative;
    if (RuleString == TEXT("KeepWorld"))
    {
        Rule = EAttachmentRule::KeepWorld;
    }
    else if (RuleString == TEXT("SnapToTarget"))
    {
        Rule = EAttachmentRule::SnapToTarget;
    }

    AActor* ChildActor = nullptr;
    AActor* ParentActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;
        if (Actor->GetName() == ChildName) { ChildActor = Actor; }
        if (Actor->GetName() == ParentName) { ParentActor = Actor; }
    }
    if (!ChildActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ChildName));
    }
    if (!ParentActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent actor not found: %s"), *ParentName));
    }

    FAttachmentTransformRules TransformRules(Rule, Rule, Rule, false);
    bool bSuccess = ChildActor->AttachToActor(ParentActor, TransformRules, FName(*SocketName));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ChildName);
    ResultObj->SetStringField(TEXT("parent"), ParentName);
    ResultObj->SetStringField(TEXT("attachment_rule"), RuleString);
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    return ResultObj;
}


TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString FolderPath = TEXT("/Game/Materials");
    Params->TryGetStringField(TEXT("path"), FolderPath);

    // Color base (default gris neutro)
    FLinearColor BaseColorValue(0.5f, 0.5f, 0.5f, 1.0f);
    const TArray<TSharedPtr<FJsonValue>>* ColorArray;
    if (Params->TryGetArrayField(TEXT("base_color"), ColorArray) && ColorArray->Num() >= 3)
    {
        BaseColorValue.R = (*ColorArray)[0]->AsNumber();
        BaseColorValue.G = (*ColorArray)[1]->AsNumber();
        BaseColorValue.B = (*ColorArray)[2]->AsNumber();
    }

    double RoughnessValue = 0.5;
    Params->TryGetNumberField(TEXT("roughness"), RoughnessValue);

    double MetallicValue = 0.0;
    Params->TryGetNumberField(TEXT("metallic"), MetallicValue);

    FString PackageName = FolderPath / MaterialName;
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for material"));
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
        UMaterial::StaticClass(), Package, FName(*MaterialName), RF_Standalone | RF_Public, nullptr, GWarn));

    if (!NewMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material asset"));
    }

    // Parametro de color base (queda editable, no hardcodeado)
    UMaterialExpressionVectorParameter* ColorParam = NewObject<UMaterialExpressionVectorParameter>(NewMaterial);
    ColorParam->ParameterName = FName(TEXT("BaseColor"));
    ColorParam->DefaultValue = BaseColorValue;
    NewMaterial->GetExpressionCollection().AddExpression(ColorParam);
    NewMaterial->GetEditorOnlyData()->BaseColor.Expression = ColorParam;

    // Parametro de roughness
    UMaterialExpressionScalarParameter* RoughnessParam = NewObject<UMaterialExpressionScalarParameter>(NewMaterial);
    RoughnessParam->ParameterName = FName(TEXT("Roughness"));
    RoughnessParam->DefaultValue = RoughnessValue;
    NewMaterial->GetExpressionCollection().AddExpression(RoughnessParam);
    NewMaterial->GetEditorOnlyData()->Roughness.Expression = RoughnessParam;

    // Parametro de metallic
    UMaterialExpressionScalarParameter* MetallicParam = NewObject<UMaterialExpressionScalarParameter>(NewMaterial);
    MetallicParam->ParameterName = FName(TEXT("Metallic"));
    MetallicParam->DefaultValue = MetallicValue;
    NewMaterial->GetExpressionCollection().AddExpression(MetallicParam);
    NewMaterial->GetEditorOnlyData()->Metallic.Expression = MetallicParam;

    NewMaterial->PreEditChange(nullptr);
    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewMaterial);

    // Guardar el .uasset a disco para que sobreviva un reinicio del editor
    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, NewMaterial, *PackageFileName, SaveArgs);

    // Asignacion opcional directa a un actor
    FString AssignToActor;
    bool bAssigned = false;
    if (Params->TryGetStringField(TEXT("assign_to_actor"), AssignToActor) && !AssignToActor.IsEmpty())
    {
        for (TActorIterator<AActor> It(GWorld); It; ++It)
        {
            if (It->GetName() == AssignToActor)
            {
                int32 SlotIndex = 0;
                double SlotIndexNum;
                if (Params->TryGetNumberField(TEXT("slot_index"), SlotIndexNum))
                {
                    SlotIndex = (int32)SlotIndexNum;
                }
                TArray<UActorComponent*> Components;
                It->GetComponents(UStaticMeshComponent::StaticClass(), Components);
                for (UActorComponent* Comp : Components)
                {
                    if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Comp))
                    {
                        MeshComp->SetMaterial(SlotIndex, NewMaterial);
                        bAssigned = true;
                    }
                }
                break;
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), NewMaterial->GetPathName());
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("assigned_to_actor"), bAssigned);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetMaterialProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    UMaterialInterface* MaterialInterface = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!MaterialInterface)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to load material at '%s'"), *MaterialPath));
    }

    EBlendMode CurrentBlendMode = MaterialInterface->GetBlendMode();

    FMaterialShadingModelField ShadingModels = MaterialInterface->GetShadingModels();
    EMaterialShadingModel FirstShadingModel = MSM_DefaultLit;
    for (int32 i = 0; i < MSM_NUM; ++i)
    {
        if (ShadingModels.HasShadingModel((EMaterialShadingModel)i))
        {
            FirstShadingModel = (EMaterialShadingModel)i;
            break;
        }
    }

    bool bIsInstance = MaterialInterface->IsA<UMaterialInstance>();

    float OpacityParam = 1.0f;
    MaterialInterface->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Opacity")), OpacityParam);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("blend_mode"), UEnum::GetValueAsString(CurrentBlendMode));
    ResultObj->SetStringField(TEXT("shading_model"), UEnum::GetValueAsString(FirstShadingModel));
    ResultObj->SetBoolField(TEXT("is_material_instance"), bIsInstance);
    ResultObj->SetNumberField(TEXT("opacity_param"), OpacityParam);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetMaterialBlendMode(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    FString BlendModeStr;
    if (!Params->TryGetStringField(TEXT("blend_mode"), BlendModeStr))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blend_mode' parameter"));
    }

    FString B = BlendModeStr.ToLower();
    EBlendMode NewBlendMode;
    if (B == TEXT("opaque"))              NewBlendMode = BLEND_Opaque;
    else if (B == TEXT("masked"))         NewBlendMode = BLEND_Masked;
    else if (B == TEXT("translucent"))    NewBlendMode = BLEND_Translucent;
    else if (B == TEXT("additive"))       NewBlendMode = BLEND_Additive;
    else if (B == TEXT("modulate"))       NewBlendMode = BLEND_Modulate;
    else if (B == TEXT("alphacomposite")) NewBlendMode = BLEND_AlphaComposite;
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Unknown blend_mode '%s'. Valid: opaque, masked, translucent, additive, modulate, alphacomposite"),
            *BlendModeStr));
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Failed to load UMaterial en '%s' (si es un Material Instance, esta funcion no aplica)"),
            *MaterialPath));
    }

    EBlendMode OldBlendMode = Material->GetBlendMode();

    Material->BlendMode = NewBlendMode;
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    UPackage* Package = Material->GetOutermost();
    FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, Material, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("old_blend_mode"), UEnum::GetValueAsString(OldBlendMode));
    ResultObj->SetStringField(TEXT("new_blend_mode"), UEnum::GetValueAsString(NewBlendMode));
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}


TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCreateMossStoneMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString FolderPath = TEXT("/Game/Materials");
    Params->TryGetStringField(TEXT("path"), FolderPath);

    FLinearColor StoneColorValue(0.5f, 0.5f, 0.45f, 1.0f);
    const TArray<TSharedPtr<FJsonValue>>* StoneColorArray;
    if (Params->TryGetArrayField(TEXT("stone_color"), StoneColorArray) && StoneColorArray->Num() >= 3)
    {
        StoneColorValue.R = (*StoneColorArray)[0]->AsNumber();
        StoneColorValue.G = (*StoneColorArray)[1]->AsNumber();
        StoneColorValue.B = (*StoneColorArray)[2]->AsNumber();
    }

    FLinearColor MossColorValue(0.15f, 0.35f, 0.12f, 1.0f);
    const TArray<TSharedPtr<FJsonValue>>* MossColorArray;
    if (Params->TryGetArrayField(TEXT("moss_color"), MossColorArray) && MossColorArray->Num() >= 3)
    {
        MossColorValue.R = (*MossColorArray)[0]->AsNumber();
        MossColorValue.G = (*MossColorArray)[1]->AsNumber();
        MossColorValue.B = (*MossColorArray)[2]->AsNumber();
    }

    double RoughnessValue = 0.8;
    Params->TryGetNumberField(TEXT("roughness"), RoughnessValue);

    double MossAmountValue = 0.4;
    Params->TryGetNumberField(TEXT("moss_amount"), MossAmountValue);

    double NoiseScaleValue = 20.0;
    Params->TryGetNumberField(TEXT("noise_scale"), NoiseScaleValue);

    FString PackageName = FolderPath / MaterialName;
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for material"));
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
        UMaterial::StaticClass(), Package, FName(*MaterialName), RF_Standalone | RF_Public, nullptr, GWarn));

    if (!NewMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material asset"));
    }

    // -- Parametros expuestos --
    UMaterialExpressionVectorParameter* StoneColorParam = NewObject<UMaterialExpressionVectorParameter>(NewMaterial);
    StoneColorParam->ParameterName = FName(TEXT("StoneColor"));
    StoneColorParam->DefaultValue = StoneColorValue;
    NewMaterial->GetExpressionCollection().AddExpression(StoneColorParam);

    UMaterialExpressionVectorParameter* MossColorParam = NewObject<UMaterialExpressionVectorParameter>(NewMaterial);
    MossColorParam->ParameterName = FName(TEXT("MossColor"));
    MossColorParam->DefaultValue = MossColorValue;
    NewMaterial->GetExpressionCollection().AddExpression(MossColorParam);

    UMaterialExpressionScalarParameter* RoughnessParam = NewObject<UMaterialExpressionScalarParameter>(NewMaterial);
    RoughnessParam->ParameterName = FName(TEXT("Roughness"));
    RoughnessParam->DefaultValue = RoughnessValue;
    NewMaterial->GetExpressionCollection().AddExpression(RoughnessParam);

    UMaterialExpressionScalarParameter* MossAmountParam = NewObject<UMaterialExpressionScalarParameter>(NewMaterial);
    MossAmountParam->ParameterName = FName(TEXT("MossAmount"));
    MossAmountParam->DefaultValue = MossAmountValue;
    NewMaterial->GetExpressionCollection().AddExpression(MossAmountParam);

    // -- Mask: normal hacia arriba . (0,0,1), clampeada 0..1 --
    UMaterialExpressionVertexNormalWS* NormalWS = NewObject<UMaterialExpressionVertexNormalWS>(NewMaterial);
    NewMaterial->GetExpressionCollection().AddExpression(NormalWS);

    UMaterialExpressionConstant3Vector* UpVector = NewObject<UMaterialExpressionConstant3Vector>(NewMaterial);
    UpVector->Constant = FLinearColor(0.0f, 0.0f, 1.0f);
    NewMaterial->GetExpressionCollection().AddExpression(UpVector);

    UMaterialExpressionDotProduct* NdotUp = NewObject<UMaterialExpressionDotProduct>(NewMaterial);
    NdotUp->A.Expression = NormalWS;
    NdotUp->B.Expression = UpVector;
    NewMaterial->GetExpressionCollection().AddExpression(NdotUp);

    UMaterialExpressionClamp* UpMask = NewObject<UMaterialExpressionClamp>(NewMaterial);
    UpMask->Input.Expression = NdotUp;
    UpMask->MinDefault = 0.0f;
    UpMask->MaxDefault = 1.0f;
    NewMaterial->GetExpressionCollection().AddExpression(UpMask);

    // -- Ruido: conectamos EXPLICITAMENTE WorldPosition al pin Position.
    // No confiar en un supuesto default a World Position cuando el pin
    // queda sin conectar -- eso fue lo que causaba que el ruido saliera
    // constante (sin grano, solo el blend base por normal de cara).
    UMaterialExpressionWorldPosition* WorldPosNode = NewObject<UMaterialExpressionWorldPosition>(NewMaterial);
    NewMaterial->GetExpressionCollection().AddExpression(WorldPosNode);

    UMaterialExpressionNoise* NoiseNode = NewObject<UMaterialExpressionNoise>(NewMaterial);
    NoiseNode->Position.Expression = WorldPosNode;
    NoiseNode->Scale = NoiseScaleValue;
    NoiseNode->OutputMin = 0.0f;
    NoiseNode->OutputMax = 1.0f;
    NewMaterial->GetExpressionCollection().AddExpression(NoiseNode);

    // -- Combinar: UpMask * Noise * MossAmount --
    UMaterialExpressionMultiply* MaskTimesNoise = NewObject<UMaterialExpressionMultiply>(NewMaterial);
    MaskTimesNoise->A.Expression = UpMask;
    MaskTimesNoise->B.Expression = NoiseNode;
    NewMaterial->GetExpressionCollection().AddExpression(MaskTimesNoise);

    UMaterialExpressionMultiply* FinalMask = NewObject<UMaterialExpressionMultiply>(NewMaterial);
    FinalMask->A.Expression = MaskTimesNoise;
    FinalMask->B.Expression = MossAmountParam;
    NewMaterial->GetExpressionCollection().AddExpression(FinalMask);

    // -- Lerp(StoneColor, MossColor, FinalMask) -> BaseColor --
    UMaterialExpressionLinearInterpolate* ColorLerp = NewObject<UMaterialExpressionLinearInterpolate>(NewMaterial);
    ColorLerp->A.Expression = StoneColorParam;
    ColorLerp->B.Expression = MossColorParam;
    ColorLerp->Alpha.Expression = FinalMask;
    NewMaterial->GetExpressionCollection().AddExpression(ColorLerp);

    NewMaterial->GetEditorOnlyData()->BaseColor.Expression = ColorLerp;
    NewMaterial->GetEditorOnlyData()->Roughness.Expression = RoughnessParam;

    NewMaterial->PreEditChange(nullptr);
    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewMaterial);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, NewMaterial, *PackageFileName, SaveArgs);

    FString AssignToActor;
    bool bAssigned = false;
    if (Params->TryGetStringField(TEXT("assign_to_actor"), AssignToActor) && !AssignToActor.IsEmpty())
    {
        int32 SlotIndex = 0;
        double SlotIndexNum;
        if (Params->TryGetNumberField(TEXT("slot_index"), SlotIndexNum))
        {
            SlotIndex = (int32)SlotIndexNum;
        }
        for (TActorIterator<AActor> It(GWorld); It; ++It)
        {
            if (It->GetName() == AssignToActor)
            {
                TArray<UActorComponent*> Components;
                It->GetComponents(UStaticMeshComponent::StaticClass(), Components);
                for (UActorComponent* Comp : Components)
                {
                    if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Comp))
                    {
                        MeshComp->SetMaterial(SlotIndex, NewMaterial);
                        bAssigned = true;
                    }
                }
                break;
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), NewMaterial->GetPathName());
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("assigned_to_actor"), bAssigned);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}


TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleImportTexture(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
    }
    if (!FPaths::FileExists(SourcePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Archivo no encontrado: %s"), *SourcePath));
    }

    FString TextureName = FPaths::GetBaseFilename(SourcePath);
    Params->TryGetStringField(TEXT("name"), TextureName);

    FString FolderPath = TEXT("/Game/Textures");
    Params->TryGetStringField(TEXT("path"), FolderPath);

    bool bSRGB = true;
    Params->TryGetBoolField(TEXT("srgb"), bSRGB);

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
    ImportTask->Filename = SourcePath;
    ImportTask->DestinationPath = FolderPath;
    ImportTask->DestinationName = TextureName;
    ImportTask->bAutomated = true;
    ImportTask->bSave = true;
    ImportTask->bReplaceExisting = true;

    TArray<UAssetImportTask*> Tasks = { ImportTask };
    AssetTools.ImportAssetTasks(Tasks);

    if (ImportTask->GetObjects().Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Fallo al importar '%s' -- revisa que el formato sea soportado (png/jpg/tga/exr/hdr)"), *SourcePath));
    }

    UTexture2D* NewTexture = Cast<UTexture2D>(ImportTask->GetObjects()[0]);
    if (!NewTexture)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("El asset importado no es un UTexture2D"));
    }

    NewTexture->SRGB = bSRGB;
    NewTexture->PreEditChange(nullptr);
    NewTexture->PostEditChange();
    NewTexture->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("texture_path"), NewTexture->GetPathName());
    ResultObj->SetNumberField(TEXT("width"), NewTexture->GetSizeX());
    ResultObj->SetNumberField(TEXT("height"), NewTexture->GetSizeY());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCreatePBRMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString FolderPath = TEXT("/Game/Materials");
    Params->TryGetStringField(TEXT("path"), FolderPath);

    FString PackageName = FolderPath / MaterialName;
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for material"));
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
        UMaterial::StaticClass(), Package, FName(*MaterialName), RF_Standalone | RF_Public, nullptr, GWarn));
    if (!NewMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material asset"));
    }

    TArray<FString> Wired;

    auto MakeSample = [&](const FString& TexPath, EMaterialSamplerType SamplerType) -> UMaterialExpressionTextureSample*
    {
        if (TexPath.IsEmpty()) return nullptr;
        UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *TexPath);
        if (!Tex) return nullptr;
        UMaterialExpressionTextureSample* Sample = NewObject<UMaterialExpressionTextureSample>(NewMaterial);
        Sample->Texture = Tex;
        Sample->SamplerType = SamplerType;
        NewMaterial->GetExpressionCollection().AddExpression(Sample);
        return Sample;
    };

    FString BaseColorPath, NormalPath, RoughnessPath, MetallicPath, AOPath;
    Params->TryGetStringField(TEXT("base_color_texture"), BaseColorPath);
    Params->TryGetStringField(TEXT("normal_texture"), NormalPath);
    Params->TryGetStringField(TEXT("roughness_texture"), RoughnessPath);
    Params->TryGetStringField(TEXT("metallic_texture"), MetallicPath);
    Params->TryGetStringField(TEXT("ao_texture"), AOPath);

    if (UMaterialExpressionTextureSample* S = MakeSample(BaseColorPath, SAMPLERTYPE_Color))
    {
        NewMaterial->GetEditorOnlyData()->BaseColor.Expression = S;
        Wired.Add(TEXT("base_color"));
    }
    if (UMaterialExpressionTextureSample* S = MakeSample(NormalPath, SAMPLERTYPE_Normal))
    {
        NewMaterial->GetEditorOnlyData()->Normal.Expression = S;
        Wired.Add(TEXT("normal"));
    }
    if (UMaterialExpressionTextureSample* S = MakeSample(RoughnessPath, SAMPLERTYPE_LinearColor))
    {
        NewMaterial->GetEditorOnlyData()->Roughness.Expression = S;
        NewMaterial->GetEditorOnlyData()->Roughness.OutputIndex = 1;
        Wired.Add(TEXT("roughness"));
    }
    if (UMaterialExpressionTextureSample* S = MakeSample(MetallicPath, SAMPLERTYPE_LinearColor))
    {
        NewMaterial->GetEditorOnlyData()->Metallic.Expression = S;
        NewMaterial->GetEditorOnlyData()->Metallic.OutputIndex = 1;
        Wired.Add(TEXT("metallic"));
    }
    if (UMaterialExpressionTextureSample* S = MakeSample(AOPath, SAMPLERTYPE_LinearColor))
    {
        NewMaterial->GetEditorOnlyData()->AmbientOcclusion.Expression = S;
        NewMaterial->GetEditorOnlyData()->AmbientOcclusion.OutputIndex = 1;
        Wired.Add(TEXT("ao"));
    }

    if (Wired.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Ninguna textura valida -- pasa al menos base_color_texture con un path ya importado (/Game/Textures/...)"));
    }

    NewMaterial->PreEditChange(nullptr);
    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewMaterial);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, NewMaterial, *PackageFileName, SaveArgs);

    FString AssignToActor;
    bool bAssigned = false;
    if (Params->TryGetStringField(TEXT("assign_to_actor"), AssignToActor) && !AssignToActor.IsEmpty())
    {
        int32 SlotIndex = 0;
        double SlotIndexNum;
        if (Params->TryGetNumberField(TEXT("slot_index"), SlotIndexNum)) SlotIndex = (int32)SlotIndexNum;

        for (TActorIterator<AActor> It(GWorld); It; ++It)
        {
            if (It->GetName() == AssignToActor || It->GetActorLabel() == AssignToActor)
            {
                TArray<UActorComponent*> Components;
                It->GetComponents(UStaticMeshComponent::StaticClass(), Components);
                for (UActorComponent* Comp : Components)
                {
                    if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Comp))
                    {
                        MeshComp->SetMaterial(SlotIndex, NewMaterial);
                        bAssigned = true;
                    }
                }
                break;
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), NewMaterial->GetPathName());
    TArray<TSharedPtr<FJsonValue>> WiredArr;
    for (const FString& W : Wired) WiredArr.Add(MakeShared<FJsonValueString>(W));
    ResultObj->SetArrayField(TEXT("channels_wired"), WiredArr);
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("assigned_to_actor"), bAssigned);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// ---------------------------------------------------------------------------
// Fase 2: consultas y fisica en runtime (line_trace, gravity, force)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleLineTrace(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params->HasField(TEXT("start")) || !Params->HasField(TEXT("end")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'start'/'end' parameters ([x,y,z] each)"));
    }
    const FVector Start = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("start"));
    const FVector End = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("end"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FString ChannelName = TEXT("visibility");
    Params->TryGetStringField(TEXT("channel"), ChannelName);
    ECollisionChannel Channel = ECC_Visibility;
    if (ChannelName == TEXT("world_static")) Channel = ECC_WorldStatic;
    else if (ChannelName == TEXT("world_dynamic")) Channel = ECC_WorldDynamic;
    else if (ChannelName == TEXT("physics")) Channel = ECC_PhysicsBody;
    else if (ChannelName == TEXT("camera")) Channel = ECC_Camera;
    else if (ChannelName != TEXT("visibility"))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown channel '%s' (use visibility|world_static|world_dynamic|physics|camera)"), *ChannelName));
    }

    bool bTraceComplex = false;
    Params->TryGetBoolField(TEXT("trace_complex"), bTraceComplex);

    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MCPLineTrace), bTraceComplex);
    const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, Channel, QueryParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("blocking_hit"), bHit);
    ResultObj->SetStringField(TEXT("channel"), ChannelName);
    ResultObj->SetNumberField(TEXT("distance"), (End - Start).Size());
    if (bHit)
    {
        ResultObj->SetStringField(TEXT("actor"), Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT(""));
        ResultObj->SetStringField(TEXT("component"), Hit.GetComponent() ? Hit.GetComponent()->GetName() : TEXT(""));
        ResultObj->SetNumberField(TEXT("distance_hit"), Hit.Distance);
        TSharedPtr<FJsonObject> Loc = MakeShared<FJsonObject>();
        Loc->SetNumberField(TEXT("x"), Hit.Location.X); Loc->SetNumberField(TEXT("y"), Hit.Location.Y); Loc->SetNumberField(TEXT("z"), Hit.Location.Z);
        ResultObj->SetObjectField(TEXT("location"), Loc);
        TSharedPtr<FJsonObject> Norm = MakeShared<FJsonObject>();
        Norm->SetNumberField(TEXT("x"), Hit.ImpactNormal.X); Norm->SetNumberField(TEXT("y"), Hit.ImpactNormal.Y); Norm->SetNumberField(TEXT("z"), Hit.ImpactNormal.Z);
        ResultObj->SetObjectField(TEXT("normal"), Norm);
        TSharedPtr<FJsonObject> PhysMat = MakeShared<FJsonObject>();
        if (Hit.PhysMaterial.IsValid())
        {
            PhysMat->SetStringField(TEXT("name"), Hit.PhysMaterial->GetName());
            ResultObj->SetObjectField(TEXT("physical_material"), PhysMat);
        }
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetGravity(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }
    AWorldSettings* Settings = World->GetWorldSettings();
    if (!Settings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get world settings"));
    }
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetNumberField(TEXT("gravity_z"), Settings->GetGravityZ());
    ResultObj->SetBoolField(TEXT("override_global"), Settings->bGlobalGravitySet != 0);
    if (Settings->bGlobalGravitySet)
    {
        ResultObj->SetNumberField(TEXT("global_gravity_z"), Settings->GlobalGravityZ);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetGravity(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params->HasField(TEXT("gravity_z")) && !Params->HasField(TEXT("gravity")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'gravity_z' (float, cm/s^2, ej. -980 default, -1620 Luna, -3720 Marte)"));
    }
    double Value = 0.0;
    if (Params->HasField(TEXT("gravity_z"))) Value = Params->GetNumberField(TEXT("gravity_z"));
    else Value = Params->GetNumberField(TEXT("gravity"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }
    AWorldSettings* Settings = World->GetWorldSettings();
    if (!Settings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get world settings"));
    }

    Settings->Modify();
    Settings->bGlobalGravitySet = true;
    Settings->GlobalGravityZ = (float)Value;
    Settings->bWorldGravitySet = true;
    Settings->WorldGravityZ = (float)Value;

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetNumberField(TEXT("gravity_z"), Settings->GetGravityZ());
    ResultObj->SetStringField(TEXT("note"), TEXT("Afecta a cuerpos con SimulatePhysics (en editor: selecciona y pulsa Simulate; en PIE funciona al instante)."));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleApplyForce(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
        }
    }
    if (!Params->HasField(TEXT("force")) && !Params->HasField(TEXT("impulse")) && !Params->HasField(TEXT("velocity_change")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'force'|'impulse'|'velocity_change' ([x,y,z])"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    TArray<UPrimitiveComponent*> Prims;
    TargetActor->GetComponents<UPrimitiveComponent>(Prims);
    UPrimitiveComponent* Prim = nullptr;
    for (UPrimitiveComponent* C : Prims)
    {
        if (C && C->IsSimulatingPhysics())
        {
            Prim = C;
            break;
        }
    }
    bool bEnablePhysics = false;
    Params->TryGetBoolField(TEXT("enable_physics"), bEnablePhysics);
    if (!Prim && bEnablePhysics)
    {
        for (UPrimitiveComponent* C : Prims)
        {
            if (C && C->IsRegistered())
            {
                C->SetSimulatePhysics(true);
                Prim = C;
                break;
            }
        }
    }
    if (!Prim)
    {
        TArray<FString> Names;
        for (UPrimitiveComponent* C : Prims) if (C) Names.Add(C->GetName());
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Ningun componente de '%s' simula fisica. Componentes: [%s]. Pasa enable_physics=true para activarlo."),
                *ActorName, *FString::Join(Names, TEXT(", "))));
    }

    FVector Force(0, 0, 0);
    FString Mode = TEXT("force");
    if (Params->HasField(TEXT("force"))) { Force = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("force")); Mode = TEXT("force"); }
    else if (Params->HasField(TEXT("impulse"))) { Force = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("impulse")); Mode = TEXT("impulse"); }
    else { Force = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("velocity_change")); Mode = TEXT("velocity_change"); }

    if (Mode == TEXT("force")) Prim->AddForce(Force, NAME_None, /*bAccelChange=*/true);
    else if (Mode == TEXT("impulse")) Prim->AddImpulse(Force, NAME_None, /*bVelChange=*/true);
    else Prim->AddImpulse(Force, NAME_None, /*bVelChange=*/true);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("actor"), TargetActor->GetName());
    ResultObj->SetStringField(TEXT("component"), Prim->GetName());
    ResultObj->SetStringField(TEXT("mode"), Mode);
    ResultObj->SetNumberField(TEXT("mass"), Prim->GetMass());
    TSharedPtr<FJsonObject> Vel = MakeShared<FJsonObject>();
    const FVector V = Prim->GetPhysicsLinearVelocity();
    Vel->SetNumberField(TEXT("x"), V.X); Vel->SetNumberField(TEXT("y"), V.Y); Vel->SetNumberField(TEXT("z"), V.Z);
    ResultObj->SetObjectField(TEXT("velocity_after"), Vel);
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const bool bGameWorld = World && World->IsGameWorld();
    ResultObj->SetBoolField(TEXT("game_world"), bGameWorld);
    if (!bGameWorld && Prim->GetMass() <= 0.0f)
    {
        ResultObj->SetStringField(TEXT("note"),
            TEXT("Editor (no PIE): el cuerpo aun no tiene masa porque la simulacion no corre hasta pulsar Simulate o entrar en PIE; el impulso queda aplicado."));
    }
    if (bEnablePhysics)
    {
        ResultObj->SetBoolField(TEXT("physics_enabled"), true);
    }
    return ResultObj;
}
