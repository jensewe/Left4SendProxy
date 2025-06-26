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

#ifndef _INCLUDE_ISENDPROXY_
#define _INCLUDE_ISENDPROXY_
 
//WARNING! Interface not tested yet, but you can test it by yourself and report about any errors to github: https://github.com/TheByKotik/sendproxy

#include <IShareSys.h>
#include <IExtensionSys.h>
#include "dt_send.h"
#include "server_class.h"

#define SMINTERFACE_SENDPROXY_NAME		"ISendProxyInterface133"
#define SMINTERFACE_SENDPROXY_VERSION	0x133

class CBaseEntity;
class CBasePlayer;
class ISendProxyUnhookListener;

using namespace SourceMod;

enum class PropType : uint8_t
{
	Prop_Int = 0,
	Prop_Float, 
	Prop_String,
	Prop_Bool,
	Prop_Vector = 4,
	Prop_Max
};

enum class CallBackType : uint8_t
{
	Callback_PluginFunction = 1,
	Callback_CPPCallbackInterface //see ISendProxyCallbacks & ISendProxyChangeCallbacks
};
 
class ISendProxyUnhookListener
{
public:
	/*
	 * Calls when hook of the entity prop is removed
	 *
	 * @param pEntity		Pointer to CBaseEntity object that was hooked
	 * @param pProp			Pointer to SendProp that was hooked
	 * @param iType			PropType of the prop
	 * @param iCallbackType	Type of callback
	 * @param pCallback		Pointer to callback function / class
	 *
	 * @noreturn
	 */
	virtual void OnEntityPropHookRemoved(const CBaseEntity * pEntity, const SendProp * pProp, const PropType iType, const CallBackType iCallbackType, const void * pCallback) = 0;
	/*
	 * Calls when hook of the gamerules prop is removed
	 *
	 * @param pProp			Pointer to SendProp that was hooked
	 * @param iType			PropType of the prop
	 * @param iCallbackType	Type of callback
	 * @param pCallback		Pointer to callback function / class
	 *
	 * @noreturn
	 */
	virtual void OnGamerulesPropHookRemoved(const SendProp * pProp, const PropType iType, const CallBackType iCallbackType, const void * pCallback) = 0;
};

class ISendProxyCallbacks
{
public:
	/*
	 * Calls when proxy function of entity prop is called
	 *
	 * @param pEntity		Pointer to CBaseEntity object that hooked
	 * @param pProp			Pointer to SendProp that hooked
	 * @param pPlayer		Pointer to CBasePlayer object of the client that should receive the changed value
	 * @param pValue		Pointer to value of prop
	 * @param iType			PropType of the prop
	 * @param iElement		Element number
	 *
	 * @return				true, to use changed value, false, to use original
	 */
	virtual bool OnEntityPropProxyFunctionCalls(const CBaseEntity * pEntity, const SendProp * pProp, const CBasePlayer * pPlayer, void * pValue, const PropType iType, const int iElement) = 0;
	/*
	 * Calls when proxy function of gamerules prop is called
	 *
	 * @param pProp			Pointer to SendProp that hooked
	 * @param pPlayer		Pointer to CBasePlayer object of the client that should receive the changed value
	 * @param pValue		Pointer to value of prop
	 * @param iType			PropType of the prop
	 * @param iElement		Element number
	 *
	 * @return				true, to use changed value, false, to use original
	 */
	virtual bool OnGamerulesPropProxyFunctionCalls(const SendProp * pProp, const CBasePlayer * pPlayer, void * pValue, const PropType iType, const int iElement) = 0;
};

class ISendProxyChangeCallbacks
{
public:
	/*
	 * Calls when prop of entity is changed
	 *
	 * @param pEntity		Pointer to CBaseEntity object that hooked
	 * @param pProp			Pointer to SendProp that hooked
	 * @param pNewValue		Pointer to new value of prop
	 * @param pOldValue		Pointer to old value of prop
	 * @param iType			PropType of the prop
	 * @param iElement		Element number
	 *
	 * @noreturn
	 */
	virtual void OnEntityPropChange(const CBaseEntity * pEntity, const SendProp * pProp, const void * pNewValue, const void * pOldValue, const PropType iType, const int iElement) = 0;
	/*
	 * Calls when prop of gamerules is changed
	 *
	 * @param pProp			Pointer to SendProp that hooked
	 * @param pNewValue		Pointer to new value of prop
	 * @param pOldValue		Pointer to old value of prop
	 * @param iType			PropType of the prop
	 * @param iElement		Element number
	 *
	 * @noreturn
	 */
	virtual void OnGamerulesPropChange(const SendProp * pProp, const void * pNewValue, const void * pOldValue, const PropType iType, const int iElement) = 0;
};

class ISendProxyManager : public SMInterface
{
public: //SMInterface
	virtual const char * GetInterfaceName() = 0;
	virtual unsigned int GetInterfaceVersion() = 0;
	
public: //ISendProxyManager
	/*
	 * Hooks SendProp of entity, this hook removes automatically when extension in unloaded.
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be hooked
	 * @param pEntity		Pointer to CBaseEntity object that should be hooked
	 * @param iType			PropType of the prop
	 * @param iCallbackType	Type of callback
	 * @param pCallback		Pointer to callback function / class
	 *
	 * @return				true, if prop hooked, false otherwise
	 */
	virtual bool HookProxy(IExtension * pMyself, SendProp * pProp, CBaseEntity * pEntity, PropType iType, CallBackType iCallbackType, void * pCallback) = 0;
	virtual bool HookProxy(IExtension * pMyself, const char * pProp, CBaseEntity * pEntity, PropType iType, CallBackType iCallbackType, void * pCallback) = 0;
	/*
	 * Hooks gamerules SendProp, this hook removes automatically when extension in unloaded.
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be hooked
	 * @param iType			PropType of the prop
	 * @param iCallbackType	Type of callback
	 * @param pCallback		Pointer to callback function / class
	 *
	 * @return				true, if prop hooked, false otherwise
	 */
	virtual bool HookProxyGamerules(IExtension * pMyself, SendProp * pProp, PropType iType, CallBackType iCallbackType, void * pCallback) = 0;
	virtual bool HookProxyGamerules(IExtension * pMyself, const char * pProp, PropType iType, CallBackType iCallbackType, void * pCallback) = 0;
	/*
	 * Unhooks SendProp of entity
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be unhooked
	 * @param pEntity		Pointer to CBaseEntity object that should be unhooked
	 * @param iCallbackType	Type of callback
	 * @param pCallback		Pointer to callback function / class
	 *
	 * @return				true, if prop unhooked, false otherwise
	 *						P.S. This function will trigger unhook listeners
	 */
	virtual bool UnhookProxy(IExtension * pMyself, SendProp * pProp, CBaseEntity * pEntity, CallBackType iCallbackType, void * pCallback) = 0;
	virtual bool UnhookProxy(IExtension * pMyself, const char * pProp, CBaseEntity * pEntity, CallBackType iCallbackType, void * pCallback) = 0;
	/*
	 * Unhooks gamerules SendProp
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be unhooked
	 * @param iCallbackType	Type of callback
	 * @param pCallback		Pointer to callback function / class
	 *
	 * @return				true, if prop unhooked, false otherwise
	 *						P.S. This function will trigger unhook listeners
	 */
	virtual bool UnhookProxyGamerules(IExtension * pMyself, SendProp * pProp, CallBackType iCallbackType, void * pCallback) = 0;
	virtual bool UnhookProxyGamerules(IExtension * pMyself, const char * pProp, CallBackType iCallbackType, void * pCallback) = 0;
	/*
	 * Adds unhook listener to entity hook, so, when hook will be removed listener callback is called. This listener removes automatically when extension in unloaded.
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be listen
	 * @param pEntity		Pointer to CBaseEntity object that should be listen
	 * @param iCallbackType	Type of callback of entity hook
	 * @param pCallback		Pointer to callback function / class of entity hook
	 * @param pListener		Pointer to listener callback
	 *
	 * @return				true, if listener installed, false otherwise
	 */
	virtual bool AddUnhookListener(IExtension * pMyself, SendProp * pProp, CBaseEntity * pEntity, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	virtual bool AddUnhookListener(IExtension * pMyself, const char * pProp, CBaseEntity * pEntity, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	/*
	 * Adds unhook listener to gamerules hook, so, when hook will removed listener callback is called. This listener removes automatically when extension in unloaded.
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be listen
	 * @param iCallbackType	Type of callback of gamerules hook
	 * @param pCallback		Pointer to callback function / class of gamerules hook
	 * @param pListener		Pointer to listener callback
	 *
	 * @return				true, if listener installed, false otherwise
	 */
	virtual bool AddUnhookListenerGamerules(IExtension * pMyself, SendProp * pProp, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	virtual bool AddUnhookListenerGamerules(IExtension * pMyself, const char * pProp, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	/*
	 * Removes unhook listener from entity hook
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that is listening
	 * @param pEntity		Pointer to CBaseEntity object that is listening
	 * @param iCallbackType	Type of callback of entity hook
	 * @param pCallback		Pointer to callback function / class of entity hook
	 * @param pListener		Pointer to listener callback
	 *
	 * @return				true, if listener removed, false otherwise
	 */
	virtual bool RemoveUnhookListener(IExtension * pMyself, SendProp * pProp, CBaseEntity * pEntity, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	virtual bool RemoveUnhookListener(IExtension * pMyself, const char * pProp, CBaseEntity * pEntity, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	/*
	 * Removes unhook listener from gamerules hook
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that is listening
	 * @param iCallbackType	Type of callback of gamerules hook
	 * @param pCallback		Pointer to callback function / class of gamerules hook
	 * @param pListener		Pointer to listener callback
	 *
	 * @return				true, if listener removed, false otherwise
	 */
	virtual bool RemoveUnhookListenerGamerules(IExtension * pMyself, SendProp * pProp, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	virtual bool RemoveUnhookListenerGamerules(IExtension * pMyself, const char * pProp, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	/*
	 * Hooks element of SendProp array of entity, this hook removes automatically when extension in unloaded.
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be hooked
	 * @param pEntity		Pointer to CBaseEntity object that should be hooked
	 * @param iType			PropType of the prop
	 * @param iElement		Element number
	 * @param iCallbackType	Type of callback
	 * @param pCallback		Pointer to callback function / class
	 *
	 * @return				true, if prop hooked, false otherwise
	 */
	virtual bool HookProxyArray(IExtension * pMyself, SendProp * pProp, CBaseEntity * pEntity, PropType iType, int iElement, CallBackType iCallbackType, void * pCallback) = 0;
	virtual bool HookProxyArray(IExtension * pMyself, const char * pProp, CBaseEntity * pEntity, PropType iType, int iElement, CallBackType iCallbackType, void * pCallback) = 0;
	
	/*
	 * Unhooks element of SendProp array of entity
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be unhooked
	 * @param pEntity		Pointer to CBaseEntity object that should be unhooked
	 * @param iElement		Element number
	 * @param iCallbackType	Type of callback
	 * @param pCallback		Pointer to callback function / class
	 *
	 * @return				true, if prop unhooked, false otherwise
	 *						P.S. This function will trigger unhook listeners
	 */
	virtual bool UnhookProxyArray(IExtension * pMyself, SendProp * pProp, CBaseEntity * pEntity, int iElement, CallBackType iCallbackType, void * pCallback) = 0;
	virtual bool UnhookProxyArray(IExtension * pMyself, const char * pProp, CBaseEntity * pEntity, int iElement, CallBackType iCallbackType, void * pCallback) = 0;
	/*
	 * Hooks element of gamerules SendProp array, this hook removes automatically when extension in unloaded.
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be hooked
	 * @param iType			PropType of the prop
	 * @param iElement		Element number
	 * @param iCallbackType	Type of callback
	 * @param pCallback		Pointer to callback function / class
	 *
	 * @return				true, if prop hooked, false otherwise
	 */
	virtual bool HookProxyArrayGamerules(IExtension * pMyself, SendProp * pProp, PropType iType, int iElement, CallBackType iCallbackType, void * pCallback) = 0;
	virtual bool HookProxyArrayGamerules(IExtension * pMyself, const char * pProp, PropType iType, int iElement, CallBackType iCallbackType, void * pCallback) = 0;
	
	/*
	 * Unhooks element of gamerules SendProp array
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be unhooked
	 * @param iElement		Element number
	 * @param iCallbackType	Type of callback
	 * @param pCallback		Pointer to callback function / class
	 *
	 * @return				true, if prop unhooked, false otherwise
	 * 						P.S. This function will trigger unhook listeners
	 */
	virtual bool UnhookProxyArrayGamerules(IExtension * pMyself, SendProp * pProp, int iElement, CallBackType iCallbackType, void * pCallback) = 0;
	virtual bool UnhookProxyArrayGamerules(IExtension * pMyself, const char * pProp, int iElement, CallBackType iCallbackType, void * pCallback) = 0;
	
	/*
	 * Adds unhook listener to entity array hook, so, when hook will be removed listener callback is called. This listener removes automatically when extension in unloaded.
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be listen
	 * @param pEntity		Pointer to CBaseEntity object that should be listen
	 * @param iElement		Element number
	 * @param iCallbackType	Type of callback of entity hook
	 * @param pCallback		Pointer to callback function / class of entity hook
	 * @param pListener		Pointer to listener callback
	 *
	 * @return				true, if listener installed, false otherwise
	 */
	virtual bool AddUnhookListenerArray(IExtension * pMyself, SendProp * pProp, CBaseEntity * pEntity, int iElement, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	virtual bool AddUnhookListenerArray(IExtension * pMyself, const char * pProp, CBaseEntity * pEntity, int iElement, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	/*
	 * Adds unhook listener to gamerules array hook, so, when hook will removed listener callback is called. This listener removes automatically when extension in unloaded.
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that should be listen
	 * @param iElement		Element number
	 * @param iCallbackType	Type of callback of gamerules hook
	 * @param pCallback		Pointer to callback function / class of gamerules hook
	 * @param pListener		Pointer to listener callback
	 *
	 * @return				true, if listener installed, false otherwise
	 */
	virtual bool AddUnhookListenerArrayGamerules(IExtension * pMyself, SendProp * pProp, int iElement, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	virtual bool AddUnhookListenerArrayGamerules(IExtension * pMyself, const char * pProp, int iElement, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	/*
	 * Removes unhook listener from entity array hook
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that is listening
	 * @param pEntity		Pointer to CBaseEntity object that is listening
	 * @param iElement		Element number
	 * @param iCallbackType	Type of callback of entity hook
	 * @param pCallback		Pointer to callback function / class of entity hook
	 * @param pListener		Pointer to listener callback
	 *
	 * @return				true, if listener removed, false otherwise
	 */
	virtual bool RemoveUnhookListenerArray(IExtension * pMyself, SendProp * pProp, CBaseEntity * pEntity, int iElement, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	virtual bool RemoveUnhookListenerArray(IExtension * pMyself, const char * pProp, CBaseEntity * pEntity, int iElement, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	/*
	 * Removes unhook listener from gamerules array hook
	 *
	 * @param pMyself		Pointer to IExtension interface of current extension, must be valid!
	 * @param pProp			Pointer to SendProp / name of the prop that is listening
	 * @param iElement		Element number
	 * @param iCallbackType	Type of callback of gamerules hook
	 * @param pCallback		Pointer to callback function / class of gamerules hook
	 * @param pListener		Pointer to listener callback
	 *
	 * @return				true, if listener removed, false otherwise
	 */
	virtual bool RemoveUnhookListenerArrayGamerules(IExtension * pMyself, SendProp * pProp, int iElement, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	virtual bool RemoveUnhookListenerArrayGamerules(IExtension * pMyself, const char * pProp, int iElement, CallBackType iCallbackType, void * pCallback, ISendProxyUnhookListener * pListener) = 0;
	/*
	 * Checks if proxy is hooked
	 *
	 * @param pProp			Pointer to SendProp / name of the prop that should be checked
	 * @param pEntity		Pointer to CBaseEntity object that should be checked
	 *
	 * @return				true, if is hooked, false otherwise
	 */
	virtual bool IsProxyHooked(IExtension * pMyself, SendProp * pProp, CBaseEntity * pEntity) const = 0;
	virtual bool IsProxyHooked(IExtension * pMyself, const char * pProp, CBaseEntity * pEntity) const = 0;
	/*
	 * Checks if gamerules proxy is hooked
	 *
	 * @param pProp			Pointer to SendProp / name of the prop that should be checked
	 *
	 * @return				true, if is hooked, false otherwise
	 */
	virtual bool IsProxyHookedGamerules(IExtension * pMyself, SendProp * pProp) const = 0;
	virtual bool IsProxyHookedGamerules(IExtension * pMyself, const char * pProp) const = 0;
	/*
	 * Checks if proxy array is hooked
	 *
	 * @param pProp			Pointer to SendProp / name of the prop that should be checked
	 * @param pEntity		Pointer to CBaseEntity object that should be checked
	 * @param iElement		Element number
	 *
	 * @return				true, if is hooked, false otherwise
	 */
	virtual bool IsProxyHookedArray(IExtension * pMyself, SendProp * pProp, CBaseEntity * pEntity, int iElement) const = 0;
	virtual bool IsProxyHookedArray(IExtension * pMyself, const char * pProp, CBaseEntity * pEntity, int iElement) const = 0;
	/*
	 * Checks if gamerules proxy is hooked
	 *
	 * @param pProp			Pointer to SendProp / name of the prop that should be checked
	 * @param iElement		Element number
	 *
	 * @return				true, if is hooked, false otherwise
	 */
	virtual bool IsProxyHookedArrayGamerules(IExtension * pMyself, SendProp * pProp, int iElement) const = 0;
	virtual bool IsProxyHookedArrayGamerules(IExtension * pMyself, const char * pProp, int iElement) const = 0;
};

#endif