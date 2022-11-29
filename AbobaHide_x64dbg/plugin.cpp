#include "plugin.h"
#include <windows.h>
#include <stdio.h>
#include "../AbobaHide/AbobaHide.h"

static DWORD pid = 0;
static bool hidden = false;

static ULONG GetAbobaHideOptions()
{
    duint options = 0;
    if(!BridgeSettingGetUint("AbobaHide", "Options", &options))
        options = 0xffffffff;
    return (ULONG)options;
}

static bool AbobaHideCall(HIDE_COMMAND Command)
{
    HANDLE hDevice = CreateFileA("\\\\.\\AbobaHide", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if(hDevice == INVALID_HANDLE_VALUE)
    {
        _plugin_logputs("[" PLUGIN_NAME "] Could not open AbobaHide handle...");
        return false;
    }
    HIDE_INFO HideInfo;
    HideInfo.Command = Command;
    HideInfo.Pid = pid;
    HideInfo.Type = GetAbobaHideOptions();
    DWORD written = 0;
    auto result = false;
    if(WriteFile(hDevice, &HideInfo, sizeof(HIDE_INFO), &written, 0))
    {
        _plugin_logprintf("[" PLUGIN_NAME "] Process %shidden!\n", Command == UnhidePid ? "un" : "");
        result = true;
    }
    else
    {
        _plugin_logputs("[" PLUGIN_NAME "] WriteFile error...");
    }
    CloseHandle(hDevice);
    return result;
}

static bool cbAbobaHide(int argc, char* argv[])
{
    if(!hidden)
    {
        _plugin_logprintf("[" PLUGIN_NAME "] Hiding PID %X (%ud)\n", pid, pid);
        if(AbobaHideCall(HidePid))
        {
            DbgCmdExecDirect("hide");
            hidden = true;
        }
    }
    return hidden;
}

static bool cbAbobaUnhide(int argc, char* argv[])
{
    if(hidden)
    {
        _plugin_logprintf("[" PLUGIN_NAME "] Unhiding PID %X (%ud)\n", pid, pid);
        if(AbobaHideCall(UnhidePid))
            hidden = false;
    }
    return !hidden;
}

static bool cbAbobaHideOptions(int argc, char* argv[])
{
    if(argc < 2)
    {
        _plugin_logprintf("[" PLUGIN_NAME "] Options: 0x%08X\n", GetAbobaHideOptions());
    }
    else
    {
        duint options = DbgValFromString(argv[1]);
        BridgeSettingSetUint("AbobaHide", "Options", options & 0xffffffff);
        if(hidden)
            AbobaHideCall(HidePid);
        _plugin_logprintf("[" PLUGIN_NAME "] New options: 0x%08X\n", GetAbobaHideOptions());
    }
    return true;
}

PLUG_EXPORT void CBCREATEPROCESS(CBTYPE cbType, PLUG_CB_CREATEPROCESS* info)
{
    pid = info->fdProcessInfo->dwProcessId;
}

PLUG_EXPORT void CBATTACH(CBTYPE cbType, PLUG_CB_ATTACH* info)
{
    pid = info->dwProcessId;
}

PLUG_EXPORT void CBSYSTEMBREAKPOINT(CBTYPE cbType, PLUG_CB_SYSTEMBREAKPOINT* info)
{
    char* argv = "AbobaHide";
    cbAbobaHide(1, &argv);
}

PLUG_EXPORT void CBSTOPDEBUG(CBTYPE cbType, PLUG_CB_STOPDEBUG* info)
{
    char* argv = "AbobaUnhide";
    cbAbobaUnhide(1, &argv);
}

void AbobaHideInit(PLUG_INITSTRUCT* initStruct)
{
    _plugin_registercommand(pluginHandle, "AbobaHide", cbAbobaHide, true);
    _plugin_registercommand(pluginHandle, "AbobaUnhide", cbAbobaUnhide, true);
    _plugin_registercommand(pluginHandle, "AbobaHideOptions", cbAbobaHideOptions, false);
}

void AbobaHideStop()
{
    _plugin_unregistercommand(pluginHandle, "AbobaHideOptions");
    _plugin_unregistercommand(pluginHandle, "AbobaUnhide");
    _plugin_unregistercommand(pluginHandle, "AbobaHide");
}
