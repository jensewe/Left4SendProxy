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

class SendProxyManager :
	public SDKExtension,
	public IPluginsListener,
	public IConCommandBaseAccessor,
	public IClientListener,
	public ISMEntityListener
{
public:
	virtual bool SDK_OnLoad(char * error, size_t maxlength, bool late);
	virtual void SDK_OnUnload();
	virtual void SDK_OnAllLoaded();

	/**
	 * @brief Asks the extension whether it's safe to remove an external
	 * interface it's using.  If it's not safe, return false, and the
	 * extension will be unloaded afterwards.
	 *
	 * NOTE: It is important to also hook NotifyInterfaceDrop() in order to clean
	 * up resources.
	 *
	 * @param pInterface		Pointer to interface being dropped.  This
	 * 							pointer may be opaque, and it should not
	 *							be queried using SMInterface functions unless
	 *							it can be verified to match an existing
	 *							pointer of known type.
	 * @return					True to continue, false to unload this
	 * 							extension afterwards.
	 */
	bool QueryInterfaceDrop(SMInterface* pInterface) override;

	/**
	 * @brief Notifies the extension that an external interface it uses is being removed.
	 *
	 * @param pInterface		Pointer to interface being dropped.  This
	 * 							pointer may be opaque, and it should not
	 *							be queried using SMInterface functions unless
	 *							it can be verified to match an existing
	 */
	void NotifyInterfaceDrop(SMInterface* pInterface) override;

	/**
	 * @brief Return false to tell Core that your extension should be considered unusable.
	 *
	 * @param error				Error buffer.
	 * @param maxlength			Size of error buffer.
	 * @return					True on success, false otherwise.
	 */
	bool QueryRunning(char* error, size_t maxlength) override;
	
public:
#if defined SMEXT_CONF_METAMOD
	virtual bool SDK_OnMetamodLoad(ISmmAPI * ismm, char * error, size_t maxlen, bool late);
	//virtual bool SDK_OnMetamodUnload(char *error, size_t maxlength);
	//virtual bool SDK_OnMetamodPauseChange(bool paused, char *error, size_t maxlength);
#endif

public: //SDKExtension
	void OnCoreMapEnd() override;

public: //IPluginsListener
	void OnPluginUnloaded(IPlugin * plugin) override;

public: //IClientListener
	void OnClientDisconnected(int client) override;

public: //ISMEntityListener
	void OnEntityDestroyed(CBaseEntity *pEntity) override;

public: //IConCommandBaseAccessor
	bool RegisterConCommandBase(ConCommandBase* pVar) override;
};

extern SendProxyManager g_SendProxyManager;
extern ConVar *sv_parallel_packentities;
extern CFrameSnapshotManager *framesnapshotmanager;

CBaseEntity *GetGameRulesProxyEnt();

#endif // _EXTENSION_H_INC_
