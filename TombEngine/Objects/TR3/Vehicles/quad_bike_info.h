#pragma once

namespace TEN::Entities::Vehicles
{
	struct QuadBikeInfo
	{
		short TurnRate		= 0;
		short FrontRot		= 0;
		short RearRot		= 0;
		short MomentumAngle = 0;
		short ExtraRotation = 0;

		float Velocity				= 0.0f;
		float LeftVerticalVelocity  = 0.0f;
		float RightVerticalVelocity = 0.0f;

		int Revs	   = 0;
		int EngineRevs = 0;
		int Pitch	   = 0;

		int	 SmokeStart	   = 0;
		bool CanStartDrift = false;
		bool DriftStarting = false;
		bool NoDismount	   = false;

		int Flags = 0;
	};
}
