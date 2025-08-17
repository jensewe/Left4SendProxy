#include "util.h"

edict_t *UTIL_EdictOfIndex(int index)
{
	if (index < 0 || index >= MAX_EDICTS)
		return nullptr;

	return gamehelpers->EdictOfIndex(index);
}

bool IsPropValid(const SendProp *prop, PropType type)
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