#include "framework.h"
#include "Renderer/Renderer.h"

#include <execution>
#include <stack>
#include <tuple>

#include "Game/control/control.h"
#include "Game/Lara/lara_struct.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/objects.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Specific/level.h"

using namespace TEN::Renderer::Graphics;

namespace TEN::Renderer
{
	bool Renderer::PrepareDataForTheRenderer()
	{
		lastBlendMode = BlendMode::Unknown;
		lastCullMode = CULL_MODE_UNSET;
		lastDepthState = DEPTH_STATE_UNSET;

		moveableObjects.resize(ID_NUMBER_OBJECTS);
		spriteSequences.resize(ID_NUMBER_OBJECTS);
		staticObjects.resize(MAX_STATICS);
		rooms.resize(g_Level.Rooms.size());

		meshes.clear();

		TENLog("Allocated renderer object memory.", LogLevel::Info);

		animatedTextures.resize(g_Level.AnimatedTextures.size());
		for (int i = 0; i < g_Level.AnimatedTextures.size(); i++)
		{
			TEXTURE* texture = &g_Level.AnimatedTextures[i];
			Texture2D normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultNormalTexture();
			}
			else
			{
				normal = Texture2D(device.Get(), texture->normalMapData.data(), (int)texture->normalMapData.size());
			}

			TexturePair tex = std::make_tuple(Texture2D(device.Get(), texture->colorMapData.data(), (int)texture->colorMapData.size()), normal);
			animatedTextures[i] = tex;
		}

		if (animatedTextures.size() > 0)
			TENLog("Generated " + std::to_string(animatedTextures.size()) + " animated textures.", LogLevel::Info);

		std::transform(g_Level.AnimatedTexturesSequences.begin(), g_Level.AnimatedTexturesSequences.end(), std::back_inserter(animatedTextureSets), [](ANIMATED_TEXTURES_SEQUENCE& sequence) {
			RendererAnimatedTextureSet set{};
			set.NumTextures = sequence.numFrames;
			std::transform(sequence.frames.begin(), sequence.frames.end(), std::back_inserter(set.Textures), [](ANIMATED_TEXTURES_FRAME& frm) {
				RendererAnimatedTexture tex{};
				tex.UV[0].x = frm.x1;
				tex.UV[0].y = frm.y1;
				tex.UV[1].x = frm.x2;
				tex.UV[1].y = frm.y2;
				tex.UV[2].x = frm.x3;
				tex.UV[2].y = frm.y3;
				tex.UV[3].x = frm.x4;
				tex.UV[3].y = frm.y4;
				return tex;
			});
			set.Fps = sequence.Fps;
			return set;
		});

		if (animatedTextureSets.size() > 0)
			TENLog("Generated " + std::to_string(animatedTextureSets.size()) + " animated texture sets.", LogLevel::Info);

		roomTextures.resize(g_Level.RoomTextures.size());
		for (int i = 0; i < g_Level.RoomTextures.size(); i++)
		{
			TEXTURE *texture = &g_Level.RoomTextures[i];
			Texture2D normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultNormalTexture();
			}
			else
			{
				normal = Texture2D(device.Get(), texture->normalMapData.data(), (int)texture->normalMapData.size());
			}

			TexturePair tex = std::make_tuple(Texture2D(device.Get(), texture->colorMapData.data(), (int)texture->colorMapData.size()), normal);
			roomTextures[i] = tex;

#ifdef DUMP_TEXTURES
			char filename[255];
			sprintf(filename, "dump\\room_%d.png", i);

			std::ofstream outfile(filename, std::ios::out | std::ios::binary);
			outfile.write(reinterpret_cast<const char*>(texture->colorMapData.data()), texture->colorMapData.size());
#endif
		}

		if (roomTextures.size() > 0)
			TENLog("Generated " + std::to_string(roomTextures.size()) + " room texture atlases.", LogLevel::Info);

		moveablesTextures.resize(g_Level.MoveablesTextures.size());
		for (int i = 0; i < g_Level.MoveablesTextures.size(); i++)
		{
			TEXTURE *texture = &g_Level.MoveablesTextures[i];
			Texture2D normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultNormalTexture();
			}
			else
			{
				normal = Texture2D(device.Get(), texture->normalMapData.data(), (int)texture->normalMapData.size());
			}

			TexturePair tex = std::make_tuple(Texture2D(device.Get(), texture->colorMapData.data(), (int)texture->colorMapData.size()), normal);
			moveablesTextures[i] = tex;

#ifdef DUMP_TEXTURES
			char filename[255];
			sprintf(filename, "dump\\moveable_%d.png", i);

			std::ofstream outfile(filename, std::ios::out | std::ios::binary);
			outfile.write(reinterpret_cast<const char*>(texture->colorMapData.data()), texture->colorMapData.size());
#endif
		}

		if (moveablesTextures.size() > 0)
			TENLog("Generated " + std::to_string(moveablesTextures.size()) + " moveable texture atlases.", LogLevel::Info);

		staticTextures.resize(g_Level.StaticsTextures.size());
		for (int i = 0; i < g_Level.StaticsTextures.size(); i++)
		{
			TEXTURE *texture = &g_Level.StaticsTextures[i];
			Texture2D normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultNormalTexture();
			}
			else
			{
				normal = Texture2D(device.Get(), texture->normalMapData.data(), (int)texture->normalMapData.size());
			}

			TexturePair tex = std::make_tuple(Texture2D(device.Get(), texture->colorMapData.data(), (int)texture->colorMapData.size()), normal);
			staticTextures[i] = tex;

#ifdef DUMP_TEXTURES
			char filename[255];
			sprintf(filename, "dump\\static_%d.png", i);

			std::ofstream outfile(filename, std::ios::out | std::ios::binary);
			outfile.write(reinterpret_cast<const char*>(texture->colorMapData.data()), texture->colorMapData.size());
#endif
		}

		if (staticTextures.size() > 0)
			TENLog("Generated " + std::to_string(staticTextures.size()) + " static mesh texture atlases.", LogLevel::Info);

		spritesTextures.resize(g_Level.SpritesTextures.size());
		for (int i = 0; i < g_Level.SpritesTextures.size(); i++)
		{
			auto& texture = g_Level.SpritesTextures[i];
			spritesTextures[i] = Texture2D(device.Get(), texture.colorMapData.data(), (int)texture.colorMapData.size());
		}

		if (spritesTextures.size() > 0)
			TENLog("Generated " + std::to_string((int)spritesTextures.size()) + " sprite atlases.", LogLevel::Info);

		skyTexture = Texture2D(device.Get(), g_Level.SkyTexture.colorMapData.data(), (int)g_Level.SkyTexture.colorMapData.size());

		TENLog("Loaded sky texture.", LogLevel::Info);

		int totalVertices = 0;
		int totalIndices = 0;
		for (auto& room : g_Level.Rooms)
			for (auto& bucket : room.buckets)
			{ 
				totalVertices += bucket.numQuads * 4 + bucket.numTriangles * 3;
				totalIndices += bucket.numQuads * 6 + bucket.numTriangles * 3;
			}

		if (!totalVertices || !totalIndices)
			throw std::exception("Level has no textured room geometry.");

		roomsVertices.resize(totalVertices);
		roomsIndices.resize(totalIndices);

		TENLog("Loaded total " + std::to_string(totalVertices) + " room vertices.", LogLevel::Info);

		int lastVertex = 0;
		int lastIndex = 0;

		TENLog("Preparing room data...", LogLevel::Info);

		for (int i = 0; i < g_Level.Rooms.size(); i++)
		{
			ROOM_INFO& room = g_Level.Rooms[i];

			RendererRoom* r = &rooms[i];

			r->RoomNumber = i;
			r->AmbientLight = Vector4(room.ambient.x, room.ambient.y, room.ambient.z, 1.0f);
			r->ItemsToDraw.reserve(MAX_ITEMS_DRAW);
			r->EffectsToDraw.reserve(MAX_ITEMS_DRAW);
			r->TransparentFacesToDraw.reserve(MAX_TRANSPARENT_FACES_PER_ROOM);

			Vector3 boxMin = Vector3(room.x + BLOCK(1), room.maxceiling - CLICK(1), room.z + BLOCK(1));
			Vector3 boxMax = Vector3(room.x + (room.xSize - 1) * BLOCK(1), room.minfloor + CLICK(1), room.z + (room.zSize - 1) * BLOCK(1));
			Vector3 center = (boxMin + boxMax) / 2.0f;
			Vector3 extents = boxMax - center;
			r->BoundingBox = BoundingBox(center, extents);

			r->Neighbors.clear();
			for (int j : room.neighbors)
				if (g_Level.Rooms[j].Active())
					r->Neighbors.push_back(j);

			if (room.doors.size() != 0)
			{
				r->Doors.resize((int)room.doors.size());

				for (int l = 0; l < room.doors.size(); l++)
				{
					RendererDoor* door = &r->Doors[l];
					ROOM_DOOR* oldDoor = &room.doors[l];

					door->RoomNumber = oldDoor->room;
					door->Normal = oldDoor->normal;

					for (int k = 0; k < 4; k++)
					{
						door->AbsoluteVertices[k] = Vector4(
							room.x + oldDoor->vertices[k].x,
							room.y + oldDoor->vertices[k].y,
							room.z + oldDoor->vertices[k].z,
							1.0f);
					}
				}
			}

			if (room.mesh.size() != 0)
			{
				r->Statics.resize(room.mesh.size());

				for (int l = 0; l < (int)room.mesh.size(); l++)
				{
					RendererStatic* staticInfo = &r->Statics[l];
					MESH_INFO* oldMesh = &room.mesh[l];

					oldMesh->Dirty = true;

					staticInfo->ObjectNumber = oldMesh->staticNumber;
					staticInfo->RoomNumber = oldMesh->roomNumber;
					staticInfo->Color = oldMesh->color;
					staticInfo->AmbientLight = r->AmbientLight;
					staticInfo->Pose = oldMesh->pos;
					staticInfo->Scale = oldMesh->scale;
					staticInfo->OriginalVisibilityBox = StaticObjects[staticInfo->ObjectNumber].visibilityBox;
					staticInfo->IndexInRoom = l;

					staticInfo->Update();
				}
			}

			if (room.positions.empty())
				continue;
			
			for (auto& levelBucket : room.buckets)
			{
				RendererBucket bucket{};

				bucket.Animated = levelBucket.animated;
				bucket.BlendMode = static_cast<BlendMode>(levelBucket.blendMode);
				bucket.Texture = levelBucket.texture;
				bucket.StartVertex = lastVertex;
				bucket.StartIndex = lastIndex;
				bucket.NumVertices += levelBucket.numQuads * 4 + levelBucket.numTriangles * 3;
				bucket.NumIndices += levelBucket.numQuads * 6 + levelBucket.numTriangles * 3;
				bucket.Centre = Vector3::Zero;

				for (auto& poly : levelBucket.polygons)
				{
					RendererPolygon newPoly;

					newPoly.Shape = poly.shape;

					newPoly.Centre = (
						room.positions[poly.indices[0]] +
						room.positions[poly.indices[1]] +
						room.positions[poly.indices[2]]) / 3.0f;

					Vector3 p1 = room.positions[poly.indices[0]];
					Vector3 p2 = room.positions[poly.indices[1]];
					Vector3 p3 = room.positions[poly.indices[2]];

					Vector3 n = (p2 - p1).Cross(p3 - p1);
					n.Normalize();

					newPoly.Normal = n;
					
					int baseVertices = lastVertex;
					for (int k = 0; k < poly.indices.size(); k++)
					{
						Vertex* vertex = &roomsVertices[lastVertex];
						int index = poly.indices[k];

						vertex->Position.x = room.x + room.positions[index].x;
						vertex->Position.y = room.y + room.positions[index].y;
						vertex->Position.z = room.z + room.positions[index].z;

						bucket.Centre += vertex->Position;

						vertex->Normal = poly.normals[k];
						vertex->UV = poly.textureCoordinates[k];
						vertex->Color = Vector4(room.colors[index].x, room.colors[index].y, room.colors[index].z, 1.0f);
						vertex->Tangent = poly.tangents[k];
						vertex->Binormal = poly.binormals[k];
						vertex->AnimationFrameOffset = poly.animatedFrame;
						vertex->IndexInPoly = k;
						vertex->OriginalIndex = index;
						vertex->Effects = Vector4(room.effects[index].x, room.effects[index].y, room.effects[index].z, 0);

						const unsigned long long primes[]{ 73856093ULL, 19349663ULL, 83492791ULL };
						vertex->Hash = (unsigned int)std::hash<float>{}
						((vertex->Position.x)* primes[0]) ^
							((unsigned int)std::hash<float>{}(vertex->Position.y) * primes[1]) ^
							(unsigned int)std::hash<float>{}(vertex->Position.z) * primes[2];
						vertex->Bone = 0;

						lastVertex++;
					}

					if (poly.shape == 0)
					{
						newPoly.BaseIndex = lastIndex;

						roomsIndices[lastIndex + 0] = baseVertices + 0;
						roomsIndices[lastIndex + 1] = baseVertices + 1;
						roomsIndices[lastIndex + 2] = baseVertices + 3;
						roomsIndices[lastIndex + 3] = baseVertices + 2;
						roomsIndices[lastIndex + 4] = baseVertices + 3;
						roomsIndices[lastIndex + 5] = baseVertices + 1;

						lastIndex += 6;
					}
					else
					{
						newPoly.BaseIndex = lastIndex;
 
						roomsIndices[lastIndex + 0] = baseVertices + 0;
						roomsIndices[lastIndex + 1] = baseVertices + 1;
						roomsIndices[lastIndex + 2] = baseVertices + 2;

						lastIndex += 3;
					}

					bucket.Polygons.push_back(newPoly);
				}

				bucket.Centre /= bucket.NumIndices;

				r->Buckets.push_back(bucket);		
			}

			if (room.lights.size() != 0)
			{
				r->Lights.resize(room.lights.size());

				for (int l = 0; l < room.lights.size(); l++)
				{
					RendererLight* light = &r->Lights[l];
					ROOM_LIGHT* oldLight = &room.lights[l];

					if (oldLight->type == LIGHT_TYPES::LIGHT_TYPE_SUN)
					{
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->Direction = Vector3(oldLight->dx, oldLight->dy, oldLight->dz);
						light->CastShadows = oldLight->castShadows;
						light->Type = LIGHT_TYPES::LIGHT_TYPE_SUN;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == LIGHT_TYPE_POINT)
					{
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->In = oldLight->in;
						light->Out = oldLight->out;
						light->CastShadows = oldLight->castShadows;
						light->Type = LIGHT_TYPE_POINT;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == LIGHT_TYPE_SHADOW)
					{
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->In = oldLight->in;
						light->Out = oldLight->out;
						light->CastShadows = false;
						light->Type = LIGHT_TYPE_SHADOW;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == LIGHT_TYPE_SPOT)
					{
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->Direction = Vector3(oldLight->dx, oldLight->dy, oldLight->dz);
						light->In = oldLight->length;     
						light->Out = oldLight->cutoff;
						light->InRange = oldLight->in;
						light->OutRange = oldLight->out;
						light->CastShadows = oldLight->castShadows;
						light->Type = LIGHT_TYPE_SPOT;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == LIGHT_TYPE_FOG_BULB)
					{  
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b);
						light->Intensity = oldLight->intensity;
						light->In = oldLight->in;
						light->Out = oldLight->out;
						light->Type = LIGHT_TYPE_FOG_BULB;
						light->Luma = Luma(light->Color);
					} 

					// Monty's temp variables for sorting
					light->LocalIntensity = 0;
					light->Distance = 0;
					light->RoomNumber = i;
					light->AffectNeighbourRooms = light->Type != LIGHT_TYPES::LIGHT_TYPE_SUN;

					oldLight++;
				}
			}
		}
		roomsVertexBuffer = VertexBuffer(device.Get(), (int)roomsVertices.size(), roomsVertices.data());
		roomsIndexBuffer = IndexBuffer(device.Get(), (int)roomsIndices.size(), roomsIndices.data());

		std::for_each(std::execution::par_unseq,
			rooms.begin(),
			rooms.end(),
			[](RendererRoom& room)
			{
				std::sort(
					room.Buckets.begin(),
					room.Buckets.end(),
					[](RendererBucket& a, RendererBucket& b)
					{
						if (a.BlendMode == b.BlendMode)
							return (a.Texture < b.Texture);
						else
							return (a.BlendMode < b.BlendMode);
					}
				);
			}
		);

		TENLog("Preparing object data...", LogLevel::Info);
			 
		bool isSkinPresent = false;

		totalVertices = 0;
		totalIndices = 0;
		for (int i = 0; i < MoveablesIds.size(); i++)
		{
			int objNum = MoveablesIds[i];
			ObjectInfo* obj = &Objects[objNum];

			for (int j = 0; j < obj->nmeshes; j++)
			{
				MESH* mesh = &g_Level.Meshes[obj->meshIndex + j];

				for (auto& bucket : mesh->buckets)
				{
					totalVertices += bucket.numQuads * 4 + bucket.numTriangles * 3;
					totalIndices += bucket.numQuads * 6 + bucket.numTriangles * 3;
				}
			}
		}
		moveablesVertices.resize(totalVertices);
		moveablesIndices.resize(totalIndices);

		lastVertex = 0;
		lastIndex = 0;
		for (int i = 0; i < MoveablesIds.size(); i++)
		{
			int objNum = MoveablesIds[i];
			ObjectInfo *obj = &Objects[objNum];

			if (obj->nmeshes > 0)
			{
				moveableObjects[MoveablesIds[i]] = RendererObject();
				RendererObject &moveable = *moveableObjects[MoveablesIds[i]];
				moveable.Id = MoveablesIds[i];
				moveable.DoNotDraw = (obj->drawRoutine == nullptr);
				moveable.ShadowType = obj->shadowType;
													   
				for (int j = 0; j < obj->nmeshes; j++)
				{              
					// HACK: mesh pointer 0 is the placeholder for Lara's body parts and is right hand with pistols
					// We need to override the bone index because the engine will take mesh 0 while drawing pistols anim,
					// and vertices have bone index 0 and not 10.
					RendererMesh *mesh = GetRendererMeshFromTrMesh(
						&moveable,
						&g_Level.Meshes[obj->meshIndex + j],
						j, MoveablesIds[i] == ID_LARA_SKIN_JOINTS,
						MoveablesIds[i] == ID_HAIR, &lastVertex, &lastIndex);

					moveable.ObjectMeshes.push_back(mesh);
					meshes.push_back(mesh);
				}

				if (objNum == ID_IMP_ROCK || objNum == ID_ENERGY_BUBBLES || objNum == ID_BUBBLES || objNum == ID_BODY_PART)
				{
					// HACK: these objects must have nmeshes = 0 because engine will use them in a different way while drawing Effects.
					// In Core's code this was done in SETUP.C but we must do it here because we need to create renderer's meshes.
					obj->nmeshes = 0;
				}
				else
				{
					for (int j = 0; j < obj->nmeshes; j++)
					{
						moveable.LinearizedBones.push_back(new RendererBone(j));
						moveable.AnimationTransforms.push_back(Matrix::Identity);
						moveable.BindPoseTransforms.push_back(Matrix::Identity);
					}

					if (obj->nmeshes > 1)
					{
						int *bone = &g_Level.Bones[obj->boneIndex];

						std::stack<RendererBone *> stack;

						RendererBone *currentBone = moveable.LinearizedBones[0];
						RendererBone *stackBone = moveable.LinearizedBones[0];

						for (int mi = 0; mi < obj->nmeshes - 1; mi++)
						{
							int j = mi + 1;

							int opcode = *(bone++);
							int linkX = *(bone++);
							int linkY = *(bone++);
							int linkZ = *(bone++);

							byte flags = opcode & 0x1C;

							moveable.LinearizedBones[j]->ExtraRotationFlags = flags;

							switch (opcode & 0x03)
							{
							case 0:
								moveable.LinearizedBones[j]->Parent = currentBone;
								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								currentBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								break;

							case 1:
								if (stack.empty())
									continue;

								currentBone = stack.top();
								stack.pop();

								moveable.LinearizedBones[j]->Parent = currentBone;
								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								currentBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								break;

							case 2:
								stack.push(currentBone);

								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								moveable.LinearizedBones[j]->Parent = currentBone;
								currentBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								break;

							case 3:
								if (stack.empty())
									continue;

								RendererBone *theBone = stack.top();
								stack.pop();

								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								moveable.LinearizedBones[j]->Parent = theBone;
								theBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								stack.push(theBone);
								break;
							}
						}
					}

					for (int n = 0; n < obj->nmeshes; n++)
					{
						moveable.LinearizedBones[n]->Transform = Matrix::CreateTranslation(
							moveable.LinearizedBones[n]->Translation.x,
							moveable.LinearizedBones[n]->Translation.y,
							moveable.LinearizedBones[n]->Translation.z);
					}

					moveable.Skeleton = moveable.LinearizedBones[0];
					BuildHierarchy(&moveable);

					// Fix player skin joints and hair units.
					if (MoveablesIds[i] == ID_LARA_SKIN_JOINTS)
					{
						isSkinPresent = true;
						int bonesToCheck[2] = { 0, 0 };

						RendererObject& objSkin = GetRendererObject(GAME_OBJECT_ID::ID_LARA_SKIN);

						for (int j = 1; j < obj->nmeshes; j++)
						{
							RendererMesh *jointMesh = moveable.ObjectMeshes[j];
							RendererBone *jointBone = moveable.LinearizedBones[j];

							bonesToCheck[0] = jointBone->Parent->Index;
							bonesToCheck[1] = j;

							for (int b1 = 0; b1 < jointMesh->Buckets.size(); b1++)
							{
								RendererBucket *jointBucket = &jointMesh->Buckets[b1];

								for (int v1 = 0; v1 < jointBucket->NumVertices; v1++)
								{
									Vertex *jointVertex = &moveablesVertices[jointBucket->StartVertex + v1];

									bool isDone = false;

									for (int k = 0; k < 2; k++)
									{
										RendererMesh *skinMesh = objSkin.ObjectMeshes[bonesToCheck[k]];
										RendererBone *skinBone = objSkin.LinearizedBones[bonesToCheck[k]];

										for (int b2 = 0; b2 < skinMesh->Buckets.size(); b2++)
										{
											RendererBucket *skinBucket = &skinMesh->Buckets[b2];
											for (int v2 = 0; v2 < skinBucket->NumVertices; v2++)
											{
												Vertex *skinVertex = &moveablesVertices[skinBucket->StartVertex + v2];

												int x1 = moveablesVertices[jointBucket->StartVertex + v1].Position.x + jointBone->GlobalTranslation.x;
												int y1 = moveablesVertices[jointBucket->StartVertex + v1].Position.y + jointBone->GlobalTranslation.y;
												int z1 = moveablesVertices[jointBucket->StartVertex + v1].Position.z + jointBone->GlobalTranslation.z;

												int x2 = moveablesVertices[skinBucket->StartVertex + v2].Position.x + skinBone->GlobalTranslation.x;
												int y2 = moveablesVertices[skinBucket->StartVertex + v2].Position.y + skinBone->GlobalTranslation.y;
												int z2 = moveablesVertices[skinBucket->StartVertex + v2].Position.z + skinBone->GlobalTranslation.z;


												if (abs(x1 - x2) < 2 && abs(y1 - y2) < 2 && abs(z1 - z2) < 2)
												{
													jointVertex->Bone = bonesToCheck[k];
													jointVertex->Position = skinVertex->Position;
													jointVertex->Normal = skinVertex->Normal;

													isDone = true;
													break;
												}
											}

											if (isDone)
												break;
										}

										if (isDone)
											break;
									}
								}
							}
						}
					}
					else if (MoveablesIds[i] == ID_HAIR && isSkinPresent)
					{
						for (int j = 0; j < obj->nmeshes; j++)
						{
							auto* currentMesh = moveable.ObjectMeshes[j];
							auto* currentBone = moveable.LinearizedBones[j];

							for (const auto& currentBucket : currentMesh->Buckets)
							{
								for (int v1 = 0; v1 < currentBucket.NumVertices; v1++)
								{
									auto* currentVertex = &moveablesVertices[currentBucket.StartVertex + v1];
									currentVertex->Bone = j + 1;

									// Link mesh 0 to head.
									if (j == 0)
									{
										bool isYoung = (g_GameFlow->GetLevel(CurrentLevel)->GetLaraType() == LaraType::Young);

										// HACK: Hardcoded hair base parent vertices.
										int parentVertices0[] = { 37, 39, 40, 38 }; // Single braid.
										int parentVertices1[] = { 79, 78, 76, 77 }; // Left pigtail.
										int parentVertices2[] = { 68, 69, 70, 71 }; // Right pigtail.

										auto& skinObj = GetRendererObject(GAME_OBJECT_ID::ID_LARA_SKIN);
										auto* parentMesh = skinObj.ObjectMeshes[LM_HEAD];
										auto* parentBone = skinObj.LinearizedBones[LM_HEAD];

										// Link first 4 vertices.
										if (currentVertex->OriginalIndex < 4)
										{
											for (int b2 = 0; b2 < parentMesh->Buckets.size(); b2++)
											{
												auto* parentBucket = &parentMesh->Buckets[b2];
												for (int v2 = 0; v2 < parentBucket->NumVertices; v2++)
												{
													auto* parentVertex = &moveablesVertices[parentBucket->StartVertex + v2];
													
													// TODO
													if (isYoung)
													{
														if (parentVertex->OriginalIndex == parentVertices1[currentVertex->OriginalIndex])
														{
															currentVertex->Bone = 0;
															currentVertex->Position = parentVertex->Position;
															currentVertex->Normal = parentVertex->Normal;
														}
													}
													else
													{
														if (parentVertex->OriginalIndex == parentVertices0[currentVertex->OriginalIndex])
														{
															currentVertex->Bone = 0;
															currentVertex->Position = parentVertex->Position;
															currentVertex->Normal = parentVertex->Normal;
														}
													}
												}
											}
										}
									}
									// Link meshes > 0 to parent meshes.
									else
									{
										auto* parentMesh = moveable.ObjectMeshes[j - 1];
										auto* parentBone = moveable.LinearizedBones[j - 1];

										for (int b2 = 0; b2 < parentMesh->Buckets.size(); b2++)
										{
											auto* parentBucket = &parentMesh->Buckets[b2];
											for (int v2 = 0; v2 < parentBucket->NumVertices; v2++)
											{
												auto* parentVertex = &moveablesVertices[parentBucket->StartVertex + v2];

												int x1 = moveablesVertices[currentBucket.StartVertex + v1].Position.x + currentBone->GlobalTranslation.x;
												int y1 = moveablesVertices[currentBucket.StartVertex + v1].Position.y + currentBone->GlobalTranslation.y;
												int z1 = moveablesVertices[currentBucket.StartVertex + v1].Position.z + currentBone->GlobalTranslation.z;

												int x2 = moveablesVertices[parentBucket->StartVertex + v2].Position.x + parentBone->GlobalTranslation.x;
												int y2 = moveablesVertices[parentBucket->StartVertex + v2].Position.y + parentBone->GlobalTranslation.y;
												int z2 = moveablesVertices[parentBucket->StartVertex + v2].Position.z + parentBone->GlobalTranslation.z;

												if (abs(x1 - x2) < 2 && abs(y1 - y2) < 2 && abs(z1 - z2) < 2)
												{
													currentVertex->Bone = j;
													currentVertex->Position = parentVertex->Position;
													currentVertex->Normal = parentVertex->Normal;
													currentVertex->AnimationFrameOffset = parentVertex->AnimationFrameOffset;
													currentVertex->Tangent = parentVertex->Tangent;
													break;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		moveablesVertexBuffer = VertexBuffer(device.Get(), (int)moveablesVertices.size(), moveablesVertices.data());
		moveablesIndexBuffer = IndexBuffer(device.Get(), (int)moveablesIndices.size(), moveablesIndices.data());

		TENLog("Preparing static mesh data...", LogLevel::Info);

		totalVertices = 0;
		totalIndices = 0;
		for (int i = 0; i < StaticObjectsIds.size(); i++)
		{
			int objNum = StaticObjectsIds[i];
			StaticInfo* obj = &StaticObjects[objNum];
			MESH* mesh = &g_Level.Meshes[obj->meshNumber];

			for (auto& bucket : mesh->buckets)
			{
				totalVertices += bucket.numQuads * 4 + bucket.numTriangles * 3;
				totalIndices += bucket.numQuads * 6 + bucket.numTriangles * 3;
			}
		}

		staticsVertices.resize(totalVertices);
		staticsIndices.resize(totalIndices);

		lastVertex = 0;
		lastIndex = 0;
		for (int i = 0; i < StaticObjectsIds.size(); i++)
		{
			StaticInfo*obj = &StaticObjects[StaticObjectsIds[i]];
			staticObjects[StaticObjectsIds[i]] = RendererObject();
			RendererObject &staticObject = *staticObjects[StaticObjectsIds[i]];
			staticObject.Type = 1;
			staticObject.Id = StaticObjectsIds[i];

			RendererMesh *mesh = GetRendererMeshFromTrMesh(&staticObject, &g_Level.Meshes[obj->meshNumber], 0, false, false, &lastVertex, &lastIndex);

			staticObject.ObjectMeshes.push_back(mesh);
			meshes.push_back(mesh);

			staticObjects[StaticObjectsIds[i]] = staticObject;
		}

		if (staticsVertices.size() > 0)
		{
			staticsVertexBuffer = VertexBuffer(device.Get(), (int)staticsVertices.size(), staticsVertices.data());
			staticsIndexBuffer = IndexBuffer(device.Get(), (int)staticsIndices.size(), staticsIndices.data());
		}
		else
		{
			staticsVertexBuffer = VertexBuffer(device.Get(), 1);
			staticsIndexBuffer = IndexBuffer(device.Get(), 1);
		}

		TENLog("Preparing sprite data...", LogLevel::Info);
		
		// Step 5: prepare sprites
		sprites.resize(g_Level.Sprites.size());

		for (int i = 0; i < g_Level.Sprites.size(); i++)
		{
			SPRITE *oldSprite = &g_Level.Sprites[i];
			sprites[i] = RendererSprite();
			RendererSprite &sprite = sprites[i];

			sprite.UV[0] = Vector2(oldSprite->x1, oldSprite->y1);
			sprite.UV[1] = Vector2(oldSprite->x2, oldSprite->y2);
			sprite.UV[2] = Vector2(oldSprite->x3, oldSprite->y3);
			sprite.UV[3] = Vector2(oldSprite->x4, oldSprite->y4);
			sprite.Texture = &spritesTextures[oldSprite->tile];
			sprite.Width = (oldSprite->x2 - oldSprite->x1) * sprite.Texture->Width + 1;
			sprite.Height = (oldSprite->y3 - oldSprite->y2) * sprite.Texture->Height + 1;
		}

		for (int i = 0; i < MoveablesIds.size(); i++)
		{
			ObjectInfo *obj = &Objects[MoveablesIds[i]];

			if (obj->nmeshes < 0)
			{
				short numSprites = abs(obj->nmeshes);
				short baseSprite = obj->meshIndex;
				spriteSequences[MoveablesIds[i]] = RendererSpriteSequence(MoveablesIds[i], numSprites);
				RendererSpriteSequence &sequence = spriteSequences[MoveablesIds[i]];

				for (int j = baseSprite; j < baseSprite + numSprites; j++)
				{
					sequence.SpritesList[j - baseSprite] = &sprites[j];
				}

				spriteSequences[MoveablesIds[i]] = sequence;
			}
		}

		return true;
	}

	RendererMesh* Renderer::GetRendererMeshFromTrMesh(RendererObject* obj, MESH* meshPtr, short boneIndex, int isJoints, int isHairs, int* lastVertex, int* lastIndex)
	{
		RendererMesh* mesh = new RendererMesh();

		mesh->Sphere = meshPtr->sphere;
		mesh->LightMode = LIGHT_MODES(meshPtr->lightMode);

		if (meshPtr->positions.empty())
			return mesh;

		mesh->Positions.resize(meshPtr->positions.size());
		for (int i = 0; i < meshPtr->positions.size(); i++)
			mesh->Positions[i] = meshPtr->positions[i];

		for (int n = 0; n < meshPtr->buckets.size(); n++)
		{
			BUCKET* levelBucket = &meshPtr->buckets[n];
			RendererBucket bucket{};
			bucket.Animated = levelBucket->animated;
			bucket.Texture = levelBucket->texture;
			bucket.BlendMode = static_cast<BlendMode>(levelBucket->blendMode);
			bucket.StartVertex = *lastVertex;
			bucket.StartIndex = *lastIndex;
			bucket.NumVertices = levelBucket->numQuads * 4 + levelBucket->numTriangles * 3;
			bucket.NumIndices = levelBucket->numQuads * 6 + levelBucket->numTriangles * 3;

			for (int p = 0; p < (int)levelBucket->polygons.size(); p++)
			{
				POLYGON* poly = &levelBucket->polygons[p];
				RendererPolygon newPoly;

				newPoly.Shape = poly->shape;
				newPoly.Centre = (
					meshPtr->positions[poly->indices[0]] +
					meshPtr->positions[poly->indices[1]] +
					meshPtr->positions[poly->indices[2]]) / 3.0f;

				int baseVertices = *lastVertex;

				for (int k = 0; k < (int)poly->indices.size(); k++)
				{
					Vertex vertex;
					int v = poly->indices[k];

					vertex.Position.x = meshPtr->positions[v].x;
					vertex.Position.y = meshPtr->positions[v].y;
					vertex.Position.z = meshPtr->positions[v].z;
					 
					vertex.Normal.x = poly->normals[k].x;
					vertex.Normal.y = poly->normals[k].y;
					vertex.Normal.z = poly->normals[k].z;

					vertex.Tangent.x = poly->tangents[k].x;
					vertex.Tangent.y = poly->tangents[k].y;
					vertex.Tangent.z = poly->tangents[k].z;

					vertex.Binormal.x = poly->binormals[k].x;
					vertex.Binormal.y = poly->binormals[k].y;
					vertex.Binormal.z = poly->binormals[k].z;

					vertex.UV.x = poly->textureCoordinates[k].x;
					vertex.UV.y = poly->textureCoordinates[k].y;

					vertex.Color.x = meshPtr->colors[v].x;
					vertex.Color.y = meshPtr->colors[v].y;
					vertex.Color.z = meshPtr->colors[v].z;
					vertex.Color.w = 1.0f;

					vertex.Bone = meshPtr->bones[v];
					vertex.OriginalIndex = v;

					vertex.Effects = Vector4(meshPtr->effects[v].x, meshPtr->effects[v].y, meshPtr->effects[v].z, poly->shineStrength);
					vertex.Hash = (unsigned int)std::hash<float>{}
						(vertex.Position.x) ^
							(unsigned int)std::hash<float>{}(vertex.Position.y) ^
							(unsigned int)std::hash<float>{}(vertex.Position.z);

					if (obj->Type == 0)
						moveablesVertices[*lastVertex] = vertex;
					else
						staticsVertices[*lastVertex] = vertex;

					*lastVertex = *lastVertex + 1;
				}

				if (poly->shape == 0)
				{
					newPoly.BaseIndex = *lastIndex;

					if (obj->Type == 0)
					{
						moveablesIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						moveablesIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						moveablesIndices[newPoly.BaseIndex + 2] = baseVertices + 3;
						moveablesIndices[newPoly.BaseIndex + 3] = baseVertices + 2;
						moveablesIndices[newPoly.BaseIndex + 4] = baseVertices + 3;
						moveablesIndices[newPoly.BaseIndex + 5] = baseVertices + 1;
					}
					else
					{
						staticsIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						staticsIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						staticsIndices[newPoly.BaseIndex + 2] = baseVertices + 3;
						staticsIndices[newPoly.BaseIndex + 3] = baseVertices + 2;
						staticsIndices[newPoly.BaseIndex + 4] = baseVertices + 3;
						staticsIndices[newPoly.BaseIndex + 5] = baseVertices + 1;
					}

					*lastIndex = *lastIndex + 6;
				}
				else
				{
					newPoly.BaseIndex = *lastIndex;

					if (obj->Type == 0)
					{
						moveablesIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						moveablesIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						moveablesIndices[newPoly.BaseIndex + 2] = baseVertices + 2;
					}
					else
					{
						staticsIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						staticsIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						staticsIndices[newPoly.BaseIndex + 2] = baseVertices + 2;
					}

					*lastIndex = *lastIndex + 3;
				}

				bucket.Polygons.push_back(newPoly);
			}

			mesh->Buckets.push_back(bucket);
		}

		return mesh;
	}
}
