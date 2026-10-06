#include "Commands/UnrealMCPLogCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Misc/DateTime.h"

namespace
{
    // ELogVerbosity: Fatal=1, Error=2, Warning=3, Display=4, Log=5,
    // Verbose=6, VeryVerbose=7 (menor = mas severo).
    FString LMCP_SeverityToString(int32 SeverityNum)
    {
        switch (SeverityNum)
        {
        case 1: return TEXT("Fatal");
        case 2: return TEXT("Error");
        case 3: return TEXT("Warning");
        case 4: return TEXT("Display");
        case 6: return TEXT("Verbose");
        case 7: return TEXT("VeryVerbose");
        default: return TEXT("Log");
        }
    }

    // "Warning" -> 3. Devuelve -1 si el nombre no es una severidad valida.
    int32 LMCP_SeverityFromString(const FString& Name)
    {
        const FString N = Name.ToLower();
        if (N == TEXT("fatal")) return 1;
        if (N == TEXT("error")) return 2;
        if (N == TEXT("warning")) return 3;
        if (N == TEXT("display")) return 4;
        if (N == TEXT("log")) return 5;
        if (N == TEXT("verbose")) return 6;
        if (N == TEXT("veryverbose")) return 7;
        return -1;
    }
}

// Buffer circular de lineas de log. Serialize puede llegar desde cualquier
// hilo: todo el acceso va bajo Mutex. Seq es monotono (no se resetea al
// limpiar) para poder hacer "entradas nuevas desde mi snapshot".
class FMCPLogCapture : public FOutputDevice
{
public:
    struct FEntry
    {
        uint64 Seq = 0;
        FString Time;
        FString Category;
        FString Severity;
        int32 SeverityNum = 5;
        FString Message;
    };

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        if (!V || !*V)
        {
            return;
        }
        const int32 SeverityNum = (int32)(Verbosity & ELogVerbosity::VerbosityMask);
        FScopeLock Lock(&Mutex);
        FEntry& E = Entries.AddDefaulted_GetRef();
        E.Seq = ++LastSeq;
        E.Time = FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
        E.Category = Category.ToString();
        E.Severity = LMCP_SeverityToString(SeverityNum);
        E.SeverityNum = SeverityNum;
        E.Message = V;
        if (Entries.Num() > MaxEntries)
        {
            Entries.RemoveAt(0, Entries.Num() - MaxEntries);
        }
    }

    // Recorre de mas reciente a mas viejo; Out queda ordenado asi.
    void GetEntries(int32 Limit, const FString& CategoryFilter, int32 MinSeverity,
                    const FString& Contains, uint64 SinceSeq,
                    TArray<FEntry>& Out, int32& OutTotal, uint64& OutNextSeq)
    {
        FScopeLock Lock(&Mutex);
        Out.Reset();
        OutTotal = 0;
        for (int32 i = Entries.Num() - 1; i >= 0; --i)
        {
            const FEntry& E = Entries[i];
            if (SinceSeq > 0 && E.Seq <= SinceSeq)
            {
                continue;
            }
            if (!CategoryFilter.IsEmpty() && !E.Category.Equals(CategoryFilter, ESearchCase::IgnoreCase))
            {
                continue;
            }
            if (MinSeverity > 0 && E.SeverityNum > MinSeverity)
            {
                continue;
            }
            if (!Contains.IsEmpty() && !E.Message.Contains(Contains, ESearchCase::IgnoreCase))
            {
                continue;
            }
            ++OutTotal;
            if (Out.Num() < Limit)
            {
                Out.Add(E);
            }
        }
        OutNextSeq = LastSeq;
    }

    int32 Clear()
    {
        FScopeLock Lock(&Mutex);
        const int32 Cleared = Entries.Num();
        Entries.Reset();
        return Cleared;
    }

    uint64 SnapshotSeq()
    {
        FScopeLock Lock(&Mutex);
        return LastSeq;
    }

    virtual bool CanBeUsedOnAnyThread() const override { return true; }

private:
    FCriticalSection Mutex;
    TArray<FEntry> Entries;
    uint64 LastSeq = 0;
    static constexpr int32 MaxEntries = 2000;
};

FUnrealMCPLogCommands::FUnrealMCPLogCommands()
    : Capture(nullptr)
{
    Capture = new FMCPLogCapture();
    if (GLog)
    {
        GLog->AddOutputDevice(Capture);
    }
}

FUnrealMCPLogCommands::~FUnrealMCPLogCommands()
{
    if (GLog && Capture)
    {
        GLog->RemoveOutputDevice(Capture);
    }
    delete Capture;
    Capture = nullptr;
}

TSharedPtr<FJsonObject> FUnrealMCPLogCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_log_entries"))
    {
        return HandleGetLogEntries(Params);
    }
    if (CommandType == TEXT("clear_log_buffer"))
    {
        return HandleClearLogBuffer(Params);
    }
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown log command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPLogCommands::HandleGetLogEntries(const TSharedPtr<FJsonObject>& Params)
{
    if (!Capture)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Log capture no inicializado"));
    }

    double LimitRaw = 100.0;
    Params->TryGetNumberField(TEXT("limit"), LimitRaw);
    const int32 Limit = FMath::Clamp((int32)LimitRaw, 0, 1000);

    FString CategoryFilter;
    Params->TryGetStringField(TEXT("category"), CategoryFilter);

    FString MinSeverityStr;
    Params->TryGetStringField(TEXT("min_severity"), MinSeverityStr);
    int32 MinSeverity = 0;
    if (!MinSeverityStr.IsEmpty())
    {
        MinSeverity = LMCP_SeverityFromString(MinSeverityStr);
        if (MinSeverity < 0)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
                TEXT("min_severity invalido: %s (usa Fatal|Error|Warning|Display|Log|Verbose|VeryVerbose)"), *MinSeverityStr));
        }
    }

    FString Contains;
    Params->TryGetStringField(TEXT("contains"), Contains);

    double SinceSeqRaw = 0.0;
    Params->TryGetNumberField(TEXT("since_seq"), SinceSeqRaw);
    const uint64 SinceSeq = (uint64)FMath::Max(0.0, SinceSeqRaw);

    TArray<FMCPLogCapture::FEntry> Found;
    int32 Total = 0;
    uint64 NextSeq = 0;
    Capture->GetEntries(Limit, CategoryFilter, MinSeverity, Contains, SinceSeq, Found, Total, NextSeq);

    TArray<TSharedPtr<FJsonValue>> EntriesArr;
    for (const FMCPLogCapture::FEntry& E : Found)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("seq"), (double)E.Seq);
        Obj->SetStringField(TEXT("time"), E.Time);
        Obj->SetStringField(TEXT("category"), E.Category);
        Obj->SetStringField(TEXT("severity"), E.Severity);
        Obj->SetStringField(TEXT("message"), E.Message);
        EntriesArr.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetNumberField(TEXT("total"), Total);
    ResultObj->SetNumberField(TEXT("returned"), EntriesArr.Num());
    ResultObj->SetNumberField(TEXT("next_seq"), (double)NextSeq);
    ResultObj->SetArrayField(TEXT("entries"), EntriesArr);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPLogCommands::HandleClearLogBuffer(const TSharedPtr<FJsonObject>& Params)
{
    if (!Capture)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Log capture no inicializado"));
    }
    const int32 Cleared = Capture->Clear();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetNumberField(TEXT("cleared"), Cleared);
    ResultObj->SetNumberField(TEXT("next_seq"), (double)Capture->SnapshotSeq());
    return ResultObj;
}
