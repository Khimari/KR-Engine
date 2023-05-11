#include "framework.h"
#include "Objects/TR2/Entity/tr2_mercenary.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR2
{
	const auto MercenaryUziBite		   = CreatureBiteInfo(Vector3i(0, 200, 19), 17);
	const auto MercenaryAutoPistolBite = CreatureBiteInfo(Vector3i(0, 230, 9), 17);

	// TODO
	enum MercenaryState
	{

	};

	// TODO
	enum MercenaryAnim
	{

	};

	void MercenaryUziControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		auto extraHeadRot = EulerAngles::Zero;
		auto extraTorsoRot = EulerAngles::Zero;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != 13)
			{
				item->Animation.AnimNumber = 14;
				item->Animation.FrameNumber = GetAnimData(item).frameBase;
				item->Animation.ActiveState = 13;
			}
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			GetCreatureMood(item, &AI, false);
			CreatureMood(item, &AI, false);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case 1:
				if (AI.ahead)
				{
					extraHeadRot.x = AI.xAngle;
					extraHeadRot.y = AI.angle;
				}

				creature->MaxTurn = 0;

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = 2;
				}
				else if (Targetable(item, &AI))
				{
					if (AI.distance > pow(BLOCK(2), 2))
						item->Animation.TargetState = 2;

					if (GetRandomControl() >= 0x2000)
					{
						if (GetRandomControl() >= 0x4000)
						{
							item->Animation.TargetState = 11;
						}
						else
						{
							item->Animation.TargetState = 7;
						}
					}
					else
					{
						item->Animation.TargetState = 5;
					}
				}
				else
				{
					if (creature->Mood == MoodType::Attack)
					{
						item->Animation.TargetState = 3;
					}
					else if (!AI.ahead)
					{
						item->Animation.TargetState = 2;
					}
					else
					{
						item->Animation.TargetState = 1;
					}
				}

				break;

			case 2:
				creature->MaxTurn = ANGLE(7.0f);

				if (AI.ahead)
				{
					extraHeadRot.x = AI.xAngle;
					extraHeadRot.y = AI.angle;
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = 3;
				}
				else if (Targetable(item, &AI))
				{
					if (AI.distance <= pow(BLOCK(2), 2) || AI.zoneNumber != AI.enemyZone)
					{
						item->Animation.TargetState = 1;
					}
					else
					{
						item->Animation.TargetState = 12;
					}
				}
				else if (creature->Mood == MoodType::Attack)
				{
					item->Animation.TargetState = 3;
				}
				else
				{
					if (AI.ahead)
					{
						item->Animation.TargetState = 2;
					}
					else
					{
						item->Animation.TargetState = 1;
					}

				}

				break;

			case 3:
				tiltAngle = headingAngle / 3;
				creature->MaxTurn = ANGLE(10.0f);

				if (AI.ahead)
				{
					extraHeadRot.x = AI.xAngle;
					extraHeadRot.y = AI.angle;
				}

				if (creature->Mood != MoodType::Escape)
				{
					if (Targetable(item, &AI))
					{
						item->Animation.TargetState = 1;
					}
					else if (creature->Mood == MoodType::Bored)
					{
						item->Animation.TargetState = 2;
					}
				}

				break;

			case 5:
			case 7:
			case 8:
			case 9:
				creature->MaxTurn = 0;

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (item->Animation.FrameNumber == GetFrameIndex(item, 0))
				{
					if (!ShotLara(item, &AI, MercenaryUziBite, extraTorsoRot.y, 8))
						item->Animation.TargetState = 1;

					creature->MuzzleFlash[0].Bite = MercenaryUziBite;
					creature->MuzzleFlash[0].Delay = 2;
				}

				if (AI.distance < pow(BLOCK(2), 2))
					item->Animation.TargetState = 1;

				break;

			case 10:
			case 14:
				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (item->Animation.FrameNumber == GetFrameIndex(item, 0))
				{
					if (!ShotLara(item, &AI, MercenaryUziBite, extraTorsoRot.y, 8))
						item->Animation.TargetState = 1;

					creature->MuzzleFlash[0].Bite = MercenaryUziBite;
					creature->MuzzleFlash[0].Delay = 2;
				}

				if (AI.distance < pow(BLOCK(2), 2))
					item->Animation.TargetState = 2;

				break;
			}
		}

		CreatureTilt(item, tiltAngle);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, extraHeadRot.y);
		CreatureJoint(item, 3, extraHeadRot.x);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);
	}

	void MercenaryAutoPistolControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		auto extraHeadRot = EulerAngles::Zero;
		auto extraTorsoRot = EulerAngles::Zero;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != 11)
			{
				item->Animation.AnimNumber = 9;
				item->Animation.FrameNumber = GetAnimData(*item).frameBase;
				item->Animation.ActiveState = 11;
			}
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case 2:
				creature->MaxTurn = 0;

				if (AI.ahead)
				{
					extraHeadRot.x = AI.xAngle;
					extraHeadRot.y = AI.angle;
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = 4;
				}
				else if (Targetable(item, &AI))
				{
					if (AI.distance <= pow(BLOCK(2), 2))
					{
						if (GetRandomControl() >= 0x2000)
						{
							if (GetRandomControl() >= 0x4000)
							{
								item->Animation.TargetState = 5;
							}
							else
							{
								item->Animation.TargetState = 8;
							}
						}
						else
						{
							item->Animation.TargetState = 7;
						}
					}
					else
					{
						item->Animation.TargetState = 3;
					}
				}
				else
				{
					if (creature->Mood == MoodType::Attack)
						item->Animation.TargetState = 4;

					if (!AI.ahead || GetRandomControl() < 0x100)
						item->Animation.TargetState = 3;
				}

				break;

			case 3:
				creature->MaxTurn = ANGLE(7.0f);

				if (AI.ahead)
				{
					extraHeadRot.x = AI.xAngle;
					extraHeadRot.y = AI.angle;
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = 4;
				}
				else if (Targetable(item, &AI))
				{
					if (AI.distance < pow(BLOCK(2), 2) || AI.zoneNumber == AI.enemyZone || GetRandomControl() < 1024)
					{
						item->Animation.TargetState = 2;
					}
					else
					{
						item->Animation.TargetState = 1;
					}
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = 4;
				}
				else if (AI.ahead && GetRandomControl() < 1024)
				{
					item->Animation.TargetState = 2;
				}

				break;

			case 4:
				tilt = angle / 3;
				creature->MaxTurn = ANGLE(10.0f);

				if (AI.ahead)
				{
					extraHeadRot.x = AI.xAngle;
					extraHeadRot.y = AI.angle;
				}

				if (creature->Mood != MoodType::Escape && (creature->Mood == MoodType::Escape || Targetable(item, &AI)))
					item->Animation.TargetState = 2;

				break;

			case 1:
			case 5:
			case 6:
				creature->Flags = 0;

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				break;

			case 7:
			case 8:
			case 13:
				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;

					if (creature->Flags == 0)
					{
						if (GetRandomControl() < 0x2000)
							item->Animation.TargetState = 2;

						ShotLara(item, &AI, MercenaryAutoPistolBite, extraTorsoRot.y, 50);
						creature->MuzzleFlash[0].Bite = MercenaryAutoPistolBite;
						creature->MuzzleFlash[0].Delay = 2;
						creature->Flags = 1;
					}
				}
				else
				{
					item->Animation.TargetState = 2;
				}

				break;

			case 9:
				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;

					if (AI.distance < pow(BLOCK(2), 2))
						item->Animation.TargetState = 3;

					if (creature->Flags == 0)
					{
						if (!ShotLara(item, &AI, MercenaryAutoPistolBite, extraTorsoRot.y, 50))
							item->Animation.TargetState = 3;

						creature->MuzzleFlash[0].Bite = MercenaryAutoPistolBite;
						creature->MuzzleFlash[0].Delay = 2;
						creature->Flags = 1;
					}
				}
				else
				{
					item->Animation.TargetState = 3;
				}

				break;

			case 12:
				creature->Flags = 0;

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (Targetable(item, &AI))
				{
					item->Animation.TargetState = 13;
				}
				else
				{
					item->Animation.TargetState = 2;
				}

				break;

			case 10:
				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;

					if (AI.distance < pow(BLOCK(2), 2))
						item->Animation.TargetState = 3;

					if (creature->Flags != 2)
					{
						if (!ShotLara(item, &AI, MercenaryAutoPistolBite, extraTorsoRot.y, 50))
							item->Animation.TargetState = 3;

						creature->MuzzleFlash[0].Bite = MercenaryAutoPistolBite;
						creature->MuzzleFlash[0].Delay = 2;
						creature->Flags = 2;
					}
				}
				else
				{
					item->Animation.TargetState = 3;
				}

				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, extraHeadRot.y);
		CreatureJoint(item, 3, extraHeadRot.x);
		CreatureAnimation(itemNumber, angle, tilt);
	}
}
