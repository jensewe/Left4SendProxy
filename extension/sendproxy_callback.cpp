#include "sendproxy_callback.h"
#include "dt_send.h"
#include <string>

bool SendProxyPluginCallback(void *callback, const SendProp *pProp, ProxyVariant &variant, int element, int entity, int client)
{
	auto func = static_cast<IPluginFunction *>(callback);

	cell_t result = Pl_Continue;

	if (gamehelpers->ReferenceToEntity(entity) != GetGameRulesProxyEnt())
		func->PushCell(entity);

	func->PushString(pProp->GetName());

	ProxyVariant temp = variant;
	cell_t iEntity = -1;
	
	std::visit(overloaded {
		[func](int &arg)			{ func->PushCellByRef(reinterpret_cast<cell_t*>(&arg)); },
		[func](float &arg)			{ func->PushFloatByRef(&arg); },
		[func](std::string& arg)	{ func->PushStringEx(arg.data(), arg.capacity(), SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK); func->PushCell(arg.capacity()); },
		[func](Vector &arg)			{ func->PushArray(reinterpret_cast<cell_t*>(&arg), 3, SM_PARAM_COPYBACK); },
		[func, &iEntity](CBaseHandle &arg) {
			if (edict_t *edict = gamehelpers->GetHandleEntity(arg))
				iEntity = gamehelpers->IndexOfEdict(edict);
			func->PushCellByRef(&iEntity);
		},
	}, temp);

	func->PushCell(element);
	func->PushCell(client);
	func->Execute(&result);

	if (result == Pl_Changed)
	{
		bool intercept = true;

		std::visit(overloaded {
			[&variant](auto &arg) {
				variant = arg;
			},

			[func, iEntity, &intercept](CBaseHandle &arg) {
				if (edict_t *edict = gamehelpers->EdictOfIndex(iEntity)) {
					gamehelpers->SetHandleEntity(arg, edict);
				} else {
					func->GetParentRuntime()->GetDefaultContext()->BlamePluginError(
						func, "Unexpected invalid edict index (%d)", iEntity);

					intercept = false;
				}
			},
		}, temp);

		return intercept;
	}
	
	return false;
}
