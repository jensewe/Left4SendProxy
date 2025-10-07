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
	m_propHooks.init();
	m_entityInfos.init();
}

void SendPropHookManager::Clear()
{
	m_propHooks.clear();
	m_entityInfos.clear();
	ClientPacksDetour::Clear();
}

void SendPropHookManager::RemoveHook(const SendProp *pProp)
{
	m_propHooks.removeIfExists(pProp);
}

void SendPropHookManager::RemoveEntity(int entity, std::function<bool(const SendPropHook &)> pred)
{
	auto r = m_entityInfos.find(entity);
	if (!r.found())
		return;

	r->value.list.remove_if(pred);

	if (r->value.list.empty())
	{
		OnEntityLeaveHook(entity);
		m_entityInfos.remove(r);
	}
}

void SendPropHookManager::RemoveEntity(SendPropEntityInfoMap::iterator it, std::function<bool(const SendPropHook &)> pred)
{
	it->value.list.remove_if(pred);

	if (it->value.list.empty())
	{
		OnEntityLeaveHook(it->key);
		it.erase();
	}
}

void SendPropHookManager::OnEntityEnterHook(int entity)
{
	ClientPacksDetour::OnEntityHooked(entity);
}

void SendPropHookManager::OnEntityLeaveHook(int entity)
{
	ClientPacksDetour::OnEntityUnhooked(entity);
}

bool SendPropHookManager::HookEntity(int entity, SendProp *pProp, int element, PropType type, IPluginFunction *pFunc)
{
	SendPropHook hook;
	hook.element = element;
	hook.type = type;
	hook.fnProcess = SendProxyPluginCallback;
	hook.pCallback = pFunc;
	hook.pOwner = pFunc->GetParentRuntime();

	auto i = m_propHooks.findForAdd(pProp);
	AssertFatal(!i.found() || !i->value.expired());

	if (i.found())
	{
		hook.proxy = i->value.lock();
	}

	if (!hook.proxy)
	{
		hook.proxy = std::make_shared<SendProxyHook>(pProp, GlobalProxy);
		m_propHooks.add(i, pProp, hook.proxy);
	}

	auto ii = m_entityInfos.findForAdd(entity);
	if (!ii.found())
	{
		m_entityInfos.add(ii, entity);
		OnEntityEnterHook(entity);
	}
	ii->value.list.emplace_front(std::move(hook));

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
	for (auto it = m_entityInfos.iter(); !it.empty(); it.next())
	{
		RemoveEntity(it, [pOwner = plugin->GetRuntime()](const SendPropHook &hook)
					 { return hook.pOwner == pOwner; });
	}
}

void SendPropHookManager::OnExtentionUnloaded(IExtension *ext)
{
	for (auto it = m_entityInfos.iter(); !it.empty(); it.next())
	{
		RemoveEntity(it, [pOwner = ext](const SendPropHook &hook)
					 { return hook.pOwner == pOwner; });
	}
}

SendPropEntityInfo* SendPropHookManager::GetEntityHooks(int entity)
{
	auto r = m_entityInfos.find(entity);
	if (!r.found())
		return nullptr;

	return &r->value;
}

std::shared_ptr<SendProxyHook> SendPropHookManager::GetPropHook(const SendProp *pProp)
{
	auto r = m_propHooks.find(pProp);
	if (!r.found() || r->value.expired())
		return nullptr;

	return r->value.lock();
}

bool SendPropHookManager::IsPropHooked(const SendProp *pProp) const
{
	auto r = m_propHooks.find(pProp);
	return r.found() && !r->value.expired();
}

bool SendPropHookManager::IsEntityHooked(int entity) const
{
	auto r = m_entityInfos.find(entity);
	return r.found();
}

bool SendPropHookManager::IsEntityHooked(int entity, const SendProp *pProp, int element, const IPluginFunction *pFunc) const
{
	auto r = m_entityInfos.find(entity);
	if (!r.found())
		return false;

	return std::any_of(r->value.list.cbegin(), r->value.list.cend(), [&](const SendPropHook &hook) {
		return hook.proxy->GetProp() == pProp
			&& hook.pCallback == (void *)pFunc
			&& ((hook.proxy->GetProp()->GetType() != DPT_Array && hook.proxy->GetProp()->GetType() != DPT_DataTable)
			 || hook.element == element);
	});
}

bool SendPropHookManager::IsAnyEntityHooked() const
{
	return m_entityInfos.elements() > 0;
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
		g_pSM->LogError(myself, "FATAL: Leftover entity proxy %s", pProp->GetName());
		return;
	}

	ProxyVariant *pOverride = nullptr;
	TailInvoker finally(
		[&]() -> void
		{
			if (pOverride) {
				const void *pNewData = nullptr;
				std::visit(overloaded {
					[&pNewData](const auto &arg) { pNewData = &arg; },
					[&pNewData](const CBaseHandle &arg) { pNewData = &arg; },
					[&pNewData](const std::string &arg) { pNewData = arg.c_str(); },
				}, *pOverride);

				pHook->CallOriginal(pStructBase, pNewData, pOut, iElement, objectID);
			} else {
				pHook->CallOriginal(pStructBase, pData, pOut, iElement, objectID);
			}
		}
	);

	SendPropEntityInfo *pEntHook = g_pSendPropHookManager->GetEntityHooks(objectID);
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
			g_pSM->LogError(myself, "%s: SendProxy report: Unknown prop type (%s).", __func__, pProp->GetName());
			continue;
		}

		if (hook.fnProcess(hook.pCallback, pProp, pEntHook->data, hook.element, objectID, client))
		{
			pOverride = &pEntHook->data;
			return;
		}
	}
}