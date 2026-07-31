#include "Commands/UnrealMCPAICommands.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "AIController.h"

#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshBoundsVolume.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

FUnrealMCPAICommands::FUnrealMCPAICommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPAICommands::MakeError(const FString& Message)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("status"), TEXT("error"));
    Result->SetStringField(TEXT("error"), Message);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAICommands::MakeSuccess()
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("status"), TEXT("success"));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAICommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_navmesh_info"))
    {
        return HandleGetNavMeshInfo(Params);
    }
    else if (CommandType == TEXT("build_navigation"))
    {
        return HandleBuildNavigation(Params);
    }
    else if (CommandType == TEXT("find_path"))
    {
        return HandleFindPath(Params);
    }
    else if (CommandType == TEXT("create_behavior_tree"))
    {
        return HandleCreateBehaviorTree(Params);
    }
    else if (CommandType == TEXT("create_blackboard"))
    {
        return HandleCreateBlackboard(Params);
    }
    else if (CommandType == TEXT("add_blackboard_key"))
    {
        return HandleAddBlackboardKey(Params);
    }
    else if (CommandType == TEXT("run_behavior_tree_on_actor"))
    {
        return HandleRunBehaviorTreeOnActor(Params);
    }

    return MakeError(FString::Printf(TEXT("Unknown AI command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// NavMesh
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPAICommands::HandleGetNavMeshInfo(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return MakeError(TEXT("No editor world available"));
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSys)
    {
        return MakeError(TEXT("No NavigationSystem in this world (no NavMeshBoundsVolume placed yet?)"));
    }

    // NOTE: GetDefaultNavDataInstance can return nullptr if no nav data has been
    // generated yet (e.g. no NavMeshBoundsVolume in the level, or nav mesh never built).
    ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
    if (!NavMesh)
    {
        return MakeError(TEXT("No RecastNavMesh found. Place a NavMeshBoundsVolume in the level and rebuild navigation first."));
    }

    TSharedPtr<FJsonObject> Result = MakeSuccess();
    Result->SetNumberField(TEXT("cell_size"), NavMesh->CellSize);
    Result->SetNumberField(TEXT("cell_height"), NavMesh->CellHeight);
    Result->SetNumberField(TEXT("agent_radius"), NavMesh->AgentRadius);
    Result->SetNumberField(TEXT("agent_height"), NavMesh->AgentHeight);
    Result->SetNumberField(TEXT("agent_max_step_height"), NavMesh->AgentMaxStepHeight);
    Result->SetNumberField(TEXT("agent_max_slope"), NavMesh->AgentMaxSlope);

    // GetNavigableBounds() returns one FBox per nav-mesh tile/region in 5.3, so we
    // accumulate them into a single enclosing box.
    TArray<FBox> BoundsArray = NavMesh->GetNavigableBounds();
    FBox Bounds(ForceInit);
    for (const FBox& TileBox : BoundsArray)
    {
        Bounds += TileBox;
    }
    TSharedPtr<FJsonObject> BoundsJson = MakeShared<FJsonObject>();
    BoundsJson->SetNumberField(TEXT("min_x"), Bounds.Min.X);
    BoundsJson->SetNumberField(TEXT("min_y"), Bounds.Min.Y);
    BoundsJson->SetNumberField(TEXT("min_z"), Bounds.Min.Z);
    BoundsJson->SetNumberField(TEXT("max_x"), Bounds.Max.X);
    BoundsJson->SetNumberField(TEXT("max_y"), Bounds.Max.Y);
    BoundsJson->SetNumberField(TEXT("max_z"), Bounds.Max.Z);
    Result->SetObjectField(TEXT("bounds"), BoundsJson);

    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAICommands::HandleBuildNavigation(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return MakeError(TEXT("No editor world available"));
    }

    // NOTE: this is the static rebuild entry point used by the editor's "Build Paths" button.
    // In some engine versions this is exposed instead as NavSys->Build() (instance method).
    // If FNavigationSystem::Build doesn't resolve, swap to:
    //   UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    //   NavSys->Build();
    FNavigationSystem::Build(*World);

    return MakeSuccess();
}

TSharedPtr<FJsonObject> FUnrealMCPAICommands::HandleFindPath(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return MakeError(TEXT("No editor world available"));
    }

    const TArray<TSharedPtr<FJsonValue>>* StartArr;
    const TArray<TSharedPtr<FJsonValue>>* EndArr;
    if (!Params->TryGetArrayField(TEXT("start"), StartArr) || StartArr->Num() != 3 ||
        !Params->TryGetArrayField(TEXT("end"), EndArr) || EndArr->Num() != 3)
    {
        return MakeError(TEXT("'start' and 'end' must each be [x, y, z] arrays"));
    }

    FVector Start((*StartArr)[0]->AsNumber(), (*StartArr)[1]->AsNumber(), (*StartArr)[2]->AsNumber());
    FVector End((*EndArr)[0]->AsNumber(), (*EndArr)[1]->AsNumber(), (*EndArr)[2]->AsNumber());

    // On this 5.3 build, FindPathToLocationSynchronously returns a UNavigationPath*
    // (UObject-based helper) rather than an FPathFindingResult by value.
    UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, End);

    if (!NavPath || !NavPath->IsValid())
    {
        return MakeError(TEXT("No path found between the given points"));
    }

    TArray<TSharedPtr<FJsonValue>> PointsJson;
    for (const FVector& Point : NavPath->PathPoints)
    {
        TArray<TSharedPtr<FJsonValue>> XYZ;
        XYZ.Add(MakeShared<FJsonValueNumber>(Point.X));
        XYZ.Add(MakeShared<FJsonValueNumber>(Point.Y));
        XYZ.Add(MakeShared<FJsonValueNumber>(Point.Z));
        PointsJson.Add(MakeShared<FJsonValueArray>(XYZ));
    }

    TSharedPtr<FJsonObject> Out = MakeSuccess();
    Out->SetArrayField(TEXT("path_points"), PointsJson);
    Out->SetBoolField(TEXT("is_partial"), NavPath->IsPartial());
    return Out;
}

// ---------------------------------------------------------------------------
// Behavior Tree / Blackboard
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FUnrealMCPAICommands::HandleCreateBehaviorTree(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetName, PackagePath;
    if (!Params->TryGetStringField(TEXT("name"), AssetName))
    {
        return MakeError(TEXT("Missing 'name'"));
    }
    Params->TryGetStringField(TEXT("path"), PackagePath);
    if (PackagePath.IsEmpty())
    {
        PackagePath = TEXT("/Game/AI");
    }

    FString PackageName = PackagePath / AssetName;

    // NOTE: creating BT/Blackboard assets without going through IAssetTools/UFactory.
    // This works (UBehaviorTree and UBlackboardData have no special factory requirements
    // beyond default construction) but if you later want this asset to open cleanly with
    // full editor metadata (e.g. thumbnail, initial graph root node) you may prefer routing
    // through UBehaviorTreeGraphFactory-equivalent asset tools instead.
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("Failed to create package for Behavior Tree asset"));
    }

    UBehaviorTree* NewBT = NewObject<UBehaviorTree>(Package, *AssetName, RF_Public | RF_Standalone);
    if (!NewBT)
    {
        return MakeError(TEXT("Failed to create UBehaviorTree object"));
    }

    // Optional: link a blackboard asset by path if provided
    FString BlackboardPath;
    if (Params->TryGetStringField(TEXT("blackboard_path"), BlackboardPath) && !BlackboardPath.IsEmpty())
    {
        UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
        if (BB)
        {
            NewBT->BlackboardAsset = BB;
        }
    }

    FAssetRegistryModule::AssetCreated(NewBT);
    Package->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeSuccess();
    Result->SetStringField(TEXT("asset_path"), PackageName);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAICommands::HandleCreateBlackboard(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetName, PackagePath;
    if (!Params->TryGetStringField(TEXT("name"), AssetName))
    {
        return MakeError(TEXT("Missing 'name'"));
    }
    Params->TryGetStringField(TEXT("path"), PackagePath);
    if (PackagePath.IsEmpty())
    {
        PackagePath = TEXT("/Game/AI");
    }

    FString PackageName = PackagePath / AssetName;
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("Failed to create package for Blackboard asset"));
    }

    UBlackboardData* NewBB = NewObject<UBlackboardData>(Package, *AssetName, RF_Public | RF_Standalone);
    if (!NewBB)
    {
        return MakeError(TEXT("Failed to create UBlackboardData object"));
    }

    FAssetRegistryModule::AssetCreated(NewBB);
    Package->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeSuccess();
    Result->SetStringField(TEXT("asset_path"), PackageName);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAICommands::HandleAddBlackboardKey(const TSharedPtr<FJsonObject>& Params)
{
    FString BlackboardPath, KeyName, KeyType;
    if (!Params->TryGetStringField(TEXT("blackboard_path"), BlackboardPath) ||
        !Params->TryGetStringField(TEXT("key_name"), KeyName) ||
        !Params->TryGetStringField(TEXT("key_type"), KeyType))
    {
        return MakeError(TEXT("Missing 'blackboard_path', 'key_name' or 'key_type'"));
    }

    UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
    if (!BB)
    {
        return MakeError(FString::Printf(TEXT("Could not load Blackboard at %s"), *BlackboardPath));
    }

    // NOTE: Keys is a public UPROPERTY(EditAnywhere) TArray<FBlackboardEntry> on UBlackboardData
    // as of 5.3, so direct array manipulation + MarkPackageDirty is safe. If a future version
    // makes this private, use the Blackboard editor's own AddKey-equivalent utility instead.
    FBlackboardEntry Entry;
    Entry.EntryName = FName(*KeyName);

    UBlackboardKeyType* NewKeyType = nullptr;
    if (KeyType == TEXT("Bool"))       NewKeyType = NewObject<UBlackboardKeyType_Bool>(BB);
    else if (KeyType == TEXT("Int"))   NewKeyType = NewObject<UBlackboardKeyType_Int>(BB);
    else if (KeyType == TEXT("Float")) NewKeyType = NewObject<UBlackboardKeyType_Float>(BB);
    else if (KeyType == TEXT("Vector")) NewKeyType = NewObject<UBlackboardKeyType_Vector>(BB);
    else if (KeyType == TEXT("Object")) NewKeyType = NewObject<UBlackboardKeyType_Object>(BB);
    else if (KeyType == TEXT("String")) NewKeyType = NewObject<UBlackboardKeyType_String>(BB);
    else
    {
        return MakeError(FString::Printf(TEXT("Unsupported key_type '%s' (use Bool/Int/Float/Vector/Object/String)"), *KeyType));
    }

    Entry.KeyType = NewKeyType;
    BB->Keys.Add(Entry);
    BB->MarkPackageDirty();

    return MakeSuccess();
}

TSharedPtr<FJsonObject> FUnrealMCPAICommands::HandleRunBehaviorTreeOnActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName, BehaviorTreePath;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) ||
        !Params->TryGetStringField(TEXT("behavior_tree_path"), BehaviorTreePath))
    {
        return MakeError(TEXT("Missing 'actor_name' or 'behavior_tree_path'"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return MakeError(TEXT("No editor world available"));
    }

    APawn* TargetPawn = nullptr;
    for (TActorIterator<APawn> It(World); It; ++It)
    {
        if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
        {
            TargetPawn = *It;
            break;
        }
    }

    if (!TargetPawn)
    {
        return MakeError(FString::Printf(TEXT("No Pawn actor named '%s' found in the level"), *ActorName));
    }

    AAIController* AIController = Cast<AAIController>(TargetPawn->GetController());
    if (!AIController)
    {
        // NOTE: we deliberately do NOT auto-spawn/possess an AIController here.
        // If the Pawn's AIControllerClass isn't set, spawning one is possible via
        // World->SpawnActor<AAIController>() + Controller->Possess(TargetPawn), but that
        // sidesteps whatever AIController the user actually intended (custom subclass,
        // Perception config, etc). Safer to fail loud and ask the user to set it up.
        return MakeError(FString::Printf(TEXT("Pawn '%s' has no AIController possessing it (set AIControllerClass and PlayerStart/AutoPossessAI first)"), *ActorName));
    }

    UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BehaviorTreePath);
    if (!BT)
    {
        return MakeError(FString::Printf(TEXT("Could not load BehaviorTree at %s"), *BehaviorTreePath));
    }

    bool bStarted = AIController->RunBehaviorTree(BT);
    if (!bStarted)
    {
        return MakeError(TEXT("AIController->RunBehaviorTree() returned false (check that the Behavior Tree has a Blackboard assigned and a valid root node)"));
    }

    return MakeSuccess();
}
