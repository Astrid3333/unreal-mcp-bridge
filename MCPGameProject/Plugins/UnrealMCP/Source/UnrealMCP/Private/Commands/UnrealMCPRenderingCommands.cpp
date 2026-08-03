#include "Commands/UnrealMCPRenderingCommands.h"
#include "HAL/IConsoleManager.h"

TSharedPtr<FJsonObject> FUnrealMCPRenderingCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_cvar")) { return HandleGetCVar(Params); }
    if (CommandType == TEXT("set_cvar")) { return HandleSetCVar(Params); }

    TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
    Error->SetBoolField(TEXT("success"), false);
    Error->SetStringField(TEXT("message"), FString::Printf(TEXT("Unknown rendering command: %s"), *CommandType));
    return Error;
}

TSharedPtr<FJsonObject> FUnrealMCPRenderingCommands::HandleGetCVar(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    FString CVarName;
    if (!Params->TryGetStringField(TEXT("cvar_name"), CVarName))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), TEXT("Missing 'cvar_name' parameter"));
        return Result;
    }

    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
    if (!CVar)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("CVar not found: %s"), *CVarName));
        return Result;
    }

    Result->SetStringField(TEXT("cvar_name"), CVarName);
    Result->SetStringField(TEXT("value"), CVar->GetString());
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPRenderingCommands::HandleSetCVar(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    FString CVarName;
    if (!Params->TryGetStringField(TEXT("cvar_name"), CVarName))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), TEXT("Missing 'cvar_name' parameter"));
        return Result;
    }

    FString Value;
    if (!Params->TryGetStringField(TEXT("value"), Value))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), TEXT("Missing 'value' parameter"));
        return Result;
    }

    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
    if (!CVar)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("CVar not found: %s"), *CVarName));
        return Result;
    }

    CVar->Set(*Value, ECVF_SetByConsole);

    Result->SetStringField(TEXT("cvar_name"), CVarName);
    Result->SetStringField(TEXT("value"), CVar->GetString());
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}
