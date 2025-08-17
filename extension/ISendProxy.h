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
	Prop_Vector,
	Prop_EHandle,
	Prop_Max
};

enum class CallbackType : uint8_t
{
	Callback_OnChanged = 0,		// Callback only when the edict is marked changed
	Callback_Constant,			// Callback on every frame
};


#endif