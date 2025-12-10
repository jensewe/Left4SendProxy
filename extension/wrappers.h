#ifndef _WRAPPERS_H_INC_
#define _WRAPPERS_H_INC_

#include <extensions/IBinTools.h>
#include "tier1/utlvector.h"
#include "tier1/utllinkedlist.h"
#include "tier1/mempool.h"
#include "iclient.h"
#include <type_traits>

#if defined( _LINUX ) || defined( _OSX )
// linux implementation
namespace Wrappers {

inline int32 ThreadInterlockedIncrement( int32 volatile *p )
{
	Assert( (size_t)p % 4 == 0 );
	return __sync_fetch_and_add( p, 1 ) + 1;
}

inline int32 ThreadInterlockedDecrement( int32 volatile *p )
{
	Assert( (size_t)p % 4 == 0 ); 
	return __sync_fetch_and_add( p, -1 ) - 1;
}

inline int32 ThreadInterlockedExchange( int32 volatile *p, int32 value )
{
	Assert( (size_t)p % 4 == 0 );
	int32 nRet;

	// Note: The LOCK instruction prefix is assumed on the XCHG instruction and GCC gets very confused on the Mac when we use it.
	__asm __volatile(
		"xchgl %2,(%1)"
		: "=r" (nRet)
		: "r" (p), "0" (value)
		: "memory");
	return nRet;
}

inline int32 ThreadInterlockedExchangeAdd( int32 volatile *p, int32 value )
{
	Assert( (size_t)p % 4 == 0 ); 
	return __sync_fetch_and_add( p, value );
}
inline int32 ThreadInterlockedCompareExchange( int32 volatile *p, int32 value, int32 comperand )
{
	Assert( (size_t)p % 4 == 0 ); 
	return __sync_val_compare_and_swap( p, comperand, value );
}

inline bool ThreadInterlockedAssignIf( int32 volatile *p, int32 value, int32 comperand )
{
	Assert( (size_t)p % 4 == 0 );
	return __sync_bool_compare_and_swap( p, comperand, value );
}

}

template <typename T>
class CInterlockedIntTHack
{
public:
	CInterlockedIntTHack() : m_value( 0 ) 				{ COMPILE_TIME_ASSERT( sizeof(T) == sizeof(int32) ); }
	CInterlockedIntTHack( T value ) : m_value( value ) 	{}

	operator T() const				{ return m_value; }

	bool operator!() const			{ return ( m_value == 0 ); }
	bool operator==( T rhs ) const	{ return ( m_value == rhs ); }
	bool operator!=( T rhs ) const	{ return ( m_value != rhs ); }

	T operator++()					{ return (T)Wrappers::ThreadInterlockedIncrement( (int32 *)&m_value ); }
	T operator++(int)				{ return operator++() - 1; }

	T operator--()					{ return (T)Wrappers::ThreadInterlockedDecrement( (int32 *)&m_value ); }
	T operator--(int)				{ return operator--() + 1; }

	bool AssignIf( T conditionValue, T newValue )	{ return Wrappers::ThreadInterlockedAssignIf( (int32 *)&m_value, (int32)newValue, (int32)conditionValue ); }

	T operator=( T newValue )		{ Wrappers::ThreadInterlockedExchange((int32 *)&m_value, newValue); return m_value; }

	void operator+=( T add )		{ Wrappers::ThreadInterlockedExchangeAdd( (int32 *)&m_value, (int32)add ); }
	void operator-=( T subtract )	{ operator+=( -subtract ); }
	void operator*=( T multiplier )	{ 
		T original, result; 
		do 
		{ 
			original = m_value; 
			result = original * multiplier; 
		} while ( !AssignIf( original, result ) );
	}
	void operator/=( T divisor )	{ 
		T original, result; 
		do 
		{ 
			original = m_value; 
			result = original / divisor;
		} while ( !AssignIf( original, result ) );
	}

	T operator+( T rhs ) const		{ return m_value + rhs; }
	T operator-( T rhs ) const		{ return m_value - rhs; }

private:
	volatile T m_value;
};
#endif

class ServerClass;
class ClientClass;
class CEventInfo;
class CChangeFrameList;

#define INVALID_PACKED_ENTITY_HANDLE (0)
typedef int PackedEntityHandle_t;

struct UnpackedDataCache_t;

//-----------------------------------------------------------------------------
// Purpose: Individual entity data, did the entity exist and what was it's serial number
//-----------------------------------------------------------------------------
class CFrameSnapshotEntry
{
public:
	ServerClass*			m_pClass;
	int						m_nSerialNumber;
	// Keeps track of the fullpack info for this frame for all entities in any pvs:
	PackedEntityHandle_t	m_pPackedData;
};

// HLTV needs some more data per entity 
class CHLTVEntityData
{
public:
	vec_t			origin[3];	// entity position
	unsigned int	m_nNodeCluster;  // if (1<<31) is set it's a node, otherwise a cluster
};

class CFrameSnapshot
{
public:
	static void *s_pfnReleaseReference;
	static ICallWrapper *s_callReleaseReference;
	inline void ReleaseReference()
	{
		struct {
			CFrameSnapshot *pThis;
		} stack{ this };

		s_callReleaseReference->Execute(&stack, NULL);
	}

	// Index info CFrameSnapshotManager::m_FrameSnapshots.
#if defined( _LINUX ) || defined( _OSX )
	CInterlockedIntTHack<int>	m_ListIndex;
#else
	CInterlockedInt			m_ListIndex;	
#endif

	// Associated frame. 
	int						m_nTickCount; // = sv.tickcount
	
	// State information
	CFrameSnapshotEntry		*m_pEntities;	
	int						m_nNumEntities; // = sv.num_edicts

	// This list holds the entities that are in use and that also aren't entities for inactive clients.
	unsigned short			*m_pValidEntities; 
	int						m_nValidEntities;

	// Additional HLTV info
	CHLTVEntityData			*m_pHLTVEntityData; // is NULL if not in HLTV mode or array of m_pValidEntities entries

	CEventInfo				**m_pTempEntities; // temp entities
	int						m_nTempEntities;

	CUtlVector<int>			m_iExplicitDeleteSlots;

	// Snapshots auto-delete themselves when their refcount goes to zero.
#if defined( _LINUX ) || defined( _OSX )
	CInterlockedIntTHack<int> m_nReferences;
#else
	CInterlockedInt			m_nReferences;
#endif
};

class PackedEntity
{
public:
	int					GetSnapshotCreationTick() const;
	
	ServerClass *m_pServerClass;	// Valid on the server
	ClientClass	*m_pClientClass;	// Valid on the client
		
	int			m_nEntityIndex;		// Entity index.
#if defined( _LINUX ) || defined( _OSX )
	CInterlockedIntTHack<int> m_ReferenceCount;
#else
	CInterlockedInt 	m_ReferenceCount;	// reference count;
#endif

	CUtlVector<CSendProxyRecipients>	m_Recipients;

	void				*m_pData;				// Packed data.
	int					m_nBits;				// Number of bits used to encode.
	CChangeFrameList	*m_pChangeFrameList;	// Only the most current 

	// This is the tick this PackedEntity was created on
	unsigned int		m_nSnapshotCreationTick : 31;
	unsigned int		m_nShouldCheckCreationTick : 1;
};

inline int PackedEntity::GetSnapshotCreationTick() const
{
	return (int)m_nSnapshotCreationTick;
}

class CFrameSnapshotManager
{
public:
	virtual ~CFrameSnapshotManager( void );

	static void* s_pfnCreateEmptySnapshot;
	static ICallWrapper* s_callCreateEmptySnapshot;
	inline CFrameSnapshot* CreateEmptySnapshot(int tickcount, int maxEntities)
	{
		struct {
			CFrameSnapshotManager *pThis;
			int tickcount;
			int maxEntities;
		} stack{ this, tickcount, maxEntities };

		CFrameSnapshot *ret;
		s_callCreateEmptySnapshot->Execute(&stack, &ret);
		return ret;
	}

	inline void AddEntityReference( PackedEntityHandle_t handle )
	{
		m_PackedEntities[ handle ]->m_ReferenceCount++;
	}

	static void* s_pfnRemoveEntityReference;
	static ICallWrapper* s_callRemoveEntityReference;
	inline void RemoveEntityReference( PackedEntityHandle_t handle )
	{
		struct {
			CFrameSnapshotManager *pThis;
			PackedEntityHandle_t handle;
		} stack{ this, handle };

		s_callRemoveEntityReference->Execute(&stack, nullptr);
	}

public:
	uint32_t pad[21];

	template <typename T>
	class CUtlFixedLinkedListHack : public CUtlFixedLinkedList<T>
	{
	private:
		int	m_NumAlloced;		// The number of allocated elements
	};

	std::conditional_t<sizeof(CUtlFixedLinkedList<PackedEntity *>) == 40u,
		CUtlFixedLinkedListHack<PackedEntity *>,
		CUtlFixedLinkedList<PackedEntity *>
		> m_PackedEntities;

	int								m_nPackedEntityCacheCounter;  // increase with every cache access
	CUtlVector<UnpackedDataCache_t>	m_PackedEntityCache;	// cache for uncompressed packed entities

	// The most recently sent packets for each entity
	PackedEntityHandle_t	m_pLastPackedData[ MAX_EDICTS ];
	int						m_pSerialNumber[ MAX_EDICTS ];

	CThreadFastMutex		m_WriteMutex;

	CUtlVector<int>			m_iExplicitDeleteSlots;
};

class CCheckTransmitInfo;
class CGameClient
{
public:
	int GetPlayerSlot()
	{
		IClient *pClient = reinterpret_cast<IClient *>((uint8_t*)this + 4);
		return pClient->GetPlayerSlot();
	}
};

extern CFrameSnapshotManager* framesnapshotmanager;

#if SOURCE_ENGINE == SE_LEFT4DEAD || SOURCE_ENGINE == SE_LEFT4DEAD2
constexpr int MAXPLAYERS = 32;
#else
constexpr int MAXPLAYERS = SM_MAXPLAYERS;
#endif

#endif