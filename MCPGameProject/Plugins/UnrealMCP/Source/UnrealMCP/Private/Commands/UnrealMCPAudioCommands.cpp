#include "Commands/UnrealMCPAudioCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "PackageTools.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

// Audio-specific includes
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/AmbientSound.h"
#include "Components/AudioComponent.h"

FUnrealMCPAudioCommands::FUnrealMCPAudioCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_sound_attenuation"))
    {
        return HandleCreateSoundAttenuation(Params);
    }
    else if (CommandType == TEXT("create_sound_class"))
    {
        return HandleCreateSoundClass(Params);
    }
    else if (CommandType == TEXT("create_sound_cue"))
    {
        return HandleCreateSoundCue(Params);
    }
    else if (CommandType == TEXT("spawn_ambient_sound"))
    {
        return HandleSpawnAmbientSound(Params);
    }
    else if (CommandType == TEXT("set_ambient_sound_properties"))
    {
        return HandleSetAmbientSoundProperties(Params);
    }
    else if (CommandType == TEXT("play_sound_2d"))
    {
        return HandlePlaySound2D(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown audio command: %s"), *CommandType));
}

// =====================================================================
// create_sound_attenuation
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleCreateSoundAttenuation(const TSharedPtr<FJsonObject>& Params)
{
    FString AttenuationName;
    if (!Params->TryGetStringField(TEXT("name"), AttenuationName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString FolderPath = TEXT("/Game/Audio/Attenuation");
    Params->TryGetStringField(TEXT("path"), FolderPath);

    double InnerRadius = 400.0;
    Params->TryGetNumberField(TEXT("inner_radius"), InnerRadius);

    double FalloffDistance = 4000.0;
    Params->TryGetNumberField(TEXT("falloff_distance"), FalloffDistance);

    // Sphere | Capsule | Box | Cone
    FString ShapeString = TEXT("Sphere");
    Params->TryGetStringField(TEXT("shape"), ShapeString);

    FString PackageName = FolderPath / AttenuationName;
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    if (UEditorAssetLibrary::DoesAssetExist(PackageName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("SoundAttenuation '%s' already exists"), *AttenuationName));
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for SoundAttenuation"));
    }

    USoundAttenuation* NewAttenuation = NewObject<USoundAttenuation>(Package, FName(*AttenuationName), RF_Public | RF_Standalone);
    if (!NewAttenuation)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create SoundAttenuation asset"));
    }

    NewAttenuation->Attenuation.bAttenuate = true;
    NewAttenuation->Attenuation.bSpatialize = true;

    if (ShapeString == TEXT("Box"))
    {
        NewAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Box;
    }
    else if (ShapeString == TEXT("Capsule"))
    {
        NewAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Capsule;
    }
    else if (ShapeString == TEXT("Cone"))
    {
        NewAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Cone;
    }
    else
    {
        NewAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    }

    // For Sphere, X of AttenuationShapeExtents is the inner (full-volume) radius.
    // NOTE: field name confirmed against UE5.3 FBaseAttenuationSettings; if your
    // 5.5 checkout renamed this, the compiler error will point straight at it.
    NewAttenuation->Attenuation.AttenuationShapeExtents = FVector(InnerRadius, InnerRadius, InnerRadius);
    NewAttenuation->Attenuation.FalloffDistance = FalloffDistance;

    NewAttenuation->PostEditChange();
    NewAttenuation->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewAttenuation);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, NewAttenuation, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("attenuation_path"), NewAttenuation->GetPathName());
    ResultObj->SetNumberField(TEXT("inner_radius"), InnerRadius);
    ResultObj->SetNumberField(TEXT("falloff_distance"), FalloffDistance);
    ResultObj->SetStringField(TEXT("shape"), ShapeString);
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// create_sound_class
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleCreateSoundClass(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName;
    if (!Params->TryGetStringField(TEXT("name"), ClassName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString FolderPath = TEXT("/Game/Audio/Classes");
    Params->TryGetStringField(TEXT("path"), FolderPath);

    double Volume = 1.0;
    Params->TryGetNumberField(TEXT("volume"), Volume);

    double Pitch = 1.0;
    Params->TryGetNumberField(TEXT("pitch"), Pitch);

    // Optional: full asset path of an existing SoundClass to register this one under
    FString ParentClassPath;
    Params->TryGetStringField(TEXT("parent_class_path"), ParentClassPath);

    FString PackageName = FolderPath / ClassName;
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    if (UEditorAssetLibrary::DoesAssetExist(PackageName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("SoundClass '%s' already exists"), *ClassName));
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for SoundClass"));
    }

    USoundClass* NewSoundClass = NewObject<USoundClass>(Package, FName(*ClassName), RF_Public | RF_Standalone);
    if (!NewSoundClass)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create SoundClass asset"));
    }

    NewSoundClass->Properties.Volume = Volume;
    NewSoundClass->Properties.Pitch = Pitch;

    bool bLinkedToParent = false;
    if (!ParentClassPath.IsEmpty())
    {
        // SoundClass hierarchy lives on the PARENT (ChildClasses array), not on the child.
        USoundClass* ParentClass = LoadObject<USoundClass>(nullptr, *ParentClassPath);
        if (ParentClass)
        {
            ParentClass->ChildClasses.Add(NewSoundClass);
            ParentClass->MarkPackageDirty();
            bLinkedToParent = true;
        }
    }

    NewSoundClass->PostEditChange();
    NewSoundClass->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewSoundClass);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, NewSoundClass, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("sound_class_path"), NewSoundClass->GetPathName());
    ResultObj->SetNumberField(TEXT("volume"), Volume);
    ResultObj->SetNumberField(TEXT("pitch"), Pitch);
    ResultObj->SetBoolField(TEXT("linked_to_parent"), bLinkedToParent);
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// create_sound_cue  (single WavePlayer node — no branching/mixing graph)
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleCreateSoundCue(const TSharedPtr<FJsonObject>& Params)
{
    FString CueName;
    if (!Params->TryGetStringField(TEXT("name"), CueName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString SoundWavePath;
    if (!Params->TryGetStringField(TEXT("sound_wave_path"), SoundWavePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'sound_wave_path' parameter"));
    }

    FString FolderPath = TEXT("/Game/Audio/Cues");
    Params->TryGetStringField(TEXT("path"), FolderPath);

    bool bLooping = false;
    Params->TryGetBoolField(TEXT("looping"), bLooping);

    USoundWave* Wave = LoadObject<USoundWave>(nullptr, *SoundWavePath);
    if (!Wave)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("SoundWave not found: %s"), *SoundWavePath));
    }

    FString PackageName = FolderPath / CueName;
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    if (UEditorAssetLibrary::DoesAssetExist(PackageName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("SoundCue '%s' already exists"), *CueName));
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for SoundCue"));
    }

    USoundCue* NewCue = NewObject<USoundCue>(Package, FName(*CueName), RF_Public | RF_Standalone);
    if (!NewCue)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create SoundCue asset"));
    }

#if WITH_EDITOR
    // ConstructSoundNode is the same call the SoundCue editor itself uses when you
    // drag a node into the graph — it registers the node with the cue and (in editor
    // builds) creates the matching graph-visual node so it shows up correctly if you
    // open the asset afterwards.
    USoundNodeWavePlayer* WavePlayerNode = (
        NewCue->ConstructSoundNode<USoundNodeWavePlayer>(USoundNodeWavePlayer::StaticClass(), false));
    if (!WavePlayerNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to construct WavePlayer sound node"));
    }
    WavePlayerNode->SetSoundWave(Wave);
    WavePlayerNode->bLooping = bLooping;

    NewCue->FirstNode = WavePlayerNode;
    NewCue->LinkGraphNodesFromSoundNodes();
#endif

    NewCue->PostEditChange();
    NewCue->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewCue);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, NewCue, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("sound_cue_path"), NewCue->GetPathName());
    ResultObj->SetStringField(TEXT("sound_wave"), SoundWavePath);
    ResultObj->SetBoolField(TEXT("looping"), bLooping);
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// spawn_ambient_sound
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleSpawnAmbientSound(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FVector Location(0.0f, 0.0f, 0.0f);
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Same "already exists" guard used by spawn_actor
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
    AAmbientSound* NewActor = World->SpawnActor<AAmbientSound>(AAmbientSound::StaticClass(), Location, Rotation, SpawnParams);
    if (!NewActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn AmbientSound actor"));
    }

    UAudioComponent* AudioComp = NewActor->GetAudioComponent();
    if (!AudioComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AmbientSound actor has no AudioComponent"));
    }

    FString SoundPath;
    bool bSoundAssigned = false;
    if (Params->TryGetStringField(TEXT("sound_path"), SoundPath) && !SoundPath.IsEmpty())
    {
        USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
        if (!Sound)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Sound asset not found: %s"), *SoundPath));
        }
        AudioComp->SetSound(Sound);
        bSoundAssigned = true;
    }

    double VolumeMultiplier = 1.0;
    Params->TryGetNumberField(TEXT("volume_multiplier"), VolumeMultiplier);
    AudioComp->VolumeMultiplier = VolumeMultiplier;

    double PitchMultiplier = 1.0;
    Params->TryGetNumberField(TEXT("pitch_multiplier"), PitchMultiplier);
    AudioComp->PitchMultiplier = PitchMultiplier;

    bool bAutoActivate = true;
    Params->TryGetBoolField(TEXT("auto_activate"), bAutoActivate);
    AudioComp->bAutoActivate = bAutoActivate;

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), NewActor->GetName());
    ResultObj->SetBoolField(TEXT("sound_assigned"), bSoundAssigned);
    ResultObj->SetNumberField(TEXT("volume_multiplier"), VolumeMultiplier);
    ResultObj->SetNumberField(TEXT("pitch_multiplier"), PitchMultiplier);
    ResultObj->SetBoolField(TEXT("auto_activate"), bAutoActivate);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// set_ambient_sound_properties
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandleSetAmbientSoundProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    AAmbientSound* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Cast<AAmbientSound>(Actor);
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("AmbientSound actor not found: %s"), *ActorName));
    }

    UAudioComponent* AudioComp = TargetActor->GetAudioComponent();
    if (!AudioComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AmbientSound actor has no AudioComponent"));
    }

    bool bAnyChange = false;

    FString SoundPath;
    if (Params->TryGetStringField(TEXT("sound_path"), SoundPath) && !SoundPath.IsEmpty())
    {
        USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
        if (!Sound)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Sound asset not found: %s"), *SoundPath));
        }
        AudioComp->SetSound(Sound);
        bAnyChange = true;
    }

    double VolumeMultiplier;
    if (Params->TryGetNumberField(TEXT("volume_multiplier"), VolumeMultiplier))
    {
        AudioComp->VolumeMultiplier = VolumeMultiplier;
        bAnyChange = true;
    }

    double PitchMultiplier;
    if (Params->TryGetNumberField(TEXT("pitch_multiplier"), PitchMultiplier))
    {
        AudioComp->PitchMultiplier = PitchMultiplier;
        bAnyChange = true;
    }

    FString AttenuationPath;
    if (Params->TryGetStringField(TEXT("attenuation_path"), AttenuationPath) && !AttenuationPath.IsEmpty())
    {
        USoundAttenuation* Attenuation = LoadObject<USoundAttenuation>(nullptr, *AttenuationPath);
        if (!Attenuation)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("SoundAttenuation asset not found: %s"), *AttenuationPath));
        }
        AudioComp->AttenuationSettings = Attenuation;
        bAnyChange = true;
    }

    bool bIsUISound;
    if (Params->TryGetBoolField(TEXT("is_ui_sound"), bIsUISound))
    {
        AudioComp->bIsUISound = bIsUISound;
        bAnyChange = true;
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetBoolField(TEXT("changed"), bAnyChange);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// play_sound_2d  (quick test playback, not tied to any actor)
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPAudioCommands::HandlePlaySound2D(const TSharedPtr<FJsonObject>& Params)
{
    FString SoundPath;
    if (!Params->TryGetStringField(TEXT("sound_path"), SoundPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'sound_path' parameter"));
    }

    USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
    if (!Sound)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Sound asset not found: %s"), *SoundPath));
    }

    double VolumeMultiplier = 1.0;
    Params->TryGetNumberField(TEXT("volume_multiplier"), VolumeMultiplier);
    double PitchMultiplier = 1.0;
    Params->TryGetNumberField(TEXT("pitch_multiplier"), PitchMultiplier);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // NOTE: PlaySound2D in a plain editor world (not PIE) audibly plays through the
    // editor's audio device on most setups, but this is worth a quick manual check
    // on your machine — some editor configurations mute non-PIE playback.
    UGameplayStatics::PlaySound2D(World, Sound, VolumeMultiplier, PitchMultiplier);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("sound_path"), SoundPath);
    ResultObj->SetNumberField(TEXT("volume_multiplier"), VolumeMultiplier);
    ResultObj->SetNumberField(TEXT("pitch_multiplier"), PitchMultiplier);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}
