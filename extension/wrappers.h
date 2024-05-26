#ifndef _WRAPPERS_H_INC_
#define _WRAPPERS_H_INC_

#include "tier0/threadtools.h"
#include "tier1/utlvector.h"
#include "tier1/utllinkedlist.h"
#include "tier1/mempool.h"

class CFrameSnapshot;
class PackedEntity;
#define INVALID_PACKED_ENTITY_HANDLE (0)
typedef int PackedEntityHandle_t;

typedef struct
{
	PackedEntity	*pEntity;	// original packed entity
	int				counter;	// increaseing counter to find LRU entries
	int				bits;		// uncompressed data length in bits
	char			data[MAX_PACKEDENTITY_DATA]; // uncompressed data cache
} UnpackedDataCache_t;

class CFrameSnapshotManager
{
public:
	virtual ~CFrameSnapshotManager( void );

public:
	int pad[21];

	CUtlFixedLinkedList<PackedEntity *>					m_PackedEntities;

	// FIXME: Update CUtlFixedLinkedList in hl2sdk-l4d2
	int pad2;

	int								m_nPackedEntityCacheCounter;  // increase with every cache access
	CUtlVector<UnpackedDataCache_t>	m_PackedEntityCache;	// cache for uncompressed packed entities

	// The most recently sent packets for each entity
	PackedEntityHandle_t	m_pLastPackedData[ MAX_EDICTS ];
	int						m_pSerialNumber[ MAX_EDICTS ];

	CThreadFastMutex		m_WriteMutex;

	CUtlVector<int>			m_iExplicitDeleteSlots;
};

#endif