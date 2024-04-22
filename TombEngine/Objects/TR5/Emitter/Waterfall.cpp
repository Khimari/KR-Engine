#include "framework.h"
#include "Objects/TR5/Emitter/Waterfall.h"

#include "Game/collision/collide_room.h"
#include "Game/effects/effects.h"
#include "Game/camera.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Utils/object_helper.h"
#include "Specific/clock.h"
#include "Renderer/Renderer.h"

using namespace TEN::Math;
using TEN::Renderer::g_Renderer;
	
const auto WATERFALL_MAX_LIFE = 100;
const auto MAX_INVERSE_SCALE_FACTOR = 50.0f;
const auto WATERFALL_MIST_START_COLOR = Color(0.6f, 0.6f, 0.6f);
const auto WATERFALL_MIST_END_COLOR = Color(0.6f, 0.6f, 0.6f);

std::vector<WaterfallParticle> WaterfallParticles = {};

	void InitializeWaterfall(short itemNumber)
	{

	}

	void WaterfallControl(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		static const int scale = 3;

		int size = 2;
		int width = 1;
		short angle = item.Pose.Orientation.y;

		if (item.TriggerFlags != 0)
		{
			width = std::clamp(int(round(item.TriggerFlags) * 100) / 2, 0, BLOCK(8));

			// Calculate the dynamic size based on TriggerFlags
			size = std::clamp(2 + (item.TriggerFlags) / 2, 15, 20);			
		}

		float cos = phd_cos(angle - ANGLE(90.0f));
		float sin = phd_sin(angle + ANGLE(90.0f));

		int maxPosX = width * sin + item.Pose.Position.x;
		int maxPosZ = width * cos + item.Pose.Position.z;
		int minPosX = -width * sin + item.Pose.Position.x;
		int minPosZ = -width * cos + item.Pose.Position.z;

		float fadeMin = GetParticleDistanceFade(Vector3i(minPosX, item.Pose.Position.y, minPosZ));
		float fadeMax = GetParticleDistanceFade(Vector3i(maxPosX, item.Pose.Position.y, maxPosZ));

		if ((fadeMin == 0.0f) && (fadeMin == fadeMax))
			return;

		float finalFade = ((fadeMin >= 1.0f) && (fadeMin == fadeMax)) ? 1.0f : std::max(fadeMin, fadeMax);

		auto startColor = item.Model.Color / 4.0f * finalFade * float(UCHAR_MAX);
		auto endColor = item.Model.Color / 8.0f * finalFade * float(UCHAR_MAX);

		// Calculate the inverse scale factor based on size
		float inverseScaleFactor = MAX_INVERSE_SCALE_FACTOR / size;

		// Adjust step by multiplying it with the inverse scale factor
		float step = size * scale * inverseScaleFactor;

		int currentStep = 0;

		while (true)
		{
			int offset = (step * currentStep) + Random::GenerateInt(-32, 32);

			if (offset > width)
				break;


				for (int sign = -1; sign <= 1; sign += 2)
				{
					//if (Wibble & 7)
					if (Random::GenerateInt(0, 100) > std::clamp((width / 100) * 3, 20,85)) //(width/100)*3)
					{

						auto* spark = GetFreeParticle();

						spark->on = true;
						spark->roomNumber = item.RoomNumber;
						spark->friction = -2;
						spark->xVel = (BLOCK(0.2f) * cos);
						spark->yVel = 16 - (GetRandomControl() & 0xF);
						spark->zVel = (BLOCK(0.2f) * sin);

						spark->x = offset * sign * sin + Random::GenerateInt(-8, 8) + item.Pose.Position.x;
						spark->y = Random::GenerateInt(0, 16) + item.Pose.Position.y - 8;
						spark->z = offset * sign * cos + Random::GenerateInt(-8, 8) + item.Pose.Position.z;

						auto orient = EulerAngles(item.Pose.Orientation.x - ANGLE(90.0f), item.Pose.Orientation.y, item.Pose.Orientation.z);
						auto orient2 = EulerAngles(item.Pose.Orientation.x, item.Pose.Orientation.y, item.Pose.Orientation.z);
						auto dir2 = orient2.ToDirection();
						auto dir = orient.ToDirection();
						auto rotMatrix = orient.ToRotationMatrix();

						auto origin = GameVector(Vector3(spark->x, spark->y, spark->z), item.RoomNumber);
						auto origin2 = Geometry::TranslatePoint(Vector3(spark->x, spark->y, spark->z), orient2, BLOCK(0.3));

						auto pointColl = GetCollision(origin2, origin.RoomNumber, dir, BLOCK(8));

						int relFloorHeight = pointColl.Position.Floor - spark->y;

						//g_Renderer.AddDebugLine(origin2, origin2 + Vector3(0, relFloorHeight, 0), Vector4(1, 0, 0, 1));

						spark->targetPos = GameVector(origin2.x, origin2.y + relFloorHeight, origin2.z, pointColl.RoomNumber);

						char colorOffset = (Random::GenerateInt(-8, 8));
						spark->sR = std::clamp(int(startColor.x) + colorOffset, 0, UCHAR_MAX);
						spark->sG = std::clamp(int(startColor.y) + colorOffset, 0, UCHAR_MAX);
						spark->sB = std::clamp(int(startColor.z) + colorOffset, 0, UCHAR_MAX);
						spark->dR = std::clamp(int(endColor.x) + colorOffset, 0, UCHAR_MAX);
						spark->dG = std::clamp(int(endColor.y) + colorOffset, 0, UCHAR_MAX);
						spark->dB = std::clamp(int(endColor.z) + colorOffset, 0, UCHAR_MAX);
						spark->roomNumber = pointColl.RoomNumber;
						spark->colFadeSpeed = 2;
						spark->blendMode = BlendMode::Additive;

						spark->gravity = (relFloorHeight / 2) / FPS; // Adjust gravity based on relative floor height
						spark->life = spark->sLife = WATERFALL_MAX_LIFE;
						spark->fxObj = ID_WATERFALL_EMITTER;
						spark->fadeToBlack = 0;

						spark->rotAng = GetRandomControl() & 0xFFF;
						spark->scalar = (size + Random::GenerateInt(-5, 5)) / 5;
						spark->maxYvel = 0;
						spark->rotAdd = Random::GenerateInt(-16, 16);

						spark->sSize = spark->size = Random::GenerateInt(3, 10) * scale + size * 2;//-5,5
						spark->dSize = (1 * spark->size);

						spark->spriteIndex = Objects[ID_DEFAULT_SPRITES].meshIndex + (Random::GenerateInt(0, 100) > 70 ? 34 : 0);
						spark->flags = SP_SCALE | SP_DEF | SP_ROTATE;
					}

					if (sign == 1)
					{
						currentStep++;
						if (currentStep == 1)
							break;
					}
				}
			
		}
	}

	void TriggerWaterfallEmitterMist(const Vector3& pos, short room)
	{
		static const int scale = Random::GenerateInt(5, 7);

		int size = 5;

		short angle = pos.y ;


		float cos = phd_cos(angle);
		float sin = phd_sin(angle);

		int maxPosX =  sin + pos.x;
		int maxPosZ =  cos + pos.z;
		int minPosX =  sin + pos.x;
		int minPosZ =  cos + pos.z;

		float fadeMin = GetParticleDistanceFade(Vector3i(minPosX, pos.y, minPosZ));
		float fadeMax = GetParticleDistanceFade(Vector3i(maxPosX, pos.y, maxPosZ));

		if ((fadeMin == 0.0f) && (fadeMin == fadeMax))
			return;

		float finalFade = ((fadeMin >= 1.0f) && (fadeMin == fadeMax)) ? 1.0f : std::max(fadeMin, fadeMax);

		auto startColor = WATERFALL_MIST_START_COLOR * finalFade * float(UCHAR_MAX);
		auto endColor = WATERFALL_MIST_START_COLOR * finalFade * float(UCHAR_MAX);

		
		auto* spark = GetFreeParticle();
		
		spark->on = true;
		spark->roomNumber = room;
		char colorOffset = (Random::GenerateInt(-8, 8));
		spark->sR =  std::clamp(int(startColor.x) + colorOffset, 0, UCHAR_MAX);
		spark->sG =  std::clamp(int(startColor.y) + colorOffset, 0, UCHAR_MAX);
		spark->sB = std::clamp(int(startColor.z) + colorOffset, 0, UCHAR_MAX);
		spark->dR = std::clamp(int(endColor.x) + colorOffset, 0, UCHAR_MAX);
		spark->dG = std::clamp(int(endColor.y) + colorOffset, 0, UCHAR_MAX);
		spark->dB = std::clamp(int(endColor.z) + colorOffset, 0, UCHAR_MAX);

		spark->colFadeSpeed = 1;
		spark->blendMode = BlendMode::Additive;
		spark->life =  spark->sLife = Random::GenerateInt(8, 12);
		spark->fadeToBlack = spark->life ;

		spark->x = cos * Random::GenerateInt(-12, 12) + pos.x;
		spark->y = Random::GenerateInt(0, 16) + pos.y - 8;
		spark->z = sin*  Random::GenerateInt(-12, 12) + pos.z;

		spark->xVel = 0;
		spark->yVel = -Random::GenerateInt(-64, 64);
		spark->zVel = 0;

		spark->friction = 0;
		spark->rotAng = GetRandomControl() & 0xFFF;
		spark->scalar = scale;
		spark->maxYvel = 0;
		spark->rotAdd = Random::GenerateInt(-16, 16);
		spark->gravity = spark->yVel;
		spark->sSize = spark->size = Random::GenerateInt(2, 5) * scale + size;
		spark->dSize = 2 * spark->size;

		spark->spriteIndex = Objects[ID_DEFAULT_SPRITES].meshIndex + (Random::GenerateInt(0, 100) > 70 ? 34 : 0);
		spark->flags = 538;
		
	}
