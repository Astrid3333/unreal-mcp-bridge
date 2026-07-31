#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * FUnrealMCPAICommands
 * Handles NavMesh + Behavior Tree related MCP commands.
 * Same pattern as FUnrealMCPLandscapeCommands: one HandleCommand() dispatcher,
 * private HandleX() per command, all returning a JSON result object.
 */
class FUnrealMCPAICommands
{
public:
    FUnrealMCPAICommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // --- NavMesh ---
    TSharedPtr<FJsonObject> HandleGetNavMeshInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBuildNavigation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindPath(const TSharedPtr<FJsonObject>& Params);

    // --- Behavior Tree / Blackboard ---
    TSharedPtr<FJsonObject> HandleCreateBehaviorTree(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateBlackboard(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlackboardKey(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRunBehaviorTreeOnActor(const TSharedPtr<FJsonObject>& Params);

    // Shared helpers
    TSharedPtr<FJsonObject> MakeError(const FString& Message);
    TSharedPtr<FJsonObject> MakeSuccess();
};
