/**
 * vim: set ts=4 :
 * =============================================================================
 * SendVar Proxy Manager
 * Copyright (C) 2011-2019 Afronanny & AlliedModders community.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "natives.h"
#include "util.h"
#include "sendprop_hookmanager.h"

static bool IsPropValid(const SendProp *prop, PropType type)
{
	switch (type)
	{
	case PropType::Prop_Int:
		return prop->GetType() == DPT_Int;

	case PropType::Prop_EHandle:
		return prop->GetType() == DPT_Int && prop->m_nBits == NUM_NETWORKED_EHANDLE_BITS;

	case PropType::Prop_Float:
		return prop->GetType() == DPT_Float;

	case PropType::Prop_Vector:
		return prop->GetType() == DPT_Vector || prop->GetType() == DPT_VectorXY;

	case PropType::Prop_String:
		return prop->GetType() == DPT_String;
	}

	return false;
}

void UTIL_FindSendProp(SendProp* &ret, IPluginContext *pContext, int index, const char* propname, bool checkType, PropType type, int element)
{
	edict_t *edict = UTIL_EdictOfIndex(index);
	if (!edict)
		return pContext->ReportError("Invalid edict index (%d)", index);

	ServerClass *sc = edict->GetNetworkable()->GetServerClass();
	if (!sc)
		return pContext->ReportError("Cannot find ServerClass for entity %d", index);

	sm_sendprop_info_t info;
	gamehelpers->FindSendPropInfo(sc->GetName(), propname, &info);

	SendProp *pProp = info.prop;
	if (!pProp)
		return pContext->ReportError("Could not find prop %s", propname);
	
	if (pProp->GetType() == DPT_Array)
	{
		pProp = pProp->GetArrayProp();
		if (!pProp)
			return pContext->ReportError("Unexpected: Prop %s is an array but has no array prop", propname);
		
		if (element < 0 || element > info.prop->GetNumElements())
			return pContext->ReportError("Unable to find element %d of prop %s", element, propname);
	}
	else if (pProp->GetType() == DPT_DataTable)
	{
		if (!pProp->GetDataTable())
			return pContext->ReportError("Unexpected: Prop %s is a datatable but has no data table", propname);

		pProp = pProp->GetDataTable()->GetProp(element);
		if (!pProp)
			return pContext->ReportError("Unable to find element %d of prop %s", element, propname);
	}

	if (checkType && !IsPropValid(pProp, type))
	{
		switch (type)
		{
			case PropType::Prop_Int: 
				return pContext->ReportError("Prop %s is not an int!", propname);
			case PropType::Prop_Float:
				return pContext->ReportError("Prop %s is not a float!", propname);
			case PropType::Prop_String:
				return pContext->ReportError("Prop %s is not a string!", propname);
			case PropType::Prop_Vector:
				return pContext->ReportError("Prop %s is not a vector!", propname);
			case PropType::Prop_EHandle:
				return pContext->ReportError("Prop %s is not an EHandle!", propname);
			default:
				return pContext->ReportError("Unsupported prop type %d", type);
		}
	}

	ret = pProp;
}

static cell_t Native_Hook(IPluginContext *pContext, const cell_t *params)
{
	constexpr cell_t PARAM_COUNT = 5;
	if (params[0] < PARAM_COUNT)
	{
		pContext->ReportError("Expected %d params, found %d", PARAM_COUNT, params[0]);
		return false;
	}

	char *propname = nullptr;
	SendProp *pProp = nullptr;

	int index = params[1];
	pContext->LocalToString(params[2], &propname);
	PropType type = static_cast<PropType>(params[3]);
	IPluginFunction *pFunc = pContext->GetFunctionById(params[4]);
	int element = params[5];

	UTIL_FindSendProp(pProp, pContext, index, propname, true, type, element);
	if (pProp == nullptr)
		return false;
	
	if (g_pSendPropHookManager->IsEntityHooked(index, pProp, element, pFunc))
		return true;

	return g_pSendPropHookManager->HookEntity(index, pProp, element, type, pFunc);
}

static cell_t Native_Unhook(IPluginContext * pContext, const cell_t * params)
{
	constexpr cell_t PARAM_COUNT = 4;
	if (params[0] < PARAM_COUNT)
	{
		pContext->ReportError("Expected %d params, found %d", PARAM_COUNT, params[0]);
		return false;
	}

	char *propname = nullptr;
	SendProp *pProp = nullptr;

	int index = params[1];
	pContext->LocalToString(params[2], &propname);
	IPluginFunction *pFunc = pContext->GetFunctionById(params[3]);
	int element = params[4];

	UTIL_FindSendProp(pProp, pContext, index, propname, false, PropType::Prop_Max, element);
	if (pProp == nullptr)
		return false;

	if (!g_pSendPropHookManager->IsEntityHooked(index, pProp, element, pFunc))
		return false;

	g_pSendPropHookManager->UnhookEntity(index, pProp, element, pFunc);
	return true;
}

static cell_t Native_IsHooked(IPluginContext * pContext, const cell_t * params)
{
	constexpr cell_t PARAM_COUNT = 4;
	if (params[0] < PARAM_COUNT)
	{
		pContext->ReportError("Expected %d params, found %d", PARAM_COUNT, params[0]);
		return false;
	}

	char *propname = nullptr;
	SendProp *pProp = nullptr;

	int index = params[1];
	pContext->LocalToString(params[2], &propname);
	IPluginFunction *pFunc = pContext->GetFunctionById(params[3]);
	int element = params[4];

	UTIL_FindSendProp(pProp, pContext, index, propname, false, PropType::Prop_Max, element);
	if (pProp == nullptr)
		return false;

	return g_pSendPropHookManager->IsEntityHooked(index, pProp, element, pFunc);
}

static cell_t Native_HookGameRules(IPluginContext * pContext, const cell_t * params)
{
	constexpr cell_t PARAM_COUNT = 4;
	if (params[0] < PARAM_COUNT)
	{
		pContext->ReportError("Expected %d params, found %d", PARAM_COUNT, params[0]);
		return false;
	}

	CBaseEntity *pGameRulesProxy = GetGameRulesProxyEnt();
	if (!pGameRulesProxy)
	{
		pContext->ReportError("MGameRulesProxy entity not found. (Maybe try hooking later after \"round_start\").");
		return false;
	}

	char *propname = nullptr;
	SendProp *pProp = nullptr;
	int index = gamehelpers->EntityToBCompatRef(pGameRulesProxy);

	pContext->LocalToString(params[1], &propname);
	PropType type = static_cast<PropType>(params[2]);
	IPluginFunction *pFunc = pContext->GetFunctionById(params[3]);
	int element = params[4];

	UTIL_FindSendProp(pProp, pContext, index, propname, true, type, element);
	if (pProp == nullptr)
		return false;
	
	if (g_pSendPropHookManager->IsEntityHooked(index, pProp, element, pFunc))
		return true;

	return g_pSendPropHookManager->HookEntity(index, pProp, element, type, pFunc);
}

static cell_t Native_UnhookGameRules(IPluginContext * pContext, const cell_t * params)
{
	constexpr cell_t PARAM_COUNT = 3;
	if (params[0] < PARAM_COUNT)
	{
		pContext->ReportError("Expected %d params, found %d", PARAM_COUNT, params[0]);
		return false;
	}

	CBaseEntity *pGameRulesProxy = GetGameRulesProxyEnt();
	if (!pGameRulesProxy)
	{
		pContext->ReportError("MGameRulesProxy entity not found. (Maybe try hooking later after \"round_start\").");
		return false;
	}

	char *propname = nullptr;
	SendProp *pProp = nullptr;
	int index = gamehelpers->EntityToBCompatRef(pGameRulesProxy);

	pContext->LocalToString(params[1], &propname);
	IPluginFunction *pFunc = pContext->GetFunctionById(params[2]);
	int element = params[3];

	UTIL_FindSendProp(pProp, pContext, index, propname, false, PropType::Prop_Max, element);

	if (pProp == nullptr)
		return false;

	if (!g_pSendPropHookManager->IsEntityHooked(index, pProp, element, pFunc))
		return false;

	g_pSendPropHookManager->UnhookEntity(index, pProp, element, pFunc);
	return true;
}

static cell_t Native_IsHookedGameRules(IPluginContext * pContext, const cell_t * params)
{
	constexpr cell_t PARAM_COUNT = 3;
	if (params[0] < PARAM_COUNT)
	{
		pContext->ReportError("Expected %d params, found %d", PARAM_COUNT, params[0]);
		return false;
	}

	CBaseEntity *pGameRulesProxy = GetGameRulesProxyEnt();
	if (!pGameRulesProxy)
	{
		pContext->ReportError("MGameRulesProxy entity not found. (Maybe try hooking later after \"round_start\").");
		return false;
	}

	char *propname = nullptr;
	SendProp *pProp = nullptr;
	int index = gamehelpers->EntityToBCompatRef(pGameRulesProxy);

	pContext->LocalToString(params[1], &propname);
	IPluginFunction *pFunc = pContext->GetFunctionById(params[2]);
	int element = params[3];

	UTIL_FindSendProp(pProp, pContext, index, propname, false, PropType::Prop_Max, element);

	if (pProp == nullptr)
		return false;

	return g_pSendPropHookManager->IsEntityHooked(index, pProp, element, pFunc);
}

const sp_nativeinfo_t g_MyNatives[] = {
	{"SendProxy_HookEntity", Native_Hook},
	{"SendProxy_HookGameRules", Native_HookGameRules},
	{"SendProxy_UnhookEntity", Native_Unhook},
	{"SendProxy_UnhookGameRules", Native_UnhookGameRules},
	{"SendProxy_IsHookedEntity", Native_IsHooked},
	{"SendProxy_IsHookedGameRules", Native_IsHookedGameRules},
	{nullptr, nullptr}};