#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sendproxy>

public void OnPluginStart()
{
    RegConsoleCmd("sm_starthook", Cmd_StartHook);
    RegConsoleCmd("sm_endhook", Cmd_EndHook);
    RegConsoleCmd("sm_checkhook", Cmd_CheckHook);
}

Action Cmd_StartHook(int client, int args)
{
/*
   <property name='m_usingMountedGun'>
    <type>integer</type>
    <offset>13244</offset>
    <bits>1</bits>
    <flags>Unsigned</flags>
   </property>
*/

    // You can also hook other entities that share this sendprop with one callback. Use iEntity to specify.
    bool b1 = SendProxyManager.Hook(client, "m_usingMountedGun", Prop_Bool, OnSendProxy__m_usingMountedGun);

/*
      <property name='m_checkpointBoomerBilesUsed'>
       <type>integer</type>
       <offset>14624</offset>
       <bits>32</bits>
       <flags></flags>
      </property>
*/

    // will throw an error, cause its bit != 1.
    //bool b2 = SendProxyManager.Hook(client, "m_checkpointBoomerBilesUsed", Prop_Bool, OnSendProxy__m_checkpointBoomerBilesUsed);

    // okey.
    bool b2 = SendProxyManager.Hook(client, "m_checkpointBoomerBilesUsed", Prop_Int, OnSendProxy__m_checkpointBoomerBilesUsed); 

/*
      <property name='m_szScriptedHUDStringSet'>
       <type>string</type>
       <offset>1652</offset>
       <bits>0</bits>
       <flags>InsideArray</flags>
      </property>
      <property name='m_szScriptedHUDStringSet'>
       <type>array</type>
       <offset>0</offset>
       <bits>0</bits>
       <flags></flags>
      </property>
*/

    // will throw an error, cause this is an DPT_Array prop, with an array prop in the same name. (which is marked flag with SPROP_INSIDEARRAY.)
    //bool b3 = SendProxyManager.HookGameRules("m_szScriptedHUDStringSet", Prop_String, OnSendProxy__m_szScriptedHUDStringSet_1);

    // in this case, m_szScriptedHUDStringSet has 16 elements. each elements has a single sendprop with its proxy.
    // though the printed table dose not tell you its element since it is an inside array.
    // see l4d2_ems_hud.inc.
    // hook multiple elements on the same callback, use the callback element to specify.
    bool b3 = SendProxyManager.HookGameRulesArray("m_szScriptedHUDStringSet", 0, Prop_String, OnSendProxy__m_szScriptedHUDStringSet_2);
    bool b4 = SendProxyManager.HookGameRulesArray("m_szScriptedHUDStringSet", 1, Prop_String, OnSendProxy__m_szScriptedHUDStringSet_2);

/*
      <property name='m_bIsDedicatedServer'>
       <type>integer</type>
       <offset>1160</offset>
       <bits>1</bits>
       <flags>Unsigned</flags>
      </property>
*/

    // gamerules property.
    bool b5 = SendProxyManager.HookGameRules("m_bIsDedicatedServer", Prop_Bool, OnSendProxy__m_bIsDedicatedServer);

/*
            <property name='m_iAmmo'>
             <type>datatable</type>
             <offset>6260</offset>
             <bits>0</bits>
             <flags>AlwaysProxy</flags>
*/

    // m_iAmmo is an datatable type prop, which contains a table, each elements of the table is a sendprop with ordered number named "000", "001" etc.
    // Any DPT_Datatable or DPT_Array type is used with HookArray.
    bool b6 = SendProxyManager.HookArray(client, "m_iAmmo", 0, Prop_Int, OnSendProxy__m_iAmmo);
    //bool b6 = SendProxyManager.HookArray(client, "m_iAmmo", 1, Prop_Int, OnSendProxy__m_iAmmo); //1, 2, 3...
    
    PrintToServer("Started hook. %d, %d, %d, %d, %d, %d", b1, b2, b3, b4, b5, b6);
    return Plugin_Handled;
}

Action OnSendProxy__m_usingMountedGun(int iEntity, const char[] cPropName, int &iValue, int iElement, int iClient)
{
    static bool bTriggered = false;
    if (bTriggered)
        return Plugin_Continue;

    // since this is not a inside array / array / datatable sendprop, the element is 0.
    PrintToServer("OnSendProxy__m_usingMountedGun: %d, %s, %d, %d, %d", iEntity, cPropName, iValue, iElement, iClient);
    bTriggered = true;

    // Change the value that is about to be pass to the proxy and client.
    //iValue = 0;
    //return Plugin_Changed;

    return Plugin_Continue;
}

Action OnSendProxy__m_checkpointBoomerBilesUsed(int iEntity, const char[] cPropName, int &iValue, int iElement, int iClient)
{
    static bool bTriggered = false;
    if (bTriggered)
        return Plugin_Continue;

    // since this is not a inside array / array / datatable sendprop, the element is 0.
    PrintToServer("OnSendProxy__m_checkpointBoomerBilesUsed: %d, %s, %d, %d, %d", iEntity, cPropName, iValue, iElement, iClient);
    bTriggered = true;

    // Change the value that is about to be pass to the proxy and client.
    //iValue = 0;
    //return Plugin_Changed;

    return Plugin_Continue;
}

// since we pass the string to element(slot) 0, the element(slot) 1 will pass nothing.
Action OnSendProxy__m_szScriptedHUDStringSet_2(const char[] cPropName, char cModifiedValue[4096], int iElement, int iClient)
{
    static bool bTriggered0 = false;
    static bool bTriggered1 = false;
    if (bTriggered0 && bTriggered1)
        return Plugin_Continue;

    PrintToServer("OnSendProxy__m_szScriptedHUDStringSet_2: %s, %s, %d, %d", cPropName, cModifiedValue, iElement, iClient);

    if (iElement == 0)
        bTriggered0 = true;

    if (iElement == 1)
        bTriggered1 = true;

    // do not show the string we set to client.
    //strcopy(cModifiedValue, sizeof(cModifiedValue), "");
    //return Plugin_Changed;

    return Plugin_Continue;
}

Action OnSendProxy__m_bIsDedicatedServer(const char[] cPropName, int &iValue, int iElement, int iClient)
{
    static bool bTriggered = false;
    if (bTriggered)
        return Plugin_Continue;

    PrintToServer("OnSendProxy__m_bIsDedicatedServer: %s, %d, %d, %d", cPropName, iValue, iElement, iClient);
    bTriggered = true;

    // not a dedicated server (?)
    //iValue = 0;
    //return Plugin_Changed;

    return Plugin_Continue;
}

Action OnSendProxy__m_iAmmo(int iEntity, const char[] cPropName, int &iValue, int iElement, int iClient)
{
    static bool bTriggered = false;
    if (bTriggered)
        return Plugin_Continue;

    // the name would be "000", "001" ... etc.
    PrintToServer("OnSendProxy__m_iAmmo: %d, %s, %d, %d, %d", iEntity, cPropName, iValue, iElement, iClient);
    bTriggered = true;

    // Change the value that is about to be pass to the proxy and client.
    //iValue = 0;
    //return Plugin_Changed;

    return Plugin_Continue;
}

Action Cmd_EndHook(int client, int args)
{
    bool b1 = SendProxyManager.Unhook(client, "m_usingMountedGun", OnSendProxy__m_usingMountedGun);
    bool b2 = SendProxyManager.Unhook(client, "m_checkpointBoomerBilesUsed", OnSendProxy__m_checkpointBoomerBilesUsed); 
    bool b3 = SendProxyManager.UnhookGameRulesArray("m_szScriptedHUDStringSet", 0, OnSendProxy__m_szScriptedHUDStringSet_2);
    bool b4 = SendProxyManager.UnhookGameRulesArray("m_szScriptedHUDStringSet", 1, OnSendProxy__m_szScriptedHUDStringSet_2);
    bool b5 = SendProxyManager.UnhookGameRules("m_bIsDedicatedServer", OnSendProxy__m_bIsDedicatedServer);
    bool b6 = SendProxyManager.UnhookArray(client, "m_iAmmo", 0, OnSendProxy__m_iAmmo);

    PrintToServer("Ended hook. %d, %d, %d, %d, %d, %d", b1, b2, b3, b4, b5, b6);
    return Plugin_Handled;
}

Action Cmd_CheckHook(int client, int args)
{
    PrintToServer("Hooked? %d, %d, %d, %d, %d, %d", 
    SendProxyManager.IsHooked(client, "m_usingMountedGun"),
    SendProxyManager.IsHooked(client, "m_checkpointBoomerBilesUsed"),
    SendProxyManager.IsGameRulesArrayHooked("m_szScriptedHUDStringSet", 0),
    SendProxyManager.IsGameRulesArrayHooked("m_szScriptedHUDStringSet", 1),
    SendProxyManager.IsGameRulesHooked("m_bIsDedicatedServer"),
    SendProxyManager.IsArrayHooked(client, "m_iAmmo", 0));
    return Plugin_Handled;
}