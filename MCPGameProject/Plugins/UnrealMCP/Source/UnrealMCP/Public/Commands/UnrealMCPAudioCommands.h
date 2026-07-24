#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Audio-related MCP commands.
 * Covers: SoundAttenuation assets, SoundClass assets, simple SoundCue
 * creation (single WavePlayer node), AmbientSound actor spawning/config,
 * and quick 2D playback for testing.
 */
class UNREALMCP_API FUnrealMCPAudioCommands
{
public:
    FUnrealMCPAudioCommands();

    // Dispatch entry point (matches the pattern of every other Commands class)
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Asset creation
    TSharedPtr<FJsonObject> HandleCreateSoundAttenuation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateSoundClass(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateSoundCue(const TSharedPtr<FJsonObject>& Params);

    // AmbientSound actor
    TSharedPtr<FJsonObject> HandleSpawnAmbientSound(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetAmbientSoundProperties(const TSharedPtr<FJsonObject>& Params);

    // Quick test playback
    TSharedPtr<FJsonObject> HandlePlaySound2D(const TSharedPtr<FJsonObject>& Params);
};
