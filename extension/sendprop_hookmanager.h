#ifndef _SENDPROP_HOOKMANAGER_H
#define _SENDPROP_HOOKMANAGER_H

#include "extension.h"
#include "am-hashmap.h"
#include "sendproxy_callback.h"
#include "sendproxy_variant.h"
#include <forward_list>
#include <memory>
#include <functional>

class SendProp;

class SendProxyHook
{
public:
	explicit SendProxyHook(SendProp *pProp, SendVarProxyFn pfnProxy);
	~SendProxyHook();
	SendProxyHook(const SendProxyHook &other) = delete;

	const SendProp* GetProp() const { return m_pProp; }
	void CallOriginal(const void *pStructBase, const void *pData, DVariant *pOut, int iElement, int objectID)
	{
		m_fnRealProxy(m_pProp, pStructBase, pData, pOut, iElement, objectID);
	}

private:
	SendProp *m_pProp;
	SendVarProxyFn m_fnRealProxy;
};

struct SendPropHook
{
	std::shared_ptr<SendProxyHook> proxy{nullptr};
	std::function<SendProxyCallback> fnProcess{nullptr};
	void *pCallback{nullptr};
	void *pOwner{nullptr};
	int element{-1};
	PropType type{PropType::Prop_Max};
};

struct SendPropEntityInfo
{
	std::forward_list<SendPropHook> list;
	ProxyVariant data;
};

namespace SendProxy
{
	struct IntHashMapPolicy
	{
		static inline bool matches(const int32_t lookup, const int32_t compare) {
			return lookup == compare;
		}
		static inline uint32_t hash(const int32_t key) {
			return ke::HashInt32(key);
		}
	};
}

using SendPropHookMap = ke::HashMap<const SendProp *, std::weak_ptr<SendProxyHook>, ke::PointerPolicy<const SendProp>>;
using SendPropEntityInfoMap = ke::HashMap<int, SendPropEntityInfo, SendProxy::IntHashMapPolicy>;

class SendPropHookManager
{
public:
	SendPropHookManager();
	SendPropHookManager(const SendPropHookManager &other) = delete;
	SendPropHookManager(SendPropHookManager &&other) = delete;

	bool HookEntity(int entity, SendProp *pProp, int element, PropType type, IPluginFunction *callback);
	void UnhookEntity(int entity, const SendProp *pProp, int element, const void *callback);
	void UnhookEntityAll(int entity);
	SendPropEntityInfo* GetEntityHooks(int entity);

	void OnPluginUnloaded(IPlugin *plugin);
	void OnExtentionUnloaded(IExtension *ext);

	std::shared_ptr<SendProxyHook> GetPropHook(const SendProp *pProp);
	bool IsPropHooked(const SendProp *pProp) const;
	bool IsEntityHooked(int entity) const;
	bool IsEntityHooked(int entity, const SendProp *pProp, int element, const IPluginFunction *pFunc) const;
	bool IsAnyEntityHooked() const;

	void Clear();

protected:
	friend class SendProxyHook;
	void RemoveHook(const SendProp *pProp);

	void RemoveEntity(int entity, std::function<bool(const SendPropHook &)> pred);
	void RemoveEntity(SendPropEntityInfoMap::iterator it, std::function<bool(const SendPropHook &)> pred);

	void OnEntityEnterHook(int entity);
	void OnEntityLeaveHook(int entity);

private:
	SendPropHookMap m_propHooks;
	SendPropEntityInfoMap m_entityInfos;
};

extern SendPropHookManager *g_pSendPropHookManager;

#endif