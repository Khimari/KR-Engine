#include "framework.h"
#include "Specific/level.h"
#include "Game/control/control.h"
#include "Game/control/box.h"
#include "Game/items.h"
#include "Game/control/lot.h"
#include "Specific/input.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara.h"
#include "Specific/trmath.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/sphere.h"
#include "Objects/Generic/Object/rope.h"
#include "Sound/sound.h"
#include "Game/camera.h"

namespace TEN::Entities::Generic
{
	PENDULUM CurrentPendulum;
	PENDULUM AlternatePendulum;
	std::vector<ROPE_STRUCT> Ropes;
	int RopeSwing = 0;

	void InitialiseRope(short itemNumber)
	{
		PHD_VECTOR itemPos;

		ITEM_INFO* item = &g_Level.Items[itemNumber];
		short roomNumber = item->RoomNumber;

		itemPos.x = item->Position.xPos;
		itemPos.y = item->Position.yPos;
		itemPos.z = item->Position.zPos;

		FLOOR_INFO* floor = GetFloor(itemPos.x, itemPos.y, itemPos.z, &roomNumber);
		itemPos.y = GetCeiling(floor, itemPos.x, itemPos.y, itemPos.z);

		PHD_VECTOR pos;
		pos.x = 0;
		pos.y = 16384;
		pos.z = 0;

		ROPE_STRUCT rope;
		PrepareRope(&rope, &itemPos, &pos, 128, item);

		item->TriggerFlags = Ropes.size();

		Ropes.push_back(rope);
	}

	void PrepareRope(ROPE_STRUCT* rope, PHD_VECTOR* pos1, PHD_VECTOR* pos2, int length, ITEM_INFO* item)
	{
		rope->position = *pos1;
		rope->segmentLength = length << 16;

		pos2->x <<= 16;
		pos2->y <<= 16;
		pos2->z <<= 16;

		NormaliseRopeVector(pos2);

		if (item->TriggerFlags == -1)
			rope->coiled = 30;
		else
			rope->coiled = 0;

		int l = 0;
		int sum = 0;
		int il = 3145728;

		for (int i = 0; i < ROPE_SEGMENTS; ++i)
		{
			rope->segment[i].x = (int64_t)sum * pos2->x >> FP_SHIFT;
			rope->segment[i].y = (int64_t)sum * pos2->x >> FP_SHIFT;
			rope->segment[i].z = (int64_t)sum * pos2->z >> FP_SHIFT;

			rope->velocity[i].x = 0;
			rope->velocity[i].y = 0;
			rope->velocity[i].z = 0;

			if (item->TriggerFlags == -1)
			{
				rope->segment[i].x = l;
				rope->segment[i].y >>= 4;

				rope->velocity[i].x = 16384;
				rope->velocity[i].y = il;
				rope->velocity[i].z = 16384;
			}

			l += 1024;
			sum += rope->segmentLength;
			il -= 131072;
		}

		rope->active = 0;
	}

	PHD_VECTOR* NormaliseRopeVector(PHD_VECTOR* vec)
	{
		int x = vec->x >> FP_SHIFT;
		int y = vec->y >> FP_SHIFT;
		int z = vec->z >> FP_SHIFT;

		if (!x && !y && !z)
			return vec;

		int length = SQUARE(x) + SQUARE(y) + SQUARE(z);
		if (length < 0)
			length = -length;

		length = 65536 / sqrt(length);

		vec->x = ((int64_t)length * vec->x) >> FP_SHIFT;
		vec->y = ((int64_t)length * vec->y) >> FP_SHIFT;
		vec->z = ((int64_t)length * vec->z) >> FP_SHIFT;
		return vec;
	}

	void GetRopePos(ROPE_STRUCT* rope, int segmentFrame, int* x, int* y, int* z)
	{
		int segment;
		short frame;

		segment = segmentFrame / 128;
		frame = segmentFrame & 0x7F;

		*x = (rope->normalisedSegment[segment].x * frame >> FP_SHIFT) + (rope->meshSegment[segment].x >> FP_SHIFT) + rope->position.x;
		*y = (rope->normalisedSegment[segment].y * frame >> FP_SHIFT) + (rope->meshSegment[segment].y >> FP_SHIFT) + rope->position.y;
		*z = (rope->normalisedSegment[segment].z * frame >> FP_SHIFT) + (rope->meshSegment[segment].z >> FP_SHIFT) + rope->position.z;
	}

	int DotProduct(PHD_VECTOR* u, PHD_VECTOR* v)
	{
		return (u->x * v->x + u->y * v->y + u->z * v->z) >> W2V_SHIFT;
	}

	void ScaleVector(PHD_VECTOR* u, int c, PHD_VECTOR* destination)
	{
		destination->x = c * u->x >> W2V_SHIFT;
		destination->y = c * u->y >> W2V_SHIFT;
		destination->z = c * u->z >> W2V_SHIFT;
	}

	void CrossProduct(PHD_VECTOR* u, PHD_VECTOR* v, PHD_VECTOR* destination)
	{
		destination->x = (u->y * v->z - u->z * v->y) >> W2V_SHIFT;
		destination->y = (u->z * v->x - u->x * v->z) >> W2V_SHIFT;
		destination->z = (u->x * v->y - u->y * v->x) >> W2V_SHIFT;
	}

	void phd_GetMatrixAngles(int* matrix, short* angle)
	{
		angle[0] = phd_atan(sqrt(SQUARE(matrix[M22]) + SQUARE(matrix[M02])), matrix[M12]);
		if (matrix[M12] >= 0 && angle[0] > 0 || matrix[M12] < 0 && angle[0] < 0)
			angle[0] = -angle[0];
		angle[1] = phd_atan(matrix[M22], matrix[M02]);
		angle[2] = phd_atan(matrix[M00] * phd_cos(angle[1]) - matrix[M20] * phd_sin(angle[1]), matrix[M21] * phd_sin(angle[1]) - matrix[M01] * phd_cos(angle[1]));
	}

	void RopeControl(short itemNumber)
	{
		ITEM_INFO* item;
		ROPE_STRUCT* rope;

		item = &g_Level.Items[itemNumber];
		rope = &Ropes[item->TriggerFlags];
		if (TriggerActive(item))
		{
			rope->active = 1;
			RopeDynamics(rope);
		}
		else
		{
			rope->active = 0;
		}
	}

	void RopeCollision(short itemNumber, ITEM_INFO* l, COLL_INFO* coll)
	{
		ITEM_INFO* item;
		ROPE_STRUCT* rope;
		BOUNDING_BOX* frame;
		int segment;

		item = &g_Level.Items[itemNumber];
		rope = &Ropes[item->TriggerFlags];
		
		if (TrInput & IN_ACTION 
			&& Lara.gunStatus == LG_HANDS_FREE 
			&& (l->ActiveState == LS_REACH
				|| l->ActiveState == LS_JUMP_UP) 
			&& l->Airborne 
			&& l->VerticalVelocity > 0
			&& rope->active)
		{
			frame = GetBoundsAccurate(l);

			segment = RopeNodeCollision(
				rope,
				l->Position.xPos,
				l->Position.yPos + frame->Y1 + 512,
				l->Position.zPos + frame->Z2 * phd_cos(l->Position.yRot),
				l->ActiveState == LS_REACH ? 128 : 320);

			if (segment >= 0)
			{
				if (l->ActiveState == LS_REACH)
				{
					l->AnimNumber = LA_REACH_TO_ROPE_SWING;
					l->ActiveState = LS_ROPE_SWING;
					Lara.RopeControlData.Frame = g_Level.Anims[LA_ROPE_SWING].frameBase + 32 << 8;
					Lara.RopeControlData.DFrame = g_Level.Anims[LA_ROPE_SWING].frameBase + 60 << 8;
				}
				else
				{
					l->AnimNumber = LA_JUMP_UP_TO_ROPE_START;
					l->ActiveState = LS_ROPE_IDLE;
				}

				l->FrameNumber = g_Level.Anims[l->AnimNumber].frameBase;
				l->Airborne = false;
				l->VerticalVelocity = 0;

				Lara.gunStatus = LG_HANDS_BUSY;
				Lara.RopeControlData.Ptr = item->TriggerFlags;
				Lara.RopeControlData.Segment = segment;
				Lara.RopeControlData.Y = l->Position.yRot;

				DelAlignLaraToRope(l);

				CurrentPendulum.velocity.x = 0;
				CurrentPendulum.velocity.y = 0;
				CurrentPendulum.velocity.z = 0;

				ApplyVelocityToRope(segment, l->Position.yRot, 16 * LaraItem->Velocity);
			}
		}
	}

	void RopeDynamics(ROPE_STRUCT* rope)
	{
		int flag, i;
		PENDULUM* pendulumPointer;
		PHD_VECTOR vec, vec2;

		flag = 0;
		if (rope->coiled)
		{
			--rope->coiled;
			if (!rope->coiled)
			{
				for (i = 0; i < ROPE_SEGMENTS; ++i)
					rope->velocity[i].y = 0;
			}
		}
		if (Lara.RopeControlData.Ptr != -1 && rope == &Ropes[Lara.RopeControlData.Ptr])
		{
			pendulumPointer = &CurrentPendulum;
			if (CurrentPendulum.node != Lara.RopeControlData.Segment + 1)
			{
				SetPendulumPoint(rope, Lara.RopeControlData.Segment + 1);
				flag = 1;
			}
		}
		else
		{
			pendulumPointer = &AlternatePendulum;
			if (Lara.RopeControlData.Ptr == -1 && CurrentPendulum.rope)
			{
				for (i = 0; i < CurrentPendulum.node; i++)
				{
					CurrentPendulum.rope->velocity[i].x = CurrentPendulum.rope->velocity[CurrentPendulum.node].x;
					CurrentPendulum.rope->velocity[i].y = CurrentPendulum.rope->velocity[CurrentPendulum.node].y;
					CurrentPendulum.rope->velocity[i].z = CurrentPendulum.rope->velocity[CurrentPendulum.node].z;
				}
				CurrentPendulum.position.x = 0;
				CurrentPendulum.position.y = 0;
				CurrentPendulum.position.z = 0;

				CurrentPendulum.velocity.x = 0;
				CurrentPendulum.velocity.y = 0;
				CurrentPendulum.velocity.z = 0;

				CurrentPendulum.node = -1;
				CurrentPendulum.rope = NULL;
			}
		}
		if (Lara.RopeControlData.Ptr != -1)
		{
			vec.x = pendulumPointer->position.x - rope->segment[0].x;
			vec.y = pendulumPointer->position.y - rope->segment[0].y;
			vec.z = pendulumPointer->position.z - rope->segment[0].z;
			NormaliseRopeVector(&vec);

			for (i = pendulumPointer->node; i >= 0; --i)
			{
				rope->segment[i].x = rope->meshSegment[i - 1].x + ((int64_t)rope->segmentLength * vec.x >> FP_SHIFT);
				rope->segment[i].y = rope->meshSegment[i - 1].y + ((int64_t)rope->segmentLength * vec.y >> FP_SHIFT);
				rope->segment[i].z = rope->meshSegment[i - 1].z + ((int64_t)rope->segmentLength * vec.z >> FP_SHIFT);

				rope->velocity[i].x = 0;
				rope->velocity[i].y = 0;
				rope->velocity[i].z = 0;
			}

			if (flag)
			{
				vec2.x = pendulumPointer->position.x - rope->segment[pendulumPointer->node].x;
				vec2.y = pendulumPointer->position.y - rope->segment[pendulumPointer->node].y;
				vec2.z = pendulumPointer->position.z - rope->segment[pendulumPointer->node].z;

				rope->segment[pendulumPointer->node].x = pendulumPointer->position.x;
				rope->segment[pendulumPointer->node].y = pendulumPointer->position.y;
				rope->segment[pendulumPointer->node].z = pendulumPointer->position.z;
				
				for (i = pendulumPointer->node; i < ROPE_SEGMENTS; ++i)
				{
					rope->segment[i].x -= vec2.x;
					rope->segment[i].y -= vec2.y;
					rope->segment[i].z -= vec2.z;

					rope->velocity[i].x = 0;
					rope->velocity[i].y = 0;
					rope->velocity[i].z = 0;
				}
			}
			ModelRigidRope(
				rope, 
				pendulumPointer,
				&rope->velocity[0], 
				&pendulumPointer->velocity,
				rope->segmentLength * pendulumPointer->node);
		
			pendulumPointer->velocity.y += 6 << FP_SHIFT;

			pendulumPointer->position.x += pendulumPointer->velocity.x;
			pendulumPointer->position.y += pendulumPointer->velocity.y;
			pendulumPointer->position.z += pendulumPointer->velocity.z;

			pendulumPointer->velocity.x -= pendulumPointer->velocity.x >> 8;
			pendulumPointer->velocity.z -= pendulumPointer->velocity.z >> 8;
		}
		for (i = pendulumPointer->node; i < ROPE_SEGMENTS - 1; ++i)
			ModelRigid(
				&rope->segment[i],
				&rope->segment[i + 1],
				&rope->velocity[i],
				&rope->velocity[i + 1], 
				rope->segmentLength);

		for (i = 0; i < ROPE_SEGMENTS; ++i)
		{
			rope->segment[i].x += rope->velocity[i].x;
			rope->segment[i].y += rope->velocity[i].y;
			rope->segment[i].z += rope->velocity[i].z;
		}

		for (i = pendulumPointer->node; i < ROPE_SEGMENTS; ++i)
		{
			rope->velocity[i].y += 3 << FP_SHIFT;

			if (pendulumPointer->rope)
			{
				rope->velocity[i].x -= rope->velocity[i].x >> 4;
				rope->velocity[i].z -= rope->velocity[i].z >> 4;
			}
			else
			{
				rope->velocity[i].x -= rope->velocity[i].x >> 4;
				rope->velocity[i].z -= rope->velocity[i].z >> 4;
			}
		}
		rope->segment[0].x = 0;
		rope->segment[0].y = 0;
		rope->segment[0].z = 0;

		rope->velocity[0].x = 0;
		rope->velocity[0].y = 0;
		rope->velocity[0].z = 0;

		for (i = 0; i < ROPE_SEGMENTS - 1; ++i)
		{
			rope->normalisedSegment[i].x = rope->segment[i + 1].x - rope->segment[i].x;
			rope->normalisedSegment[i].y = rope->segment[i + 1].y - rope->segment[i].y;
			rope->normalisedSegment[i].z = rope->segment[i + 1].z - rope->segment[i].z;
			NormaliseRopeVector(&rope->normalisedSegment[i]);
		}

		if (Lara.RopeControlData.Ptr != -1 && rope != &Ropes[Lara.RopeControlData.Ptr])
		{
			rope->meshSegment[0].x = rope->segment[0].x;
			rope->meshSegment[0].y = rope->segment[0].y;
			rope->meshSegment[0].z = rope->segment[0].z;

			rope->meshSegment[1].x = rope->segment[0].x + ((int64_t)rope->segmentLength * rope->normalisedSegment[0].x >> FP_SHIFT);
			rope->meshSegment[1].y = rope->segment[0].y + ((int64_t)rope->segmentLength * rope->normalisedSegment[0].y >> FP_SHIFT);
			rope->meshSegment[1].z = rope->segment[0].z + ((int64_t)rope->segmentLength * rope->normalisedSegment[0].z >> FP_SHIFT);

			for (i = 2; i < ROPE_SEGMENTS; i++)
			{
				rope->meshSegment[i].x = rope->meshSegment[i - 1].x + ((int64_t)rope->segmentLength * rope->normalisedSegment[i - 1].x >> FP_SHIFT);
				rope->meshSegment[i].y = rope->meshSegment[i - 1].y + ((int64_t)rope->segmentLength * rope->normalisedSegment[i - 1].y >> FP_SHIFT);
				rope->meshSegment[i].z = rope->meshSegment[i - 1].z + ((int64_t)rope->segmentLength * rope->normalisedSegment[i - 1].z >> FP_SHIFT);
			}
		}
		else
		{
			rope->meshSegment[pendulumPointer->node].x = rope->segment[pendulumPointer->node].x;
			rope->meshSegment[pendulumPointer->node].y = rope->segment[pendulumPointer->node].y;
			rope->meshSegment[pendulumPointer->node].z = rope->segment[pendulumPointer->node].z;

			rope->meshSegment[pendulumPointer->node + 1].x = rope->segment[pendulumPointer->node].x + ((int64_t)rope->segmentLength * rope->normalisedSegment[pendulumPointer->node].x >> FP_SHIFT);
			rope->meshSegment[pendulumPointer->node + 1].y = rope->segment[pendulumPointer->node].y + ((int64_t)rope->segmentLength * rope->normalisedSegment[pendulumPointer->node].y >> FP_SHIFT);
			rope->meshSegment[pendulumPointer->node + 1].z = rope->segment[pendulumPointer->node].z + ((int64_t)rope->segmentLength * rope->normalisedSegment[pendulumPointer->node].z >> FP_SHIFT);

			for (i = pendulumPointer->node + 1; i < ROPE_SEGMENTS - 1; ++i)
			{
				rope->meshSegment[i + 1].x = rope->meshSegment[i].x + ((int64_t)rope->segmentLength * rope->normalisedSegment[i].x >> FP_SHIFT);
				rope->meshSegment[i + 1].y = rope->meshSegment[i].y + ((int64_t)rope->segmentLength * rope->normalisedSegment[i].y >> FP_SHIFT);
				rope->meshSegment[i + 1].z = rope->meshSegment[i].z + ((int64_t)rope->segmentLength * rope->normalisedSegment[i].z >> FP_SHIFT);
			}

			for (i = 0; i < pendulumPointer->node; i++)
			{
				rope->meshSegment[i].x = rope->segment[i].x;
				rope->meshSegment[i].y = rope->segment[i].y;
				rope->meshSegment[i].z = rope->segment[i].z;
			}
		}
	}

	int RopeNodeCollision(ROPE_STRUCT* rope, int x, int y, int z, int radius)
	{
		int dx, dy, dz;

		for (int i = 0; i < ROPE_SEGMENTS - 2; ++i)
		{
			if (y > rope->position.y + (rope->meshSegment[i].y >> FP_SHIFT)
				&& y < rope->position.y + (rope->meshSegment[i + 1].y >> FP_SHIFT))
			{
				dx = x - ((rope->meshSegment[i + 1].x + rope->meshSegment[i].x) >> (FP_SHIFT + 1)) - rope->position.x;
				dy = y - ((rope->meshSegment[i + 1].y + rope->meshSegment[i].y) >> (FP_SHIFT + 1)) - rope->position.y;
				dz = z - ((rope->meshSegment[i + 1].z + rope->meshSegment[i].z) >> (FP_SHIFT + 1)) - rope->position.z;
				if (SQUARE(dx) + SQUARE(dy) + SQUARE(dz) < SQUARE(radius + 64))
					return i;
			}
		}
		return -1;
	}

	void ApplyVelocityToRope(int node, short angle, short n)
	{
		SetPendulumVelocity(n * phd_sin(angle) * 4096, 0, n * phd_cos(angle) * 4096);
	}

	void SetPendulumVelocity(int x, int y, int z)
	{
		int node;

		node = 2 * (CurrentPendulum.node >> 1);
		if (node < ROPE_SEGMENTS)
		{
			int val = 4096 / (ROPE_SEGMENTS - node) * 256;

			x = (int64_t)val * x >> FP_SHIFT;
			y = (int64_t)val * y >> FP_SHIFT;
			z = (int64_t)val * z >> FP_SHIFT;
		}

		CurrentPendulum.velocity.x += x;
		CurrentPendulum.velocity.y += y;
		CurrentPendulum.velocity.z += z;
	}

	void SetPendulumPoint(ROPE_STRUCT* rope, int node)
	{
		CurrentPendulum.position.x = rope->segment[node].x;
		CurrentPendulum.position.y = rope->segment[node].y;
		CurrentPendulum.position.z = rope->segment[node].z;

		if (CurrentPendulum.node == -1)
		{
			CurrentPendulum.velocity.x += rope->velocity[node].x;
			CurrentPendulum.velocity.y += rope->velocity[node].y;
			CurrentPendulum.velocity.z += rope->velocity[node].z;
		}

		CurrentPendulum.node = node;
		CurrentPendulum.rope = rope;
	}

	void ModelRigidRope(ROPE_STRUCT* rope, PENDULUM* pendulumPointer, PHD_VECTOR* ropeVelocity, PHD_VECTOR* pendulumVelocity, int value)
	{
		PHD_VECTOR vec;
		int result;

		vec.x = pendulumPointer->position.x + pendulumVelocity->x - rope->segment[0].x;
		vec.y = pendulumPointer->position.y + pendulumVelocity->y - rope->segment[0].y;
		vec.z = pendulumPointer->position.z + pendulumVelocity->z - rope->segment[0].z;

		result = 65536 * sqrt(abs(SQUARE(vec.x >> FP_SHIFT) + SQUARE(vec.y >> FP_SHIFT) + SQUARE(vec.z >> FP_SHIFT))) - value;
		NormaliseRopeVector(&vec);

		pendulumVelocity->x -= (int64_t)result * vec.x >> FP_SHIFT;
		pendulumVelocity->y -= (int64_t)result * vec.y >> FP_SHIFT;
		pendulumVelocity->z -= (int64_t)result * vec.z >> FP_SHIFT;
	}

	void ModelRigid(PHD_VECTOR* segment, PHD_VECTOR* nextSegment, PHD_VECTOR* velocity, PHD_VECTOR* nextVelocity, int length)
	{
		PHD_VECTOR vec;
		int result;

		vec.x = nextSegment->x + nextVelocity->x - segment->x - velocity->x;
		vec.y = nextSegment->y + nextVelocity->y - segment->y - velocity->y;
		vec.z = nextSegment->z + nextVelocity->z - segment->z - velocity->z;

		result = (65536 * sqrt(abs(SQUARE(vec.x >> FP_SHIFT) + SQUARE(vec.y >> FP_SHIFT) + SQUARE(vec.z >> FP_SHIFT))) - length) / 2;
		NormaliseRopeVector(&vec);

		vec.x = (int64_t)result * vec.x >> FP_SHIFT;
		vec.y = (int64_t)result * vec.y >> FP_SHIFT;
		vec.z = (int64_t)result * vec.z >> FP_SHIFT;

		velocity->x += vec.x;
		velocity->y += vec.y;
		velocity->z += vec.z;

		nextVelocity->x -= vec.x;
		nextVelocity->y -= vec.y;
		nextVelocity->z -= vec.z;
	}

	void UpdateRopeSwing(ITEM_INFO* item)
	{
		if (Lara.RopeControlData.MaxXForward > 9000)
		{
			Lara.RopeControlData.MaxXForward = 9000;
		}

		if (Lara.RopeControlData.MaxXBackward > 9000)
		{
			Lara.RopeControlData.MaxXBackward = 9000;
		}

		if (Lara.RopeControlData.Direction)
		{
			if (item->Position.xRot > 0 && item->Position.xRot - Lara.RopeControlData.LastX < -100)
			{
				Lara.RopeControlData.ArcFront = Lara.RopeControlData.LastX;
				Lara.RopeControlData.Direction = 0;
				Lara.RopeControlData.MaxXBackward = 0;
				int frame = 15 * Lara.RopeControlData.MaxXForward / 18000 + g_Level.Anims[LA_ROPE_SWING].frameBase + 47 << 8;
				if (frame > Lara.RopeControlData.DFrame)
				{
					Lara.RopeControlData.DFrame = frame;
					RopeSwing = 1;
				}
				else
				{
					RopeSwing = 0;
				}

				SoundEffect(SFX_TR4_LARA_ROPE_CREAK, &item->Position, 0);
			}
			else if (Lara.RopeControlData.LastX < 0 && Lara.RopeControlData.Frame == Lara.RopeControlData.DFrame)
			{
				RopeSwing = 0;
				Lara.RopeControlData.DFrame = 15 * Lara.RopeControlData.MaxXBackward / 18000 + g_Level.Anims[LA_ROPE_SWING].frameBase + 47 << 8;
				Lara.RopeControlData.FrameRate = 15 * Lara.RopeControlData.MaxXBackward / 9000 + 1;
			}
			else if (Lara.RopeControlData.FrameRate < 512)
			{
				int num = RopeSwing ? 31 : 7;

				Lara.RopeControlData.FrameRate += num * Lara.RopeControlData.MaxXBackward / 9000 + 1;
			}
		}
		else
		{
			if (item->Position.xRot < 0 && item->Position.xRot - Lara.RopeControlData.LastX > 100)
			{
				Lara.RopeControlData.ArcBack = Lara.RopeControlData.LastX;
				Lara.RopeControlData.Direction = 1;
				Lara.RopeControlData.MaxXForward = 0;
				int frame = g_Level.Anims[LA_ROPE_SWING].frameBase - 15 * Lara.RopeControlData.MaxXBackward / 18000 + 17 << 8;
				if (frame < Lara.RopeControlData.DFrame)
				{
					Lara.RopeControlData.DFrame = frame;
					RopeSwing = 1;
				}
				else
				{
					RopeSwing = 0;
				}

				SoundEffect(SFX_TR4_LARA_ROPE_CREAK, &item->Position, 0);
			}
			else if (Lara.RopeControlData.LastX > 0 && Lara.RopeControlData.Frame == Lara.RopeControlData.DFrame)
			{
				RopeSwing = 0;

				Lara.RopeControlData.DFrame = g_Level.Anims[LA_ROPE_SWING].frameBase - 15 * Lara.RopeControlData.MaxXForward / 18000 + 17 << 8;
				Lara.RopeControlData.FrameRate = 15 * Lara.RopeControlData.MaxXForward / 9000 + 1;
			}
			else if (Lara.RopeControlData.FrameRate < 512)
			{
				int num = RopeSwing ? 31 : 7;

				Lara.RopeControlData.FrameRate += num * Lara.RopeControlData.MaxXForward / 9000 + 1;
			}
		}

		Lara.RopeControlData.LastX = item->Position.xRot;
		if (Lara.RopeControlData.Direction)
		{
			if (item->Position.xRot > Lara.RopeControlData.MaxXForward)
				Lara.RopeControlData.MaxXForward = item->Position.xRot;
		}
		else
		{
			if (item->Position.xRot < -Lara.RopeControlData.MaxXBackward)
				Lara.RopeControlData.MaxXBackward = abs(item->Position.xRot);
		}
	}

	void JumpOffRope(ITEM_INFO* item)
	{
		if (Lara.RopeControlData.Ptr != -1)
		{
			if (item->Position.xRot >= 0)
			{
				item->VerticalVelocity = -112;
				item->Velocity = item->Position.xRot / 128;
			}
			else
			{
				item->Velocity = 0;
				item->VerticalVelocity = -20;
			}

			item->Position.xRot = 0;
			item->Airborne = true;

			Lara.gunStatus = LG_HANDS_FREE;

			if (item->FrameNumber - g_Level.Anims[LA_ROPE_SWING].frameBase > 42)
			{
				item->AnimNumber = LA_ROPE_SWING_TO_REACH_1;
			}
			else if (item->FrameNumber - g_Level.Anims[LA_ROPE_SWING].frameBase > 21)
			{
				item->AnimNumber = LA_ROPE_SWING_TO_REACH_2;
			}
			else
			{
				item->AnimNumber = LA_ROPE_SWING_TO_REACH_3;
			}

			item->FrameNumber = g_Level.Anims[item->AnimNumber].frameBase;
			item->ActiveState = LS_REACH;
			item->TargetState = LS_REACH;

			Lara.RopeControlData.Ptr = -1;
		}
	}

	void FallFromRope(ITEM_INFO* item)
	{
		item->Velocity = abs(CurrentPendulum.velocity.x >> FP_SHIFT) + abs(CurrentPendulum.velocity.z >> FP_SHIFT) >> 1;
		item->Position.xRot = 0;
		item->Position.yPos += 320;

		item->AnimNumber = LA_FALL_START;
		item->FrameNumber = g_Level.Anims[LA_FALL_START].frameBase;
		item->ActiveState = LS_JUMP_FORWARD;
		item->TargetState = LS_JUMP_FORWARD;

		item->VerticalVelocity = 0;
		item->Airborne = true;

		Lara.gunStatus = LG_HANDS_FREE;
		Lara.RopeControlData.Ptr = -1;
	}

	void LaraClimbRope(ITEM_INFO* item, COLL_INFO* coll)
	{
		if (!(TrInput & IN_ACTION))
		{
			FallFromRope(item);
		}
		else
		{
			Camera.targetAngle = ANGLE(30.0f);
			if (Lara.RopeControlData.Count)
			{
				if (!Lara.RopeControlData.Flag)
				{
					--Lara.RopeControlData.Count;
					Lara.RopeControlData.Offset += Lara.RopeControlData.DownVel;
					if (!Lara.RopeControlData.Count)
						Lara.RopeControlData.Flag = 1;
					return;
				}
			}
			else
			{
				if (!Lara.RopeControlData.Flag)
				{
					ROPE_STRUCT* rope = &Ropes[Lara.RopeControlData.Ptr];
					Lara.RopeControlData.Offset = 0;
					Lara.RopeControlData.DownVel = (unsigned int)(rope->meshSegment[Lara.RopeControlData.Segment + 1].y - rope->meshSegment[Lara.RopeControlData.Segment].y) >> 17;
					Lara.RopeControlData.Count = 0;
					Lara.RopeControlData.Offset += Lara.RopeControlData.DownVel;
					Lara.RopeControlData.Flag = 1;
					return;
				}
			}

			if (item->AnimNumber == LA_ROPE_DOWN && item->FrameNumber == g_Level.Anims[item->AnimNumber].frameEnd)
			{
				SoundEffect(SFX_TR4_LARA_POLE_LOOP, &LaraItem->Position, 0);
				item->FrameNumber = g_Level.Anims[item->AnimNumber].frameBase;
				Lara.RopeControlData.Flag = 0;
				++Lara.RopeControlData.Segment;
				Lara.RopeControlData.Offset = 0;
			}

			if (!(TrInput & IN_BACK) || Lara.RopeControlData.Segment >= 21)
				item->TargetState = LS_ROPE_IDLE;
		}
	}

	void DelAlignLaraToRope(ITEM_INFO* item)
	{
		ROPE_STRUCT* rope;
		short ropeY;
		PHD_VECTOR vec, vec2, vec3, vec4, vec5, pos, pos2, diff, diff2;
		int matrix[12];
		short angle[3];
		ANIM_FRAME* frame;

		vec.x = 4096;
		vec.y = 0;
		vec.z = 0;

		frame = (ANIM_FRAME*)GetBestFrame(item);
		ropeY = Lara.RopeControlData.Y - ANGLE(90);
		rope = &Ropes[Lara.RopeControlData.Ptr];

		GetRopePos(rope, (Lara.RopeControlData.Segment - 1 << 7) + frame->offsetY, &pos.x, &pos.y, &pos.z);
		GetRopePos(rope, (Lara.RopeControlData.Segment - 1 << 7) + frame->offsetY - 192, &pos2.x, &pos2.y, &pos2.z);

		diff.x = pos.x - pos2.x << 16;
		diff.y = pos.y - pos2.y << 16;
		diff.z = pos.z - pos2.z << 16;
		NormaliseRopeVector(&diff);
		diff.x >>= 2;
		diff.y >>= 2;
		diff.z >>= 2;
		ScaleVector(&diff, DotProduct(&vec, &diff), &vec2);
		vec2.x = vec.x - vec2.x;
		vec2.y = vec.y - vec2.y;
		vec2.z = vec.z - vec2.z;

		vec3.x = vec2.x;
		vec3.y = vec2.y;
		vec3.z = vec2.z;
		vec4.x = vec2.x;
		vec4.y = vec2.y;
		vec4.z = vec2.z;

		diff2.x = diff.x;
		diff2.y = diff.y;
		diff2.z = diff.z;

		ScaleVector(&vec3, 16384 * phd_cos(ropeY), &vec3);
		ScaleVector(&diff2, DotProduct(&diff2, &vec2), &diff2);
		ScaleVector(&diff2, 4096 - 16384 * phd_cos(ropeY), &diff2);

		CrossProduct(&diff, &vec2, &vec4);
		ScaleVector(&vec4, 16384 * phd_sin(ropeY), &vec4);
		diff2.x += vec3.x;
		diff2.y += vec3.y;
		diff2.z += vec3.z;

		vec2.x = diff2.x + vec4.x << 16;
		vec2.y = diff2.y + vec4.y << 16;
		vec2.z = diff2.z + vec4.z << 16;
		NormaliseRopeVector(&vec2);
		vec2.x >>= 2;
		vec2.y >>= 2;
		vec2.z >>= 2;

		CrossProduct(&diff, &vec2, &vec5);

		vec5.x <<= 16;
		vec5.y <<= 16;
		vec5.z <<= 16;
		NormaliseRopeVector(&vec5);
		vec5.x >>= 2;
		vec5.y >>= 2;
		vec5.z >>= 2;

		matrix[M00] = vec5.x;
		matrix[M01] = diff.x;
		matrix[M02] = vec2.x;
		matrix[M10] = vec5.y;
		matrix[M11] = diff.y;
		matrix[M12] = vec2.y;
		matrix[M20] = vec5.z;
		matrix[M21] = diff.z;
		matrix[M22] = vec2.z;

		phd_GetMatrixAngles(matrix, angle);

		item->Position.xPos = rope->position.x + (rope->meshSegment[Lara.RopeControlData.Segment].x >> FP_SHIFT);
		item->Position.yPos = rope->position.y + (rope->meshSegment[Lara.RopeControlData.Segment].y >> FP_SHIFT) + Lara.RopeControlData.Offset;
		item->Position.zPos = rope->position.z + (rope->meshSegment[Lara.RopeControlData.Segment].z >> FP_SHIFT);

		Matrix rotMatrix = Matrix::CreateFromYawPitchRoll(
			TO_RAD(angle[1]),
			TO_RAD(angle[0]),
			TO_RAD(angle[2])
		);

		rotMatrix = rotMatrix.Transpose();

		item->Position.xPos += -112 * rotMatrix.m[0][2];
		item->Position.yPos += -112 * rotMatrix.m[1][2];
		item->Position.zPos += -112 * rotMatrix.m[2][2];

		item->Position.xRot = angle[0];
		item->Position.yRot = angle[1];
		item->Position.zRot = angle[2];
	}
}
