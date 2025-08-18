#if defined _SENDPROXYMANAGER_INC_
	#endinput
#endif
#define _SENDPROXYMANAGER_INC_

#define SENDPROXY_LIB "sendproxy2"

enum SendPropType
{
	Prop_Int,
	Prop_Float,
	Prop_String,
	Prop_Vector,
	Prop_EHandle,
	Prop_Max
};

typeset SendProxyCallback
{
	function Action (int entity, const char[] prop, int &value, int element, int client); //Prop_Int, Prop_EHandle
	function Action (int entity, const char[] prop, float &value, int element, int client); //Prop_Float
	function Action (int entity, const char[] prop, char[] value, int maxlength, int element, int client); //Prop_String
	function Action (int entity, const char[] prop, float value[3], int element, int client); //Prop_Vector
};

typeset SendProxyCallbackGamerules
{
	function Action (const char[] prop, int &value, int element, int client); //Prop_Int, Prop_EHandle
	function Action (const char[] prop, float &value, int element, int client); //Prop_Float
	function Action (const char[] prop, char[] value, int maxlength, int element, int client); //Prop_String
	function Action (const char[] prop, float value[3], int element, int client); //Prop_Vector
};

//Returns true upon success, false upon failure
native bool SendProxy_HookEntity(int entity, const char[] prop, SendPropType type, SendProxyCallback callback, int element = 0);
native bool SendProxy_UnhookEntity(int entity, const char[] prop, SendProxyCallback callback, int element = 0);
native bool SendProxy_IsHookedEntity(int entity, const char[] prop, SendProxyCallback callback, int element = 0);
native bool SendProxy_HookGameRules(const char[] prop, SendPropType type, SendProxyCallbackGamerules callback, int element = 0);
native bool SendProxy_UnhookGameRules(const char[] prop, SendProxyCallbackGamerules callback, int element = 0);
native bool SendProxy_IsHookedGameRules(const char[] prop, SendProxyCallbackGamerules callback, int element = 0);

public __ext_sendproxymanager_SetNTVOptional()
{
#if !defined REQUIRE_EXTENSIONS
    MarkNativeAsOptional("SendProxy_HookEntity");
    MarkNativeAsOptional("SendProxy_UnhookEntity");
    MarkNativeAsOptional("SendProxy_IsHookedEntity");
    MarkNativeAsOptional("SendProxy_HookGameRules");
    MarkNativeAsOptional("SendProxy_UnhookGameRules");
    MarkNativeAsOptional("SendProxy_IsHookedGameRules");
#endif  
}

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