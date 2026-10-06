#include "Commands/UnrealMCPEditorCommands.h"
#include "Misc/ScopeLock.h"
#include "IPythonScriptPlugin.h"
#include "Components/BrushComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Commands/UnrealMCPFoliageCommands.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Containers/Ticker.h"
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
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/EngineBaseTypes.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"

FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
}

namespace
{
    // Estado de operaciones diferidas de editor (save/open) ejecutadas en
    // FTSTicker fuera del tick del mundo (ver HandleSaveLevel/HandleOpenLevel).
    FString GLMCP_PendingKind;   // "save" | "open" | vacio
    FString GLMCP_PendingTarget;
    FString GLMCP_LastOpError;
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
    else if (CommandType == TEXT("execute_console_command"))
    {
        return HandleExecuteConsoleCommand(Params);
    }
    else if (CommandType == TEXT("execute_python"))
    {
        return HandleExecutePython(Params);
    }
    else if (CommandType == TEXT("get_editor_state"))
    {
        return HandleGetEditorState(Params);
    }
    else if (CommandType == TEXT("get_selection"))
    {
        return HandleGetSelection(Params);
    }
    else if (CommandType == TEXT("set_selection"))
    {
        return HandleSetSelection(Params);
    }
    else if (CommandType == TEXT("editor_undo"))
    {
        return HandleUndo(Params);
    }
    else if (CommandType == TEXT("editor_redo"))
    {
        return HandleRedo(Params);
    }
    else if (CommandType == TEXT("save_current_level"))
    {
        return HandleSaveLevel(Params);
    }
    else if (CommandType == TEXT("open_level"))
    {
        return HandleOpenLevel(Params);
    }
    else if (CommandType == TEXT("play_start"))
    {
        return HandlePlayStart(Params);
    }
    else if (CommandType == TEXT("play_stop"))
    {
        return HandlePlayStop(Params);
    }
    else if (CommandType == TEXT("play_status"))
    {
        return HandlePlayStatus(Params);
    }
    else if (CommandType == TEXT("simulate_input"))
    {
        return HandleSimulateInput(Params);
    }
    else if (CommandType == TEXT("list_functions"))
    {
        return HandleListFunctions(Params);
    }
    else if (CommandType == TEXT("call_actor_function"))
    {
        return HandleCallActorFunction(Params);
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
        // Modify() graba el actor al transaction buffer y marca su paquete como
        // dirty. Con World Partition el actor vive en un paquete externo
        // (__ExternalActors__/...) asi que ademas marcamos el paquete del mapa,
        // que es el que consulta get_editor_state.
        NewActor->Modify();
        if (World)
        {
            World->MarkPackageDirty();
        }

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

            // Modify() marca su paquete (con World Partition es el externo del
            // actor); ademas marcamos el paquete del mapa, que es el que
            // consulta get_editor_state. Borrar actores deja el mapa sin
            // guardar, igual que en el editor.
            Actor->Modify();
            if (GWorld)
            {
                GWorld->MarkPackageDirty();
            }

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

// Definida mas abajo (junto a HandleCreateMaterial); usada aqui para crear
// el asset de foliage type en disco.
static UPackage* LoadOrCreateAssetPackage(const FString& PackageName, FString& OutErrorMsg);

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

    // Asset en disco en lugar de NewObject transitorio: AddMesh hace
    // check(!IsPartitionedWorld() || InType->IsAsset()) (InstancedFoliage.cpp:3860)
    // y un transient package no sobrevive a un reload del nivel.
    const FString TypeBaseName = FString::Printf(TEXT("FT_%s"), *Mesh->GetName());
    FString FoliagePackageName = FString::Printf(TEXT("/Game/MCPTests/Foliage/%s"), *TypeBaseName);
    FoliagePackageName = UPackageTools::SanitizePackageName(FoliagePackageName);
    FString PackageErrorMsg;
    UPackage* FoliagePackage = LoadOrCreateAssetPackage(FoliagePackageName, PackageErrorMsg);
    if (!FoliagePackage)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(PackageErrorMsg);
    }
    UFoliageType_InstancedStaticMesh* FoliageType =
        FindObject<UFoliageType_InstancedStaticMesh>(FoliagePackage, *TypeBaseName);
    if (FoliageType && FoliageType->GetStaticMesh() != Mesh)
    {
        // Colision de nombre con otro mesh: sufijo determinista por path.
        const FString UniqueName = FString::Printf(TEXT("%s_%08X"), *TypeBaseName, GetTypeHash(MeshPath));
        FoliageType = FindObject<UFoliageType_InstancedStaticMesh>(FoliagePackage, *UniqueName);
        if (!FoliageType)
        {
            FoliageType = NewObject<UFoliageType_InstancedStaticMesh>(FoliagePackage, FName(*UniqueName),
                                                                     RF_Standalone | RF_Public);
            FoliageType->SetStaticMesh(Mesh);
            FAssetRegistryModule::AssetCreated(FoliageType);
            FoliagePackage->MarkPackageDirty();
        }
    }
    else if (!FoliageType)
    {
        FoliageType = NewObject<UFoliageType_InstancedStaticMesh>(FoliagePackage, FName(*TypeBaseName),
                                                                 RF_Standalone | RF_Public);
        FoliageType->SetStaticMesh(Mesh);
        FAssetRegistryModule::AssetCreated(FoliageType);
        FoliagePackage->MarkPackageDirty();
    }

    FString FoliageErr;
    AInstancedFoliageActor* IFA = FUnrealMCPFoliageCommands::GetOrCreateFoliageActor(FoliageErr);
    if (!IFA)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FoliageErr);
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

    // El label se decide ANTES del spawn: SpawnParams.Name = FinalLabel para que
    // el FName del actor coincida con el label del editor (attach/find por nombre
    // o label funcionan igual).
    FString FinalLabel = NewActorName.IsEmpty() ? (SourceActorName + TEXT("_Copy")) : NewActorName;

    // Spawn con nombre explicito + construction diferido:
    // - NameMode Requested: si el FName ya existe genera uno con sufijo (el
    //   default Required_Fatal abortaria el editor).
    // - bDeferConstruction: los construction scripts corren en FinishSpawning,
    //   despues de copiar las propiedades.
    // - CopyPropertiesForUnrelatedObjects exige components NO registrados
    //   (ensure UnrealEngine.cpp:16828). PostSpawnInitialize llama
    //   RegisterAllComponents() aunque bDeferConstruction sea true
    //   (Actor.cpp:3787), asi que se desregistran a mano antes de copiar y se
    //   re-registran despues — mismo patron que KismetReinstanceUtilities.
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*FinalLabel);
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bDeferConstruction = true;

    AActor* NewActor = GWorld->SpawnActor<AActor>(SourceActor->GetClass(), NewTransform, SpawnParams);
    if (!NewActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn duplicated actor"));
    }

    NewActor->UnregisterAllComponents();

    // Copia mesh, materiales y demas propiedades del actor original (mismo mecanismo
    // que usa el editor internamente al duplicar objetos no relacionados por herencia directa)
    UEngine::CopyPropertiesForUnrelatedObjects(SourceActor, NewActor);

    if (NewActor->GetWorld() && NewActor->GetWorld()->bIsWorldInitialized)
    {
        NewActor->RegisterAllComponents();
    }
    UGameplayStatics::FinishSpawningActor(NewActor, NewTransform);
    NewActor->SetActorTransform(NewTransform);

    NewActor->SetActorLabel(FinalLabel);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("source_actor"), SourceActorName);
    // GetName() real: si el FName pedido ya existia, SpawnParams.NameMode
    // Requested genera un FName con sufijo y el label no refleja ese nombre.
    ResultObj->SetStringField(TEXT("new_actor"), NewActor->GetName());
    ResultObj->SetStringField(TEXT("new_actor_label"), NewActor->GetActorLabel());
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

    // Fuzzy: exacto por GetName() o GetActorLabel() primero; si no, prefijo de
    // GetName(). El harness renombra/usa labels del editor que no siempre
    // coinciden con el FName (p. ej. tras duplicate con nombre ya existente).
    AActor* ChildActor = nullptr;
    AActor* ChildPartial = nullptr;
    AActor* ParentActor = nullptr;
    AActor* ParentPartial = nullptr;
    auto MatchesExact = [](AActor* A, const FString& N) -> bool
    {
        return A->GetName() == N || A->GetActorLabel() == N;
    };
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;
        if (MatchesExact(Actor, ChildName)) { ChildActor = Actor; }
        else if (!ChildPartial && Actor->GetName().StartsWith(ChildName)) { ChildPartial = Actor; }
        if (MatchesExact(Actor, ParentName)) { ParentActor = Actor; }
        else if (!ParentPartial && Actor->GetName().StartsWith(ParentName)) { ParentPartial = Actor; }
    }
    if (!ChildActor) { ChildActor = ChildPartial; }
    if (!ParentActor) { ParentActor = ParentPartial; }
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
    ResultObj->SetStringField(TEXT("actor"), ChildActor->GetName());
    ResultObj->SetStringField(TEXT("parent"), ParentActor->GetName());
    ResultObj->SetStringField(TEXT("attachment_rule"), RuleString);
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    return ResultObj;
}


// Carga (si ya existe en disco) o crea un package de asset.
// Un package recien creado con CreatePackage sobre un archivo que YA existe
// queda con IsFullyLoaded()==false (Package.cpp:316, bHasBeenFullyLoaded=false
// y GetFileSize()!=0) y aborta el editor al guardarse (SavePackage2.cpp:195
// -> ESavePackageResult::Error). Ese era el crash del run 2 del harness en
// actor_create_material. LoadPackage marcа el package fully-loaded al
// terminar (LinkerLoad -> MarkAsFullyLoaded), por eso se carga en vez de
// crearlo. No usar FullyLoad() aqui: la carga explicita con LoadPackage es
// la misma via y queda mas clara.
static UPackage* LoadOrCreateAssetPackage(const FString& PackageName, FString& OutErrorMsg)
{
    UPackage* Package = FindPackage(nullptr, *PackageName);
    if (!Package || !Package->IsFullyLoaded())
    {
        if (FPackageName::DoesPackageExist(PackageName))
        {
            Package = LoadPackage(nullptr, *PackageName, LOAD_None);
        }
    }
    if (!Package)
    {
        Package = CreatePackage(*PackageName);
    }
    if (!Package)
    {
        OutErrorMsg = FString::Printf(TEXT("Failed to create package: %s"), *PackageName);
        return nullptr;
    }
    if (!Package->IsFullyLoaded())
    {
        OutErrorMsg = FString::Printf(
            TEXT("Package '%s' is not fully loaded; refusing to save it (would clobber on-disk content)"),
            *PackageName);
        return nullptr;
    }
    return Package;
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

    FString PackageErrorMsg;
    UPackage* Package = LoadOrCreateAssetPackage(PackageName, PackageErrorMsg);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(PackageErrorMsg);
    }

    // Reusar la si ya existe (disco o memoria): reconstruir en sitio
    // conserva las referencias de actores que ya la tengan asignada.
    bool bNewlyCreated = false;
    UMaterial* NewMaterial = FindObject<UMaterial>(Package, *MaterialName);
    if (NewMaterial)
    {
        for (UMaterialExpression* OldExpr : NewMaterial->GetExpressions())
        {
            if (OldExpr)
            {
                OldExpr->Rename(nullptr, GetTransientPackage(),
                    REN_DontCreateRedirectors | REN_ForceGlobalUnique);
            }
        }
        NewMaterial->GetExpressionCollection().Empty();
        UMaterialEditorOnlyData* EditorOnly = NewMaterial->GetEditorOnlyData();
        if (EditorOnly)
        {
            EditorOnly->BaseColor.Expression = nullptr;
            EditorOnly->Roughness.Expression = nullptr;
            EditorOnly->Metallic.Expression = nullptr;
            EditorOnly->Normal.Expression = nullptr;
            EditorOnly->AmbientOcclusion.Expression = nullptr;
        }
    }
    else
    {
        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
            UMaterial::StaticClass(), Package, FName(*MaterialName), RF_Standalone | RF_Public, nullptr, GWarn));
        bNewlyCreated = true;
    }

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
    if (bNewlyCreated)
    {
        FAssetRegistryModule::AssetCreated(NewMaterial);
    }

    // Guardar el .uasset a disco para que sobreviva un reinicio del editor
    if (!Package->IsFullyLoaded())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Package '%s' is not fully loaded; save cancelado para no perder contenido en disco"),
            *PackageName));
    }
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
    if (!Package->IsFullyLoaded())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Package '%s' is not fully loaded; save cancelado para no perder contenido en disco"),
            *Package->GetName()));
    }
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

    FString PackageErrorMsg;
    UPackage* Package = LoadOrCreateAssetPackage(PackageName, PackageErrorMsg);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(PackageErrorMsg);
    }

    // Reusar la si ya existe (disco o memoria): reconstruir en sitio
    // conserva las referencias de actores que ya la tengan asignada.
    bool bNewlyCreated = false;
    UMaterial* NewMaterial = FindObject<UMaterial>(Package, *MaterialName);
    if (NewMaterial)
    {
        for (UMaterialExpression* OldExpr : NewMaterial->GetExpressions())
        {
            if (OldExpr)
            {
                OldExpr->Rename(nullptr, GetTransientPackage(),
                    REN_DontCreateRedirectors | REN_ForceGlobalUnique);
            }
        }
        NewMaterial->GetExpressionCollection().Empty();
        UMaterialEditorOnlyData* EditorOnly = NewMaterial->GetEditorOnlyData();
        if (EditorOnly)
        {
            EditorOnly->BaseColor.Expression = nullptr;
            EditorOnly->Roughness.Expression = nullptr;
            EditorOnly->Metallic.Expression = nullptr;
            EditorOnly->Normal.Expression = nullptr;
            EditorOnly->AmbientOcclusion.Expression = nullptr;
        }
    }
    else
    {
        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
            UMaterial::StaticClass(), Package, FName(*MaterialName), RF_Standalone | RF_Public, nullptr, GWarn));
        bNewlyCreated = true;
    }

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
    if (bNewlyCreated)
    {
        FAssetRegistryModule::AssetCreated(NewMaterial);
    }

    if (!Package->IsFullyLoaded())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Package '%s' is not fully loaded; save cancelado para no perder contenido en disco"),
            *PackageName));
    }
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

    FString PackageErrorMsg;
    UPackage* Package = LoadOrCreateAssetPackage(PackageName, PackageErrorMsg);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(PackageErrorMsg);
    }

    // Reusar la si ya existe (disco o memoria): reconstruir en sitio
    // conserva las referencias de actores que ya la tengan asignada.
    bool bNewlyCreated = false;
    UMaterial* NewMaterial = FindObject<UMaterial>(Package, *MaterialName);
    if (NewMaterial)
    {
        for (UMaterialExpression* OldExpr : NewMaterial->GetExpressions())
        {
            if (OldExpr)
            {
                OldExpr->Rename(nullptr, GetTransientPackage(),
                    REN_DontCreateRedirectors | REN_ForceGlobalUnique);
            }
        }
        NewMaterial->GetExpressionCollection().Empty();
        UMaterialEditorOnlyData* EditorOnly = NewMaterial->GetEditorOnlyData();
        if (EditorOnly)
        {
            EditorOnly->BaseColor.Expression = nullptr;
            EditorOnly->Roughness.Expression = nullptr;
            EditorOnly->Metallic.Expression = nullptr;
            EditorOnly->Normal.Expression = nullptr;
            EditorOnly->AmbientOcclusion.Expression = nullptr;
        }
    }
    else
    {
        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
            UMaterial::StaticClass(), Package, FName(*MaterialName), RF_Standalone | RF_Public, nullptr, GWarn));
        bNewlyCreated = true;
    }
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
    if (bNewlyCreated)
    {
        FAssetRegistryModule::AssetCreated(NewMaterial);
    }

    if (!Package->IsFullyLoaded())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Package '%s' is not fully loaded; save cancelado para no perder contenido en disco"),
            *PackageName));
    }
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

// ---------------------------------------------------------------------------
// --- Fase 3: control de juego (gameplay runtime) ---
// ---------------------------------------------------------------------------

namespace UnrealMCPGameDetail
{
    /** true si la propiedad numerica es de tipo entero sin signo (FNumericProperty no expone IsUnsigned). */
    static bool IsUnsignedNumeric(const FNumericProperty* NP)
    {
        return NP && (NP->IsA<FByteProperty>() || NP->IsA<FUInt16Property>() ||
                      NP->IsA<FUInt32Property>() || NP->IsA<FUInt64Property>());
    }
    /** Output device que acumula texto para devolverlo al cliente. */
    class FStringOutputDevice : public FOutputDevice
    {
    public:
        FString Buffer;
        FCriticalSection Mutex;
        virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
        {
            // Puede ser llamado desde cualquier hilo si se registra en GLog.
            FScopeLock Lock(&Mutex);
            if (Buffer.Len() > 4000) { return; }
            if (!Buffer.IsEmpty()) { Buffer += LINE_TERMINATOR; }
            Buffer += V;
        }
    };

    static UWorld* GetEditorWorld()
    {
        return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    }

    static AActor* FindActorByName(UWorld* World, const FString& Name)
    {
        if (!World || Name.IsEmpty()) { return nullptr; }
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
        AActor* Partial = nullptr;
        for (AActor* Actor : AllActors)
        {
            if (!Actor) { continue; }
            if (Actor->GetName() == Name) { return Actor; }
            if (!Partial && (Actor->GetName().StartsWith(Name) || Actor->GetActorLabel() == Name)) { Partial = Actor; }
        }
        return Partial;
    }

    /** JSON -> texto apto para FProperty::ImportText_Direct (escalares y structs tipados). */
    static bool JsonToImportText(const TSharedPtr<FJsonValue>& Value, FProperty* Prop, FString& OutText)
    {
        if (!Value.IsValid() || !Prop) { return false; }

        if (Value->Type == EJson::Boolean) { OutText = Value->AsBool() ? TEXT("true") : TEXT("false"); return true; }
        if (Value->Type == EJson::Number) { OutText = FString::Printf(TEXT("%g"), Value->AsNumber()); return true; }
        if (Value->Type == EJson::String) { OutText = Value->AsString(); return true; }
        if (Value->Type != EJson::Array && Value->Type != EJson::Object) { return false; }

        auto ReadComponents = [&](TArray<double>& OutComp, TMap<FString, double>& OutKV) -> bool
        {
            if (Value->Type == EJson::Array)
            {
                for (const TSharedPtr<FJsonValue>& E : Value->AsArray())
                {
                    if (E->Type != EJson::Number) { return false; }
                    OutComp.Add(E->AsNumber());
                }
                return true;
            }
            TSharedPtr<FJsonObject> O = Value->AsObject();
            if (!O.IsValid()) { return false; }
            for (const auto& Pair : O->Values)
            {
                if (Pair.Value->Type == EJson::Number) { OutKV.Add(Pair.Key.ToLower(), Pair.Value->AsNumber()); }
                else if (Pair.Value->Type == EJson::Boolean) { OutKV.Add(Pair.Key.ToLower(), Pair.Value->AsBool() ? 1.0 : 0.0); }
                else { return false; }
            }
            return true;
        };

        auto GetKV = [](const TArray<double>& C, const TMap<FString, double>& KV, const TCHAR* A, const TCHAR* B, double& Out) -> bool
        {
            if (KV.Contains(A)) { Out = KV[A]; return true; }
            if (B && KV.Contains(B)) { Out = KV[B]; return true; }
            return false;
        };

        TArray<double> Comp;
        TMap<FString, double> KV;
        if (!ReadComponents(Comp, KV)) { return false; }

        if (FStructProperty* SP = CastField<FStructProperty>(Prop))
        {
            const UScriptStruct* St = SP->Struct;
            double X = 0, Y = 0, Z = 0, W = 1;
            const bool bFromObj = (Value->Type == EJson::Object);

            if (St == TBaseStructure<FVector>::Get())
            {
                if (bFromObj) { GetKV(Comp, KV, TEXT("x"), nullptr, X); GetKV(Comp, KV, TEXT("y"), nullptr, Y); GetKV(Comp, KV, TEXT("z"), nullptr, Z); }
                else { if (Comp.Num() < 3) { return false; } X = Comp[0]; Y = Comp[1]; Z = Comp[2]; }
                OutText = FString::Printf(TEXT("(X=%g,Y=%g,Z=%g)"), X, Y, Z);
                return true;
            }
            if (St == TBaseStructure<FVector2D>::Get())
            {
                if (bFromObj) { GetKV(Comp, KV, TEXT("x"), nullptr, X); GetKV(Comp, KV, TEXT("y"), nullptr, Y); }
                else { if (Comp.Num() < 2) { return false; } X = Comp[0]; Y = Comp[1]; }
                OutText = FString::Printf(TEXT("(X=%g,Y=%g)"), X, Y);
                return true;
            }
            if (St == TBaseStructure<FRotator>::Get())
            {
                double Pitch = 0, Yaw = 0, Roll = 0;
                if (bFromObj)
                {
                    if (!GetKV(Comp, KV, TEXT("pitch"), TEXT("x"), Pitch)) { Pitch = 0; }
                    if (!GetKV(Comp, KV, TEXT("yaw"), TEXT("y"), Yaw)) { Yaw = 0; }
                    if (!GetKV(Comp, KV, TEXT("roll"), TEXT("z"), Roll)) { Roll = 0; }
                }
                else { if (Comp.Num() < 3) { return false; } Pitch = Comp[0]; Yaw = Comp[1]; Roll = Comp[2]; }
                OutText = FString::Printf(TEXT("(Pitch=%g,Yaw=%g,Roll=%g)"), Pitch, Yaw, Roll);
                return true;
            }
            if (St == TBaseStructure<FColor>::Get())
            {
                double R = 0, G = 0, B = 0, A = 255;
                if (bFromObj) { GetKV(Comp, KV, TEXT("r"), nullptr, R); GetKV(Comp, KV, TEXT("g"), nullptr, G); GetKV(Comp, KV, TEXT("b"), nullptr, B); GetKV(Comp, KV, TEXT("a"), nullptr, A); }
                else { if (Comp.Num() < 3) { return false; } R = Comp[0]; G = Comp[1]; B = Comp[2]; if (Comp.Num() > 3) { A = Comp[3]; } }
                OutText = FString::Printf(TEXT("(R=%d,G=%d,B=%d,A=%d)"), (int32)R, (int32)G, (int32)B, (int32)A);
                return true;
            }
            if (St == TBaseStructure<FLinearColor>::Get())
            {
                double R = 0, G = 0, B = 0, A = 1;
                if (bFromObj) { GetKV(Comp, KV, TEXT("r"), nullptr, R); GetKV(Comp, KV, TEXT("g"), nullptr, G); GetKV(Comp, KV, TEXT("b"), nullptr, B); GetKV(Comp, KV, TEXT("a"), nullptr, A); }
                else { if (Comp.Num() < 3) { return false; } R = Comp[0]; G = Comp[1]; B = Comp[2]; if (Comp.Num() > 3) { A = Comp[3]; } }
                OutText = FString::Printf(TEXT("(R=%g,G=%g,B=%g,A=%g)"), R, G, B, A);
                return true;
            }

            // Struct generico: objeto JSON {campo=valor} -> texto "(Campo=Valor,...)"
            if (Value->Type == EJson::Object)
            {
                TSharedPtr<FJsonObject> StructObj = Value->AsObject();
                if (!StructObj.IsValid()) { return false; }
                FString Inner;
                for (const auto& Pair : StructObj->Values)
                {
                    double Num = 0;
                    if (Pair.Value->Type == EJson::Number) { Num = Pair.Value->AsNumber(); }
                    else if (Pair.Value->Type == EJson::Boolean) { Num = Pair.Value->AsBool() ? 1.0 : 0.0; }
                    else if (Pair.Value->Type == EJson::String) { Inner += FString::Printf(TEXT("%s=\"%s\","), *Pair.Key, *Pair.Value->AsString()); continue; }
                    else { return false; }
                    Inner += FString::Printf(TEXT("%s=%g,"), *Pair.Key, Num);
                }
                if (Inner.EndsWith(TEXT(","))) { Inner.RemoveAt(Inner.Len() - 1); }
                OutText = FString::Printf(TEXT("(%s)"), *Inner);
                return true;
            }
            return false;
        }

        if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
        {
            if (Value->Type != EJson::Array) { return false; }
            FString Inner;
            for (const TSharedPtr<FJsonValue>& E : Value->AsArray())
            {
                FString ElemText;
                if (!JsonToImportText(E, AP->Inner, ElemText)) { return false; }
                Inner += ElemText + TEXT(",");
            }
            if (Inner.EndsWith(TEXT(","))) { Inner.RemoveAt(Inner.Len() - 1); }
            OutText = FString::Printf(TEXT("(%s)"), *Inner);
            return true;
        }
        return false;
    }

    /** Asigna un valor JSON a un buffer de parametros. World se usa para resolver actores por nombre. */
    static bool SetParmFromJson(FProperty* Prop, const TSharedPtr<FJsonValue>& Value, void* Dest, UWorld* World, FString& Err)
    {
        if (!Prop || !Dest || !Value.IsValid()) { Err = TEXT("valor invalido"); return false; }
        void* Ptr = Prop->ContainerPtrToValuePtr<void>(Dest, 0);

        if (FBoolProperty* BP = CastField<FBoolProperty>(Prop))
        {
            bool bValue = false;
            if (Value->Type == EJson::Boolean) { bValue = Value->AsBool(); }
            else if (Value->Type == EJson::Number) { bValue = Value->AsNumber() != 0.0; }
            else if (Value->Type == EJson::String) { bValue = FCString::ToBool(*Value->AsString()); }
            else { Err = TEXT("se esperaba bool"); return false; }
            BP->SetPropertyValue(Ptr, bValue);
            return true;
        }

        if (FNumericProperty* NP = CastField<FNumericProperty>(Prop))
        {
            double Num = 0.0;
            if (Value->Type == EJson::Number) { Num = Value->AsNumber(); }
            else if (Value->Type == EJson::Boolean) { Num = Value->AsBool() ? 1.0 : 0.0; }
            else if (Value->Type == EJson::String) { Num = FCString::Atod(*Value->AsString()); }
            else { Err = TEXT("se esperaba un numero"); return false; }
            if (NP->IsFloatingPoint()) { NP->SetFloatingPointPropertyValue(Ptr, Num); }
            else { NP->SetIntPropertyValue(Ptr, (int64)Num); }
            return true;
        }

        if (FEnumProperty* EP = CastField<FEnumProperty>(Prop))
        {
            if (Value->Type == EJson::Number && EP->GetUnderlyingProperty())
            {
                EP->GetUnderlyingProperty()->SetIntPropertyValue(Ptr, (int64)Value->AsNumber());
                return true;
            }
            if (Value->Type == EJson::String)
            {
                const TCHAR* R = EP->ImportText_Direct(*Value->AsString(), Ptr, nullptr, PPF_None, nullptr);
                if (R) { return true; }
            }
            Err = TEXT("enum no reconocido (usa nombre o numero)");
            return false;
        }

        if (FByteProperty* ByP = CastField<FByteProperty>(Prop))
        {
            if (ByP->Enum)
            {
                if (Value->Type == EJson::String)
                {
                    const TCHAR* R = ByP->ImportText_Direct(*Value->AsString(), Ptr, nullptr, PPF_None, nullptr);
                    if (R) { return true; }
                }
                else if (Value->Type == EJson::Number)
                {
                    *(uint8*)Ptr = (uint8)Value->AsNumber();
                    return true;
                }
                Err = TEXT("enum (byte) no reconocido");
                return false;
            }
        }

        if (FStrProperty* SP = CastField<FStrProperty>(Prop))
        {
            if (Value->Type != EJson::String) { Err = TEXT("se esperaba string"); return false; }
            *(FString*)Ptr = Value->AsString();
            return true;
        }
        if (FNameProperty* NP2 = CastField<FNameProperty>(Prop))
        {
            if (Value->Type != EJson::String) { Err = TEXT("se esperaba string"); return false; }
            *(FName*)Ptr = FName(*Value->AsString());
            return true;
        }
        if (FTextProperty* TP = CastField<FTextProperty>(Prop))
        {
            if (Value->Type != EJson::String) { Err = TEXT("se esperaba string"); return false; }
            *(FText*)Ptr = FText::FromString(Value->AsString());
            return true;
        }

        if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
        {
            UObject* Obj = nullptr;
            if (Value->Type == EJson::Null) { Obj = nullptr; }
            else if (Value->Type == EJson::String)
            {
                const FString S = Value->AsString();
                Obj = FindActorByName(World, S);
                if (!Obj) { Obj = LoadObject<UObject>(nullptr, *S); }
                if (!Obj) { Obj = FindObject<UObject>(nullptr, *S); }
                if (!Obj && S.EndsWith(TEXT("_C")) == false)
                {
                    TArray<AActor*> AllActors;
                    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
                    for (AActor* A : AllActors) { if (A && A->GetActorLabel() == S) { Obj = A; break; } }
                }
                if (!Obj)
                {
                    Err = FString::Printf(TEXT("objeto no encontrado: %s (puede ser un actor en escena o una ruta /Game/...)"), *S);
                    return false;
                }
            }
            else { Err = TEXT("se esperaba string con nombre/ruta del objeto"); return false; }
            *(UObject**)Ptr = Obj;
            return true;
        }

        FString Text;
        if (!JsonToImportText(Value, Prop, Text))
        {
            Err = FString::Printf(TEXT("tipo no soportado: %s"), *Prop->GetCPPType());
            return false;
        }
        const TCHAR* R = Prop->ImportText_Direct(*Text, Ptr, nullptr, PPF_None, nullptr);
        if (!R)
        {
            Err = FString::Printf(TEXT("no se pudo interpretar '%s' para %s"), *Text, *Prop->GetCPPType());
            return false;
        }
        return true;
    }

    /** Parametro (ya procesado) -> JSON con tipos nativos cuando es posible. */
    static TSharedPtr<FJsonValue> ParmToJson(FProperty* Prop, const void* Src)
    {
        // Src ya es el puntero AL VALOR (no al contenedor): no aplicar
        // ContainerPtrToValuePtr otra vez (doble offset = memoria basura).
        if (!Prop || !Src) { return MakeShared<FJsonValueNull>(); }
        const void* Ptr = Src;

        if (FBoolProperty* BP = CastField<FBoolProperty>(Prop)) { return MakeShared<FJsonValueBoolean>(BP->GetPropertyValue(Ptr)); }
        if (FNumericProperty* NP = CastField<FNumericProperty>(Prop))
        {
            if (NP->IsFloatingPoint()) { return MakeShared<FJsonValueNumber>(NP->GetFloatingPointPropertyValue(Ptr)); }
            if (IsUnsignedNumeric(NP)) { return MakeShared<FJsonValueNumber>((double)NP->GetUnsignedIntPropertyValue(Ptr)); }
            return MakeShared<FJsonValueNumber>((double)NP->GetSignedIntPropertyValue(Ptr));
        }
        if (FStrProperty* SP = CastField<FStrProperty>(Prop)) { return MakeShared<FJsonValueString>(*(const FString*)Ptr); }
        if (FNameProperty* NP3 = CastField<FNameProperty>(Prop)) { return MakeShared<FJsonValueString>(((const FName*)Ptr)->ToString()); }
        if (FTextProperty* TP = CastField<FTextProperty>(Prop)) { return MakeShared<FJsonValueString>(((const FText*)Ptr)->ToString()); }

        if (FEnumProperty* EP = CastField<FEnumProperty>(Prop))
        {
            if (const FNumericProperty* UP = EP->GetUnderlyingProperty())
            {
                const int64 Val = IsUnsignedNumeric(UP) ? (int64)UP->GetUnsignedIntPropertyValue(Ptr) : UP->GetSignedIntPropertyValue(Ptr);
                if (const UEnum* E = EP->GetEnum())
                {
                    return MakeShared<FJsonValueString>(E->GetNameStringByValue(Val));
                }
                return MakeShared<FJsonValueNumber>((double)Val);
            }
        }
        if (FByteProperty* ByP = CastField<FByteProperty>(Prop))
        {
            const uint8 Val = *(const uint8*)Ptr;
            if (ByP->Enum) { return MakeShared<FJsonValueString>(ByP->Enum->GetNameStringByValue(Val)); }
            return MakeShared<FJsonValueNumber>((double)Val);
        }

        if (FStructProperty* SP2 = CastField<FStructProperty>(Prop))
        {
            const UScriptStruct* St = SP2->Struct;
            auto MakeNumObj = [](std::initializer_list<std::pair<const TCHAR*, double>> Pairs)
            {
                TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
                for (const auto& P : Pairs) { O->SetNumberField(P.first, P.second); }
                return MakeShared<FJsonValueObject>(O);
            };
            if (St == TBaseStructure<FVector>::Get())
            {
                const FVector* V = (const FVector*)Ptr;
                return MakeNumObj({ {TEXT("x"), V->X}, {TEXT("y"), V->Y}, {TEXT("z"), V->Z} });
            }
            if (St == TBaseStructure<FVector2D>::Get())
            {
                const FVector2D* V = (const FVector2D*)Ptr;
                return MakeNumObj({ {TEXT("x"), V->X}, {TEXT("y"), V->Y} });
            }
            if (St == TBaseStructure<FRotator>::Get())
            {
                const FRotator* V = (const FRotator*)Ptr;
                return MakeNumObj({ {TEXT("pitch"), V->Pitch}, {TEXT("yaw"), V->Yaw}, {TEXT("roll"), V->Roll} });
            }
            if (St == TBaseStructure<FColor>::Get())
            {
                const FColor* V = (const FColor*)Ptr;
                return MakeNumObj({ {TEXT("r"), (double)V->R}, {TEXT("g"), (double)V->G}, {TEXT("b"), (double)V->B}, {TEXT("a"), (double)V->A} });
            }
            if (St == TBaseStructure<FLinearColor>::Get())
            {
                const FLinearColor* V = (const FLinearColor*)Ptr;
                return MakeNumObj({ {TEXT("r"), V->R}, {TEXT("g"), V->G}, {TEXT("b"), V->B}, {TEXT("a"), V->A} });
            }
            FString Text;
            Prop->ExportText_Direct(Text, Ptr, nullptr, nullptr, PPF_None, nullptr);
            return MakeShared<FJsonValueString>(Text);
        }

        if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
        {
            UObject* Obj = *(UObject* const*)Ptr;
            return MakeShared<FJsonValueString>(Obj ? Obj->GetName() : FString());
        }

        if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
        {
            FScriptArrayHelper Helper(AP, Ptr);
            TArray<TSharedPtr<FJsonValue>> Items;
            const int32 Count = FMath::Min(Helper.Num(), 64);
            for (int32 i = 0; i < Count; ++i) { Items.Add(ParmToJson(AP->Inner, Helper.GetRawPtr(i))); }
            if (Helper.Num() > 64) { Items.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("...(%d mas)"), Helper.Num() - 64))); }
            return MakeShared<FJsonValueArray>(Items);
        }

        FString Text;
        Prop->ExportText_Direct(Text, Ptr, nullptr, nullptr, PPF_None, nullptr);
        return MakeShared<FJsonValueString>(Text);
    }

    /** Resuelve un target (ruta /Game/... de Blueprint, nombre de asset o nombre de clase) a UClass*. */
    static UClass* ResolveTargetClass(const FString& Target, FString& OutSource)
    {
        if (Target.Contains(TEXT("/")))
        {
            FString Base = Target;
            if (Base.EndsWith(TEXT(".uasset"))) { Base = Base.LeftChop(8); }
            if (Base.StartsWith(TEXT("/Script/")))
            {
                if (UClass* Cls = FindObject<UClass>(nullptr, *Base)) { OutSource = TEXT("native_class_path"); return Cls; }
                if (UClass* Cls = LoadClass<UObject>(nullptr, *Base)) { OutSource = TEXT("native_class_path"); return Cls; }
            }
            TArray<FString> Candidates;
            if (Base.EndsWith(TEXT("_C"))) { Candidates.Add(Base); }
            else { Candidates.Add(Base + TEXT("_C")); }
            int32 DotIdx = INDEX_NONE;
            if (Base.FindLastChar(TEXT('.'), DotIdx) && DotIdx > 0)
            {
                Candidates.Add(Base.Left(DotIdx + 1) + Base.Mid(DotIdx + 1) + TEXT("_C"));
            }
            for (const FString& C : Candidates)
            {
                if (UClass* Cls = LoadClass<UObject>(nullptr, *C)) { OutSource = TEXT("class_path"); return Cls; }
            }
        }
        else
        {
            if (UClass* Cls = FindObject<UClass>(nullptr, *Target)) { OutSource = TEXT("native_class"); return Cls; }
            if (UClass* Cls = FindObject<UClass>(nullptr, *(Target + TEXT("_C")))) { OutSource = TEXT("native_class"); return Cls; }

            FAssetRegistryModule* ARM = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
            if (ARM)
            {
                TArray<FAssetData> Assets;
                ARM->Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Assets);
                for (const FAssetData& A : Assets)
                {
                    if (A.AssetName != FName(*Target)) { continue; }
                    UObject* Loaded = A.ToSoftObjectPath().TryLoad();
                    if (UBlueprint* BP = Cast<UBlueprint>(Loaded))
                    {
                        if (BP->GeneratedClass) { OutSource = TEXT("blueprint_asset"); return BP->GeneratedClass; }
                    }
                }
            }
        }
        return nullptr;
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
    FString Cmd;
    if (!Params->TryGetStringField(TEXT("command"), Cmd) || Cmd.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'command' parameter"));
    }
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible"));
    }

    UnrealMCPGameDetail::FStringOutputDevice Out;
    UWorld* World = UnrealMCPGameDetail::GetEditorWorld();
    // GEditor->Exec solo escribe en 'Out' lo que el propio comando emite a ese
    // dispositivo; la mayoria de comandos de consola loguean via GLog global,
    // que va a los output devices registrados. Registramos 'Out' ahi para
    // capturar tambien esa salida (se quita siempre, aunque Exec lance).
    // Nota: UE_LOG es diferido (se vacia en el hilo de logging); registrar la
    // linea ANTES de AddOutputDevice no evita que aparezca en 'Out' si el
    // flush ocurre despues. Por eso solo logueamos fuera de la ventana.
    if (GLog) { GLog->AddOutputDevice(&Out); }
    const bool bOk = GEditor->Exec(World, *Cmd, Out);
    if (GLog) { GLog->RemoveOutputDevice(&Out); }
    UE_LOG(LogTemp, Display, TEXT("HandleExecuteConsoleCommand: ok=%d cmd=%s"), bOk ? 1 : 0, *Cmd);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), bOk);
    R->SetStringField(TEXT("command"), Cmd);
    FString OutputCopy;
    { FScopeLock Lock(&Out.Mutex); OutputCopy = Out.Buffer; }
    R->SetStringField(TEXT("output"), OutputCopy);
    if (!bOk)
    {
        R->SetStringField(TEXT("note"), TEXT("El motor devolvio false: comando desconocido o sin efecto."));
    }
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleExecutePython(const TSharedPtr<FJsonObject>& Params)
{
    FString Code;
    if (!Params->TryGetStringField(TEXT("code"), Code) || Code.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Falta el parametro requerido 'code'"));
    }

    IPythonScriptPlugin* Py = FModuleManager::LoadModulePtr<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
    if (!Py)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Modulo PythonScriptPlugin no cargado (plugin habilitado en .uproject?)"));
    }
    if (!Py->IsPythonAvailable())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Python no disponible en este build del plugin"));
    }

    FString ModeStr = TEXT("file");
    Params->TryGetStringField(TEXT("mode"), ModeStr);
    ModeStr = ModeStr.ToLower();
    EPythonCommandExecutionMode Mode = EPythonCommandExecutionMode::ExecuteFile;
    if (ModeStr == TEXT("statement"))
    {
        Mode = EPythonCommandExecutionMode::ExecuteStatement;
    }
    else if (ModeStr == TEXT("evaluate"))
    {
        Mode = EPythonCommandExecutionMode::EvaluateStatement;
    }
    else if (ModeStr != TEXT("file"))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Mode invalido: usar file, statement o evaluate"));
    }

    FString ScopeStr = TEXT("public");
    Params->TryGetStringField(TEXT("scope"), ScopeStr);
    ScopeStr = ScopeStr.ToLower();
    EPythonFileExecutionScope Scope = EPythonFileExecutionScope::Public;
    if (ScopeStr == TEXT("private"))
    {
        Scope = EPythonFileExecutionScope::Private;
    }
    else if (ScopeStr != TEXT("public"))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Scope invalido: usar public o private"));
    }

    FPythonCommandEx Cmd;
    Cmd.Command = Code;
    Cmd.ExecutionMode = Mode;
    Cmd.FileExecutionScope = Scope;
    Cmd.Flags |= EPythonCommandFlags::Unattended;

    UE_LOG(LogTemp, Display, TEXT("HandleExecutePython: mode=%s scope=%s code_len=%d"), *ModeStr, *ScopeStr, Code.Len());
    const bool bOk = Py->ExecPythonCommandEx(Cmd);

    FString Output;
    int32 NumErrors = 0;
    for (const FPythonLogOutputEntry& E : Cmd.LogOutput)
    {
        if (Output.Len() > 8000)
        {
            Output += TEXT("...[output truncado]");
            break;
        }
        switch (E.Type)
        {
        case EPythonLogOutputType::Error: Output += TEXT("[Error] "); NumErrors++; break;
        case EPythonLogOutputType::Warning: Output += TEXT("[Warning] "); break;
        default: break;
        }
        Output += E.Output;
        Output += LINE_TERMINATOR;
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), bOk);
    R->SetStringField(TEXT("mode"), ModeStr);
    R->SetStringField(TEXT("scope"), ScopeStr);
    R->SetStringField(TEXT("output"), Output);
    if (Mode == EPythonCommandExecutionMode::EvaluateStatement)
    {
        R->SetStringField(TEXT("result"), Cmd.CommandResult.Left(4000));
    }
    if (!bOk)
    {
        // El bridge solo copia 'error'/'message' al envelope de error:
        // el traceback debe ir en 'error' para que el cliente lo reciba.
        R->SetStringField(TEXT("error"), Cmd.CommandResult.Left(4000));
        R->SetNumberField(TEXT("errors"), NumErrors);
    }
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePlayStart(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible")); }

    FString Mode = TEXT("play");
    Params->TryGetStringField(TEXT("mode"), Mode);
    Mode = Mode.ToLower();
    if (Mode != TEXT("play") && Mode != TEXT("simulate"))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid 'mode': use 'play' or 'simulate'"));
    }

    if (GEditor->IsPlaySessionInProgress())
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetBoolField(TEXT("already_playing"), true);
        R->SetBoolField(TEXT("request_queued"), GEditor->IsPlaySessionRequestQueued());
        R->SetStringField(TEXT("world"), GEditor->PlayWorld ? GEditor->PlayWorld->GetName() : TEXT(""));
        return R;
    }

    FRequestPlaySessionParams P;
    P.SessionDestination = EPlaySessionDestinationType::InProcess;
    P.WorldType = (Mode == TEXT("simulate")) ? EPlaySessionWorldType::SimulateInEditor : EPlaySessionWorldType::PlayInEditor;
    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
    {
        if (FLevelEditorModule* LE = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
        {
            TSharedPtr<IAssetViewport> VP = LE->GetFirstActiveViewport();
            if (VP.IsValid()) { P.DestinationSlateViewport = VP; }
        }
    }

    GEditor->RequestPlaySession(P);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("mode"), Mode);
    R->SetBoolField(TEXT("queued"), true);
    R->SetStringField(TEXT("note"), TEXT("Sesion encolada; el arranque ocurre en el siguiente tick del editor. Consulte game_play_status."));
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePlayStop(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible")); }
    const bool bWasPlaying = GEditor->IsPlayingSessionInEditor();
    const bool bWasQueued = GEditor->IsPlaySessionRequestQueued();
    if (bWasQueued) { GEditor->CancelRequestPlaySession(); }
    if (bWasPlaying || bWasQueued) { GEditor->RequestEndPlayMap(); }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("was_playing"), bWasPlaying);
    R->SetBoolField(TEXT("was_queued"), bWasQueued);
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePlayStatus(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible")); }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("playing"), GEditor->IsPlayingSessionInEditor());
    R->SetBoolField(TEXT("request_queued"), GEditor->IsPlaySessionRequestQueued());
    const bool bInProgress = GEditor->IsPlaySessionInProgress();
    R->SetBoolField(TEXT("in_progress"), bInProgress);

    UWorld* PlayWorld = GEditor->PlayWorld;
    R->SetStringField(TEXT("world"), PlayWorld ? PlayWorld->GetName() : TEXT(""));
    FString WorldType;
    if (PlayWorld)
    {
        switch (PlayWorld->WorldType)
        {
        case EWorldType::Editor: WorldType = TEXT("EDITOR"); break;
        case EWorldType::Game: WorldType = TEXT("GAME"); break;
        case EWorldType::PIE: WorldType = TEXT("PIE"); break;
        default: WorldType = TEXT("OTHER"); break;
        }
    }
    R->SetStringField(TEXT("world_type"), WorldType);
    if (PlayWorld && PlayWorld->WorldType == EWorldType::Editor)
    {
        R->SetStringField(TEXT("note"), TEXT("En curso como simulacion en editor (SIE/PIE todavia no tomo el mundo)."));
    }
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSimulateInput(const TSharedPtr<FJsonObject>& Params)
{
    FString KeyName, Action = TEXT("press");
    if (!Params->TryGetStringField(TEXT("key"), KeyName) || KeyName.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
    }
    Params->TryGetStringField(TEXT("action"), Action);
    Action = Action.ToLower();

    double Amount = 1.0;
    if (Params->HasField(TEXT("amount"))) { Amount = Params->GetNumberField(TEXT("amount")); }

    bool bGamepad = false;
    if (Params->HasField(TEXT("gamepad"))) { bGamepad = Params->GetBoolField(TEXT("gamepad")); }

    EInputEvent Event = IE_Pressed;
    if (Action == TEXT("press")) { Event = IE_Pressed; }
    else if (Action == TEXT("release")) { Event = IE_Released; }
    else if (Action == TEXT("repeat")) { Event = IE_Repeat; }
    else if (Action == TEXT("axis")) { Event = IE_Axis; }
    else if (Action == TEXT("tap")) { Event = IE_Pressed; }
    else { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid 'action': use press|release|repeat|axis|tap")); }

    if (!GEditor || !GEditor->PlayWorld)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No hay sesion activa: ejecute primero game_play_start(mode='play'). El input simulado solo funciona durante PIE."));
    }

    const FKey Key{FName(*KeyName)};
    if (!Key.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Tecla desconocida: %s. Ejemplos validos: W, A, S, D, SpaceBar, Escape, LeftMouseButton, MouseX, Gamepad_FaceButton_Bottom"), *KeyName));
    }

    APlayerController* PC = nullptr;
    for (FConstPlayerControllerIterator It = GEditor->PlayWorld->GetPlayerControllerIterator(); It; ++It)
    {
        if (It && It->Get()) { PC = It->Get(); break; }
    }
    if (!PC)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No hay PlayerController en la sesion activa (¿mode='simulate' no tiene controlador?)."));
    }

    auto Send = [&](EInputEvent Ev)
    {
        FInputKeyParams P(Key, Ev, Amount, bGamepad);
        PC->InputKey(P);
    };
    Send(Event);
    bool bSentRelease = false;
    if (Action == TEXT("tap"))
    {
        Send(IE_Released);
        bSentRelease = true;
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("key"), KeyName);
    R->SetStringField(TEXT("action"), Action);
    R->SetNumberField(TEXT("amount"), Amount);
    R->SetBoolField(TEXT("gamepad"), bGamepad);
    R->SetStringField(TEXT("player_controller"), PC->GetName());
    R->SetStringField(TEXT("world"), GEditor->PlayWorld->GetName());
    if (bSentRelease) { R->SetBoolField(TEXT("release_sent"), true); }
    if (Key.IsDigital() && Event == IE_Axis)
    {
        R->SetStringField(TEXT("note"), TEXT("Evento 'axis' sobre tecla de boton: puede no producir movimiento."));
    }
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleListFunctions(const TSharedPtr<FJsonObject>& Params)
{
    FString Target;
    if (!Params->TryGetStringField(TEXT("target"), Target) || Target.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target' parameter (ruta /Game/..., nombre de Blueprint o nombre de actor en escena)"));
    }
    bool bIncludeAll = false;
    if (Params->HasField(TEXT("include_all"))) { bIncludeAll = Params->GetBoolField(TEXT("include_all")); }
    int32 Limit = 200;
    if (Params->HasField(TEXT("limit"))) { Limit = (int32)Params->GetNumberField(TEXT("limit")); }

    UWorld* World = UnrealMCPGameDetail::GetEditorWorld();
    FString Source;
    UClass* Cls = nullptr;

    if (AActor* Actor = UnrealMCPGameDetail::FindActorByName(World, Target))
    {
        Cls = Actor->GetClass();
        Source = FString::Printf(TEXT("actor:%s"), *Actor->GetName());
    }
    if (!Cls) { Cls = UnrealMCPGameDetail::ResolveTargetClass(Target, Source); }
    if (!Cls)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("No se pudo resolver '%s' a una clase. Use una ruta tipo /Game/Path/BP_Name, el nombre del Blueprint, o el nombre de un actor en escena."), *Target));
    }

    struct FFnItem { FName Name; bool bCallable; bool bEvent; bool bImpl; int32 NumParms; };
    TArray<FFnItem> Items;
    for (TFieldIterator<UFunction> It(Cls); It; ++It)
    {
        UFunction* Fn = *It;
        if (!Fn || Fn->HasAnyFunctionFlags(FUNC_Delegate)) { continue; }
        const bool bCallable = Fn->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure);
        const bool bEvent = Fn->HasAnyFunctionFlags(FUNC_Event);
        const bool bImpl = Fn->HasMetaData(TEXT("BlueprintImplementableEvent")) || Fn->HasMetaData(TEXT("BlueprintNativeEvent")) || Fn->HasAnyFunctionFlags(FUNC_BlueprintEvent);
        if (!bIncludeAll && !bCallable && !bEvent && !bImpl) { continue; }
        Items.Add({ FName(*Fn->GetName()), bCallable, bEvent, bImpl, (int32)Fn->NumParms });
    }
    Items.Sort([](const FFnItem& A, const FFnItem& B) { return A.Name.LexicalLess(B.Name); });

    TArray<TSharedPtr<FJsonValue>> FnArray;
    int32 Added = 0;
    for (const FFnItem& Item : Items)
    {
        if (Added >= Limit) { break; }
        UFunction* Fn = Cls->FindFunctionByName(Item.Name);
        if (!Fn) { continue; }
        TSharedPtr<FJsonObject> FO = MakeShared<FJsonObject>();
        FO->SetStringField(TEXT("name"), Item.Name.ToString());
        FO->SetNumberField(TEXT("num_params"), Item.NumParms);
        FO->SetBoolField(TEXT("blueprint_callable"), Item.bCallable);
        FO->SetBoolField(TEXT("event"), Item.bEvent);
        const FString ScriptName = Fn->GetMetaData(TEXT("ScriptName"));
        if (!ScriptName.IsEmpty()) { FO->SetStringField(TEXT("script_name"), ScriptName); }
        if (Item.bImpl) { FO->SetBoolField(TEXT("blueprint_event"), Fn->HasMetaData(TEXT("BlueprintImplementableEvent")) || Fn->HasMetaData(TEXT("BlueprintNativeEvent"))); }

        TArray<TSharedPtr<FJsonValue>> ParamArray;
        for (TFieldIterator<FProperty> Pit(Fn); Pit && Pit->HasAnyPropertyFlags(CPF_Parm); ++Pit)
        {
            FProperty* P = *Pit;
            TSharedPtr<FJsonObject> PO = MakeShared<FJsonObject>();
            PO->SetStringField(TEXT("name"), P->GetName());
            PO->SetStringField(TEXT("type"), P->GetCPPType());
            FString Dir = TEXT("in");
            if (P->HasAnyPropertyFlags(CPF_ReturnParm)) { Dir = TEXT("return"); }
            else if (P->HasAnyPropertyFlags(CPF_OutParm) && !P->HasAnyPropertyFlags(CPF_ConstParm)) { Dir = TEXT("out"); }
            PO->SetStringField(TEXT("direction"), Dir);
            if (P->HasAnyPropertyFlags(CPF_ReferenceParm)) { PO->SetBoolField(TEXT("reference"), true); }
            if (!P->HasAnyPropertyFlags(CPF_RequiredParm)) { PO->SetBoolField(TEXT("optional"), true); }
            ParamArray.Add(MakeShared<FJsonValueObject>(PO));
        }
        FO->SetArrayField(TEXT("params"), ParamArray);
        FnArray.Add(MakeShared<FJsonValueObject>(FO));
        ++Added;
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("target"), Target);
    R->SetStringField(TEXT("class"), Cls->GetName());
    R->SetStringField(TEXT("source"), Source);
    R->SetNumberField(TEXT("count"), Added);
    R->SetBoolField(TEXT("truncated"), Added < Items.Num());
    R->SetNumberField(TEXT("total_matching"), Items.Num());
    R->SetArrayField(TEXT("functions"), FnArray);
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCallActorFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName, FuncName;
    if (!Params->TryGetStringField(TEXT("actor"), ActorName) || ActorName.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("function"), FuncName) || FuncName.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function' parameter"));
    }

    UWorld* World = UnrealMCPGameDetail::GetEditorWorld();
    AActor* TargetActor = UnrealMCPGameDetail::FindActorByName(World, ActorName);
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Actor not found: %s (use find_actors_by_name para listar candidatos)"), *ActorName));
    }

    UClass* Cls = TargetActor->GetClass();
    UFunction* Fn = Cls->FindFunctionByName(FName(*FuncName));
    if (!Fn)
    {
        // Muchas funciones de Blueprint se llaman distinto en reflection
        // (ej. SetActorLocation -> K2_SetActorLocation): probar ScriptName/DisplayName.
        for (TFieldIterator<UFunction> It(Cls); It && !Fn; ++It)
        {
            UFunction* C = *It;
            const FString ScriptName = C->GetMetaData(TEXT("ScriptName"));
            const FString DisplayName = C->GetMetaData(TEXT("DisplayName"));
            if (ScriptName.Equals(FuncName, ESearchCase::IgnoreCase) ||
                DisplayName.Equals(FuncName, ESearchCase::IgnoreCase) ||
                DisplayName.Replace(TEXT(" "), TEXT("")).Equals(FuncName, ESearchCase::IgnoreCase))
            {
                Fn = C;
            }
        }
    }
    if (!Fn)
    {
        FString Names;
        int32 Shown = 0;
        for (TFieldIterator<UFunction> It(Cls); It && Shown < 8; ++It)
        {
            if ((*It)->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Event))
            {
                if (!Names.IsEmpty()) { Names += TEXT(", "); }
                Names += (*It)->GetName();
                ++Shown;
            }
        }
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Funcion '%s' no existe en %s. Candidatas: %s"), *FuncName, *Cls->GetName(), *Names));
    }

    TSharedPtr<FJsonObject> In;
    if (Params->HasField(TEXT("params")) && Params->GetObjectField(TEXT("params")).IsValid())
    {
        In = Params->GetObjectField(TEXT("params"));
    }

    TArray<FProperty*> Props;
    for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It) { Props.Add(*It); }

    // Faltantes: parametros de entrada sin valor (los out sin const son referencia).
    // Como en Blueprints: solo CPF_RequiredParm es obligatorio; el resto usa el
    // default por tipo (0/false/vacio) y se reporta en "defaults_applied".
    TArray<FString> Missing;
    TArray<FString> DefaultsApplied;
    for (FProperty* P : Props)
    {
        const bool bRet = P->HasAnyPropertyFlags(CPF_ReturnParm);
        const bool bOutOnly = P->HasAnyPropertyFlags(CPF_OutParm) && !P->HasAnyPropertyFlags(CPF_ConstParm);
        if (bRet || bOutOnly) { continue; }
        if (!In.IsValid() || !In->HasField(P->GetName()))
        {
            if (P->HasAnyPropertyFlags(CPF_RequiredParm))
            {
                Missing.Add(FString::Printf(TEXT("%s (%s)"), *P->GetName(), *P->GetCPPType()));
            }
            else
            {
                DefaultsApplied.Add(P->GetName());
            }
        }
    }
    if (Missing.Num() > 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Faltan parametros de entrada: %s"), *FString::Join(Missing, TEXT(", "))));
    }

    const int32 ParamsSize = FMath::Max((int32)Fn->ParmsSize, 1);
    uint8* Args = (uint8*)FMemory_Alloca(ParamsSize);
    FMemory::Memzero(Args, ParamsSize);
    for (FProperty* P : Props) { P->InitializeValue(P->ContainerPtrToValuePtr<void>(Args, 0)); }

    TArray<FString> SetErrors;
    for (FProperty* P : Props)
    {
        const bool bRet = P->HasAnyPropertyFlags(CPF_ReturnParm);
        const bool bOutOnly = P->HasAnyPropertyFlags(CPF_OutParm) && !P->HasAnyPropertyFlags(CPF_ConstParm);
        if (bRet || bOutOnly) { continue; }
        if (!In || !In->HasField(P->GetName())) { continue; }
        FString Err;
        if (!UnrealMCPGameDetail::SetParmFromJson(P, In->Values.FindRef(P->GetName()), Args, World, Err))
        {
            SetErrors.Add(FString::Printf(TEXT("%s: %s"), *P->GetName(), *Err));
        }
    }
    if (SetErrors.Num() > 0)
    {
        for (FProperty* P : Props) { P->DestroyValue(P->ContainerPtrToValuePtr<void>(Args, 0)); }
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parametros invalidos: %s"), *FString::Join(SetErrors, TEXT("; "))));
    }

    // Invocacion: replica el flujo interno de UObject::ProcessEvent
    // (Frame de parametros + FFrame + Invoke). Se usa este flujo manual porque
    // ProcessEvent() no ejecuta la funcion en este build del editor: todas las
    // condiciones previas (IsValid, callspace Local, game thread) pasan, pero la
    // llamada no tiene efecto; Invoke directo si lo tiene (verificado).
    uint8* Frame = (uint8*)FMemory::Malloc(FMath::Max((int32)Fn->PropertiesSize, 1), Fn->GetMinAlignment());
    FMemory::Memzero(Frame, Fn->PropertiesSize);
    if (Fn->ParmsSize) { FMemory::Memcpy(Frame, Args, Fn->ParmsSize); }
    for (FProperty* LP = Fn->FirstPropertyToInit; LP != nullptr; LP = LP->PostConstructLinkNext)
    {
        LP->InitializeValue_InContainer(Frame);
    }
    FFrame NewStack(TargetActor, Fn, Frame, nullptr, Fn->ChildProperties);
    TArray<FOutParmRec*> OutRecs;
    if (Fn->HasAnyFunctionFlags(FUNC_HasOutParms))
    {
        FOutParmRec** LastOut = &NewStack.OutParms;
        for (FProperty* P = (FProperty*)Fn->ChildProperties; P != nullptr && (P->PropertyFlags & CPF_Parm) == CPF_Parm; P = (FProperty*)P->Next)
        {
            if (P->HasAnyPropertyFlags(CPF_OutParm))
            {
                FOutParmRec* Out = new FOutParmRec();
                OutRecs.Add(Out);
                Out->PropAddr = P->ContainerPtrToValuePtr<uint8>(Args, 0);
                Out->Property = P;
                Out->NextOutParm = nullptr;
                if (*LastOut)
                {
                    (*LastOut)->NextOutParm = Out;
                    LastOut = &(*LastOut)->NextOutParm;
                }
                else
                {
                    *LastOut = Out;
                }
            }
        }
    }
    uint8* RetAddr = (Fn->ReturnValueOffset != MAX_uint16) ? (Args + Fn->ReturnValueOffset) : nullptr;
    Fn->Invoke(TargetActor, NewStack, RetAddr);
    for (FProperty* P = Fn->DestructorLink; P != nullptr; P = P->DestructorLinkNext)
    {
        if (!P->IsInContainer(Fn->ParmsSize))
        {
            P->DestroyValue_InContainer(Frame);
        }
        else if (!(P->PropertyFlags & CPF_OutParm))
        {
            FMemory::Memcpy(P->ContainerPtrToValuePtr<uint8>(Args, 0), P->ContainerPtrToValuePtr<uint8>(Frame, 0), P->ArrayDim * P->ElementSize);
        }
    }
    for (FOutParmRec* O : OutRecs) { delete O; }
    FMemory::Free(Frame);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("actor"), TargetActor->GetName());
    R->SetStringField(TEXT("class"), Cls->GetName());
    R->SetStringField(TEXT("function"), Fn->GetName());
    if (DefaultsApplied.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> JArr;
        for (const FString& N : DefaultsApplied) { JArr.Add(MakeShared<FJsonValueString>(N)); }
        R->SetArrayField(TEXT("defaults_applied"), JArr);
    }
    if (Fn->EventGraphFunction != nullptr)
    {
        R->SetStringField(TEXT("event_graph_fn"), Fn->EventGraphFunction->GetName());
    }
    for (FProperty* P : Props)
    {
        const void* Ptr = P->ContainerPtrToValuePtr<const void>(Args, 0);
        if (P->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            R->SetField(TEXT("return_value"), UnrealMCPGameDetail::ParmToJson(P, Ptr));
        }
        else if (P->HasAnyPropertyFlags(CPF_OutParm) && !P->HasAnyPropertyFlags(CPF_ConstParm))
        {
            TSharedPtr<FJsonObject> OutObj = R->HasField(TEXT("out_params")) ? R->GetObjectField(TEXT("out_params")) : MakeShared<FJsonObject>();
            OutObj->SetField(P->GetName(), UnrealMCPGameDetail::ParmToJson(P, Ptr));
            R->SetObjectField(TEXT("out_params"), OutObj);
        }
    }
    if (Fn->HasMetaData(TEXT("BlueprintImplementableEvent")) && !Fn->HasAnyFunctionFlags(FUNC_Native))
    {
        R->SetStringField(TEXT("note"), TEXT("BlueprintImplementableEvent sin implementacion: la llamada no hace nada."));
    }

    for (FProperty* P : Props) { P->DestroyValue(P->ContainerPtrToValuePtr<void>(Args, 0)); }
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetEditorState(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible")); }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);

    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (EditorWorld)
    {
        R->SetStringField(TEXT("map_name"), EditorWorld->GetOutermost()->GetName());
        R->SetBoolField(TEXT("dirty"), EditorWorld->GetOutermost()->IsDirty());
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(EditorWorld, AActor::StaticClass(), AllActors);
        R->SetNumberField(TEXT("actor_count"), AllActors.Num());
    }
    else
    {
        R->SetStringField(TEXT("map_name"), TEXT(""));
        R->SetBoolField(TEXT("dirty"), false);
        R->SetNumberField(TEXT("actor_count"), 0);
    }

    TArray<TSharedPtr<FJsonValue>> SelArr;
    if (USelection* Sel = GEditor->GetSelectedActors())
    {
        for (int32 i = 0; i < Sel->Num(); ++i)
        {
            if (AActor* A = Cast<AActor>(Sel->GetSelectedObject(i))) { SelArr.Add(MakeShared<FJsonValueString>(A->GetName())); }
        }
    }
    R->SetArrayField(TEXT("selection"), SelArr);

    R->SetBoolField(TEXT("can_undo"), GEditor->Trans && GEditor->Trans->CanUndo());
    R->SetBoolField(TEXT("can_redo"), GEditor->Trans && GEditor->Trans->CanRedo());

    R->SetBoolField(TEXT("playing"), GEditor->IsPlayingSessionInEditor());
    R->SetBoolField(TEXT("request_queued"), GEditor->IsPlaySessionRequestQueued());
    UWorld* PlayWorld = GEditor->PlayWorld;
    R->SetStringField(TEXT("play_world"), PlayWorld ? PlayWorld->GetName() : TEXT(""));
    FString WorldType;
    if (PlayWorld)
    {
        switch (PlayWorld->WorldType)
        {
        case EWorldType::Editor: WorldType = TEXT("EDITOR"); break;
        case EWorldType::Game: WorldType = TEXT("GAME"); break;
        case EWorldType::PIE: WorldType = TEXT("PIE"); break;
        default: WorldType = TEXT("OTHER"); break;
        }
    }
    R->SetStringField(TEXT("play_world_type"), WorldType);
    R->SetStringField(TEXT("pending_kind"), GLMCP_PendingKind);
    R->SetStringField(TEXT("pending_target"), GLMCP_PendingTarget);
    R->SetStringField(TEXT("last_op_error"), GLMCP_LastOpError);
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetSelection(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible")); }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    TArray<TSharedPtr<FJsonValue>> SelArr;
    if (USelection* Sel = GEditor->GetSelectedActors())
    {
        for (int32 i = 0; i < Sel->Num(); ++i)
        {
            if (AActor* A = Cast<AActor>(Sel->GetSelectedObject(i))) { SelArr.Add(MakeShared<FJsonValueString>(A->GetName())); }
        }
    }
    R->SetArrayField(TEXT("selection"), SelArr);
    R->SetNumberField(TEXT("count"), (int32)SelArr.Num());
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetSelection(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible")); }
    const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
    if (!Params || !Params->TryGetArrayField(TEXT("names"), Names))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parametro requerido: names (array de nombres de actor)"));
    }

    TMap<FString, AActor*> WorldActors;
    if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
    {
        for (TActorIterator<AActor> It(EditorWorld); It; ++It) { WorldActors.Add(It->GetName(), *It); }
    }

    USelection* Sel = GEditor->GetSelectedActors();
    if (!Sel) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor->GetSelectedActors() nulo")); }
    Sel->DeselectAll();

    TArray<TSharedPtr<FJsonValue>> Selected;
    TArray<TSharedPtr<FJsonValue>> NotFound;
    for (const TSharedPtr<FJsonValue>& V : *Names)
    {
        FString Name;
        if (V->TryGetString(Name))
        {
            if (AActor** Found = WorldActors.Find(Name)) { Sel->Select(*Found); Selected.Add(MakeShared<FJsonValueString>(Name)); }
            else { NotFound.Add(MakeShared<FJsonValueString>(Name)); }
        }
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetArrayField(TEXT("selected"), Selected);
    R->SetArrayField(TEXT("not_found"), NotFound);
    R->SetNumberField(TEXT("count"), (int32)Selected.Num());
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleUndo(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible")); }
    if (!GEditor->Trans) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Transactor (GEditor->Trans) no disponible")); }
    if (!GEditor->Trans->CanUndo())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Nada que deshacer (undo buffer vacio)"));
    }
    const bool bUndone = GEditor->UndoTransaction();
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("undone"), bUndone);
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleRedo(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible")); }
    if (!GEditor->Trans) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Transactor (GEditor->Trans) no disponible")); }
    if (!GEditor->Trans->CanRedo())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Nada que rehacer (redo buffer vacio)"));
    }
    const bool bRedone = GEditor->RedoTransaction();
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("redone"), bRedone);
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSaveLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible")); }
    bool bContent = false;
    if (Params) { Params->TryGetBoolField(TEXT("content"), bContent); }
    FString Path;
    if (Params) { Params->TryGetStringField(TEXT("path"), Path); }
    Path = Path.TrimStartAndEnd();

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No hay mundo de editor activo")); }
    const FString Pkg = World->GetOutermost()->GetName();

    // Validacion sincronica (sin dialogos): decidir el filename destino.
    FString Filename;
    if (!Path.IsEmpty())
    {
        if (!FPackageName::TryConvertLongPackageNameToFilename(Path, Filename, FPackageName::GetMapPackageExtension()))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("path invalido (se espera un paquete /Game/...): %s"), *Path));
        }
    }
    else if (Pkg.StartsWith(TEXT("/Game/")))
    {
        if (!FPackageName::TryConvertLongPackageNameToFilename(Pkg, Filename, FPackageName::GetMapPackageExtension()))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("No se pudo convertir %s a filename"), *Pkg));
        }
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Mapa todavia sin guardar (%s): pasa path=\"/Game/Maps/Nombre\" para guardarlo"), *Pkg));
    }

    // Paquete destino esperado: si SaveMap internamente duplica el mundo
    // (bPackageExists en FileHelpers.cpp), el mundo original conserva el nombre
    // viejo y hay que recargar el mapa guardado, igual que hace el editor
    // (LevelEditorActions.cpp: SaveLevelAs -> LoadMap(SavedFilename)).
    const FString TargetPkg = !Path.IsEmpty() ? Path : Pkg;

    // Se ejecuta en el proximo tick de FTSTicker (fuera de UWorld::Tick):
    // un save largo no debe correr dentro de un grupo de tick del puente.
    GLMCP_PendingKind = TEXT("save");
    GLMCP_PendingTarget = Filename;
    GLMCP_LastOpError.Empty();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [Filename, TargetPkg, bContent](float) -> bool
        {
            bool bSaved = false;
            bool bContentSaved = true;
            if (GEditor)
            {
                if (UWorld* W = GEditor->GetEditorWorldContext().World())
                {
                    bSaved = FEditorFileUtils::SaveMap(W, Filename);
                    if (bSaved)
                    {
                        FAssetRegistryModule::AssetCreated(W);
                        if (bContent) { bContentSaved = FEditorFileUtils::SaveDirtyPackages(false, false, true); }

                        UWorld* CurW = GEditor->GetEditorWorldContext().World();
                        if (CurW && CurW->GetOutermost()->GetName() != TargetPkg)
                        {
                            if (!FEditorFileUtils::LoadMap(Filename))
                            {
                                GLMCP_LastOpError = FString::Printf(
                                    TEXT("Mapa guardado en %s pero el reload al nuevo asset fallo"), *TargetPkg);
                            }
                        }
                    }
                }
            }
            else
            {
                GLMCP_LastOpError = TEXT("GEditor desaparecio durante el save diferido");
            }
            if (!bSaved && GLMCP_LastOpError.IsEmpty())
            {
                GLMCP_LastOpError = FString::Printf(TEXT("SaveMap devolvio false: %s"), *Filename);
            }
            else if (!bContentSaved && GLMCP_LastOpError.IsEmpty())
            {
                GLMCP_LastOpError = TEXT("SaveDirtyPackages(content) devolvio false");
            }
            GLMCP_PendingKind.Empty();
            GLMCP_PendingTarget.Empty();
            return false; // one-shot
        }));

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("deferred"), true);
    R->SetStringField(TEXT("path"), Filename);
    R->SetBoolField(TEXT("content"), bContent);
    R->SetStringField(TEXT("message"),
        TEXT("Save diferido: consulta editor_get_state hasta que pending_kind este vacio"));
    return R;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor no disponible")); }
    FString Path;
    if (!Params || !Params->TryGetStringField(TEXT("path"), Path) || Path.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parametro requerido: path (ej. /Game/Maps/Untitled)"));
    }
    Path = Path.TrimStartAndEnd();
    bool bDiscard = false;
    if (Params) { Params->TryGetBoolField(TEXT("discard_changes"), bDiscard); }

    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!bDiscard && EditorWorld && EditorWorld->GetOutermost()->IsDirty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Hay cambios sin guardar en %s; guarda primero (editor_save_level) o pasa discard_changes=true"),
                *EditorWorld->GetOutermost()->GetName()));
    }

    // Se ejecuta en el proximo tick de FTSTicker (fuera de UWorld::Tick):
    // LoadMap destruye el mundo; dentro de un grupo de tick del puente
    // provoca el assertion !LevelList.Contains(TickTaskLevel) y SIGSEGV.
    GLMCP_PendingKind = TEXT("open");
    GLMCP_PendingTarget = Path;
    GLMCP_LastOpError.Empty();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [Path](float) -> bool
        {
            bool bLoaded = false;
            if (GEditor)
            {
                bLoaded = FEditorFileUtils::LoadMap(Path, false, false);
            }
            else
            {
                GLMCP_LastOpError = TEXT("GEditor desaparecio durante el load diferido");
            }
            if (!bLoaded && GLMCP_LastOpError.IsEmpty())
            {
                GLMCP_LastOpError = FString::Printf(TEXT("LoadMap devolvio false: %s"), *Path);
            }
            GLMCP_PendingKind.Empty();
            GLMCP_PendingTarget.Empty();
            return false; // one-shot
        }));

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("deferred"), true);
    R->SetStringField(TEXT("path"), Path);
    R->SetStringField(TEXT("message"),
        TEXT("Open diferido: consulta editor_get_state hasta que pending_kind este vacio"));
    return R;
}

