#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for asset commands (ciclo 11, bloque 3):
 * importar archivos a /Game, crear assets y buscar en el AssetRegistry.
 */
class UNREALMCP_API FUnrealMCPAssetCommands
{
public:
    FUnrealMCPAssetCommands();

    // Handle asset commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // import_asset: archivo del disco -> paquete en /Game (cualquier formato
    // con factory registrada: png/jpg/obj/fbx/csv/...).
    TSharedPtr<FJsonObject> HandleImportAsset(const TSharedPtr<FJsonObject>& Params);

    // find_assets: busqueda en el AssetRegistry por nombre/clase/ruta.
    TSharedPtr<FJsonObject> HandleFindAssets(const TSharedPtr<FJsonObject>& Params);

    // create_asset: crea Material o Blueprint vacio en /Game.
    TSharedPtr<FJsonObject> HandleCreateAsset(const TSharedPtr<FJsonObject>& Params);
};
