#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sendproxy>

public void OnPluginStart()
{
    RegConsoleCmd("sm_testflag", Cmd_TestFlag);
}

// Test sendprop flags.
Action Cmd_TestFlag(int client, int args)
{

// CTerrorPlayer as example.

/*
    <property name='m_flFriction'>
        <type>float</type>
        <offset>708</offset>
        <bits>8</bits>
        <flags>RoundDown</flags>
    </property>
*/
    int a1 = GetEntSendPropFlag(client, "m_flFriction");
    PrintToServer("m_flFriction flag: %d, %d", a1, (a1 & SPROP_ROUNDDOWN) != 0);

/*
   <property name='m_isFallingFromLedge'>
    <type>integer</type>
    <offset>14834</offset>
    <bits>1</bits>
    <flags>Unsigned</flags>
   </property>
*/
    int a2 = GetEntSendPropFlag(client, "m_isFallingFromLedge");
    PrintToServer("m_isFallingFromLedge flag: %d, %d", a2, (a2 & SPROP_UNSIGNED) != 0);
    
/*
   <property name='m_hangNormal'>
    <type>vector</type>
    <offset>14880</offset>
    <bits>0</bits>
    <flags>CoordMP</flags>
   </property>
*/
    int a3 = GetEntSendPropFlag(client, "m_hangNormal");
    PrintToServer("m_hangNormal flag: %d, %d", a3, (a3 & SPROP_COORD_MP) != 0);

// multiple flags.

/*
   <property name='m_pounceVictim'>
    <type>integer</type>
    <offset>15972</offset>
    <bits>21</bits>
    <flags>Unsigned|NoScale</flags>
   </property>
*/
    int a4 = GetEntSendPropFlag(client, "m_pounceVictim");
    PrintToServer("m_pounceVictim flag: %d, %d, %d, %d", a4, (a4 & SPROP_UNSIGNED) != 0, (a4 & SPROP_NOSCALE) != 0, ((a4 & SPROP_UNSIGNED) && (a4 & SPROP_NOSCALE)));

// Always Proxy. / the type is array or datatable.

/*
   <property name='m_vocalizationSubjectTimer'>
    <type>datatable</type>
    <offset>11112</offset>
    <bits>0</bits>
    <flags>AlwaysProxy</flags>
*/
    int a5 = GetEntSendPropFlag(client, "m_vocalizationSubjectTimer");
    PrintToServer("m_hangNormal flag: %d, %d", a5, (a5 & SPROP_PROXY_ALWAYS_YES) != 0);

// no flags at all

/*
   <property name='m_iVersusTeam'>
    <type>integer</type>
    <offset>16308</offset>
    <bits>3</bits>
    <flags></flags>
   </property>
*/
    int a6 = GetEntSendPropFlag(client, "m_iVersusTeam");
    PrintToServer("m_iVersusTeam flag: %d", a6);

// CTerrorGameRulesProxy

/*
      <property name='m_iSurvivorScore'>
       <type>datatable</type>
       <offset>908</offset>
       <bits>0</bits>
       <flags>AlwaysProxy</flags>
*/
    int a7 = GetGameRulesSendPropFlag("m_iSurvivorScore");
    PrintToServer("m_iSurvivorScore flag: %d, %d", a7, (a7 & SPROP_PROXY_ALWAYS_YES) != 0);

// no flags at all.

/*
      <property name='m_nScavengeItemsRemaining'>
       <type>integer</type>
       <offset>1144</offset>
       <bits>32</bits>
       <flags></flags>
      </property>
*/

    int a8 = GetGameRulesSendPropFlag("m_nScavengeItemsRemaining");
    PrintToServer("m_nScavengeItemsRemaining flag: %d", a8);
    return Plugin_Handled;
}