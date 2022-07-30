#include "framework.h"
#include "Math/Vector3i.h"

Vector3Int const Vector3Int::Zero = Vector3Int(0, 0, 0);

Vector3Int::Vector3Int()
{
	*this = Vector3Int::Zero;
}

Vector3Int::Vector3Int(int x, int y, int z)
{
	this->x = x;
	this->y = y;
	this->z = z;
}

Vector3 Vector3Int::ToVector3()
{
	return Vector3(x, y, z);
}

bool Vector3Int::operator ==(Vector3Int vector)
{
	return (x == vector.x && y == vector.y && z == vector.z);
}

bool Vector3Int::operator !=(Vector3Int vector)
{
	return (x != vector.x || y != vector.y || z != vector.z);
}

Vector3Int Vector3Int::operator =(Vector3Int vector)
{
	this->x = vector.x;
	this->y = vector.y;
	this->z = vector.z;
	return *this;
}

Vector3Int Vector3Int::operator +(Vector3Int vector)
{
	return Vector3Int(x + vector.x, y + vector.y, z + vector.z);
}

Vector3Int Vector3Int::operator -(Vector3Int vector)
{
	return Vector3Int(x - vector.x, y - vector.y, z - vector.z);
}

Vector3Int Vector3Int::operator *(Vector3Int vector)
{
	return Vector3Int(x * vector.x, y * vector.y, z * vector.z);
}

Vector3Int Vector3Int::operator *(float value)
{
	return Vector3Int((int)round(x * value), (int)round(y * value), (int)round(z * value));
}

Vector3Int Vector3Int::operator /(float value)
{
	return Vector3Int((int)round(x / value), (int)round(y / value), (int)round(z / value));
}

Vector3Int& Vector3Int::operator +=(Vector3Int vector)
{
	*this = *this + vector;
	return *this;
}

Vector3Int& Vector3Int::operator -=(Vector3Int vector)
{
	*this = *this - vector;
	return *this;
}

Vector3Int& Vector3Int::operator *=(Vector3Int vector)
{
	*this = *this * vector;
	return *this;
}

Vector3Int& Vector3Int::operator *=(float value)
{
	*this = *this * value;
	return *this;
}

Vector3Int& Vector3Int::operator /=(float value)
{
	*this = *this / value;
	return *this;
}
