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

#ifndef _EXTENSION_H_INC_
#define _EXTENSION_H_INC_

 /*
	TODO:
		Implement interface:
			Add common function for prop change hooks
			Add remove listeners for prop change hooks
 */

#include "smsdk_ext.h"
#include <string>
#include <stdint.h>
#include "convar.h"
#include "dt_send.h"
#include "server_class.h"
#include "ISendProxy.h"
#include <eiface.h>
#include <ISDKHooks.h>
#include <ISDKTools.h>
#include "wrappers.h"
#include "CDetour/detours.h"

#define GET_CONVAR(name) \
	name = g_pCVar->FindVar(#name); \
	if (name == nullptr) { \
		if (error != nullptr && maxlen != 0) { \
			ismm->Format(error, maxlen, "Could not find ConVar: " #name); \
		} \
		return false; \
	}

class IServerGameEnts;

void GlobalProxy(const SendProp *pProp, const void *pStructBase, const void* pData, DVariant *pOut, int iElement, int objectID);
void GlobalProxyGamerules(const SendProp *pProp, const void *pStructBase, const void* pData, DVariant *pOut, int iElement, int objectID);
bool IsPropValid(SendProp *, PropType);
char * strncpynull(char * pDestination, const char * pSource, size_t szCount);
void DestroyDetours(CDetour *pDetour);

struct ListenerCallbackInfo
{
	IExtension *							m_pExt;
	IExtensionInterface *					m_pExtAPI;
	ISendProxyUnhookListener *				m_pCallBack;
};

struct CallBackInfo
{
	CallBackInfo() { memset(this, 0, sizeof(CallBackInfo)); }
	CallBackInfo(const CallBackInfo & rObj) { memcpy(this, &rObj, sizeof(CallBackInfo)); }
	const CallBackInfo & operator=(const CallBackInfo & rObj) { return *(new CallBackInfo(rObj)); }
	void *									pOwner; //Pointer to plugin context or IExtension *
	void *									pCallback;
	CallBackType							iCallbackType;
};

struct SendPropHook
{
	SendPropHook() { vListeners = new CUtlVector<ListenerCallbackInfo>(); }
	SendPropHook(const SendPropHook & rObj)
	{
		memcpy(this, &rObj, sizeof(SendPropHook));
		vListeners = new CUtlVector<ListenerCallbackInfo>();
		*vListeners = *rObj.vListeners;
	}
	~SendPropHook() { delete vListeners; }
	CallBackInfo							sCallbackInfo;
	SendProp *								pVar;
	edict_t *								pEnt;
	SendVarProxyFn							pRealProxy;
	int										objectID;
	PropType								PropType;
	int										Offset;
	int										Element{0};
	CUtlVector<ListenerCallbackInfo> *		vListeners;
};

struct SendPropHookGamerules
{
	SendPropHookGamerules() { vListeners = new CUtlVector<ListenerCallbackInfo>(); }
	SendPropHookGamerules(const SendPropHookGamerules & rObj)
	{
		memcpy(this, &rObj, sizeof(SendPropHookGamerules));
		vListeners = new CUtlVector<ListenerCallbackInfo>();
		*vListeners = *rObj.vListeners;
	}
	~SendPropHookGamerules() { delete vListeners; }
	CallBackInfo							sCallbackInfo;
	SendProp *								pVar;
	SendVarProxyFn							pRealProxy;
	PropType								PropType;
	int										Element{0};
	CUtlVector<ListenerCallbackInfo> *		vListeners;
};

struct PropChangeHook
{
	PropChangeHook() { vCallbacksInfo = new CUtlVector<CallBackInfo>(); }
	PropChangeHook(const PropChangeHook & rObj)
	{
		memcpy(this, &rObj, sizeof(PropChangeHook));
		vCallbacksInfo = new CUtlVector<CallBackInfo>();
		*vCallbacksInfo = *rObj.vCallbacksInfo;
	}
	~PropChangeHook() { delete vCallbacksInfo; }
	union //unfortunately we MUST use union instead of std::variant cuz we should prevent libstdc++ linking in linux =|
	{
		int									iLastValue;
		float								flLastValue;
		Vector								vecLastValue;
		char								cLastValue[4096];
	};
	SendProp *								pVar;
	PropType								PropType;
	unsigned int							Offset;
	int										objectID;
	int										Element{0};
	CUtlVector<CallBackInfo> *				vCallbacksInfo;
};

struct PropChangeHookGamerules
{
	PropChangeHookGamerules() { vCallbacksInfo = new CUtlVector<CallBackInfo>(); }
	PropChangeHookGamerules(const PropChangeHookGamerules & rObj)
	{
		memcpy(this, &rObj, sizeof(PropChangeHookGamerules));
		vCallbacksInfo = new CUtlVector<CallBackInfo>();
		*vCallbacksInfo = *rObj.vCallbacksInfo;
	}
	~PropChangeHookGamerules() { delete vCallbacksInfo; }
	union //unfortunately we MUST use union instead of std::variant cuz we should prevent libstdc++ linking in linux =|
	{
		int									iLastValue;
		float								flLastValue;
		Vector								vecLastValue;
		char								cLastValue[4096];
	};
	SendProp *								pVar;
	PropType								PropType;
	unsigned int							Offset;
	int										Element{0};
	CUtlVector<CallBackInfo> *				vCallbacksInfo;
};
 
class SendProxyManager :
	public SDKExtension,
	public IPluginsListener,
	public ISMEntityListener
{
public: //sm
	virtual bool SDK_OnLoad(char * error, size_t maxlength, bool late);
	virtual void SDK_OnUnload();
	virtual void SDK_OnAllLoaded();
	
	virtual void OnCoreMapEnd();
	virtual void OnCoreMapStart(edict_t *, int, int);

public: //other
	virtual void OnPluginUnloaded(IPlugin * plugin);
	//returns true upon success
	bool AddHookToList(SendPropHook hook);
	bool AddHookToListGamerules(SendPropHookGamerules hook);

	//returns false if prop type not supported or entity is invalid
	bool AddChangeHookToList(PropChangeHook sHook, CallBackInfo * pInfo);
	bool AddChangeHookToListGamerules(PropChangeHookGamerules sHook, CallBackInfo * pInfo);

	void UnhookProxy(int i);
	void UnhookProxyGamerules(int i);

	void UnhookChange(int i, CallBackInfo * pInfo);
	void UnhookChangeGamerules(int i, CallBackInfo * pInfo);
	virtual int GetClientCount() const;
public: // ISMEntityListener
	virtual void OnEntityDestroyed(CBaseEntity * pEntity);
public:
#if defined SMEXT_CONF_METAMOD
	virtual bool SDK_OnMetamodLoad(ISmmAPI * ismm, char * error, size_t maxlen, bool late);
	//virtual bool SDK_OnMetamodUnload(char *error, size_t maxlength);
	//virtual bool SDK_OnMetamodPauseChange(bool paused, char *error, size_t maxlength);
#endif
};

extern SendProxyManager g_SendProxyManager;
extern IServerGameEnts * gameents;
extern CUtlVector<SendPropHook> g_Hooks;
extern CUtlVector<SendPropHookGamerules> g_HooksGamerules;
extern CUtlVector<PropChangeHook> g_ChangeHooks;
extern CUtlVector<PropChangeHookGamerules> g_ChangeHooksGamerules;
extern const char * g_szGameRulesProxy;
constexpr int g_iEdictCount = 2048; //default value, we do not need to get it manually cuz it is constant
extern ISDKTools * g_pSDKTools;
extern void * g_pGameRules;

#endif // _EXTENSION_H_INC_
