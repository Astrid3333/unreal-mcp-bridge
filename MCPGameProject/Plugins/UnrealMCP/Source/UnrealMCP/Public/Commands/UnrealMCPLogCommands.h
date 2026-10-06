#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class FMCPLogCapture;

/**
 * Handler class for log commands (ciclo 11, bloque 4):
 * buffer circular de lineas del log del editor (FOutputDevice) para
 * leerlas (get_log_entries) o limpiarlas (clear_log_buffer).
 */
class UNREALMCP_API FUnrealMCPLogCommands
{
public:
    FUnrealMCPLogCommands();
    ~FUnrealMCPLogCommands();

    // Handle log commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // get_log_entries: ultimas lineas con filtros (category, min_severity,
    // contains, since_seq) y orden mas-reciente-primero.
    TSharedPtr<FJsonObject> HandleGetLogEntries(const TSharedPtr<FJsonObject>& Params);

    // clear_log_buffer: vacia el buffer (el contador seq sigue creciendo).
    TSharedPtr<FJsonObject> HandleClearLogBuffer(const TSharedPtr<FJsonObject>& Params);

    FMCPLogCapture* Capture;
};
