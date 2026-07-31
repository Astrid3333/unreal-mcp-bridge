#include "Commands/UnrealMCPSequencerCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Components/LightComponent.h"

FUnrealMCPSequencerCommands::FUnrealMCPSequencerCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPSequencerCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_level_sequence"))
    {
        return HandleCreateLevelSequence(Params);
    }
    else if (CommandType == TEXT("add_actor_to_sequence"))
    {
        return HandleAddActorToSequence(Params);
    }
    else if (CommandType == TEXT("add_camera_cut_track"))
    {
        return HandleAddCameraCutTrack(Params);
    }
    else if (CommandType == TEXT("set_playback_range"))
    {
        return HandleSetPlaybackRange(Params);
    }
    else if (CommandType == TEXT("open_level_sequence"))
    {
        return HandleOpenLevelSequence(Params);
    }
    else if (CommandType == TEXT("add_transform_keyframe"))
    {
        return HandleAddTransformKeyframe(Params);
    }
    else if (CommandType == TEXT("add_property_keyframe"))
    {
        return HandleAddPropertyKeyframe(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown sequencer command: %s"), *CommandType));
}

// Shared helper: find an existing possessable's Guid by actor name.
static bool FindPossessableGuidByName(UMovieScene* MovieScene, const FString& ActorName, FGuid& OutGuid)
{
    int32 Count = MovieScene->GetPossessableCount();
    for (int32 i = 0; i < Count; ++i)
    {
        const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);
        if (Possessable.GetName() == ActorName)
        {
            OutGuid = Possessable.GetGuid();
            return true;
        }
    }
    return false;
}

// =====================================================================
// create_level_sequence
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPSequencerCommands::HandleCreateLevelSequence(const TSharedPtr<FJsonObject>& Params)
{
    FString Name;
    if (!Params->TryGetStringField(TEXT("name"), Name))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }
    FString PackagePath = TEXT("/Game/Cinematics");
    Params->TryGetStringField(TEXT("package_path"), PackagePath);

    double FpsValue = 30.0;
    Params->TryGetNumberField(TEXT("fps"), FpsValue);
    double LengthSeconds = 5.0;
    Params->TryGetNumberField(TEXT("length_seconds"), LengthSeconds);

    FString PackageName = PackagePath / Name;
    if (FPackageName::DoesPackageExist(PackageName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("A package already exists at: %s"), *PackageName));
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    ULevelSequence* NewSequence = NewObject<ULevelSequence>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
    if (!NewSequence)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create LevelSequence"));
    }
    NewSequence->Initialize();

    UMovieScene* MovieScene = NewSequence->GetMovieScene();
    if (!MovieScene)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    FFrameRate DisplayRate(static_cast<int32>(FpsValue), 1);
    MovieScene->SetDisplayRate(DisplayRate);

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameNumber EndFrame = (LengthSeconds * TickResolution).RoundToFrame();
    MovieScene->SetPlaybackRange(TRange<FFrameNumber>(FFrameNumber(0), EndFrame));

    FAssetRegistryModule::AssetCreated(NewSequence);
    Package->MarkPackageDirty();

    FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, NewSequence, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("sequence_path"), PackageName + TEXT(".") + Name);
    ResultObj->SetNumberField(TEXT("fps"), FpsValue);
    ResultObj->SetNumberField(TEXT("length_seconds"), LengthSeconds);
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// add_actor_to_sequence
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPSequencerCommands::HandleAddActorToSequence(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'sequence_path' parameter"));
    }
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }
    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    FGuid ExistingGuid;
    if (FindPossessableGuidByName(MovieScene, ActorName, ExistingGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' is already bound in this sequence"), *ActorName));
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
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found in level: %s"), *ActorName));
    }

    FGuid NewGuid = MovieScene->AddPossessable(ActorName, TargetActor->GetClass());
    Sequence->BindPossessableObject(NewGuid, *TargetActor, TargetActor->GetWorld());
    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("sequence_path"), SequencePath);
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetStringField(TEXT("binding_guid"), NewGuid.ToString());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// add_camera_cut_track -- NOTE: FMovieSceneObjectBindingID's constructor
// signature drifted across 5.x point releases. If this line fails to
// compile, grep the header (see instructions) and we adjust just this
// one call.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPSequencerCommands::HandleAddCameraCutTrack(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'sequence_path' parameter"));
    }
    FString CameraActorName;
    if (!Params->TryGetStringField(TEXT("camera_actor_name"), CameraActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'camera_actor_name' parameter"));
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }
    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    FGuid CameraGuid;
    if (!FindPossessableGuidByName(MovieScene, CameraActorName, CameraGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Camera actor '%s' is not bound in this sequence yet -- call add_actor_to_sequence first"), *CameraActorName));
    }

    UMovieSceneCameraCutTrack* CutTrack = MovieScene->FindTrack<UMovieSceneCameraCutTrack>();
    if (!CutTrack)
    {
        CutTrack = MovieScene->AddTrack<UMovieSceneCameraCutTrack>();
    }
    if (!CutTrack)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Camera Cut Track"));
    }

    UMovieSceneCameraCutSection* NewSection = NewObject<UMovieSceneCameraCutSection>(CutTrack, NAME_None, RF_Transactional);
    NewSection->SetCameraBindingID(FMovieSceneObjectBindingID(CameraGuid));
    NewSection->SetRange(MovieScene->GetPlaybackRange());
    CutTrack->AddSection(*NewSection);

    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("sequence_path"), SequencePath);
    ResultObj->SetStringField(TEXT("camera_actor_name"), CameraActorName);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// set_playback_range
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPSequencerCommands::HandleSetPlaybackRange(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'sequence_path' parameter"));
    }
    double StartSeconds = 0.0;
    Params->TryGetNumberField(TEXT("start_seconds"), StartSeconds);
    double EndSeconds = 5.0;
    if (!Params->TryGetNumberField(TEXT("end_seconds"), EndSeconds))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'end_seconds' parameter"));
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }
    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameNumber StartFrame = (StartSeconds * TickResolution).RoundToFrame();
    FFrameNumber EndFrame = (EndSeconds * TickResolution).RoundToFrame();
    MovieScene->SetPlaybackRange(TRange<FFrameNumber>(StartFrame, EndFrame));
    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("sequence_path"), SequencePath);
    ResultObj->SetNumberField(TEXT("start_seconds"), StartSeconds);
    ResultObj->SetNumberField(TEXT("end_seconds"), EndSeconds);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// open_level_sequence
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPSequencerCommands::HandleOpenLevelSequence(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'sequence_path' parameter"));
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get AssetEditorSubsystem"));
    }
    AssetEditorSubsystem->OpenEditorForAsset(Sequence);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("sequence_path"), SequencePath);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}


// =====================================================================
// add_transform_keyframe
// NOTE: con LWC activo, los canales de transform son FMovieSceneDoubleChannel
// (no Float). Si GetChannels<FMovieSceneDoubleChannel>() devuelve menos de 9,
// o AddKey no compila con esta firma, grepear MovieSceneDoubleChannel.h
// buscando "AddKey" y ajustar solo esa linea.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPSequencerCommands::HandleAddTransformKeyframe(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'sequence_path' parameter"));
    }
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    double TimeSeconds = 0.0;
    if (!Params->TryGetNumberField(TEXT("time_seconds"), TimeSeconds))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'time_seconds' parameter"));
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }
    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    FGuid ActorGuid;
    if (!FindPossessableGuidByName(MovieScene, ActorName, ActorGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' is not bound in this sequence yet -- call add_actor_to_sequence first"), *ActorName));
    }

    UMovieScene3DTransformTrack* TransformTrack = MovieScene->FindTrack<UMovieScene3DTransformTrack>(ActorGuid);
    if (!TransformTrack)
    {
        TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(ActorGuid);
    }
    if (!TransformTrack)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create transform track"));
    }

    UMovieScene3DTransformSection* TransformSection = nullptr;
    if (TransformTrack->GetAllSections().Num() > 0)
    {
        TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->GetAllSections()[0]);
    }
    else
    {
        TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
        TransformSection->SetRange(TRange<FFrameNumber>::All());
        TransformTrack->AddSection(*TransformSection);
    }
    if (!TransformSection)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get/create transform section"));
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameNumber KeyTime = (TimeSeconds * TickResolution).RoundToFrame();

    TArrayView<FMovieSceneDoubleChannel*> Channels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
    if (Channels.Num() < 9)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Expected 9 transform channels, got %d -- revisar MovieScene3DTransformSection.h"), Channels.Num()));
    }

    bool bKeyedAny = false;
    const TArray<TSharedPtr<FJsonValue>>* LocationArr;
    if (Params->TryGetArrayField(TEXT("location"), LocationArr) && LocationArr->Num() == 3)
    {
        for (int32 i = 0; i < 3; ++i) { Channels[i]->GetData().AddKey(KeyTime, FMovieSceneDoubleValue((*LocationArr)[i]->AsNumber())); }
        bKeyedAny = true;
    }
    const TArray<TSharedPtr<FJsonValue>>* RotationArr;
    if (Params->TryGetArrayField(TEXT("rotation"), RotationArr) && RotationArr->Num() == 3)
    {
        for (int32 i = 0; i < 3; ++i) { Channels[3 + i]->GetData().AddKey(KeyTime, FMovieSceneDoubleValue((*RotationArr)[i]->AsNumber())); }
        bKeyedAny = true;
    }
    const TArray<TSharedPtr<FJsonValue>>* ScaleArr;
    if (Params->TryGetArrayField(TEXT("scale"), ScaleArr) && ScaleArr->Num() == 3)
    {
        for (int32 i = 0; i < 3; ++i) { Channels[6 + i]->GetData().AddKey(KeyTime, FMovieSceneDoubleValue((*ScaleArr)[i]->AsNumber())); }
        bKeyedAny = true;
    }

    if (!bKeyedAny)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Provide at least one of 'location', 'rotation', 'scale' as a 3-element array"));
    }

    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("sequence_path"), SequencePath);
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetNumberField(TEXT("time_seconds"), TimeSeconds);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}


// =====================================================================
// add_property_keyframe
// Anima una propiedad float de un COMPONENTE del actor (ej. Intensity
// de una luz), no del actor en si. Crea un possessable hijo para el
// componente si no existe, y un UMovieSceneFloatTrack sobre ese hijo.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPSequencerCommands::HandleAddPropertyKeyframe(const TSharedPtr<FJsonObject>& Params)
{
    FString SequencePath;
    if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'sequence_path' parameter"));
    }
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter (e.g. 'Intensity')"));
    }
    double TimeSeconds = 0.0;
    if (!Params->TryGetNumberField(TEXT("time_seconds"), TimeSeconds))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'time_seconds' parameter"));
    }
    double Value = 0.0;
    if (!Params->TryGetNumberField(TEXT("value"), Value))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }

    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
    if (!Sequence)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("LevelSequence not found: %s"), *SequencePath));
    }
    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelSequence has no MovieScene"));
    }

    FGuid ActorGuid;
    if (!FindPossessableGuidByName(MovieScene, ActorName, ActorGuid))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' is not bound in this sequence yet -- call add_actor_to_sequence first"), *ActorName));
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
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found in level: %s"), *ActorName));
    }

    ULightComponent* LightComp = TargetActor->FindComponentByClass<ULightComponent>();
    if (!LightComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no LightComponent -- este comando por ahora solo soporta propiedades de luz"), *ActorName));
    }

    FGuid ComponentGuid;
    FString ComponentName = LightComp->GetName();
    bool bFoundComponent = FindPossessableGuidByName(MovieScene, ComponentName, ComponentGuid);

    if (!bFoundComponent)
    {
        ComponentGuid = MovieScene->AddPossessable(ComponentName, LightComp->GetClass());
        Sequence->BindPossessableObject(ComponentGuid, *LightComp, TargetActor);

        FMovieScenePossessable* ComponentPossessable = MovieScene->FindPossessable(ComponentGuid);
        if (ComponentPossessable)
        {
            ComponentPossessable->SetParent(ActorGuid, MovieScene);
        }
    }

    UMovieSceneFloatTrack* PropertyTrack = MovieScene->FindTrack<UMovieSceneFloatTrack>(ComponentGuid);
    if (!PropertyTrack)
    {
        PropertyTrack = MovieScene->AddTrack<UMovieSceneFloatTrack>(ComponentGuid);
        if (PropertyTrack)
        {
            PropertyTrack->SetPropertyNameAndPath(*PropertyName, PropertyName);
        }
    }
    if (!PropertyTrack)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create property track"));
    }

    UMovieSceneFloatSection* PropertySection = nullptr;
    if (PropertyTrack->GetAllSections().Num() > 0)
    {
        PropertySection = Cast<UMovieSceneFloatSection>(PropertyTrack->GetAllSections()[0]);
    }
    else
    {
        PropertySection = Cast<UMovieSceneFloatSection>(PropertyTrack->CreateNewSection());
        PropertySection->SetRange(TRange<FFrameNumber>::All());
        PropertyTrack->AddSection(*PropertySection);
    }
    if (!PropertySection)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get/create property section"));
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameNumber KeyTime = (TimeSeconds * TickResolution).RoundToFrame();

    TArrayView<FMovieSceneFloatChannel*> Channels = PropertySection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
    if (Channels.Num() < 1)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Property section has no float channels"));
    }
    Channels[0]->GetData().AddKey(KeyTime, FMovieSceneFloatValue(static_cast<float>(Value)));

    Sequence->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("sequence_path"), SequencePath);
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetStringField(TEXT("component_name"), ComponentName);
    ResultObj->SetStringField(TEXT("property_name"), PropertyName);
    ResultObj->SetNumberField(TEXT("time_seconds"), TimeSeconds);
    ResultObj->SetNumberField(TEXT("value"), Value);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}
