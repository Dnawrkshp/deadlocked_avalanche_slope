#include <tamtypes.h>
#include "moby_bounds.h"

#define MOBY_BOUNDS_MIN_EXTENT      (0.05f)
#define MOBY_BOUNDS_FALLBACK_EXTENT (0.5f)

struct MobyBoundsMeshInfo
{
	u8 HighLodCount;
	u8 LowLodCount;
	u8 MetalCount;
	u8 MetalBegin;
} __attribute__((packed));

struct MobyBoundsClassHeader
{
	/* 0x00 */ u32 PacketTableOffset;
	/* 0x04 */ struct MobyBoundsMeshInfo MeshInfo;
	/* 0x08 */ u8 Pad08[0x1c];
	/* 0x24 */ float Scale;
} __attribute__((packed));

struct MobyBoundsPacketEntry
{
	/* 0x00 */ u32 VifListOffset;
	/* 0x04 */ u16 VifListSize;
	/* 0x06 */ u16 VifListTextureUnpackOffset;
	/* 0x08 */ u32 VertexOffset;
	/* 0x0c */ u8 VertexDataSize;
	/* 0x0d */ u8 UnknownD;
	/* 0x0e */ u8 UnknownE;
	/* 0x0f */ u8 TransferVertexCount;
} __attribute__((packed));

struct MobyBoundsVertexHeader
{
	/* 0x00 */ u16 MatrixTransferCount;
	/* 0x02 */ u16 TwoWayBlendVertexCount;
	/* 0x04 */ u16 ThreeWayBlendVertexCount;
	/* 0x06 */ u16 MainVertexCount;
	/* 0x08 */ u16 DuplicateVertexCount;
	/* 0x0a */ u16 TransferVertexCount;
	/* 0x0c */ u16 VertexTableOffset;
	/* 0x0e */ u16 UnknownE;
} __attribute__((packed));

struct MobyBoundsVertex
{
	/* 0x00 */ u8 Pad00[0x0a];
	/* 0x0a */ s16 X;
	/* 0x0c */ s16 Y;
	/* 0x0e */ s16 Z;
} __attribute__((packed));

//--------------------------------------------------------------------------
int mobyBoundsFloatValid(float value)
{
	return value == value && value > -1000000.0f && value < 1000000.0f;
}

//--------------------------------------------------------------------------
void mobyBoundsClampTinyExtents(VECTOR halfExtents)
{
	int i;

	for (i = 0; i < 3; ++i)
	{
		if (!mobyBoundsFloatValid(halfExtents[i]))
			halfExtents[i] = MOBY_BOUNDS_FALLBACK_EXTENT;
		else if (halfExtents[i] < MOBY_BOUNDS_MIN_EXTENT)
			halfExtents[i] = MOBY_BOUNDS_MIN_EXTENT;
	}

	halfExtents[3] = 0.0f;
}

//--------------------------------------------------------------------------
int mobyBoundsFromPClass(void *pClass, float scale, VECTOR outOffset, VECTOR outHalfExtents)
{
	u8 *base = (u8 *)pClass;
	struct MobyBoundsClassHeader *header;
	struct MobyBoundsPacketEntry *packetTable;
	float vertexScale;
	float minX = 100000000.0f;
	float minY = 100000000.0f;
	float minZ = 100000000.0f;
	float maxX = -100000000.0f;
	float maxY = -100000000.0f;
	float maxZ = -100000000.0f;
	int vertexCount = 0;
	int packetIndex;

	if (!pClass || !outOffset || !outHalfExtents)
		return 0;

	header = (struct MobyBoundsClassHeader *)base;
	if (!header->PacketTableOffset || !header->MeshInfo.HighLodCount)
		return 0;

  // float scale = header->Scale;
	if (!mobyBoundsFloatValid(scale) || scale <= 0.0f)
		return 0;

	vertexScale = scale / 1024.0f;
	packetTable = (struct MobyBoundsPacketEntry *)header->PacketTableOffset;

	for (packetIndex = 0; packetIndex < header->MeshInfo.HighLodCount; ++packetIndex)
	{
		struct MobyBoundsPacketEntry *packet = &packetTable[packetIndex];
		struct MobyBoundsVertexHeader *vertexHeader;
		struct MobyBoundsVertex *vertices;
		int storedVertexCount;
		int maxVertexCount;
		int i;

		if (!packet->VertexOffset || !packet->VertexDataSize)
			continue;

		vertexHeader = (struct MobyBoundsVertexHeader *)packet->VertexOffset;
		if (vertexHeader->VertexTableOffset < sizeof(struct MobyBoundsVertexHeader))
			continue;

		storedVertexCount = vertexHeader->TwoWayBlendVertexCount
			+ vertexHeader->ThreeWayBlendVertexCount
			+ vertexHeader->MainVertexCount;
		if (storedVertexCount <= 0)
			continue;

		maxVertexCount = ((int)packet->VertexDataSize * 0x10 - (int)vertexHeader->VertexTableOffset) / (int)sizeof(struct MobyBoundsVertex);
		if (maxVertexCount <= 0)
			continue;
		if (storedVertexCount > maxVertexCount)
			storedVertexCount = maxVertexCount;

		vertices = (struct MobyBoundsVertex *)((u8 *)vertexHeader + vertexHeader->VertexTableOffset);
		for (i = 0; i < storedVertexCount; ++i)
		{
			float x = vertices[i].X * vertexScale;
			float y = vertices[i].Y * vertexScale;
			float z = vertices[i].Z * vertexScale;

			if (!mobyBoundsFloatValid(x) || !mobyBoundsFloatValid(y) || !mobyBoundsFloatValid(z))
				continue;

			if (x < minX) minX = x;
			if (y < minY) minY = y;
			if (z < minZ) minZ = z;
			if (x > maxX) maxX = x;
			if (y > maxY) maxY = y;
			if (z > maxZ) maxZ = z;
			++vertexCount;
		}
	}

	if (vertexCount <= 0)
		return 0;

	outOffset[0] = (minX + maxX) * 0.5f;
	outOffset[1] = (minY + maxY) * 0.5f;
	outOffset[2] = (minZ + maxZ) * 0.5f;
	outOffset[3] = 0.0f;

	outHalfExtents[0] = (maxX - minX) * 0.5f;
	outHalfExtents[1] = (maxY - minY) * 0.5f;
	outHalfExtents[2] = (maxZ - minZ) * 0.5f;
	outHalfExtents[3] = 0.0f;

	mobyBoundsClampTinyExtents(outHalfExtents);
	return 1;
}
