#if !defined _SENDPROXYMANAGER_INC_
#define _SENDPROXYMANAGER_INC_

// used for LibraryExists(); etc.
#define SENDPROXY_LIB "sendproxy13"

// SendProp::m_Flags.
// public/dt_common.h
#define SPROP_UNSIGNED			(1<<0)	// Unsigned integer data.

#define SPROP_COORD				(1<<1)	// If this is set, the float/vector is treated like a world coordinate.
										// Note that the bit count is ignored in this case.

#define SPROP_NOSCALE			(1<<2)	// For floating point, don't scale into range, just take value as is.

#define SPROP_ROUNDDOWN			(1<<3)	// For floating point, limit high value to range minus one bit unit

#define SPROP_ROUNDUP			(1<<4)	// For floating point, limit low value to range minus one bit unit

#define SPROP_NORMAL			(1<<5)	// If this is set, the vector is treated like a normal (only valid for vectors)
							
#define SPROP_EXCLUDE			(1<<6)	// This is an exclude prop (not excludED, but it points at another prop to be excluded).

#define SPROP_XYZE				(1<<7)	// Use XYZ/Exponent encoding for vectors.

#define SPROP_INSIDEARRAY		(1<<8)	// This tells us that the property is inside an array, so it shouldn't be put into the
										// flattened property list. Its array will point at it when it needs to.

#define SPROP_PROXY_ALWAYS_YES	(1<<9)	// Set for datatable props using one of the default datatable proxies like
										// SendProxy_DataTableToDataTable that always send the data to all clients.

#define SPROP_IS_A_VECTOR_ELEM	(1<<10)	// Set automatically if SPROP_VECTORELEM is used.

#define SPROP_COLLAPSIBLE		(1<<11)	// Set automatically if it's a datatable with an offset of 0 that doesn't change the pointer
										// (ie: for all automatically-chained base classes).
										// In this case, it can get rid of this SendPropDataTable altogether and spare the
										// trouble of walking the hierarchy more than necessary.

#define SPROP_COORD_MP					(1<<12) // Like SPROP_COORD, but special handling for multiplayer games
#define SPROP_COORD_MP_LOWPRECISION 	(1<<13) // Like SPROP_COORD, but special handling for multiplayer games where the fractional component only gets a 3 bits instead of 5
#define SPROP_COORD_MP_INTEGRAL			(1<<14) // SPROP_COORD_MP, but coordinates are rounded to integral boundaries
#define SPROP_CELL_COORD				(1<<15) // Like SPROP_COORD, but special encoding for cell coordinates that can't be negative, bit count indicate maximum value
#define SPROP_CELL_COORD_LOWPRECISION 	(1<<16) // Like SPROP_CELL_COORD, but special handling where the fractional component only gets a 3 bits instead of 5
#define SPROP_CELL_COORD_INTEGRAL		(1<<17) // SPROP_CELL_COORD, but coordinates are rounded to integral boundaries

#define SPROP_CHANGES_OFTEN				(1<<18)	// this is an often changed field, moved to head of sendtable so it gets a small index

#define SPROP_VARINT					(1<<19)	// use var int encoded (google protobuf style), note you want to include SPROP_UNSIGNED if needed, its more efficient

#define SPROP_NUMFLAGBITS_NETWORKED		20


// This is server side only, it's used to mark properties whose SendProxy_* functions encode against gpGlobals->tickcount (the only ones that currently do this are
//  m_flAnimTime and m_flSimulationTime.  MODs shouldn't need to mess with this probably
#define SPROP_ENCODED_AGAINST_TICKCOUNT	(1<<20)

// See SPROP_NUMFLAGBITS_NETWORKED for the ones which are networked
#define SPROP_NUMFLAGBITS				21

enum SendPropType 
{
	Prop_Int,
	Prop_Float,
	Prop_String,
	Prop_Bool,
	Prop_Vector = 4,
	Prop_Max
};

/**
 * Notice:
 * If a SendProp has its own SendProxy function (Or marked as SPROP_PROXY_ALWAYS_YES),
 * Your callback will be called before the original SendProxy function.
 * If your callback falied to call, the original SendProxy function will still be called.
 * 
 * In proxy callbacks, change the value dose not change the original and actual server-side value.
 * But it dose change the value that will be sent to the client.
*/

/**
 * Warning:
 * Do not run entity logic from proxies under any circumstances. 
 * They can be run at unexpected times (e.g. during prediction validation) or not run at all (e.g. full updates). 
 * Use PostDataUpdate() instead.
 * https://developer.valvesoftware.com/wiki/Networking_Entities
*/
typeset SendProxyCallback
{
	/**
	 * The proxy function to call when the entity's SendProp is about to be sent.
	 * 
	 * @param iEntity		The entity you hooked with.
  	 * @param cPropName		The name of the SendProp.
	 * @param iValue		The value of the SendProp that is about to be send. See PropType for type.
  	 * @param iElement		The element number if this data is part of an array. 
	 * 						Useful when the SendProp's type is DPT_Array or DPT_DataTable.
	 * 
  	 * @param iClient		The client index that this proxy send to.
	 * 
	 * @return				Plugin_Changed to use changed value, others otherwise.
	*/
	function Action (int iEntity, const char[] cPropName, int &iValue, int iElement, int iClient);
	function Action (int iEntity, const char[] cPropName, float &flValue, int iElement, int iClient);
	function Action (int iEntity, const char[] cPropName, char cModifiedValue[4096], int iElement, int iClient);
	function Action (int iEntity, const char[] cPropName, float vecValues[3], int iElement, int iClient);
};

typeset SendProxyCallbackGamerules
{
	/**
	 * The proxy function to call when the GameRules's SendProp is about to be sent.
	 * 
	 * @param cPropName		The name of the SendProp.
	 * @param iValue		The value of the SendProp that is about to be send. See PropType for type.
  	 * @param iElement		The element number if this data is part of an array. 
	 * 						Useful when the SendProp's type is DPT_Array or DPT_DataTable.
	 * 
  	 * @param iClient		The client index that this proxy send to.
	 * 
	 * @return 				Plugin_Changed to use changed value, others otherwise.
	*/
	function Action (const char[] cPropName, int &iValue, int iElement, int iClient);
	function Action (const char[] cPropName, float &flValue, int iElement, int iClient);
	function Action (const char[] cPropName, char cModifiedValue[4096], int iElement, int iClient);
	function Action (const char[] cPropName, float vecValues[3], int iElement, int iClient);
};

/**
 * Notice: 
 * Changed Callbacks is just literally hooks GameFrame.
 * Use this only if you do not might take actions in every frames.
*/
typeset PropChangedCallback
{
	function void(int iEntity, const char[] cPropName, int iOldValue, int iNewValue, int iElement);
	function void(int iEntity, const char[] cPropName, float flOldValue, float flNewValue, int iElement);
	function void(int iEntity, const char[] cPropName, const char[] cOldValue, const char[] cNewValue, int iElement);
	function void(int iEntity, const char[] cPropName, const float vecOldValue[3], const float vecNewValue[3], int iElement);
};

typeset GameRulesPropChangedCallback
{
	function void(const char[] cPropName, int iOldValue, int iNewValue, int iElement);
	function void(const char[] cPropName, float flOldValue, float flNewValue, int iElement);
	function void(const char[] cPropName, const char[] cOldValue, const char[] cNewValue, int iElement);
	function void(const char[] cPropName, const float vecOldValue[3], const float vecNewValue[3], int iElement);
};

methodmap SendProxyManager
{
	/*
	 * Hooks/Unhooks a SendProp of entity.
	 * This hook will be removed automatically when extension unloaded or upon the destruction of entity.
	 * 
	 * @note Do not use this on a SendProp marked as SPROP_INSIDEARRAY. Use HookArray* instead.
	 *
	 * @param iEntity		The entity to hook/unhook.
	 * @param cPropName		The name of the SendProp.
	 * @param stType		The type of the SendProp.
	 * @param pCallback		The callback function to linked to call / unlinked.
	 *
	 * @return				true, if prop hooked/unhooked, false otherwise
	 */
	public static native bool Hook(int iEntity, const char[] cPropName, SendPropType stType, SendProxyCallback pCallback);
	public static native bool Unhook(int iEntity, const char[] cPropName, SendProxyCallback pCallback);

	/*
	 * Hooks/Unhooks a SendProp of GameRules.
	 * This hook will be removed automatically when extension unloaded.
	 * 
	 * @note Do not use this on a SendProp marked as SPROP_INSIDEARRAY. Use HookArray* instead.
	 *
	 * @param cPropName		The name of the SendProp.
	 * @param stType		The type of the SendProp.
	 * @param pCallback		The callback function to linked to call / unlinked.
	 *
	 * @return				true, if prop hooked/unhooked, false otherwise
	*/
	public static native bool HookGameRules(const char[] cPropName, SendPropType stType, SendProxyCallbackGamerules pCallback);
	public static native bool UnhookGameRules(const char[] cPropName, SendProxyCallbackGamerules pCallback);

	/*
	 * Hooks/Unhooks a element of a SendProp array of an entity.
	 * This hook will be removed automatically when extension unloaded or upon the destruction of entity.
	 * Every element of the array is a single base SendProp. Meaning, every element will be need a single hook.
	 * Use this only if the SendProp's type is DPT_Array or DPT_DataTable.
	 *
	 * @param iEntity		The entity to hook/unhook.
	 * @param cPropName		The name of the SendProp.
	 * @param iElement		The element number of the SendProp array.
	 * @param stType		The type of the SendProp.
	 * @param pCallback		The callback function to linked to call / unlinked.
	 *
	 * @return				true, if prop hooked/unhooked, false otherwise
	*/
	public static native bool HookArray(int iEntity, const char[] cPropName, int iElement, SendPropType stType, SendProxyCallback pCallback);
	public static native bool UnhookArray(int iEntity, const char[] cPropName, int iElement, SendProxyCallback pCallback);
	
	/*
	 * Hooks/Unhooks a element of a SendProp array of GameRules.
	 * This hook will be removed automatically when extension unloaded.
	 * Every element of the array is a single base SendProp. Meaning, every element will be need a single hook.
	 * Use this only if the SendProp's type is DPT_Array or DPT_DataTable.
	 *
	 * @param cPropName		The name of the SendProp.
	 * @param iElement		The element number of the SendProp array.
	 * @param stType		The type of the SendProp.
	 * @param pCallback		The callback function to linked to call / unlinked.
	 *
	 * @return				true, if prop hooked/unhooked, false otherwise
	*/
	public static native bool HookGameRulesArray(const char[] cPropName, int iElement, SendPropType stType, SendProxyCallbackGamerules pCallback);
	public static native bool UnhookGameRulesArray(const char[] cPropName, int iElement, SendProxyCallbackGamerules pCallback);

	/*
	 * Checks if a SendProp is hooked.
	 *
	 * @param iEntity		The entity to check.
	 * @param cPropName		The name of the SendProp.
	 * @param iElement		The element number of the SendProp array.
	 *
	 * @return				true, if prop is hooked, false otherwise
	*/
	public static native bool IsHooked(int iEntity, const char[] cPropName);
	public static native bool IsGameRulesHooked(const char[] cPropName);
	public static native bool IsArrayHooked(int iEntity, const char[] cPropName, int iElement);
	public static native bool IsGameRulesArrayHooked(const char[] cPropName, int iElement);

	/** 
	 * Hooks/Unhooks a SendProp change of an entity.
	 * 
	 * @note Do not use this on a SendProp marked as SPROP_INSIDEARRAY. Use HookArray* instead.
	 *
	 * @param iEntity		The entity to hook/unhook.
	 * @param cPropName		The name of the SendProp.
	 * @param stType		The type of the SendProp.
  	 * @param pCallback		The callback function to linked to call / unlinked.
	 *
	 * @return				true, if prop is hooked/unhooked, false otherwise
	*/
	public static native bool HookChange(int iEntity, const char[] cPropName, SendPropType stType, PropChangedCallback pCallback);
	public static native bool UnhookChange(int iEntity, const char[] cPropName, PropChangedCallback pCallback);

	/**
	 * Hooks/Unhooks a SendProp change of GameRules.
	 * 
	 * @note Do not use this on a SendProp marked as SPROP_INSIDEARRAY. Use HookArray* instead.
	 * 
	 * @param cPropName		The name of the SendProp.
  	 * @param stType		The type of the SendProp.
  	 * @param pCallback		The callback function to linked to call / unlinked.
	 * 
	 * @return				true, if prop is hooked/unhooked, false otherwise
	*/
	public static native bool HookGameRulesChange(const char[] cPropName, SendPropType stType, GameRulesPropChangedCallback pCallback);
	public static native bool UnhookGameRulesChange(const char[] cPropName, GameRulesPropChangedCallback pCallback);

	/**
	 * Hooks/Unhooks a SendProp change of an entity's array element.
  	 * Every element of the array is a single base SendProp. Meaning, every element will be need a single hook.
	 * 
  	 * @param iEntity		The entity to hook/unhook.
  	 * @param cPropName		The name of the SendProp.
  	 * @param iElement		The element number of the SendProp array.
  	 * @param stType		The type of the SendProp.
  	 * @param pCallback		The callback function to linked to call / unlinked.
	 * 
	 * @return				true, if prop is hooked/unhooked, false otherwise
	*/
	public static native bool HookArrayChange(int iEntity, const char[] cPropName, int iElement, SendPropType stType, PropChangedCallback pCallback);
	public static native bool UnhookArrayChange(int iEntity, const char[] cPropName, int iElement, PropChangedCallback pCallback);

	/**
	 * Hooks/Unhooks a SendProp change of GameRules's array element.
	 * Every element of the array is a single base SendProp. Meaning, every element will be need a single hook.
  	 *
  	 * @param cPropName		The name of the SendProp.
  	 * @param iElement		The element number of the SendProp array.
  	 * @param stType		The type of the SendProp.
  	 * @param pCallback		The callback function to linked to call / unlinked.
  	 *
  	 * @return				true, if prop is hooked/unhooked, false otherwise
	*/
	public static native bool HookGameRulesArrayChange(const char[] cPropName, int iElement, SendPropType stType, PropChangedCallback pCallback);
 	public static native bool UnhookGameRulesArrayChange(const char[] cPropName, int iElement, PropChangedCallback pCallback);

	/**
	 * Checks if a SendProp change is hooked.
	 * 
	 * @param iEntity		The entity to check.
  	 * @param cPropName		The name of the SendProp.
	 * @param iElement		The element number of the SendProp array.
	 * 
  	 * @return				true, if prop is hooked, false otherwise
	*/
	public static native bool IsChangeHooked(int iEntity, const char[] cPropName);
	public static native bool IsGameRulesChangeHooked(const char[] cPropName);
	public static native bool IsArrayChangeHooked(int iEntity, const char[] cPropName, int iElement);
	public static native bool IsGameRulesArrayChangeHooked(const char[] cPropName, int iElement);
}

/**
 * Returns the SendProp flag of a given entity's/GameRules's SendProp.
 * 
 *  @param iEntity		The entity to check.
 *  @param cPropName	The name of the SendProp.
 *  @param stType		The type of the SendProp.
 * 
 *  @return				The SendProp flag bit of the SendProp, or -1 if not found.
 */
native int GetEntSendPropFlag(int iEntity, const char[] cPropName);
native int GetGameRulesSendPropFlag(const char[] cPropName);

#if !defined REQUIRE_EXTENSIONS
public __ext_sendproxymanager_SetNTVOptional()
{
	MarkNativeAsOptional("SendProxyManager.Hook");
	MarkNativeAsOptional("SendProxyManager.Unhook");

	MarkNativeAsOptional("SendProxyManager.HookGameRules");
	MarkNativeAsOptional("SendProxyManager.UnhookGameRules");

	MarkNativeAsOptional("SendProxyManager.HookArray");
	MarkNativeAsOptional("SendProxyManager.UnhookArray");

	MarkNativeAsOptional("SendProxyManager.HookGameRulesArray");
	MarkNativeAsOptional("SendProxyManager.UnhookGameRulesArray");
	
	MarkNativeAsOptional("SendProxyManager.IsHooked");
	MarkNativeAsOptional("SendProxyManager.IsGameRulesHooked");
	MarkNativeAsOptional("SendProxyManager.IsArrayHooked");
	MarkNativeAsOptional("SendProxyManager.IsGameRulesArrayHooked");

	MarkNativeAsOptional("SendProxyManager.HookChange");
	MarkNativeAsOptional("SendProxyManager.UnhookPropChange");

	MarkNativeAsOptional("SendProxyManager.HookGameRulesChange");
	MarkNativeAsOptional("SendProxyManager.UnhookGameRulesChange");

	MarkNativeAsOptional("SendProxyManager.HookArrayChange");
	MarkNativeAsOptional("SendProxyManager.UnhookArrayChange");

	MarkNativeAsOptional("SendProxyManager.HookGameRulesArrayChange");
	MarkNativeAsOptional("SendProxyManager.UnhookGameRulesArrayChange");

	MarkNativeAsOptional("SendProxyManager.IsChangeHooked");
	MarkNativeAsOptional("SendProxyManager.IsGameRulesChangeHooked");
	MarkNativeAsOptional("SendProxyManager.IsArrayChangeHooked");
	MarkNativeAsOptional("SendProxyManager.IsGameRulesArrayChangeHooked");

	MarkNativeAsOptional("GetEntPropFlag");
	MarkNativeAsOptional("GetGameRulesSendPropFlag");
}
#endif  

public Extension __ext_sendproxymanager = 
{
	name = "SendProxy Manager",
	file = "sendproxy.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};

#endif