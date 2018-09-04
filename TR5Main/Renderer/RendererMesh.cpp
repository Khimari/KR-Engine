#include "RendererMesh.h"
#include "Enums.h"

#include <stdio.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <DxErr.h>
#include <vector>

struct RendererVertex;
struct RendererPolygon;

RendererMesh::RendererMesh(LPDIRECT3DDEVICE9 device)
{
	m_device = device;

	for (__int32 i = 0; i < NUM_BUCKETS; i++)
		m_buckets[i] = new RendererBucket(device);
	 
	for (__int32 i = 0; i < NUM_BUCKETS; i++)
		m_animatedBuckets[i] = new RendererBucket(device);
}

RendererMesh::~RendererMesh()
{
	for (__int32 i = 0; i < NUM_BUCKETS; i++)
		delete m_buckets[i];

	for (__int32 i = 0; i < NUM_BUCKETS; i++)
		delete m_animatedBuckets[i];
}

RendererBucket* RendererMesh::GetBucket(__int32 bucketIndex)
{
	return m_buckets[bucketIndex];
}

RendererBucket* RendererMesh::GetAnimatedBucket(__int32 bucketIndex)
{
	return m_animatedBuckets[bucketIndex];
}