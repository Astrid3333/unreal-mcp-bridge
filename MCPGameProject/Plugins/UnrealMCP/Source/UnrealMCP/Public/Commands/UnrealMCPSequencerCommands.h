#pragma once
#include "CoreMinimal.h"
#include "Json.h"

class UNREALMCP_API FUnrealMCPSequencerCommands
{
public:
    FUnrealMCPSequencerCommands();
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
private:
    TSharedPtr<FJsonObject> HandleCreateLevelSequence(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddActorToSequence(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddCameraCutTrack(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPlaybackRange(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleOpenLevelSequence(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddTransformKeyframe(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddPropertyKeyframe(const TSharedPtr<FJsonObject>& Params);
};
