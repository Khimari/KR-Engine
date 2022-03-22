#include "framework.h"
#include "Objects/TR3/Vehicles/upv.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/collision/sphere.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/los.h"
#include "Game/effects/bubble.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/savegame.h"
#include "Objects/TR3/Vehicles/upv_info.h"
#include "Sound/sound.h"
#include "Specific/level.h"
#include "Specific/input.h"
#include "Specific/setup.h"

BITE_INFO UPVBites[6] =
{
	{ 0, 0, 0, 3 },
	{ 0, 96, 256, 0 },
	{ -128, 0, -64, 1 },
	{ 0, 0, -64, 1 },
	{ 128, 0, -64, 2 },
	{ 0, 0, -64, 2 }
};

#define	UPV_CONTROL 1
#define	UPV_SURFACE 2
#define	UPV_DIVE 4
#define	UPV_DEAD 8

#define ACCELERATION		0x40000
#define FRICTION			0x18000
#define MAX_VELOCITY		0x400000
#define ROT_ACCELERATION	0x400000
#define ROT_SLOWACCEL		0x200000
#define ROT_FRICTION 		0x100000
#define MAX_ROTATION		0x1c00000
#define UPDOWN_ACCEL		(ANGLE(2.0f) * (USHRT_MAX + 1))
#define UPDOWN_SLOWACCEL	(ANGLE(1.0f) * (USHRT_MAX + 1))
#define UPDOWN_FRICTION		(ANGLE(1.0f) * (USHRT_MAX + 1))
#define MAX_UPDOWN			(ANGLE(2.0f) * (USHRT_MAX + 1))
#define UPDOWN_LIMIT		ANGLE(80.0f)
#define UPDOWN_SPEED		10
#define SURFACE_DIST		210
#define SURFACE_ANGLE		ANGLE(30.0f)
#define DIVE_ANGLE			ANGLE(15.0f)
#define DIVE_SPEED			ANGLE(5.0f)
#define UPV_DRAW_SHIFT		128
#define UPV_RADIUS			300
#define UPV_HEIGHT			400
#define UPV_LENGTH			SECTOR(1)
#define FRONT_TOLERANCE		(ANGLE(45.0f) * (USHRT_MAX + 1))
#define TOP_TOLERANCE		(ANGLE(45.0f) * (USHRT_MAX + 1))
#define WALL_DEFLECT		(ANGLE(2.0f) * (USHRT_MAX + 1))
#define DISMOUNT_DISTANCE 	SECTOR(1)
#define HARPOON_VELOCITY	CLICK(1)
#define HARPOON_RELOAD		15

#define UPV_TURBINE_BONE 3

#define DEATH_FRAME_1					16
#define DEATH_FRAME_2					17
#define DISMOUNT_SURFACE_FRAME			51
#define MOUNT_SURFACE_SOUND_FRAME		30
#define MOUNT_SURFACE_CONTROL_FRAME		50
#define DISMOUNT_UNDERWATER_FRAME		42
#define MOUNT_UNDERWATER_SOUND_FRAME	30
#define MOUNT_UNDERWATER_CONTROL_FRAME	42

#define UPV_IN_PROPEL	IN_JUMP
#define UPV_IN_UP		IN_FORWARD
#define UPV_IN_DOWN		IN_BACK
#define UPV_IN_LEFT		IN_LEFT
#define UPV_IN_RIGHT	IN_RIGHT
#define UPV_IN_FIRE		IN_ACTION
#define UPV_IN_DISMOUNT	IN_ROLL

enum UPVState
{
	UPV_STATE_DEATH = 0,
	UPV_STATE_HIT = 1,
	UPV_STATE_DISMOUNT_WATER_SURFACE = 2,
	UPV_STATE_UNK1 = 3,
	UPV_STATE_MOVE = 4,
	UPV_STATE_IDLE = 5,
	UPV_STATE_UNK2 = 6, // TODO
	UPV_STATE_UNK3 = 7, // TODO
	UPV_STATE_MOUNT = 8,
	UPV_STATE_DISMOUNT_UNDERWATER = 9
};

// TODO
enum UPVAnim
{
	UPV_ANIM_DEATH = 0,

	UPV_ANIM_IDLE = 5,

	UPV_ANIM_DISMOUNT_SURFACE = 9,
	UPV_ANIM_MOUNT_SURFACE_START = 10,
	UPV_ANIM_MOUNT_SURFACE_END = 11,
	UPV_ANIM_DISMOUNT_UNDERWATER = 12,
	UPV_ANIM_MOUNT_UNDERWATER = 13,
};

enum UPVBiteFlags
{
	UPV_FAN = 0,
	UPV_FRONT_LIGHT = 1,
	UPV_LEFT_FIN_LEFT = 2,
	UPV_LEFT_FIN_RIGHT = 3,
	UPV_RIGHT_FIN_RIGHT = 4,
	UPV_RIGHT_FIN_LEFT = 5
};

void UPVInitialise(short itemNumber)
{
	auto* UPVItem = &g_Level.Items[itemNumber];
	UPVItem->Data = UPVInfo();
	auto* UPV = (UPVInfo*)UPVItem->Data;

	UPV->Velocity = 0;
	UPV->Rot = 0;
	UPV->Flags = UPV_SURFACE;
	UPV->HarpoonTimer = 0;
	UPV->HarpoonLeft = false;
}

static void FireUPVHarpoon(ITEM_INFO* laraItem, ITEM_INFO* UPVItem)
{
	auto* lara = GetLaraInfo(laraItem);
	auto UPV = (UPVInfo*)UPVItem->Data;

	short itemNumber = CreateItem();

	if (itemNumber != NO_ITEM)
	{
		auto* harpoonItem = &g_Level.Items[itemNumber];
		harpoonItem->ObjectNumber = ID_HARPOON;
		harpoonItem->Shade = 0xC210;
		harpoonItem->RoomNumber = UPVItem->RoomNumber;

		PHD_VECTOR pos{ (UPV->HarpoonLeft ? 22 : -22), 24, 230 };
		GetJointAbsPosition(UPVItem, &pos, UPV_TURBINE_BONE);

		harpoonItem->Position.xPos = pos.x;
		harpoonItem->Position.yPos = pos.y;
		harpoonItem->Position.zPos = pos.z;
		InitialiseItem(itemNumber);

		harpoonItem->Position.xRot = UPVItem->Position.xRot;
		harpoonItem->Position.yRot = UPVItem->Position.yRot;
		harpoonItem->Position.zRot = 0;

		// TODO: Huh?
		harpoonItem->Animation.VerticalVelocity = -HARPOON_VELOCITY * phd_sin(harpoonItem->Position.xRot);
		harpoonItem->Animation.Velocity = HARPOON_VELOCITY * phd_cos(harpoonItem->Position.xRot);
		harpoonItem->HitPoints = HARPOON_TIME;
		harpoonItem->ItemFlags[0] = 1;

		AddActiveItem(itemNumber);

		SoundEffect(SFX_LARA_HARPOON_FIRE_WATER, &laraItem->Position, 2);

		if (lara->Weapons[(int)LaraWeaponType::HarpoonGun].Ammo[(int)WeaponAmmoType::Ammo1])
			lara->Weapons[(int)LaraWeaponType::HarpoonGun].Ammo[(int)WeaponAmmoType::Ammo1]--;

		Statistics.Game.AmmoUsed++;
		UPV->HarpoonLeft = !UPV->HarpoonLeft;
	}
}

static void TriggerUPVMist(long x, long y, long z, long velocity, short angle)
{
	auto* sptr = &Sparks[GetFreeSpark()];

	sptr->on = 1;
	sptr->sR = 0;
	sptr->sG = 0;
	sptr->sB = 0;

	sptr->dR = 64;
	sptr->dG = 64;
	sptr->dB = 64;

	sptr->colFadeSpeed = 4 + (GetRandomControl() & 3);
	sptr->fadeToBlack = 12;
	sptr->sLife = sptr->life = (GetRandomControl() & 3) + 20;
	sptr->transType = TransTypeEnum::COLADD;
	sptr->extras = 0;
	sptr->dynamic = -1;

	sptr->x = x + ((GetRandomControl() & 15) - 8);
	sptr->y = y + ((GetRandomControl() & 15) - 8);
	sptr->z = z + ((GetRandomControl() & 15) - 8);
	long zv = velocity * phd_cos(angle) / 4;
	long xv = velocity * phd_sin(angle) / 4;
	sptr->xVel = xv + ((GetRandomControl() & 127) - 64);
	sptr->yVel = 0;
	sptr->zVel = zv + ((GetRandomControl() & 127) - 64);
	sptr->friction = 3;

	if (GetRandomControl() & 1)
	{
		sptr->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
		sptr->rotAng = GetRandomControl() & 4095;

		if (GetRandomControl() & 1)
			sptr->rotAdd = -(GetRandomControl() & 15) - 16;
		else
			sptr->rotAdd = (GetRandomControl() & 15) + 16;
	}
	else
		sptr->flags = SP_SCALE | SP_DEF | SP_EXPDEF;

	sptr->scalar = 3;
	sptr->gravity = sptr->maxYvel = 0;
	long size = (GetRandomControl() & 7) + (velocity / 2) + 16;
	sptr->size = sptr->sSize = size / 4;
	sptr->dSize = size;
}

void UPVEffects(short itemNumber)
{
	if (itemNumber == NO_ITEM)
		return;

	auto* laraItem = LaraItem;
	auto* lara = GetLaraInfo(laraItem);
	auto* UPVItem = &g_Level.Items[itemNumber];
	auto* UPV = (UPVInfo*)UPVItem->Data;

	PHD_VECTOR pos;

	if (lara->Vehicle == itemNumber)
	{
		if (!UPV->Velocity)
			UPV->FanRot += ANGLE(2.0f);
		else
			UPV->FanRot += UPV->Velocity / 4069;

		if (UPV->Velocity)
		{
			pos = { UPVBites[UPV_FAN].x, UPVBites[UPV_FAN].y, UPVBites[UPV_FAN].z };
			GetJointAbsPosition(UPVItem, &pos, UPVBites[UPV_FAN].meshNum);

			TriggerUPVMist(pos.x, pos.y + UPV_DRAW_SHIFT, pos.z, abs(UPV->Velocity) / (USHRT_MAX + 1), UPVItem->Position.yRot + ANGLE(180.0f));

			if ((GetRandomControl() & 1) == 0)
			{
				PHD_3DPOS pos2;
				pos2.xPos = pos.x + (GetRandomControl() & 63) - 32;
				pos2.yPos = pos.y + UPV_DRAW_SHIFT;
				pos2.zPos = pos.z + (GetRandomControl() & 63) - 32;
				short probedRoomNumber = GetCollision(pos2.xPos, pos2.yPos, pos2.zPos, UPVItem->RoomNumber).RoomNumber;
				
				CreateBubble((PHD_VECTOR*)&pos2, probedRoomNumber, 4, 8, BUBBLE_FLAG_CLUMP, 0, 0, 0);
			}
		}
	}
	
	for (int lp = 0; lp < 2; lp++)
	{
		int random = 31 - (GetRandomControl() & 3);
		pos = { UPVBites[UPV_FRONT_LIGHT].x, UPVBites[UPV_FRONT_LIGHT].y, UPVBites[UPV_FRONT_LIGHT].z << (lp * 6) };
		GetJointAbsPosition(UPVItem, &pos, UPVBites[UPV_FRONT_LIGHT].meshNum);

		GAME_VECTOR	source, target;
		if (lp == 1)
		{
			target.x = pos.x;
			target.y = pos.y;
			target.z = pos.z;
			target.roomNumber = UPVItem->RoomNumber;
			LOS(&source, &target);
			pos = { target.x, target.y, target.z };
		}
		else
		{
			source.x = pos.x;
			source.y = pos.y;
			source.z = pos.z;
			source.roomNumber = UPVItem->RoomNumber;
		}

		TriggerDynamicLight(pos.x, pos.y, pos.z, 16 + (lp << 3), random, random, random);
	}

	if (UPV->HarpoonTimer)
		UPV->HarpoonTimer--;
}

static bool TestUPVDismount(ITEM_INFO* laraItem, ITEM_INFO* UPVItem)
{
	auto* lara = GetLaraInfo(laraItem);

	if (lara->WaterCurrentPull.x || lara->WaterCurrentPull.z)
		return false;

	short moveAngle = UPVItem->Position.yRot + ANGLE(180.0f);
	int velocity = DISMOUNT_DISTANCE * phd_cos(UPVItem->Position.xRot);
	int x = UPVItem->Position.xPos + velocity * phd_sin(moveAngle);
	int z = UPVItem->Position.zPos + velocity * phd_cos(moveAngle);
	int y = UPVItem->Position.yPos - DISMOUNT_DISTANCE * phd_sin(-UPVItem->Position.xRot);

	auto probe = GetCollision(x, y, z, UPVItem->RoomNumber);
	if ((probe.Position.Floor - probe.Position.Ceiling) < CLICK(1) ||
		probe.Position.Floor < y ||
		probe.Position.Ceiling > y ||
		probe.Position.Floor == NO_HEIGHT ||
		probe.Position.Ceiling == NO_HEIGHT)
	{
		return false;
	}

	return true;
}

static bool TestUPVMount(ITEM_INFO* laraItem, ITEM_INFO* UPVItem)
{
	auto* lara = GetLaraInfo(laraItem);

	if (!(TrInput & IN_ACTION) ||
		lara->Control.HandStatus != HandStatus::Free ||
		laraItem->Animation.Airborne)
	{
		return false;
	}

	int y = abs(laraItem->Position.yPos - (UPVItem->Position.yPos - CLICK(0.5f)));
	if (y > CLICK(1))
		return false;

	int distance = pow(laraItem->Position.xPos - UPVItem->Position.xPos, 2) + pow(laraItem->Position.zPos - UPVItem->Position.zPos, 2);
	if (distance > pow(CLICK(2), 2))
		return false;

	short deltaAngle = abs(laraItem->Position.yRot - UPVItem->Position.yRot);
	if (deltaAngle > ANGLE(35.0f) || deltaAngle < -ANGLE(35.0f))
		return false;

	if (GetCollision(UPVItem).Position.Floor < -32000)
		return false;

	return true;
}

static void DoCurrent(ITEM_INFO* laraItem, ITEM_INFO* UPVItem)
{
	auto* lara = GetLaraInfo(laraItem);

	PHD_VECTOR target;

	if (!lara->WaterCurrentActive)
	{
		int absVel = abs(lara->WaterCurrentPull.x);
		int shift;
		if (absVel > 16)
			shift = 4;
		else if (absVel > 8)
			shift = 3;
		else
			shift = 2;

		lara->WaterCurrentPull.x -= lara->WaterCurrentPull.x >> shift;

		if (abs(lara->WaterCurrentPull.x) < 4)
			lara->WaterCurrentPull.x = 0;

		absVel = abs(lara->WaterCurrentPull.z);
		if (absVel > 16)
			shift = 4;
		else if (absVel > 8)
			shift = 3;
		else
			shift = 2;

		lara->WaterCurrentPull.z -= lara->WaterCurrentPull.z >> shift;
		if (abs(lara->WaterCurrentPull.z) < 4)
			lara->WaterCurrentPull.z = 0;

		if (lara->WaterCurrentPull.x == 0 && lara->WaterCurrentPull.z == 0)
			return;
	}
	else
	{
		int sinkVal = lara->WaterCurrentActive - 1;
		target.x = g_Level.Sinks[sinkVal].x;
		target.y = g_Level.Sinks[sinkVal].y;
		target.z = g_Level.Sinks[sinkVal].z;
		
		int angle = ((mGetAngle(target.x, target.z, laraItem->Position.xPos, laraItem->Position.zPos) - ANGLE(90.0f)) / 16) & 4095;

		int dx = target.x - laraItem->Position.xPos;
		int dz = target.z - laraItem->Position.zPos;

		int velocity = g_Level.Sinks[sinkVal].strength;
		dx = phd_sin(angle * 16) * velocity * SECTOR(1);
		dz = phd_cos(angle * 16) * velocity * SECTOR(1);

		lara->WaterCurrentPull.x += ((dx - lara->WaterCurrentPull.x) / 16);
		lara->WaterCurrentPull.z += ((dz - lara->WaterCurrentPull.z) / 16);
	}

	lara->WaterCurrentActive = 0;
	UPVItem->Position.xPos += lara->WaterCurrentPull.x / CLICK(1);
	UPVItem->Position.zPos += lara->WaterCurrentPull.z / CLICK(1);
}

static void BackgroundCollision(ITEM_INFO* laraItem, ITEM_INFO* UPVItem)
{
	auto* lara = GetLaraInfo(laraItem);
	auto* UPV = (UPVInfo*)UPVItem->Data;
	CollisionInfo cinfo, * coll = &cinfo; // ??

	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -UPV_HEIGHT;
	coll->Setup.LowerCeilingBound = UPV_HEIGHT;
	coll->Setup.UpperCeilingBound = MAX_HEIGHT;
	coll->Setup.OldPosition.x = UPVItem->Position.xPos;
	coll->Setup.OldPosition.y = UPVItem->Position.yPos;
	coll->Setup.OldPosition.z = UPVItem->Position.zPos;
	coll->Setup.Radius = UPV_RADIUS;
	coll->Setup.FloorSlopeIsWall = false;
	coll->Setup.FloorSlopeIsPit = false;
	coll->Setup.DeathFlagIsPit = false;
	coll->Setup.NoMonkeyFlagIsWall = false;
	coll->Setup.EnableSpasm = false;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.Mode = CollisionProbeMode::Quadrants;

	if ((UPVItem->Position.xRot >= -(SHRT_MAX / 2 + 1)) && (UPVItem->Position.xRot <= (SHRT_MAX / 2 + 1)))
	{
		lara->Control.MoveAngle = UPVItem->Position.yRot;
		coll->Setup.ForwardAngle = lara->Control.MoveAngle;
	}
	else
	{
		lara->Control.MoveAngle = UPVItem->Position.yRot - ANGLE(180.0f);
		coll->Setup.ForwardAngle = lara->Control.MoveAngle;
	}

	int height = phd_sin(UPVItem->Position.xRot) * UPV_LENGTH;
	if (height < 0)
		height = -height;
	if (height < 200)
		height = 200;

	coll->Setup.UpperFloorBound = -height;
	coll->Setup.Height = height;

	GetCollisionInfo(coll, UPVItem, PHD_VECTOR(0, height / 2, 0));
	ShiftItem(UPVItem, coll);

	if (coll->CollisionType == CT_FRONT)
	{
		if (UPV->XRot > FRONT_TOLERANCE)
			UPV->XRot += WALL_DEFLECT;
		else if (UPV->XRot < -FRONT_TOLERANCE)
			UPV->XRot -= WALL_DEFLECT;
		else
		{
			if (abs(UPV->Velocity) >= MAX_VELOCITY)
			{
				laraItem->Animation.TargetState = UPV_STATE_HIT;
				UPV->Velocity = -UPV->Velocity / 2;
			}
			else
				UPV->Velocity = 0;
		}
	}
	else if (coll->CollisionType == CT_TOP)
	{
		if (UPV->XRot >= -TOP_TOLERANCE)
			UPV->XRot -= WALL_DEFLECT;
	}
	else if (coll->CollisionType == CT_TOP_FRONT)
		UPV->Velocity = 0;
	else if (coll->CollisionType == CT_LEFT)
		UPVItem->Position.yRot += ANGLE(5.0f);
	else if (coll->CollisionType == CT_RIGHT)
		UPVItem->Position.yRot -= ANGLE(5.0f);
	else if (coll->CollisionType == CT_CLAMP)
	{
		UPVItem->Position.xPos = coll->Setup.OldPosition.x;
		UPVItem->Position.yPos = coll->Setup.OldPosition.y;
		UPVItem->Position.zPos = coll->Setup.OldPosition.z;
		UPV->Velocity = 0;
		return;
	}

	if (coll->Middle.Floor < 0)
	{
		UPVItem->Position.yPos += coll->Middle.Floor;
		UPV->XRot += WALL_DEFLECT;
	}
}

static void UPVControl(ITEM_INFO* laraItem, ITEM_INFO* UPVItem)
{
	auto* lara = GetLaraInfo(laraItem);
	auto* UPV = (UPVInfo*)UPVItem->Data;

	TestUPVDismount(laraItem, UPVItem);

	int anim = laraItem->Animation.AnimNumber - Objects[ID_UPV_LARA_ANIMS].animIndex;
	int frame = laraItem->Animation.FrameNumber - g_Level.Anims[laraItem->Animation.AnimNumber].frameBase;

	switch (laraItem->Animation.ActiveState)
	{
	case UPV_STATE_MOVE:
		if (laraItem->HitPoints <= 0)
		{
			laraItem->Animation.TargetState = UPV_STATE_DEATH;
			break;
		}

		if (TrInput & UPV_IN_LEFT)
			UPV->Rot -= ROT_ACCELERATION;

		else if (TrInput & UPV_IN_RIGHT)
			UPV->Rot += ROT_ACCELERATION;

		if (UPV->Flags & UPV_SURFACE)
		{
			int xa = UPVItem->Position.xRot - SURFACE_ANGLE;
			int ax = SURFACE_ANGLE - UPVItem->Position.xRot;

			if (xa > 0)
			{
				if (xa > ANGLE(1.0f))
					UPVItem->Position.xRot -= ANGLE(1.0f);
				else
					UPVItem->Position.xRot -= ANGLE(0.1f);
			}
			else if (ax)
			{
				if (ax > ANGLE(1.0f))
					UPVItem->Position.xRot += ANGLE(1.0f);
				else
					UPVItem->Position.xRot += ANGLE(0.1f);
			}
			else
				UPVItem->Position.xRot = SURFACE_ANGLE;
		}
		else
		{
			if (TrInput & UPV_IN_UP)
				UPV->XRot -= UPDOWN_ACCEL;
			else if (TrInput & UPV_IN_DOWN)
				UPV->XRot += UPDOWN_ACCEL;
		}

		if (TrInput & UPV_IN_PROPEL)
		{
			if (TrInput & UPV_IN_UP &&
				UPV->Flags & UPV_SURFACE &&
				UPVItem->Position.xRot > -DIVE_ANGLE)
			{
				UPV->Flags |= UPV_DIVE;
			}

			UPV->Velocity += ACCELERATION;
		}

		else
			laraItem->Animation.TargetState = UPV_STATE_IDLE;

		break;

	case UPV_STATE_IDLE:
		if (laraItem->HitPoints <= 0)
		{
			laraItem->Animation.TargetState = UPV_STATE_DEATH;
			break;
		}

		if (TrInput & UPV_IN_LEFT)
			UPV->Rot -= ROT_SLOWACCEL;
		else if (TrInput & UPV_IN_RIGHT)
			UPV->Rot += ROT_SLOWACCEL;

		if (UPV->Flags & UPV_SURFACE)
		{
			int xa = UPVItem->Position.xRot - SURFACE_ANGLE;
			int ax = SURFACE_ANGLE - UPVItem->Position.xRot;
			if (xa > 0)
			{
				if (xa > ANGLE(1.0f))
					UPVItem->Position.xRot -= ANGLE(1.0f);
				else
					UPVItem->Position.xRot -= ANGLE(0.1f);
			}
			else if (ax)
			{
				if (ax > ANGLE(1.0f))
					UPVItem->Position.xRot += ANGLE(1.0f);
				else
					UPVItem->Position.xRot += ANGLE(0.1f);
			}
			else
				UPVItem->Position.xRot = SURFACE_ANGLE;
		}
		else
		{
			if (TrInput & UPV_IN_UP)
				UPV->XRot -= UPDOWN_ACCEL;
			else if (TrInput & UPV_IN_DOWN)
				UPV->XRot += UPDOWN_ACCEL;
		}

		if (TrInput & UPV_IN_DISMOUNT && TestUPVDismount(laraItem, UPVItem))
		{
			if (UPV->Velocity > 0)
				UPV->Velocity -= ACCELERATION;
			else
			{
				if (UPV->Flags & UPV_SURFACE)
					laraItem->Animation.TargetState = UPV_STATE_DISMOUNT_WATER_SURFACE;
				else
					laraItem->Animation.TargetState = UPV_STATE_DISMOUNT_UNDERWATER;

				//sub->Flags &= ~UPV_CONTROL; having this here causes the UPV glitch, moving it directly to the states' code is better

				StopSoundEffect(SFX_TR3_UPV_LOOP);
				SoundEffect(SFX_TR3_UPV_STOP, (PHD_3DPOS*)&UPVItem->Position.xPos, 2);
			}
		}

		else if (TrInput & UPV_IN_PROPEL)
		{
			if (TrInput & UPV_IN_UP &&
				UPVItem->Position.xRot > -DIVE_ANGLE &&
				UPV->Flags & UPV_SURFACE)
			{
				UPV->Flags |= UPV_DIVE;
			}

			laraItem->Animation.TargetState = UPV_STATE_MOVE;
		}

		break;

	case UPV_STATE_MOUNT:
		if (anim == UPV_ANIM_MOUNT_SURFACE_END)
		{
			UPVItem->Position.yPos += 4;
			UPVItem->Position.xRot += ANGLE(1.0f);

			if (frame == MOUNT_SURFACE_SOUND_FRAME)
				SoundEffect(SFX_TR3_UPV_LOOP, (PHD_3DPOS*)&UPVItem->Position.xPos, 2);

			if (frame == MOUNT_SURFACE_CONTROL_FRAME)
				UPV->Flags |= UPV_CONTROL;
		}

		else if (anim == UPV_ANIM_MOUNT_UNDERWATER)
		{
			if (frame == MOUNT_UNDERWATER_SOUND_FRAME)
				SoundEffect(SFX_TR3_UPV_LOOP, (PHD_3DPOS*)&UPVItem->Position.xPos, 2);

			if (frame == MOUNT_UNDERWATER_CONTROL_FRAME)
				UPV->Flags |= UPV_CONTROL;
		}

		break;

	case UPV_STATE_DISMOUNT_UNDERWATER:
		if (anim == UPV_ANIM_DISMOUNT_UNDERWATER && frame == DISMOUNT_UNDERWATER_FRAME)
		{
			UPV->Flags &= ~UPV_CONTROL;

			PHD_VECTOR vec = { 0, 0, 0 };
			GetLaraJointPosition(&vec, LM_HIPS);

			GAME_VECTOR LPos;
			LPos.x = vec.x;
			LPos.y = vec.y;
			LPos.z = vec.z;
			LPos.roomNumber = UPVItem->RoomNumber;

			GAME_VECTOR VPos;
			VPos.x = UPVItem->Position.xPos;
			VPos.y = UPVItem->Position.yPos;
			VPos.z = UPVItem->Position.zPos;
			VPos.roomNumber = UPVItem->RoomNumber;
			LOSAndReturnTarget(&VPos, &LPos, 0);

			laraItem->Position.xPos = LPos.x;
			laraItem->Position.yPos = LPos.y;
			laraItem->Position.zPos = LPos.z;

			SetAnimation(laraItem, LA_UNDERWATER_IDLE);
			laraItem->Animation.VerticalVelocity = 0;
			laraItem->Animation.Airborne = false;
			laraItem->Position.xRot = laraItem->Position.zRot = 0;

			UpdateItemRoom(laraItem, 0);

			lara->Control.WaterStatus = WaterStatus::Underwater;
			lara->Control.HandStatus = HandStatus::Free;
			lara->Vehicle = NO_ITEM;

			UPVItem->HitPoints = 0;
		}

		break;

	case UPV_STATE_DISMOUNT_WATER_SURFACE:
		if (anim == UPV_ANIM_DISMOUNT_SURFACE && frame == DISMOUNT_SURFACE_FRAME)
		{
			UPV->Flags &= ~UPV_CONTROL;
			int waterDepth, waterHeight, heightFromWater;

			waterDepth = GetWaterSurface(laraItem);
			waterHeight = GetWaterHeight(laraItem);

			if (waterHeight != NO_HEIGHT)
				heightFromWater = laraItem->Position.yPos - waterHeight;
			else
				heightFromWater = NO_HEIGHT;

			PHD_VECTOR vec = { 0, 0, 0 };
			GetLaraJointPosition(&vec, LM_HIPS);

			laraItem->Position.xPos = vec.x;
			//laraItem->pos.yPos += -heightFromWater + 1; // Doesn't work as intended.
			laraItem->Position.yPos = vec.y;
			laraItem->Position.zPos = vec.z;

			SetAnimation(laraItem, LA_ONWATER_IDLE);
			laraItem->Animation.VerticalVelocity = 0;
			laraItem->Animation.Airborne = false;
			laraItem->Position.xRot = 0;
			laraItem->Position.zRot = 0;

			UpdateItemRoom(laraItem, -LARA_HEIGHT / 2);

			ResetLaraFlex(laraItem);
			lara->Control.WaterStatus = WaterStatus::TreadWater;
			lara->WaterSurfaceDist = -heightFromWater;
			lara->Control.HandStatus = HandStatus::Free;
			lara->Vehicle = NO_ITEM;

			UPVItem->HitPoints = 0;
		}
		else
		{
			UPV->XRot -= UPDOWN_ACCEL;
			if (UPVItem->Position.xRot < 0)
				UPVItem->Position.xRot = 0;
		}

		break;

	case UPV_STATE_DEATH:
		if (anim == UPV_ANIM_DEATH && (frame == DEATH_FRAME_1 || frame == DEATH_FRAME_2))
		{
			PHD_VECTOR vec = { 0, 0, 0 };
			GetLaraJointPosition(&vec, LM_HIPS);

			laraItem->Position.xPos = vec.x;
			laraItem->Position.yPos = vec.y;
			laraItem->Position.zPos = vec.z;
			laraItem->Position.xRot = 0;
			laraItem->Position.zRot = 0;

			SetAnimation(UPVItem, LA_UNDERWATER_DEATH, 17);
			laraItem->Animation.VerticalVelocity = 0;
			laraItem->Animation.Airborne = false;
			
			UPV->Flags |= UPV_DEAD;
		}

		UPVItem->Animation.Velocity = 0;
		break;
	}

	if (UPV->Flags & UPV_DIVE)
	{
		if (UPVItem->Position.xRot > -DIVE_ANGLE)
			UPVItem->Position.xRot -= DIVE_SPEED;
		else
			UPV->Flags &= ~UPV_DIVE;
	}

	if (UPV->Velocity > 0)
	{
		UPV->Velocity -= FRICTION;
		if (UPV->Velocity < 0)
			UPV->Velocity = 0;
	}
	else if (UPV->Velocity < 0)
	{
		UPV->Velocity += FRICTION;
		if (UPV->Velocity > 0)
			UPV->Velocity = 0;
	}

	if (UPV->Velocity > MAX_VELOCITY)
		UPV->Velocity = MAX_VELOCITY;
	else if (UPV->Velocity < -MAX_VELOCITY)
		UPV->Velocity = -MAX_VELOCITY;

	if (UPV->Rot > 0)
	{
		UPV->Rot -= ROT_FRICTION;
		if (UPV->Rot < 0)
			UPV->Rot = 0;
	}
	else if (UPV->Rot < 0)
	{
		UPV->Rot += ROT_FRICTION;
		if (UPV->Rot > 0)
			UPV->Rot = 0;
	}

	if (UPV->XRot > 0)
	{
		UPV->XRot -= UPDOWN_FRICTION;
		if (UPV->XRot < 0)
			UPV->XRot = 0;
	}
	else if (UPV->XRot < 0)
	{
		UPV->XRot += UPDOWN_FRICTION;
		if (UPV->XRot > 0)
			UPV->XRot = 0;
	}

	if (UPV->Rot > MAX_ROTATION)
		UPV->Rot = MAX_ROTATION;
	else if (UPV->Rot < -MAX_ROTATION)
		UPV->Rot = -MAX_ROTATION;

	if (UPV->XRot > MAX_UPDOWN)
		UPV->XRot = MAX_UPDOWN;
	else if (UPV->XRot < -MAX_UPDOWN)
		UPV->XRot = -MAX_UPDOWN;
}

void NoGetOnCollision(short itemNumber, ITEM_INFO* laraItem, CollisionInfo* coll)
{
	auto* item = &g_Level.Items[itemNumber];

	if (!TestBoundsCollide(item, laraItem, coll->Setup.Radius))
		return;
	if (!TestCollision(item, laraItem))
		return;

	ItemPushItem(item, laraItem, coll, 0, 0);
}

void UPVCollision(short itemNumber, ITEM_INFO* laraItem, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(laraItem);
	auto* UPVItem = &g_Level.Items[itemNumber];

	if (laraItem->HitPoints <= 0 || lara->Vehicle != NO_ITEM)
		return;

	if (TestUPVMount(laraItem, UPVItem))
	{
		lara->Vehicle = itemNumber;
		lara->Control.WaterStatus = WaterStatus::Dry;

		if (lara->Control.Weapon.GunType == LaraWeaponType::Flare)
		{
			CreateFlare(laraItem, ID_FLARE_ITEM, 0);
			UndrawFlareMeshes(laraItem);

			lara->Flare.ControlLeft = false;
			lara->Control.Weapon.RequestGunType = lara->Control.Weapon.GunType = LaraWeaponType::None;
		}

		laraItem->Position.xPos = UPVItem->Position.xPos;
		laraItem->Position.yPos = UPVItem->Position.yPos;
		laraItem->Position.zPos = UPVItem->Position.zPos;
		laraItem->Position.xRot = UPVItem->Position.xRot;
		laraItem->Position.yRot = UPVItem->Position.yRot;
		laraItem->Position.zRot = UPVItem->Position.zRot;
		lara->Control.HandStatus = HandStatus::Busy;
		UPVItem->HitPoints = 1;

		if (laraItem->Animation.ActiveState == LS_ONWATER_IDLE || laraItem->Animation.ActiveState == LS_ONWATER_FORWARD)
		{
			laraItem->Animation.AnimNumber = Objects[ID_UPV_LARA_ANIMS].animIndex + UPV_ANIM_MOUNT_SURFACE_START;
			laraItem->Animation.ActiveState = laraItem->Animation.TargetState = UPV_STATE_MOUNT;
		}
		else
		{
			laraItem->Animation.AnimNumber = Objects[ID_UPV_LARA_ANIMS].animIndex + UPV_ANIM_MOUNT_UNDERWATER;
			laraItem->Animation.ActiveState = laraItem->Animation.TargetState = UPV_STATE_MOUNT;
		}

		laraItem->Animation.FrameNumber = g_Level.Anims[laraItem->Animation.AnimNumber].frameBase;
		AnimateItem(laraItem);
	}
	else
	{
		UPVItem->Position.yPos += UPV_DRAW_SHIFT;
		NoGetOnCollision(itemNumber, laraItem, coll);
		UPVItem->Position.yPos -= UPV_DRAW_SHIFT;
	}
}

bool UPVControl(ITEM_INFO* laraItem, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(laraItem);
	auto* UPVItem = &g_Level.Items[lara->Vehicle];
	auto* UPV = (UPVInfo*)UPVItem->Data;
	
	auto oldPos = UPVItem->Position;
	auto probe = GetCollision(UPVItem);

	if (!(UPV->Flags & UPV_DEAD))
	{
		UPVControl(laraItem, UPVItem);

		UPVItem->Animation.Velocity = UPV->Velocity / (USHRT_MAX + 1);

		UPVItem->Position.xRot += UPV->XRot / (USHRT_MAX + 1);
		UPVItem->Position.yRot += UPV->Rot / (USHRT_MAX + 1);
		UPVItem->Position.zRot = UPV->Rot / (USHRT_MAX + 1);

		if (UPVItem->Position.xRot > UPDOWN_LIMIT)
			UPVItem->Position.xRot = UPDOWN_LIMIT;
		else if (UPVItem->Position.xRot < -UPDOWN_LIMIT)
			UPVItem->Position.xRot = -UPDOWN_LIMIT;

		UPVItem->Position.xPos += phd_sin(UPVItem->Position.yRot) * UPVItem->Animation.Velocity * phd_cos(UPVItem->Position.xRot);
		UPVItem->Position.yPos -= phd_sin(UPVItem->Position.xRot) * UPVItem->Animation.Velocity;
		UPVItem->Position.zPos += phd_cos(UPVItem->Position.yRot) * UPVItem->Animation.Velocity * phd_cos(UPVItem->Position.xRot);
	}

	int newHeight = GetCollision(UPVItem).Position.Floor;
	int waterHeight = GetWaterHeight(UPVItem);

	if ((newHeight - waterHeight) < UPV_HEIGHT || (newHeight < UPVItem->Position.yPos - UPV_HEIGHT / 2))
	{
		UPVItem->Position.xPos = oldPos.xPos;
		UPVItem->Position.yPos = oldPos.yPos;
		UPVItem->Position.zPos = oldPos.zPos;
		UPVItem->Animation.Velocity = 0;
	}

	UPVItem->Floor = probe.Position.Floor;

	if (UPV->Flags & UPV_CONTROL && !(UPV->Flags & UPV_DEAD))
	{
		if (!TestEnvironment(ENV_FLAG_WATER, UPVItem->RoomNumber) &&
			waterHeight != NO_HEIGHT)
		{
			if ((waterHeight - UPVItem->Position.yPos) >= -SURFACE_DIST)
				UPVItem->Position.yPos = waterHeight + SURFACE_DIST;

			if (!(UPV->Flags & UPV_SURFACE))
			{
				SoundEffect(SFX_TR4_LARA_BREATH, &laraItem->Position, 2);
				UPV->Flags &= ~UPV_DIVE;
			}

			UPV->Flags |= UPV_SURFACE;
		}

		else if ((waterHeight - UPVItem->Position.yPos) >= -SURFACE_DIST && waterHeight != NO_HEIGHT)
		{
			UPVItem->Position.yPos = waterHeight + SURFACE_DIST;

			if (!(UPV->Flags & UPV_SURFACE))
			{
				SoundEffect(SFX_TR4_LARA_BREATH, &laraItem->Position, 2);
				UPV->Flags &= ~UPV_DIVE;
			}

			UPV->Flags |= UPV_SURFACE;
		}

		else
			UPV->Flags &= ~UPV_SURFACE;

		if (!(UPV->Flags & UPV_SURFACE))
		{
			if (laraItem->HitPoints > 0)
			{
				lara->Air--;
				if (lara->Air < 0)
				{
					lara->Air = -1;
					laraItem->HitPoints -= 5;
				}
			}
		}
		else
		{
			if (laraItem->HitPoints >= 0)
			{
				lara->Air += 10;
				if (lara->Air > LARA_AIR_MAX)
					lara->Air = LARA_AIR_MAX;
			}
		}
	}

	TestTriggers(UPVItem, false);
	UPVEffects(lara->Vehicle);

	if (!(UPV->Flags & UPV_DEAD) &&
		lara->Vehicle != NO_ITEM)
	{
		DoCurrent(laraItem, UPVItem);

		if (TrInput & UPV_IN_FIRE &&
			UPV->Flags & UPV_CONTROL &&
			!UPV->HarpoonTimer)
		{
			if (laraItem->Animation.ActiveState != UPV_STATE_DISMOUNT_UNDERWATER &&
				laraItem->Animation.ActiveState != UPV_STATE_DISMOUNT_WATER_SURFACE &&
				laraItem->Animation.ActiveState != UPV_STATE_MOUNT)
			{
				FireUPVHarpoon(laraItem, UPVItem);
				UPV->HarpoonTimer = HARPOON_RELOAD;
			}
		}

		if (probe.RoomNumber != UPVItem->RoomNumber)
		{
			ItemNewRoom(lara->Vehicle, probe.RoomNumber);
			ItemNewRoom(lara->ItemNumber, probe.RoomNumber);
		}

		laraItem->Position.xPos = UPVItem->Position.xPos;
		laraItem->Position.yPos = UPVItem->Position.yPos;
		laraItem->Position.zPos = UPVItem->Position.zPos;
		laraItem->Position.xRot = UPVItem->Position.xRot;
		laraItem->Position.yRot = UPVItem->Position.yRot;
		laraItem->Position.zRot = UPVItem->Position.zRot;

		AnimateItem(laraItem);
		BackgroundCollision(laraItem, UPVItem);

		if (UPV->Flags & UPV_CONTROL)
			SoundEffect(SFX_TR3_UPV_LOOP, (PHD_3DPOS*)&UPVItem->Position.xPos, 2 | 4 | 0x1000000 | (UPVItem->Animation.Velocity * (USHRT_MAX + 1)));

		UPVItem->Animation.AnimNumber = Objects[ID_UPV].animIndex + (laraItem->Animation.AnimNumber - Objects[ID_UPV_LARA_ANIMS].animIndex);
		UPVItem->Animation.FrameNumber = g_Level.Anims[UPVItem->Animation.AnimNumber].frameBase + (laraItem->Animation.FrameNumber - g_Level.Anims[laraItem->Animation.AnimNumber].frameBase);

		if (UPV->Flags & UPV_SURFACE)
			Camera.targetElevation = -ANGLE(60.0f);
		else
			Camera.targetElevation = 0;

		return true;
	}
	else if (UPV->Flags & UPV_DEAD)
	{
		AnimateItem(laraItem);

		if (probe.RoomNumber != UPVItem->RoomNumber)
			ItemNewRoom(lara->Vehicle, probe.RoomNumber);

		BackgroundCollision(laraItem, UPVItem);

		UPV->XRot = 0;

		SetAnimation(UPVItem, UPV_ANIM_IDLE);
		UPVItem->Animation.VerticalVelocity = 0;
		UPVItem->Animation.Velocity = 0;
		UPVItem->Animation.Airborne = true;
		AnimateItem(UPVItem);

		return true;
	}
	else
		return false;
}
