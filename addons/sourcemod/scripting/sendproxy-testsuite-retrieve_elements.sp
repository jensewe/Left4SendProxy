#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sendproxy>

public void OnPluginStart()
{
    RegConsoleCmd("sm_getelement", Cmd_GetElement);
}

Action Cmd_GetElement(int client, int args)
{
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
/*
            <property name='m_iAmmo'>
             <type>datatable</type>
             <offset>6260</offset>
             <bits>0</bits>
             <flags>AlwaysProxy</flags>
*/
    // m_szScriptedHUDStringSet is type DPT_Array, with array prop marked flag SPROP_INSIDEARRAY.
    // m_iAmmo is type DPT_Datatable.
    int elem1 = GetGameRulesSendPropNumElements("m_szScriptedHUDStringSet");
    int elem2 = GetEntSendPropNumElements(client, "m_iAmmo");

/*
            <property name='m_flFriction'>
             <type>float</type>
             <offset>708</offset>
             <bits>8</bits>
             <flags>RoundDown</flags>
            </property>
*/
/*
      <property name='m_bIsDedicatedServer'>
       <type>integer</type>
       <offset>1160</offset>
       <bits>1</bits>
       <flags>Unsigned</flags>
      </property>
*/
    // these props are not array or datatable, they will return -1.
    int elem3 = GetEntSendPropNumElements(client, "m_flFriction");
    int elem4 = GetGameRulesSendPropNumElements("m_bIsDedicatedServer");

    PrintToServer("Number of Elements: %d, %d, %d, %d", elem1, elem2, elem3, elem4);
    return Plugin_Handled;
}