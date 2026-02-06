#ifndef _SENDPROXY_CALLBACK_H
#define _SENDPROXY_CALLBACK_H

#include "extension.h"
#include "sendproxy_variant.h"

using SendProxyCallback = bool (void *callback, const SendProp *pProp, ProxyVariant &variant, int element, int entity, int client);

bool SendProxyPluginCallback(void *callback, const SendProp *pProp, ProxyVariant &variant, int element, int entity, int client);
// bool SendProxyExtCallback(void *callback, const SendProp *pProp, ProxyVariant &variant, int element, int entity, int client);

#endif