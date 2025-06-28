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

#ifndef SENDPROXY_NATIVES_INC
#define SENDPROXY_NATIVES_INC

#include "extension.h"
extern const sp_nativeinfo_t g_MyNatives[];

#define CHECK_VALID_ENTITY_SENDPROP(entity, propName) \
	edict_t * pEnt = gamehelpers->EdictOfIndex(entity);\
	if (!pEnt)\
 		return pContext->ThrowNativeError("Invalid Edict Index %d, edict_t is null.", entity);\
	ServerClass * sc = pEnt->GetNetworkable()->GetServerClass();\
	if (!sc)\
		return pContext->ThrowNativeError("Cannot find ServerClass for entity %d", entity);\
	sm_sendprop_info_t info;\
	gamehelpers->FindSendPropInfo(sc->GetName(), propName, &info);\
	if (!info.prop)\
		return pContext->ThrowNativeError("Could not find prop %s", propName);

#define CHECK_VALID_GAMERULES_SENDPROP(propName)\
	pContext->LocalToString(params[1], &propName);\
	sm_sendprop_info_t info;\
	gamehelpers->FindSendPropInfo(g_szGameRulesProxy, propName, &info);\
	SendProp * pProp = info.prop;\
	if (!pProp)\
		return pContext->ThrowNativeError("Could not find prop %s", propName);

#define CHECK_PROP_BASE_DATA_TYPE(pProp, propType) \
	if (!IsPropValid(pProp, propType)) \
	{ \
		switch (propType) \
		{ \
			case PropType::Prop_Int: \
				return pContext->ThrowNativeError("Prop %s is not an int!", pProp->GetName());\
			case PropType::Prop_Float:\
				return pContext->ThrowNativeError("Prop %s is not a float!", pProp->GetName());\
			case PropType::Prop_String:\
				return pContext->ThrowNativeError("Prop %s is not a string!", pProp->GetName());\
			case PropType::Prop_Bool:\
				return pContext->ThrowNativeError("Prop %s is not a bool!", pProp->GetName());\
			case PropType::Prop_Vector:\
				return pContext->ThrowNativeError("Prop %s is not a vector!", pProp->GetName());\
			default:\
				return pContext->ThrowNativeError("Unsupported prop type %d", propType);\
		}\
	}

#define CHECK_ARRAYPROP_WITH_BASE_TYPE(element, propName) \
    switch (info.prop->GetType()) \
	{ \
	case DPT_Array:\
		{\
			pProp = info.prop->GetArrayProp(); \
			if (!pProp) \
				return pContext->ThrowNativeError("Prop %s does not contain any elements", propName); \
			if (element > info.prop->GetNumElements() - 1 || element < 0) \
				return pContext->ThrowNativeError("Could not find element %d in %s", element, info.prop->GetName()); \
			CHECK_PROP_BASE_DATA_TYPE(pProp, propType) \
			break; \
		}\
	case DPT_DataTable:\
		{\
			SendTable * st = info.prop->GetDataTable();\
			if (!st)\
				return pContext->ThrowNativeError("Prop %s does not contain any elements", propName);\
			if (element > st->GetNumProps() - 1 || element < 0)\
				return pContext->ThrowNativeError("Could not find element %d in %s", element, info.prop->GetName());\
			pProp = st->GetProp(element);\
			if (!pProp)\
				return pContext->ThrowNativeError("Could not find element %d in %s", element, info.prop->GetName());\
			CHECK_PROP_BASE_DATA_TYPE(pProp, propType)\
			break;\
		}\
	default:\
		return pContext->ThrowNativeError("Prop %s does not contain any elements", propName);\
	}

#define CHECK_ARRAYPROP_NO_BASE_TYPE(element, propName)\
	switch (info.prop->GetType())\
	{\
	case DPT_Array:\
		{\
			pProp = info.prop->GetArrayProp();\
			if (!pProp)\
				return pContext->ThrowNativeError("Prop %s does not contain any elements", propName);\
			if (element > info.prop->GetNumElements() - 1 || element < 0)\
				return pContext->ThrowNativeError("Could not find element %d in %s", element, info.prop->GetName());\
			break;\
		}\
	case DPT_DataTable:\
		{\
			SendTable * st = info.prop->GetDataTable();\
			if (!st)\
				return pContext->ThrowNativeError("Prop %s does not contain any elements", propName);\
			if (element > st->GetNumProps() - 1 || element < 0)\
				return pContext->ThrowNativeError("Could not find element %d in %s", element, info.prop->GetName());\
			pProp = st->GetProp(element);\
			if (!pProp)\
				return pContext->ThrowNativeError("Could not find element %d in %s", element, info.prop->GetName());\
			break;\
		}\
	default:\
		return pContext->ThrowNativeError("Prop %s does not contain any elements", propName);\
	}

#endif