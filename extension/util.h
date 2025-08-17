#ifndef _SENDPROXY_UTIL_H
#define _SENDPROXY_UTIL_H

#include "extension.h"

#define GET_CONVAR(name) \
	name = g_pCVar->FindVar(#name); \
	if (name == nullptr) { \
		if (error != nullptr && maxlen != 0) { \
			ismm->Format(error, maxlen, "Could not find ConVar: " #name); \
		} \
		return false; \
	}

edict_t *UTIL_EdictOfIndex(int index);
bool IsPropValid(const SendProp *prop, PropType type);

class ConVarSaveSet
{
public:
	explicit ConVarSaveSet(ConVar *cvar, const char *value)
		: m_cvar(cvar), m_savevalue(m_cvar->GetString())
	{
		m_cvar->SetValue(value);
	}

	ConVarSaveSet() = delete;
	ConVarSaveSet(const ConVarSaveSet &other) = delete;

	~ConVarSaveSet()
	{
		m_cvar->SetValue(m_savevalue.data());
	}

private:
	ConVar *m_cvar;
	std::string m_savevalue;
};

inline edict_t *UTIL_EdictOfIndex(int index)
{
	if (index < 0 || index >= MAX_EDICTS)
		return nullptr;

	return gamehelpers->EdictOfIndex(index);
}

inline bool IsPropValid(const SendProp *prop, PropType type)
{
	switch (type)
	{
	case PropType::Prop_Int:
		return prop->GetType() == DPT_Int;

	case PropType::Prop_EHandle:
		return prop->GetType() == DPT_Int && prop->m_nBits == NUM_NETWORKED_EHANDLE_BITS;

	case PropType::Prop_Float:
		return prop->GetType() == DPT_Float;

	case PropType::Prop_Vector:
		return prop->GetType() == DPT_Vector || prop->GetType() == DPT_VectorXY;

	case PropType::Prop_String:
		return prop->GetType() == DPT_String;
	}

	return false;
}

#endif