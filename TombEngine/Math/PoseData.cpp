#include "framework.h"
#include "Math/PoseData.h"

#include "Math/Angles/Vector3s.h"
#include "Math/Vector3i.h"

//using namespace TEN::Math::Angles;

//namespace TEN::Math
//{
	PHD_3DPOS::PHD_3DPOS()
	{
		this->Position	  = Vector3Int::Zero;
		this->Orientation = Vector3Shrt::Zero;
	}

	PHD_3DPOS::PHD_3DPOS(Vector3Int pos)
	{
		this->Position = pos;
		this->Orientation = Vector3Shrt::Zero;
	}

	PHD_3DPOS::PHD_3DPOS(int xPos, int yPos, int zPos)
	{
		this->Position = Vector3Int(xPos, yPos, zPos);
		this->Orientation = Vector3Shrt::Zero;
	}

	PHD_3DPOS::PHD_3DPOS(Vector3Shrt orient)
	{
		this->Position = Vector3Int::Zero;
		this->Orientation = orient;
	}

	PHD_3DPOS::PHD_3DPOS(short xOrient, short yOrient, short zOrient)
	{
		this->Position = Vector3Int::Zero;
		this->Orientation = Vector3Shrt(xOrient, yOrient, zOrient);
	}

	PHD_3DPOS::PHD_3DPOS(Vector3Int pos, Vector3Shrt orient)
	{
		this->Position = pos;
		this->Orientation = orient;
	}

	PHD_3DPOS::PHD_3DPOS(Vector3Int pos, short xOrient, short yOrient, short zOrient)
	{
		this->Position = pos;
		this->Orientation = Vector3Shrt(xOrient, yOrient, zOrient);
	}

	PHD_3DPOS::PHD_3DPOS(int xPos, int yPos, int zPos, Vector3Shrt orient)
	{
		this->Position = Vector3Int(xPos, yPos, zPos);
		this->Orientation = orient;
	}

	PHD_3DPOS::PHD_3DPOS(int xPos, int yPos, int zPos, short xOrient, short yOrient, short zOrient)
	{
		this->Position = Vector3Int(xPos, yPos, zPos);
		this->Orientation = Vector3Shrt(xOrient, yOrient, zOrient);
	}
//}
