#include "framework.h"
#include "Objects/TR4/Entity/tr4_setha.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Specific/level.h"
#include "Math/Math.h"
#include "Specific/setup.h"

using namespace TEN::Effects::Items;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto SETH_KNEEL_ATTACK_DAMAGE = 200;
	constexpr auto SETH_GRAB_ATTACK_DAMAGE	= 250;

	// TODO: Rename all of this.
	constexpr auto SETH_IDLE_RANGE = SQUARE(BLOCK(4));
	constexpr auto SETH_WALK_RANGE = SQUARE(BLOCK(3));
	constexpr auto SETH_RECOIL_RANGE = SQUARE(BLOCK(2));
	constexpr auto SETH_GRAB_ATTACK_RANGE = SQUARE(BLOCK(1));
	constexpr auto SETH_SHOOT_RIGHT_LEFT_RANGE = SQUARE(BLOCK(2.5f));
	constexpr auto SETH_KNEEL_ATTACK_RANGE = SQUARE(BLOCK(3));
	constexpr auto SETH_BIG_PROJECTILE_RANGE = SQUARE(BLOCK(4));

	const auto SETH_WALK_TURN_RATE_MAX = ANGLE(7.0f);
	const auto SETH_RUN_TURN_RATE_MAX  = ANGLE(11.0f);

	const auto SethKneelAttackJoints1 = std::vector<unsigned int>{ 13, 14, 15 };
	const auto SethKneelAttackJoints2 = std::vector<unsigned int>{ 16, 17, 18 };

	const auto SethBite1 = BiteInfo(Vector3(0.0f, 220.0f, 50.0f), 17);
	const auto SethBite2 = BiteInfo(Vector3(0.0f, 220.0f, 50.0f), 13);
	const auto SethAttack1 = BiteInfo(Vector3(-16.0f, 200.0f, 32.0f), 13);
	const auto SethAttack2 = BiteInfo(Vector3(16.0f, 200.0f, 32.0f), 17);

	constexpr auto LARA_ANIM_SETH_DEATH = 14;

	enum SethState
	{
		// No state 0.
		SETH_STATE_IDLE = 1,
		SETH_STATE_WALK_FORWARD = 2,
		SETH_STATE_RUN_FORWARD = 3,
		SETH_STATE_KNEEL_ATTACK = 4,
		SETH_STATE_JUMP = 5,
		SETH_STATE_GET_SHOT = 6,
		SETH_STATE_GET_HEAVY_SHOT = 7,
		SETH_STATE_ATTACK_GRAB = 8,
		SETH_STATE_ATTACK_KILL_LARA = 9,
		SETH_STATE_REGENERATE = 10,
		SETH_STATE_SHOOT_RIGHT_AND_LEFT = 11,
		SETH_STATE_JUMP_AIR_SHOOT = 12,
		SETH_STATE_BIG_PROJECTILE = 13,
		SETH_STATE_FLY = 14,
		SETH_STATE_FLY_AIR_SHOOT = 15,
		SETH_STATE_FLY_GET_HEAVY_SHOT = 16
	};

	enum SethAnim
	{
		SETH_ANIM_SHOOT_RIGHT_AND_LEFT = 0,
		SETH_ANIM_PREPARE_JUMP = 1,
		SETH_ANIM_JUMP = 2,
		SETH_ANIM_JUMP_LANDING = 3,
		SETH_ANIM_JUMP_AIR_SHOOT = 4,
		SETH_ANIM_AIM_BIG_PROJECTILE = 5,
		SETH_ANIM_SHOOT_BIG_PROJECTILE = 6,
		SETH_ANIM_UNAIM_BIG_PROJECTILE = 7,
		SETH_ANIM_WALK_FORWARD = 8,
		SETH_ANIM_WALK_FORWARD_TO_RUN = 9,
		SETH_ANIM_RUN_FORWARD = 10,
		SETH_ANIM_IDLE_GET_SHOT = 11,
		SETH_ANIM_IDLE = 12,
		SETH_ANIM_ATTACK_GRAB = 13,
		SETH_ANIM_ATTACK_KILL_LARA = 14,
		SETH_ANIM_PREPARE_KNEEL_ATTACK = 15,
		SETH_ANIM_KNEEL_ATTACK = 16,
		SETH_ANIM_GET_HEAVY_SHOT_SOMERSAULT = 17,
		SETH_ANIM_GET_HEAVY_SHOT_SOMERSAULT_END = 18,
		SETH_ANIM_REGENERATE = 19,
		SETH_ANIM_WALK_FORWARD_TO_IDLE = 20,
		SETH_ANIM_IDLE_TO_WALK = 21,
		SETH_ANIM_RUN_FORWARD_TO_IDLE = 22,
		SETH_ANIM_RUN_FORWARD_TO_WALK =23,
		SETH_ANIM_FLY_AIR_SHOOT = 24,
		SETH_ANIM_FLY_GET_HEAVY_SHOT = 25,
		SETH_ANIM_IDLE_JUMP_TO_FLY = 26,
		SETH_ANIM_FLY_TO_LAND = 27,
		SETH_ANIM_FLY_IDLE = 28
	};

	void InitialiseSeth(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		ClearItem(itemNumber);
		InitialiseCreature(itemNumber);
		SetAnimation(&item, SETH_ANIM_IDLE);
	}

	void SethControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(item);

		int x = item->Pose.Position.x;
		int y = item->Pose.Position.y;
		int z = item->Pose.Position.z;

		int dx = 870 * phd_sin(item->Pose.Orientation.y);
		int dz = 870 * phd_cos(item->Pose.Orientation.y);

		int ceiling = GetCollision(x, y, z, item->RoomNumber).Position.Ceiling;

		x += dx;
		z += dz;
		int height1 = GetCollision(x, y, z, item->RoomNumber).Position.Floor;

		x += dx;
		z += dz;
		int height2 = GetCollision(x, y, z, item->RoomNumber).Position.Floor;

		x += dx;
		z += dz;
		int height3 = GetCollision(x, y, z, item->RoomNumber).Position.Floor;

		bool canJump = false;
		if ((y < (height1 - CLICK(1.5f)) || y < (height2 - CLICK(1.5f))) &&
			(y < (height3 + CLICK(1)) && y > (height3 - CLICK(1)) || height3 == NO_HEIGHT))
		{
			canJump = true;
		}

		x = item->Pose.Position.x - dx;
		z = item->Pose.Position.z - dz;
		int height4 = GetCollision(x, y, z, item->RoomNumber).Position.Floor;

		AI_INFO AI;
		short angle = 0;

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;
		}
		else
		{
			if (item->AIBits & AMBUSH)
				GetAITarget(&creature);
			else
				creature.Enemy = LaraItem;

			CreatureAIInfo(item, &AI);

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature.MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case SETH_STATE_IDLE:
				creature.LOT.IsJumping = false;
				creature.Flags = 0;

				if (item->Animation.RequiredState)
				{
					item->Animation.TargetState = item->Animation.RequiredState;
					break;
				}
				else if (AI.distance < SETH_GRAB_ATTACK_RANGE && AI.bite)
				{
					item->Animation.TargetState = SETH_STATE_ATTACK_GRAB;
					break;
				}
				else if (LaraItem->Pose.Position.y >= (item->Pose.Position.y - BLOCK(1)))
				{
					if (AI.distance < SETH_SHOOT_RIGHT_LEFT_RANGE && AI.ahead &&
						 Targetable(item, &AI) && Random::TestProbability(1 / 2.0f))
					{
						item->Animation.TargetState = SETH_STATE_SHOOT_RIGHT_AND_LEFT;
						item->ItemFlags[0] = 0;
						break;
					}
					else if (ceiling != NO_HEIGHT &&
						ceiling < (item->Pose.Position.y - BLOCK(7 / 8.0f)) &&
						height4 != NO_HEIGHT &&
						height4 > (item->Pose.Position.y - BLOCK(1)) &&
						Random::TestProbability(1 / 2.0f))
					{
						item->Pose.Position.y -= BLOCK(3 / 2.0f);
						if (Targetable(item, &AI))
						{
							item->Pose.Position.y += BLOCK(3 / 2.0f);
							item->Animation.TargetState = SETH_STATE_JUMP_AIR_SHOOT;
							item->ItemFlags[0] = 0;
						}
						else
						{
							item->Pose.Position.y += BLOCK(3 / 2.0f);
							item->Animation.TargetState = SETH_STATE_WALK_FORWARD;
						}

						break;
					}
					else
					{
						if (AI.distance < SETH_KNEEL_ATTACK_RANGE && AI.ahead &&
							AI.angle < ANGLE(33.75f) && AI.angle > ANGLE(-33.75f))
						{
							if (Targetable(item, &AI))
							{
								item->Animation.TargetState = SETH_STATE_KNEEL_ATTACK;
								break;
							}
						}
						else if (AI.distance < SETH_BIG_PROJECTILE_RANGE &&
							AI.angle < ANGLE(45.0f) && AI.angle > ANGLE(-45.0f) &&
							height4 != NO_HEIGHT &&
							height4 >= (item->Pose.Position.y - CLICK(1)) &&
							Targetable(item, &AI))
						{
							item->Animation.TargetState = SETH_STATE_BIG_PROJECTILE;
							item->ItemFlags[0] = 0;
							break;
						}
						else if (canJump)
						{
							item->Animation.TargetState = SETH_STATE_JUMP;
							break;
						}
					}
				}
				else
				{
					if (creature.ReachedGoal)
					{
						item->Animation.TargetState = SETH_STATE_FLY;
						break;
					}
					else
					{
						item->AIBits = AMBUSH;
						creature.HurtByLara = true;
					}
				}

				item->Animation.TargetState = SETH_STATE_WALK_FORWARD;
				break;

			case SETH_STATE_WALK_FORWARD:
				creature.MaxTurn = SETH_WALK_TURN_RATE_MAX;

				if ((AI.bite && AI.distance < SETH_IDLE_RANGE) ||
					canJump || creature.ReachedGoal)
				{
					item->Animation.TargetState = SETH_STATE_IDLE;
				}
				else if (AI.distance > SETH_WALK_RANGE)
				{
					item->Animation.TargetState = SETH_STATE_RUN_FORWARD;
				}
				
				break;

			case SETH_STATE_RUN_FORWARD:
				creature.MaxTurn = SETH_RUN_TURN_RATE_MAX;

				if ((AI.bite && AI.distance < SETH_IDLE_RANGE) ||
					canJump || creature.ReachedGoal)
				{
					item->Animation.TargetState = SETH_STATE_IDLE;
				}
				else if (AI.distance < SETH_WALK_RANGE)
				{
					item->Animation.TargetState = SETH_STATE_WALK_FORWARD;
				}
				
				break;

			case SETH_STATE_KNEEL_ATTACK:
				if (canJump)
				{
					if (item->Animation.AnimNumber == (Objects[item->ObjectNumber].animIndex + SETH_ANIM_PREPARE_KNEEL_ATTACK) &&
						item->Animation.FrameNumber == g_Level.Anims[item->Animation.AnimNumber].frameBase)
					{
						creature.MaxTurn = 0;
						creature.ReachedGoal = true;
					}
				}

				if (!creature.Flags)
				{
					if (item->TouchBits.TestAny())
					{
						if (item->Animation.AnimNumber == (Objects[item->ObjectNumber].animIndex + SETH_ANIM_KNEEL_ATTACK))
						{
							if (item->TouchBits.Test(SethKneelAttackJoints1))
							{
								DoDamage(creature.Enemy, SETH_KNEEL_ATTACK_DAMAGE);
								CreatureEffect2(item, SethBite1, 25, -1, DoBloodSplat);
								creature.Flags = 1; // Flag 1 = is attacking.
							}

							if (item->TouchBits.Test(SethKneelAttackJoints2))
							{
								DoDamage(creature.Enemy, SETH_KNEEL_ATTACK_DAMAGE);
								CreatureEffect2(item, SethBite2, 25, -1, DoBloodSplat);
								creature.Flags = 1; // Flag 1 = is attacking.
							}
						}
					}
				}

				break;

			case SETH_STATE_JUMP:
				creature.MaxTurn = 0;
				creature.ReachedGoal = true;
				break;

			case SETH_STATE_GET_HEAVY_SHOT:
				if (item->Animation.AnimNumber == (Objects[item->Animation.AnimNumber].animIndex + SETH_ANIM_GET_HEAVY_SHOT_SOMERSAULT) &&
					item->Animation.FrameNumber == g_Level.Anims[item->Animation.AnimNumber].frameEnd)
				{
					if (Random::TestProbability(1 / 2.0f))
						item->Animation.RequiredState = SETH_STATE_REGENERATE;
				}

				break;

			case SETH_STATE_ATTACK_GRAB:
				creature.MaxTurn = 0;

				if (abs(AI.angle) >= ANGLE(3.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(3.0f);
					else
						item->Pose.Orientation.y -= ANGLE(3.0f);
				}
				else
					item->Pose.Orientation.y += AI.angle;

				if (!creature.Flags)
				{
					if (item->TouchBits.TestAny())
					{
						if (item->Animation.FrameNumber > (g_Level.Anims[item->Animation.AnimNumber].frameBase + SETH_ANIM_PREPARE_KNEEL_ATTACK) &&
							item->Animation.FrameNumber < (g_Level.Anims[item->Animation.AnimNumber].frameBase + SETH_ANIM_IDLE_JUMP_TO_FLY))
						{
							DoDamage(creature.Enemy, SETH_GRAB_ATTACK_DAMAGE);
							CreatureEffect2(item, SethBite1, 25, -1, DoBloodSplat);
							creature.Flags = 1; // Flag 1 = is attacking.
						}
					}
				}

				if (LaraItem->HitPoints < 0)
				{
					SethKill(item, LaraItem);
					ItemCustomBurn(LaraItem, Vector3(0.0f, 0.8f, 0.1f), Vector3(0.0f, 0.9f, 0.8f), 6 * FPS);
					creature.MaxTurn = 0;
					return;
				}

				break;

			case SETH_STATE_SHOOT_RIGHT_AND_LEFT:
			case SETH_STATE_JUMP_AIR_SHOOT:
			case SETH_STATE_BIG_PROJECTILE:
			case SETH_STATE_FLY_AIR_SHOOT:
				creature.MaxTurn = 0;

				if (item->Animation.ActiveState == SETH_STATE_FLY_AIR_SHOOT)
					creature.Target.y = LaraItem->Pose.Position.y;

				if (abs(AI.angle) >= ANGLE(3.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(3.0f);
					else
						item->Pose.Orientation.y -= ANGLE(3.0f);
					
					SethAttack(itemNumber);
				}
				else
				{
					item->Pose.Orientation.y += AI.angle;
					SethAttack(itemNumber);
				}

				break;

			case SETH_STATE_FLY:
				if (item->Animation.AnimNumber != (Objects[item->Animation.AnimNumber].animIndex + SETH_ANIM_IDLE_JUMP_TO_FLY))
				{
					item->Animation.IsAirborne = false;
					creature.MaxTurn = 0;
					creature.Target.y = LaraItem->Pose.Position.y;
					creature.LOT.Fly = 16;

					if (abs(AI.angle) >= ANGLE(3.0f))
					{
						if (AI.angle >= 0)
							item->Pose.Orientation.y += ANGLE(3.0f);
						else
							item->Pose.Orientation.y -= ANGLE(3.0f);
					}
					else
						item->Pose.Orientation.y += AI.angle;
				}

				if (LaraItem->Pose.Position.y <= (item->Floor - BLOCK(1 / 2.0f)))
				{
					if (Targetable(item, &AI))
					{
						item->Animation.TargetState = SETH_STATE_FLY_AIR_SHOOT;
						item->ItemFlags[0] = 0;
					}
				}
				else
				{
					item->Animation.IsAirborne = true;
					creature.LOT.Fly = 0;

					if ((item->Pose.Position.y - item->Floor) > 0)
						item->Animation.TargetState = SETH_STATE_IDLE;
				}

				break;

			default:
				break;
			}
		}

		if (item->HitStatus)
		{
			if ((Lara.Control.Weapon.GunType == LaraWeaponType::Shotgun || Lara.Control.Weapon.GunType == LaraWeaponType::Revolver) &&
				AI.distance < SETH_RECOIL_RANGE && !(creature.LOT.IsJumping))
			{
				if (item->Animation.ActiveState != SETH_STATE_JUMP_AIR_SHOOT)
				{
					if (item->Animation.ActiveState <= SETH_STATE_BIG_PROJECTILE)
					{
						if (abs(height4 - item->Pose.Position.y) >= BLOCK(1 / 2.0f))
							SetAnimation(item, SETH_ANIM_IDLE_GET_SHOT);
						else
							SetAnimation(item, SETH_ANIM_GET_HEAVY_SHOT_SOMERSAULT);
					}
					else
						SetAnimation(item, SETH_ANIM_FLY_GET_HEAVY_SHOT);
				}
			}
		}

		CreatureAnimation(itemNumber, angle, 0);
	}

	void SethAttack(int itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		item->ItemFlags[0]++;

		auto pos1 = GetJointPosition(item, SethAttack1.meshNum, Vector3i(SethAttack1.Position));
		auto pos2 = GetJointPosition(item, SethAttack2.meshNum, Vector3i(SethAttack2.Position));

		int size;

		switch (item->Animation.ActiveState)
		{
		case SETH_STATE_SHOOT_RIGHT_AND_LEFT:
		case SETH_STATE_FLY_AIR_SHOOT:
			if (item->ItemFlags[0] < 78 && (GetRandomControl() & 0x1F) < item->ItemFlags[0])
			{
				// Spawn spark particles.
				for (int i = 0; i < 2; i++)
				{
					auto pos = Vector3(
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos1.x,
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos1.y,
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos1.z);
					auto velocity = Vector3(
						pos1.x - pos.x,
						pos1.y - pos.y,
						Random::GenerateInt(-BLOCK(1), BLOCK(1)));
					TriggerSethSparks1(pos, velocity);

					pos = Vector3(
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos2.x,
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos2.y,
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos2.z);
					velocity = Vector3(
						(pos2.x - pos.x) * 8,
						(pos2.y - pos.y) * 8,
						(Random::GenerateInt(-BLOCK(1), BLOCK(1))) * 8);
					TriggerSethSparks1(pos, velocity);
				}
			}

			size = item->ItemFlags[0] * 2;
			if (size > 128)
				size = 128;

			if ((Wibble & 0xF) == 8)
			{
				if (item->ItemFlags[0] < 127)
					TriggerSethSparks2(itemNumber, 2, size);
			}
			else if (!(Wibble & 0xF) && item->ItemFlags[0] < 103)
			{
				TriggerSethSparks2(itemNumber, 3, size);
			}

			if (item->ItemFlags[0] >= 96 && item->ItemFlags[0] <= 99)
			{
				auto pos = GetJointPosition(item, SethAttack1.meshNum, Vector3i(SethAttack1.Position.x, SethAttack1.Position.y * 2, SethAttack1.Position.z));
				auto orient = Geometry::GetOrientToPoint(pos1.ToVector3(), pos.ToVector3());
				auto attackPose = Pose(pos1, orient);
				SpawnSethProjectile(&attackPose, item->RoomNumber, 0);
			}
			else if (item->ItemFlags[0] >= 122 && item->ItemFlags[0] <= 125)
			{
				auto pos = GetJointPosition(item, SethAttack2.meshNum, Vector3i(SethAttack2.Position.x, SethAttack2.Position.y * 2, SethAttack2.Position.z));
				auto orient = Geometry::GetOrientToPoint(pos2.ToVector3(), pos.ToVector3());
				auto attackPose = Pose(pos2, orient);
				SpawnSethProjectile(&attackPose, item->RoomNumber, 0);
			}

			break;

		case SETH_STATE_JUMP_AIR_SHOOT:
			size = item->ItemFlags[0] * 4;
			if (size > 160)
				size = 160;

			if ((Wibble & 0xF) == 8)
			{
				if (item->ItemFlags[0] < 132)
					TriggerSethSparks2(itemNumber, 2, size);
			}
			else if (!(Wibble & 0xF) && item->ItemFlags[0] < 132)
			{
				TriggerSethSparks2(itemNumber, 3, size);
			}

			if (item->ItemFlags[0] >= 60 && item->ItemFlags[0] <= 74 ||
				item->ItemFlags[0] >= 112 && item->ItemFlags[0] <= 124)
			{
				if (Wibble & 4)
				{
					auto pos = GetJointPosition(item, SethAttack1.meshNum, Vector3i(SethAttack1.Position.x, SethAttack1.Position.y * 2, SethAttack1.Position.z));
					auto orient = Geometry::GetOrientToPoint(pos1.ToVector3(), pos.ToVector3());
					auto attackPose = Pose(pos1, orient);
					SpawnSethProjectile(&attackPose, item->RoomNumber, 0);

					pos = GetJointPosition(item, SethAttack2.meshNum, Vector3i(SethAttack2.Position.x, SethAttack2.Position.y * 2, SethAttack2.Position.z));
					orient = Geometry::GetOrientToPoint(pos2.ToVector3(), pos.ToVector3());
					attackPose = Pose(pos2, orient);
					SpawnSethProjectile(&attackPose, item->RoomNumber, 0);
				}
			}

			break;

		case SETH_STATE_BIG_PROJECTILE:
			if (item->ItemFlags[0] > 40 &&
				item->ItemFlags[0] < 100 &&
				(GetRandomControl() & 7) < item->ItemFlags[0] - 40)
			{
				// Spawn spark particles.
				for (int i = 0; i < 2; i++)
				{
					auto pos = Vector3(
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos1.x,
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos1.y,
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos1.z);
					auto velocity = Vector3(
						pos1.x - pos.x,
						pos1.y - pos.y,
						BLOCK(1) - Random::GenerateInt(0, BLOCK(2)));
					TriggerSethSparks1(pos, velocity);

					pos = Vector3(
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos2.x,
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos2.y,
						Random::GenerateInt(-BLOCK(1), BLOCK(1)) + pos2.z);
					velocity = Vector3(
						pos2.x - pos.x,
						pos2.y - pos.y,
						Random::GenerateInt(-BLOCK(1), BLOCK(1)));
					TriggerSethSparks1(pos, velocity);
				}
			}

			size = item->ItemFlags[0] * 2;
			if (size > 128)
				size = 128;

			if ((Wibble & 0xF) == 8)
			{
				if (item->ItemFlags[0] < 103)
					TriggerSethSparks2(itemNumber, 2, size);
			}
			else if (!(Wibble & 0xF) && item->ItemFlags[0] < 103)
			{
				TriggerSethSparks2(itemNumber, 3, size);
			}

			if (item->ItemFlags[0] == 102)
			{
				auto pos = GetJointPosition(item, SethAttack1.meshNum, Vector3i(SethAttack2.Position.x, SethAttack2.Position.y * 2, SethAttack2.Position.z));
				auto orient = Geometry::GetOrientToPoint(pos1.ToVector3(), pos.ToVector3());
				auto attackPose = Pose(pos1, orient);
				SpawnSethProjectile(&attackPose, item->RoomNumber, 1);
			}

			break;

		default:
			break;
		}
	}

	void SethKill(ItemInfo* item, ItemInfo* laraItem)
	{
		SetAnimation(item, SETH_ANIM_ATTACK_KILL_LARA);

		LaraItem->Animation.AnimNumber = Objects[ID_LARA_EXTRA_ANIMS].animIndex + LARA_ANIM_SETH_DEATH;
		LaraItem->Animation.FrameNumber = g_Level.Anims[LaraItem->Animation.AnimNumber].frameBase;
		LaraItem->Animation.ActiveState = SETH_STATE_BIG_PROJECTILE;
		LaraItem->Animation.TargetState = SETH_STATE_BIG_PROJECTILE;
		LaraItem->Animation.IsAirborne = false;
		LaraItem->Pose = Pose(item->Pose.Position, 0, item->Pose.Orientation.y, 0);

		if (item->RoomNumber != LaraItem->RoomNumber)
			ItemNewRoom(Lara.ItemNumber, item->RoomNumber);

		AnimateItem(LaraItem);
		Lara.ExtraAnim = 1;
		LaraItem->HitPoints = -1;
		Lara.HitDirection = -1;
		Lara.Air = -1;
		Lara.Control.HandStatus = HandStatus::Busy;
		Lara.Control.Weapon.GunType = LaraWeaponType::None;

		Camera.pos.RoomNumber = LaraItem->RoomNumber;
		Camera.type = CameraType::Chase;
		Camera.flags = CF_FOLLOW_CENTER;
		Camera.targetAngle = ANGLE(170.0f);
		Camera.targetElevation = ANGLE(-25.0f);
	}

	void TriggerSethSparks1(const Vector3& pos, const Vector3& velocity)
	{
		float distance2D = Vector2::Distance(
			Vector2(pos.x, pos.z),
			Vector2(LaraItem->Pose.Position.x, LaraItem->Pose.Position.z));
		if (distance2D > BLOCK(16))
			return;

		auto& spark = *GetFreeParticle();

		spark.on = true;
		spark.sR = 0;
		spark.sG = 0;
		spark.sB = 0;
		spark.dR = 64;
		spark.dG = Random::GenerateInt(128, 192);
		spark.dB = spark.dG + 32;
		spark.life = 16;
		spark.sLife = 16;
		spark.colFadeSpeed = 4;
		spark.blendMode = BLEND_MODES::BLENDMODE_ADDITIVE;
		spark.fadeToBlack = 4;
		spark.x = pos.x;
		spark.y = pos.y;
		spark.z = pos.z;
		spark.xVel = velocity.x;
		spark.yVel = velocity.y;
		spark.zVel = velocity.z;
		spark.friction = 34;
		spark.scalar = 1;
		spark.sSize = spark.size = Random::GenerateInt(4, 8);
		spark.maxYvel = 0;
		spark.gravity = 0;
		spark.dSize = Random::GenerateInt(1, 2);
		spark.flags = 0;
	}

	void TriggerSethSparks2(short itemNumber, char node, int size)
	{
		const auto& item = g_Level.Items[itemNumber];

		float distance2D = Vector2::Distance(
			Vector2(item.Pose.Position.x, item.Pose.Position.z),
			Vector2(LaraItem->Pose.Position.x, LaraItem->Pose.Position.z));
		if (distance2D > BLOCK(16))
			return;

		auto& spark = *GetFreeParticle();

		spark.on = true;
		spark.sR = 0;
		spark.sG = 0;
		spark.sB = 0;
		spark.dR = 0;
		spark.dG = Random::GenerateInt(32, 160);
		spark.dB = spark.dG + 64;
		spark.fadeToBlack = 8;
		spark.colFadeSpeed = (GetRandomControl() & 3) + 4;
		spark.blendMode = BLEND_MODES::BLENDMODE_ADDITIVE;
		spark.life = spark.sLife = (GetRandomControl() & 7) + 20;
		spark.x = (GetRandomControl() & 0xF) - 8;
		spark.y = 0;
		spark.z = (GetRandomControl() & 0xF) - 8;
		spark.xVel = GetRandomControl() - 128;
		spark.yVel = 0;
		spark.zVel = GetRandomControl() - 128;
		spark.friction = 5;
		spark.flags = SP_NODEATTACH | SP_EXPDEF | SP_ITEM | SP_ROTATE | SP_SCALE | SP_DEF;
		spark.rotAng = Random::GenerateAngle(0, ANGLE(22.5f));

		if (Random::TestProbability(1 / 2.0f))
			spark.rotAdd = -32 - (GetRandomControl() & 0x1F);
		else
			spark.rotAdd = (GetRandomControl() & 0x1F) + 32;

		spark.maxYvel = 0;
		spark.gravity = (GetRandomControl() & 0x1F) + 16;
		spark.fxObj = itemNumber;
		spark.nodeNumber = node;
		spark.scalar = 2;
		spark.sSize = spark.size = GetRandomControl() & 0xF + size;
		spark.dSize = spark.size / 16;
	}

	void SpawnSethProjectile(Pose* pose, int roomNumber, int flags)
	{
		short fxNumber = CreateNewEffect(roomNumber);
		if (fxNumber == -1)
			return;

		auto& fx = EffectList[fxNumber];

		fx.pos = Vector3i(
			pose->Position.x,
			pose->Position.y - Random::GenerateInt(-32, 32),
			pose->Position.z);
		fx.pos.Orientation = EulerAngles(
			pose->Orientation.x,
			pose->Orientation.y,
			0);
		fx.roomNumber = roomNumber;
		fx.counter = Random::GenerateInt() * 2 + ANGLE(-180.0f);
		fx.flag1 = flags;
		fx.objectNumber = ID_ENERGY_BUBBLES;
		fx.speed = (GetRandomControl() & 0x1F) - (flags == 1 ? 64 : 0) + 96;
		fx.frameNumber = Objects[ID_ENERGY_BUBBLES].meshIndex + 2 * flags;
	}
}
