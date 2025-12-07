#pragma once

#define GET_CONVAR(name) \
	name = g_pCVar->FindVar(#name); \
	if (name == nullptr) { \
		if (error != nullptr && maxlen != 0) { \
			ismm->Format(error, maxlen, "Could not find ConVar: " #name); \
		} \
		return false; \
	}

class ConVarScopedSet
{
public:
	explicit ConVarScopedSet(ConVar *cvar, const char *value)
		: m_cvar(cvar), m_savevalue(m_cvar->GetString())
	{
		m_cvar->SetValue(value);
	}

	ConVarScopedSet() = delete;
	ConVarScopedSet(const ConVarScopedSet &other) = delete;

	~ConVarScopedSet()
	{
		m_cvar->SetValue(m_savevalue.data());
	}

private:
	ConVar *m_cvar;
	std::string m_savevalue;
};

class AutoGameConfig
{
public:
	static std::optional<AutoGameConfig> Load(const char *name)
	{
		char buffer[256];
		IGameConfig *gc;
		if (!gameconfs->LoadGameConfigFile(name, &gc, buffer, sizeof(buffer)))
		{
			smutils->LogError(myself, "Could not read config file (%s) (%s)", name, buffer);
			return {};
		}
		return AutoGameConfig(gc);
	}

protected:
	AutoGameConfig(IGameConfig *gc) : gc_(gc) {}

public:
	AutoGameConfig() : gc_(nullptr) {}

	AutoGameConfig(AutoGameConfig &&other) noexcept
	{
		gc_ = other.gc_;
		other.gc_ = nullptr;
	}
	AutoGameConfig &operator=(AutoGameConfig &&other)
	{
		gc_ = other.gc_;
		other.gc_ = nullptr;
		return *this;
	}

	AutoGameConfig(const AutoGameConfig &other) = delete;
	AutoGameConfig &operator=(const AutoGameConfig &other) = delete;

	~AutoGameConfig()
	{
		Destroy();
	}

	void Destroy() noexcept
	{
		if (gc_ != nullptr)
		{
			gameconfs->CloseGameConfigFile(gc_);
			gc_ = nullptr;
		}
	}

	operator IGameConfig *() const noexcept
	{
		return gc_;
	}

	IGameConfig *operator->() const noexcept
	{
		return gc_;
	}

private:
	IGameConfig *gc_;
};

#define GAMECONF_GETADDRESS(conf, key, var)                                      \
	do                                                                           \
	{                                                                            \
		if (!conf->GetAddress(key, (void **)var))                                \
		{                                                                        \
			ke::SafeStrcpy(error, maxlen, "Unable to find address (" key ")\n"); \
			return false;                                                        \
		}                                                                        \
	} while (false)

#define GAMECONF_GETOFFSET(conf, key, var)                                      \
	do                                                                          \
	{                                                                           \
		if (!conf->GetOffset(key, (void **)var))                                \
		{                                                                       \
			ke::SafeStrcpy(error, maxlen, "Unable to find offset (" key ")\n"); \
			return false;                                                       \
		}                                                                       \
	} while (false)

#define GAMECONF_GETSIGNATURE(conf, key, var)                                      \
	do                                                                             \
	{                                                                              \
		if (!conf->GetMemSig(key, (void **)var))                                   \
		{                                                                          \
			ke::SafeStrcpy(error, maxlen, "Unable to find signature (" key ")\n"); \
			return false;                                                          \
		}                                                                          \
	} while (false)

inline edict_t *UTIL_EdictOfIndex(int index)
{
	if (index < 0 || index >= MAX_EDICTS)
		return nullptr;

	return gamehelpers->EdictOfIndex(index);
}

extern CGlobalVars *gpGlobals;
inline CBaseEntity *FindEntityByNetClass(int start, const char *classname)
{
	if (gpGlobals == nullptr)
		return nullptr;

	int maxEntities = gpGlobals->maxEntities;
	for (int i = start; i < maxEntities; i++)
	{
		CBaseEntity *pEntity = gamehelpers->ReferenceToEntity(i);
		if (pEntity == nullptr)
		{
			continue;
		}

		IServerNetworkable* pNetwork = ((IServerUnknown *)pEntity)->GetNetworkable();
		if (pNetwork == nullptr)
		{
			continue;
		}

		ServerClass *pServerClass = pNetwork->GetServerClass();
		if (pServerClass == nullptr)
		{
			continue;
		}

		const char *name = pServerClass->GetName();
		if (!strcmp(name, classname))
		{
			return pEntity;
		}
	}

	return nullptr;
}
