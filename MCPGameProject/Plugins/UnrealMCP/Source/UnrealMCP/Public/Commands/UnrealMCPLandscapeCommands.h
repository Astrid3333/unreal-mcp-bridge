#pragma once
#include "CoreMinimal.h"
#include "Json.h"

class UNREALMCP_API FUnrealMCPLandscapeCommands
{
public:
    FUnrealMCPLandscapeCommands();
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleGetLandscapeInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSculptLandscapeRegion(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePaintLandscapeLayer(const TSharedPtr<FJsonObject>& Params);
};
