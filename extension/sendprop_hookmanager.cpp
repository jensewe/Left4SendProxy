#include "sendprop_hookmanager.h"
#include "clientpacks_detours.h"
#include <algorithm>

void GlobalProxy(const SendProp *pProp, const void *pStructBase, const void *pData, DVariant *pOut, int iElement, int objectID);

SendProxyHook::SendProxyHook(SendProp *pProp, SendVarProxyFn pfnProxy)
{
	m_pProp = pProp;
	m_fnRealProxy = m_pProp->GetProxyFn();
	m_pProp->SetProxyFn( pfnProxy );
}

SendProxyHook::~SendProxyHook()
{
	m_pProp->SetProxyFn( m_fnRealProxy );
	g_pSendPropHookManager->RemoveHook(m_pProp);
}

SendPropHookManager::SendPropHookManager()
{
}

void SendPropHookManager::Clear()
{
	m_propHooks.clear();
	m_entityInfos.clear();
	ClientPacksDetour::Clear();
}

void SendPropHookManager::RemoveHook(const SendProp *pProp)
{
	m_propHooks.erase(pProp);
}

void SendPropHookManager::RemoveEntity(int entity, std::function<bool(const SendPropHook &)> pred)
{
	if (auto&& it = m_entityInfos.find(entity); it != m_entityInfos.end())
		RemoveEntityAt(it, pred);
}

SendPropHookManager::SendPropEntityInfoMap::iterator
SendPropHookManager::RemoveEntityAt(SendPropEntityInfoMap::iterator &it, std::function<bool(const SendPropHook &)> pred)
{
	auto &&[entity, info] = *it;
	Assert(info != nullptr);

	info->list.remove_if(pred);
	if (info->list.empty())
	{
		OnEntityLeaveHook(entity);
		return m_entityInfos.erase(it);
	}
	return ++it;
}

bool SendPropHookManager::HookEntity(int entity, SendProp *pProp, int element, PropType type, IPluginFunction *pFunc) noexcept
{
	Assert(m_propHooks.find(pProp) == m_propHooks.end() || !m_propHooks[pProp].expired());

	SendPropHook hook;
	hook.element = element;
	hook.type = type;
	hook.fnProcess = SendProxyPluginCallback;
	hook.pCallback = pFunc;
	hook.pOwner = pFunc->GetParentRuntime();
	hook.proxy = m_propHooks[pProp].lock();

	if (hook.proxy == nullptr)
	{
		hook.proxy = std::make_shared<SendProxyHook>(pProp, GlobalProxy);
		m_propHooks[pProp] = hook.proxy;
	}

	if (m_entityInfos.find(entity) == m_entityInfos.end())
	{
		m_entityInfos.emplace(entity, std::make_shared<SendPropEntityInfo>());
		OnEntityEnterHook(entity);
	}
	m_entityInfos.at(entity)->list.emplace_front(std::move(hook));

	return true;
}

void SendPropHookManager::UnhookEntity(int entity, const SendProp *pProp, int element, const void *pCallback)
{
	RemoveEntity(entity, [&](const SendPropHook &hook)
				 { return hook.proxy->GetProp() == pProp && hook.element == element && hook.pCallback == pCallback; });
}

void SendPropHookManager::UnhookEntityAll(int entity)
{
	RemoveEntity(entity, [&](const SendPropHook &hook)
				 { return true; });
}

void SendPropHookManager::OnPluginUnloaded(IPlugin *plugin)
{
	for (auto it = m_entityInfos.begin(); it != m_entityInfos.end();)
	{
		it = RemoveEntityAt(it, [pOwner = plugin->GetRuntime()](const SendPropHook &hook)
							{ return hook.pOwner == pOwner; });
	}
}

void SendPropHookManager::OnExtentionUnloaded(IExtension *ext)
{
	for (auto it = m_entityInfos.begin(); it != m_entityInfos.end();)
	{
		it = RemoveEntityAt(it, [pOwner = ext](const SendPropHook &hook)
							{ return hook.pOwner == pOwner; });
	}
}

std::shared_ptr<SendPropEntityInfo>
SendPropHookManager::GetEntityHooks(int entity) noexcept
{
	if (m_entityInfos.find(entity)!= m_entityInfos.end())
		return m_entityInfos.at(entity);

	return nullptr;
}

std::shared_ptr<SendProxyHook>
SendPropHookManager::GetPropHook(const SendProp *pProp) noexcept
{
	if (m_propHooks.find(pProp) != m_propHooks.end())
		return m_propHooks.at(pProp).lock();

	return nullptr;
}

bool SendPropHookManager::IsPropHooked(const SendProp *pProp) const
{
	const auto it = m_propHooks.find(pProp);
	return it != m_propHooks.end() && !it->second.expired();
}

bool SendPropHookManager::IsEntityHooked(int entity) const
{
	const auto it = m_entityInfos.find(entity);
	return it != m_entityInfos.end();
}

bool SendPropHookManager::IsEntityHooked(int entity, const SendProp *pProp, int element, const IPluginFunction *pFunc) const
{
	const auto it = m_entityInfos.find(entity);
	if (it == m_entityInfos.end())
		return false;

	return std::any_of(it->second->list.cbegin(), it->second->list.cend(),
		[&](const SendPropHook &hook)
		{
			return hook.proxy->GetProp() == pProp
				&& hook.pCallback == (void *)pFunc
				&& ((hook.proxy->GetProp()->GetType() != DPT_Array && hook.proxy->GetProp()->GetType() != DPT_DataTable)
				 || hook.element == element);
		}
	);
}

bool SendPropHookManager::IsAnyEntityHooked() const
{
	return m_entityInfos.size() > 0;
}

void SendPropHookManager::OnEntityEnterHook(int entity)
{
	ClientPacksDetour::OnEntityHooked(entity);
}

void SendPropHookManager::OnEntityLeaveHook(int entity)
{
	ClientPacksDetour::OnEntityUnhooked(entity);
}

class TailInvoker
{
public:
	explicit TailInvoker(std::function<void()> fn) : m_call(fn) {}
	TailInvoker() = delete;
	TailInvoker(const TailInvoker &other) = delete;
	~TailInvoker() { m_call(); }

private:
	std::function<void()> m_call;
};

// !! MUST BE CALLED IN MAIN THREAD
void GlobalProxy(const SendProp *pProp, const void *pStructBase, const void * pData, DVariant *pOut, int iElement, int objectID)
{
	auto pHook = g_pSendPropHookManager->GetPropHook(pProp);
	Assert(pHook != nullptr);
	if (!pHook)
	{
		LogError("FATAL: Leftover entity proxy %s", pProp->GetName());
		return;
	}

	ProxyVariant *pOverride = nullptr;
	TailInvoker finally(
		[&, hook = std::move(pHook)]() -> void
		{
			if (pOverride) {
				const void *pNewData = nullptr;
				std::visit(overloaded {
					[&pNewData](const auto &arg) { pNewData = &arg; },
					[&pNewData](const CBaseHandle &arg) { pNewData = &arg; },
					[&pNewData](const std::string &arg) { pNewData = arg.c_str(); },
				}, *pOverride);

				hook->CallOriginal(pStructBase, pNewData, pOut, iElement, objectID);
			} else {
				hook->CallOriginal(pStructBase, pData, pOut, iElement, objectID);
			}
		}
	);

	std::shared_ptr<SendPropEntityInfo> pEntHook = g_pSendPropHookManager->GetEntityHooks(objectID);
	if (!pEntHook)
		return;

	int client = ClientPacksDetour::GetCurrentClientIndex();
	if (client == -1)
		return;

	for (const SendPropHook& hook : pEntHook->list)
	{
		if (hook.proxy->GetProp() != pProp)
			continue;
		
		if (pProp->IsInsideArray() && hook.element != iElement)
			continue;
		
		if (hook.type == PropType::Prop_Int) {
			pEntHook->data = *reinterpret_cast<const int *>(pData);
		} else if (hook.type == PropType::Prop_Float) {
			pEntHook->data = *reinterpret_cast<const float *>(pData);
		} else if (hook.type == PropType::Prop_Vector) {
			pEntHook->data = *reinterpret_cast<const Vector *>(pData);
		} else if (hook.type == PropType::Prop_String) {
			pEntHook->data.emplace<std::string>(reinterpret_cast<const char *>(pData), DT_MAX_STRING_BUFFERSIZE);
		} else if (hook.type == PropType::Prop_EHandle) {
			pEntHook->data = *reinterpret_cast<const CBaseHandle *>(pData);
		} else {
			LogError("%s: SendProxy report: Unknown prop type (%s).", __func__, pProp->GetName());
			continue;
		}

		if (hook.fnProcess(hook.pCallback, pProp, pEntHook->data, hook.element, objectID, client))
		{
			gamehelpers->EdictOfIndex(objectID)->m_fStateFlags |= FL_EDICT_CHANGED;
			pOverride = &pEntHook->data;
			return;
		}
	}
}