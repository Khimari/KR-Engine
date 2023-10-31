#include "framework.h"

#include <algorithm>
#include <chrono>
#include <execution>
#include <filesystem>

#include "ConstantBuffers/CameraMatrixBuffer.h"
#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/effects/Hair.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Objects/Effects/tr4_locusts.h"
#include "Objects/Generic/Object/rope.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"
#include "Objects/TR5/Emitter/tr5_bats_emitter.h"
#include "Objects/TR5/Emitter/tr5_rats_emitter.h"
#include "Renderer/RenderView.h"
#include "Renderer/Renderer.h"
#include "Specific/configuration.h"
#include "Specific/level.h"
#include "Specific/winmain.h"
#include "Renderer/Structures/RendererSortableObject.h"

extern GUNSHELL_STRUCT Gunshells[MAX_GUNSHELL];

namespace TEN::Renderer
{
	using namespace std::chrono;
	using namespace TEN::Effects::Hair;
	using namespace TEN::Entities::Generic;
	using namespace TEN::Hud;
	using namespace TEN::Renderer::Structures;

	void Renderer::RenderBlobShadows(RenderView& renderView)
	{
		auto nearestSpheres = std::vector<Sphere>{};
		nearestSpheres.reserve(g_Configuration.ShadowBlobsMax);

		// Collect player spheres.
		static const std::array<LARA_MESHES, 4> sphereMeshes = { LM_HIPS, LM_TORSO, LM_LFOOT, LM_RFOOT };
		static const std::array<float, 4> sphereScaleFactors = { 6.0f, 3.2f, 2.8f, 2.8f };

		for (auto& room : renderView.RoomsToDraw) 
		{
			for (auto& i : room->ItemsToDraw) 
			{
				auto& nativeItem = g_Level.Items[i->ItemNumber];
				 
				// Skip everything that isn't alive or a vehicle.
				if (Objects[nativeItem.ObjectNumber].shadowType == ShadowMode::None)
					continue;

				if (nativeItem.IsLara())
				{
					for (auto i = 0; i < sphereMeshes.size(); i++)
					{
						if (!nativeItem.MeshBits.Test(sphereMeshes[i]))
							continue;

						auto& mesh = g_Level.Meshes[nativeItem.Model.MeshIndex[sphereMeshes[i]]];

						// Push foot spheres a little lower.
						auto offset = Vector3i(mesh.sphere.Center.x, mesh.sphere.Center.y, mesh.sphere.Center.z);
						if (sphereMeshes[i] == LM_LFOOT || sphereMeshes[i] == LM_RFOOT)
							offset.y += 8;

						auto& newSphere = nearestSpheres.emplace_back();
						newSphere.position = GetJointPosition(LaraItem, sphereMeshes[i], offset).ToVector3();;
						newSphere.radius = mesh.sphere.Radius * sphereScaleFactors[i];
					}
				}
				else
				{
					auto bounds = GameBoundingBox(&nativeItem);
					auto extents = bounds.GetExtents();
					auto center = Geometry::TranslatePoint(nativeItem.Pose.Position.ToVector3(), nativeItem.Pose.Orientation, bounds.GetCenter());
					center.y = nativeItem.Pose.Position.y;

					auto& newSphere = nearestSpheres.emplace_back();
					newSphere.position = center;
					newSphere.radius = std::max(abs(extents.x), abs(extents.z)) * 1.5f;
				}
			}
		}

		if (nearestSpheres.size() > g_Configuration.ShadowBlobsMax) 
		{
			std::sort(nearestSpheres.begin(), nearestSpheres.end(), [](const Sphere& a, const Sphere& b) 
			{
				auto& laraPos = LaraItem->Pose.Position;
				return Vector3::Distance(laraPos.ToVector3(), a.position) < Vector3::Distance(laraPos.ToVector3(), b.position);
			});

			std::copy(nearestSpheres.begin(), nearestSpheres.begin() + g_Configuration.ShadowBlobsMax, _stShadowMap.Spheres);
			_stShadowMap.NumSpheres = g_Configuration.ShadowBlobsMax;
		}
		else 
		{
			std::copy(nearestSpheres.begin(), nearestSpheres.end(), _stShadowMap.Spheres);
			_stShadowMap.NumSpheres = (int)nearestSpheres.size();
		}
	}

	void Renderer::ClearShadowMap()
	{
		for (int step = 0; step < _shadowMap.RenderTargetView.size(); step++)
		{
			_context->ClearRenderTargetView(_shadowMap.RenderTargetView[step].Get(), Colors::White);
			_context->ClearDepthStencilView(_shadowMap.DepthStencilView[step].Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f, 0);
		}
	}

	void Renderer::RenderShadowMap(RendererItem* item, RenderView& renderView)
	{
		// Doesn't cast shadow
		if (_moveableObjects[item->ObjectNumber].value().ShadowType == ShadowMode::None)
			return;

		// Only render for Lara if such setting is active
		if (g_Configuration.ShadowType == ShadowMode::Lara && _moveableObjects[item->ObjectNumber].value().ShadowType != ShadowMode::Lara)
			return;
		
		// No shadow light found
		if (_shadowLight == nullptr)
			return;

		// Shadow light found but type is incorrect
		if (_shadowLight->Type != LightType::Point && _shadowLight->Type != LightType::Spot)
			return; 

		// Reset GPU state
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);

		int steps = _shadowLight->Type == LightType::Shadow ? 6 : 1;
		for (int step = 0; step < steps; step++) 
		{
			// Bind render target
			_context->OMSetRenderTargets(1, _shadowMap.RenderTargetView[step].GetAddressOf(),
				_shadowMap.DepthStencilView[step].Get());

			_context->RSSetViewports(1, &_shadowMapViewport);
			ResetScissor();

			if (_shadowLight->Position == item->Position)
				return;

			UINT stride = sizeof(Vertex);
			UINT offset = 0;

			// Set shaders
			_context->VSSetShader(_vsShadowMap.Get(), nullptr, 0);
			_context->PSSetShader(_psShadowMap.Get(), nullptr, 0);

			_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_context->IASetInputLayout(_inputLayout.Get());
			_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			// Set texture
			BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[0]), SamplerStateRegister::AnisotropicClamp);
			BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[0]), SamplerStateRegister::AnisotropicClamp);

			// Set camera matrices
			Matrix view;
			Matrix projection;
			if (_shadowLight->Type == LightType::Point)
			{
				view = Matrix::CreateLookAt(_shadowLight->Position, _shadowLight->Position +
					RenderTargetCube::forwardVectors[step] * BLOCK(10),
					RenderTargetCube::upVectors[step]);

				projection = Matrix::CreatePerspectiveFieldOfView(90.0f * PI / 180.0f, 1.0f, 16.0f, _shadowLight->Out);

			}
			else if (_shadowLight->Type == LightType::Spot)
			{
				view = Matrix::CreateLookAt(_shadowLight->Position,
					_shadowLight->Position + _shadowLight->Direction * BLOCK(10),
					Vector3(0.0f, -1.0f, 0.0f));

				// Vertex lighting fades out in 1024-steps. increase angle artificially for a bigger blend radius.
				float projectionAngle = _shadowLight->OutRange * 1.5f * (PI / 180.0f);
				projection = Matrix::CreatePerspectiveFieldOfView(projectionAngle, 1.0f, 16.0f, _shadowLight->Out);
			}

			CCameraMatrixBuffer shadowProjection;
			shadowProjection.ViewProjection = view * projection;
			_cbCameraMatrices.updateData(shadowProjection, _context.Get());
			BindConstantBufferVS(ConstantBufferRegister::Camera, _cbCameraMatrices.get());

			_stShadowMap.LightViewProjections[step] = (view * projection);

			SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

			RendererObject& obj = GetRendererObject((GAME_OBJECT_ID)item->ObjectNumber);

			_stItem.World = item->World;
			_stItem.Color = item->Color;
			_stItem.AmbientLight = item->AmbientLight;
			memcpy(_stItem.BonesMatrices, item->AnimationTransforms, sizeof(Matrix) * MAX_BONES);
			for (int k = 0; k < MAX_BONES; k++)
				_stItem.BoneLightModes[k] = (int)LightMode::Static;

			_cbItem.updateData(_stItem, _context.Get());
			BindConstantBufferVS(ConstantBufferRegister::Item, _cbItem.get());
			BindConstantBufferPS(ConstantBufferRegister::Item, _cbItem.get());

			for (int k = 0; k < obj.ObjectMeshes.size(); k++)
			{
				auto* mesh = GetMesh(item->MeshIndex[k]);

				for (auto& bucket : mesh->Buckets)
				{
					if (bucket.NumVertices == 0)
						continue;

					if (bucket.BlendMode != BlendMode::Opaque && bucket.BlendMode != BlendMode::AlphaTest)
						continue;

					// Draw vertices
					DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

					_numMoveablesDrawCalls++;
				}
			}

			if (item->ObjectNumber == ID_LARA)
			{
				RendererRoom& room = _rooms[item->RoomNumber];

				DrawLaraHolsters(item, &room, RendererPass::ShadowMap);
				DrawLaraJoints(item, &room, RendererPass::ShadowMap);
				DrawLaraHair(item, &room, RendererPass::ShadowMap);
			}
		}
	}

	void Renderer::DrawGunShells(RenderView& view)
	{
		auto& room = _rooms[LaraItem->RoomNumber];
		auto* item = &_items[LaraItem->Index];

		int gunShellsCount = 0;
		short objectNumber = 0;

		for (int i = 0; i < MAX_GUNSHELL; i++)
		{
			auto* gunshell = &Gunshells[i];

			if (gunshell->counter <= 0)
			{
				continue;
			}

			objectNumber = gunshell->objectNumber;

			Matrix translation = Matrix::CreateTranslation(
				gunshell->pos.Position.x, 
				gunshell->pos.Position.y,
				gunshell->pos.Position.z
			);
			Matrix rotation = gunshell->pos.Orientation.ToRotationMatrix();
			Matrix world = rotation * translation;

			_stInstancedStaticMeshBuffer.StaticMeshes[gunShellsCount].World = world;
			_stInstancedStaticMeshBuffer.StaticMeshes[gunShellsCount].Ambient = room.AmbientLight;
			_stInstancedStaticMeshBuffer.StaticMeshes[gunShellsCount].Color = room.AmbientLight;
			_stInstancedStaticMeshBuffer.StaticMeshes[gunShellsCount].LightMode = (int)LightMode::Dynamic;
			BindInstancedStaticLights(item->LightsToDraw, gunShellsCount);

			gunShellsCount++;
		}

		if (gunShellsCount > 0)
		{
			auto& moveableObject = *_moveableObjects[objectNumber];

			_context->VSSetShader(_vsInstancedStaticMeshes.Get(), nullptr, 0);
			_context->PSSetShader(_psInstancedStaticMeshes.Get(), nullptr, 0);

			UINT stride = sizeof(Vertex);
			UINT offset = 0;

			_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_context->IASetInputLayout(_inputLayout.Get());
			_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

			_cbInstancedStaticMeshBuffer.updateData(_stInstancedStaticMeshBuffer, _context.Get());

			BindConstantBufferVS(ConstantBufferRegister::InstancedStatics, _cbInstancedStaticMeshBuffer.get());
			BindConstantBufferPS(ConstantBufferRegister::InstancedStatics, _cbInstancedStaticMeshBuffer.get());

			auto* mesh = moveableObject.ObjectMeshes[0];

			for (auto& bucket : mesh->Buckets)
			{
				if (bucket.NumVertices == 0 && bucket.BlendMode == BlendMode::Opaque)
				{
					continue;
				}

				BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
				BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

				DrawIndexedInstancedTriangles(bucket.NumIndices, gunShellsCount, bucket.StartIndex, 0);

				_numStaticsDrawCalls++;
			}
		}
	}

	void Renderer::DrawRopes(RenderView& view)
	{
		for (auto& rope : Ropes)
		{
			if (!rope.active)
				continue;

			auto world = Matrix::CreateTranslation(rope.position.x, rope.position.y, rope.position.z);

			Vector3 absolute[24];

			for (int n = 0; n < ROPE_SEGMENTS; n++)
			{
				auto* s = &rope.meshSegment[n];
				Vector3 t;
				Vector3 output;

				t.x = s->x >> FP_SHIFT;
				t.y = s->y >> FP_SHIFT;
				t.z = s->z >> FP_SHIFT;

				output = Vector3::Transform(t, world);

				auto absolutePosition = output;
				absolute[n] = absolutePosition;
			}

			for (int n = 0; n < ROPE_SEGMENTS - 1; n++)
			{
				auto pos1 = absolute[n];
				auto pos2 = absolute[n + 1];

				auto d = pos2 - pos1;
				d.Normalize();

				auto c = (pos1 + pos2) / 2.0f;

				AddSpriteBillboardConstrained(&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_EMPTY1],
					c,
					_rooms[rope.room].AmbientLight,
					(PI / 2),
					1.0f,
					{ 32,
					Vector3::Distance(pos1, pos2) },
					BlendMode::AlphaBlend,
					d, false, view);
			}
		}
	}

	void Renderer::DrawLinesIn2DSpace()
	{
		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::None);

		_context->VSSetShader(_vsSolid.Get(), nullptr, 0);
		_context->PSSetShader(_psSolid.Get(), nullptr, 0);
		auto worldMatrix = Matrix::CreateOrthographicOffCenter(0, _screenWidth, _screenHeight, 0, _viewport.MinDepth, _viewport.MaxDepth);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_primitiveBatch->Begin();

		for (const auto& line : _lines2DToDraw)
		{
			auto v1 = Vertex();
			v1.Position.x = line.Origin.x;
			v1.Position.y = line.Origin.y;
			v1.Position.z = 1.0f;
			v1.Color = line.Color;

			auto v2 = Vertex();
			v2.Position.x = line.Target.x;
			v2.Position.y = line.Target.y;
			v2.Position.z = 1.0f;
			v2.Color = line.Color;

			v1.Position = Vector3::Transform(v1.Position, worldMatrix);
			v2.Position = Vector3::Transform(v2.Position, worldMatrix);

			v1.Position.z = 0.5f;
			v2.Position.z = 0.5f;

			_primitiveBatch->DrawLine(v1, v2);
		}

		_primitiveBatch->End();

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
	}

	void Renderer::DrawSpiders(RenderView& view)
	{
		/*XMMATRIX world;
		UINT cPasses = 1;

		if (Objects[ID_SPIDERS_EMITTER].loaded)
		{
			ObjectInfo* obj = &Objects[ID_SPIDERS_EMITTER];
			RendererObject* moveableObj = m_moveableObjects[ID_SPIDERS_EMITTER].get();
			short* meshPtr = Meshes[Objects[ID_SPIDERS_EMITTER].meshIndex + ((Wibble >> 2) & 2)];
			RendererMesh* mesh = m_meshPointersToMesh[meshPtr];
			RendererBucket* bucket = mesh->GetBucket(bucketIndex);

			if (bucket->NumVertices == 0)
				return true;

			setGpuStateForBucket(bucketIndex);

			m_device->SetStreamSource(0, bucket->GetVertexBuffer(), 0, sizeof(Vertex));
			m_device->SetIndices(bucket->GetIndexBuffer());

			LPD3DXEFFECT effect;
			if (pass == RENDERER_PASS_SHADOW_MAP)
				effect = m_shaderDepth->GetEffect();
			else if (pass == RENDERER_PASS_RECONSTRUCT_DEPTH)
				effect = m_shaderReconstructZBuffer->GetEffect();
			else if (pass == RENDERER_PASS_GBUFFER)
				effect = m_shaderFillGBuffer->GetEffect();
			else
				effect = m_shaderTransparent->GetEffect();

			effect->SetBool(effect->GetParameterByName(nullptr, "UseSkinning"), false);
			effect->SetInt(effect->GetParameterByName(nullptr, "ModelType"), MODEL_TYPE_MOVEABLE);

			if (bucketIndex == RENDERER_BUCKET_SOLID || bucketIndex == RENDERER_BUCKET_SOLID_DS)
				effect->SetInt(effect->GetParameterByName(nullptr, "BlendMode"), BlendMode::Opaque);
			else
				effect->SetInt(effect->GetParameterByName(nullptr, "BlendMode"), BlendMode::AlphaTest);

			for (int i = 0; i < NUM_SPIDERS; i++)
			{
				SPIDER_STRUCT* spider = &Spiders[i];

				if (spider->on)
				{
					XMMATRIXTranslation(&m_tempTranslation, spider->pos.xPos, spider->pos.yPos, spider->pos.zPos);
					XMMATRIXRotationYawPitchRoll(&m_tempRotation, spider->pos.yRot, spider->pos.xRot, spider->pos.zRot);
					XMMATRIXMultiply(&m_tempWorld, &m_tempRotation, &m_tempTranslation);
					effect->SetMatrix(effect->GetParameterByName(nullptr, "World"), &m_tempWorld);

					effect->SetVector(effect->GetParameterByName(nullptr, "AmbientLight"), &rooms[spider->roomNumber]->AmbientLight);

					for (int iPass = 0; iPass < cPasses; iPass++)
					{
						effect->BeginPass(iPass);
						effect->CommitChanges();

						drawPrimitives(D3DPT_TRIANGLELIST, 0, 0, bucket->NumVertices, 0, bucket->Indices.size() / 3);

						effect->EndPass();
					}
				}
			}
		}*/
	}

	void Renderer::DrawRats(RenderView& view)
	{
		_context->VSSetShader(_vsStatics.Get(), NULL, 0);
		_context->PSSetShader(_psStatics.Get(), NULL, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		if (Objects[ID_RATS_EMITTER].loaded)
		{
			RendererObject& moveableObj = *_moveableObjects[ID_RATS_EMITTER];

			_stStatic.LightMode = (int)moveableObj.ObjectMeshes[0]->LightMode;

			for (int i = 0; i < NUM_RATS; i++)
			{
				auto* rat = &Rats[i];

				if (rat->On)
				{
					RendererMesh* mesh = GetMesh(Objects[ID_RATS_EMITTER].meshIndex + (rand() % 8));
					Matrix translation = Matrix::CreateTranslation(rat->Pose.Position.x, rat->Pose.Position.y, rat->Pose.Position.z);
					Matrix rotation = rat->Pose.Orientation.ToRotationMatrix();
					Matrix world = rotation * translation;

					_stStatic.World = world;
					_stStatic.Color = Vector4::One;
					_stStatic.AmbientLight = _rooms[rat->RoomNumber].AmbientLight;
					BindStaticLights(_rooms[rat->RoomNumber].LightsToDraw);
					_cbStatic.updateData(_stStatic, _context.Get());
					BindConstantBufferVS(ConstantBufferRegister::Static, _cbStatic.get());
					BindConstantBufferPS(ConstantBufferRegister::Static, _cbStatic.get());

					for (int b = 0; b < mesh->Buckets.size(); b++)
					{
						RendererBucket* bucket = &mesh->Buckets[b];

						if (bucket->Polygons.empty())
							continue;

						DrawIndexedTriangles(bucket->NumIndices, bucket->StartIndex, 0);

						_numMoveablesDrawCalls++;
					}
				}
			}
		}
	}

	void Renderer::DrawBats(RenderView& view)
	{
		if (!Objects[ID_BATS_EMITTER].loaded)
			return;

		RendererMesh* mesh = GetMesh(Objects[ID_BATS_EMITTER].meshIndex + (GlobalCounter & 3));

		int batsCount = 0;

		for (int i = 0; i < NUM_BATS; i++)
		{
			auto* bat = &Bats[i];

			if (bat->On)
			{
				RendererRoom& room = _rooms[bat->RoomNumber];

				Matrix translation = Matrix::CreateTranslation(bat->Pose.Position.x, bat->Pose.Position.y, bat->Pose.Position.z);
				Matrix rotation = bat->Pose.Orientation.ToRotationMatrix();
				Matrix world = rotation * translation;

				_stInstancedStaticMeshBuffer.StaticMeshes[batsCount].World = world;
				_stInstancedStaticMeshBuffer.StaticMeshes[batsCount].Ambient = room.AmbientLight;
				_stInstancedStaticMeshBuffer.StaticMeshes[batsCount].Color = Vector4::One;
				_stInstancedStaticMeshBuffer.StaticMeshes[batsCount].LightMode = (int)mesh->LightMode;
				BindInstancedStaticLights(room.LightsToDraw, batsCount);

				batsCount++;
			}
		}

		if (batsCount > 0)
		{
			_context->VSSetShader(_vsInstancedStaticMeshes.Get(), nullptr, 0);
			_context->PSSetShader(_psInstancedStaticMeshes.Get(), nullptr, 0);

			UINT stride = sizeof(Vertex);
			UINT offset = 0;

			_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_context->IASetInputLayout(_inputLayout.Get());
			_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

			_cbInstancedStaticMeshBuffer.updateData(_stInstancedStaticMeshBuffer, _context.Get());

			BindConstantBufferVS(ConstantBufferRegister::InstancedStatics, _cbInstancedStaticMeshBuffer.get());
			BindConstantBufferPS(ConstantBufferRegister::InstancedStatics, _cbInstancedStaticMeshBuffer.get());

			for (auto& bucket : mesh->Buckets)
			{
				if (bucket.NumVertices == 0 && bucket.BlendMode == BlendMode::Opaque)
				{
					continue;
				}

				SetBlendMode(bucket.BlendMode);

				BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
				BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

				DrawIndexedInstancedTriangles(bucket.NumIndices, batsCount, bucket.StartIndex, 0);

				_numStaticsDrawCalls++;
			}
		}
	}
	void Renderer::DrawScarabs(RenderView& view)
	{
		if (!Objects[ID_LITTLE_BEETLE].loaded)
			return;

		const auto& mesh = *GetMesh(Objects[ID_LITTLE_BEETLE].meshIndex + ((Wibble >> 2) % 2));

		unsigned int beetleCount = 0;
		for (int i = 0; i < TEN::Entities::TR4::NUM_BEETLES; i++)
		{
			const auto& beetle = TEN::Entities::TR4::BeetleSwarm[i];

			if (beetle.On)
			{
				auto& room = _rooms[beetle.RoomNumber];

				auto tMatrix = Matrix::CreateTranslation(beetle.Pose.Position.x, beetle.Pose.Position.y, beetle.Pose.Position.z);
				auto rotMatrix = beetle.Pose.Orientation.ToRotationMatrix();
				auto worldMatrix = rotMatrix * tMatrix;

				_stInstancedStaticMeshBuffer.StaticMeshes[beetleCount].World = worldMatrix;
				_stInstancedStaticMeshBuffer.StaticMeshes[beetleCount].Ambient = room.AmbientLight;
				_stInstancedStaticMeshBuffer.StaticMeshes[beetleCount].Color = Vector4::One;
				_stInstancedStaticMeshBuffer.StaticMeshes[beetleCount].LightMode = (int)mesh.LightMode;
				BindInstancedStaticLights(room.LightsToDraw, beetleCount);

				beetleCount++;
			}
		}

		if (beetleCount > 0)
		{
			_context->VSSetShader(_vsInstancedStaticMeshes.Get(), nullptr, 0);
			_context->PSSetShader(_psInstancedStaticMeshes.Get(), nullptr, 0);

			unsigned int stride = sizeof(Vertex);
			unsigned int offset = 0;

			_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_context->IASetInputLayout(_inputLayout.Get());
			_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

			_cbInstancedStaticMeshBuffer.updateData(_stInstancedStaticMeshBuffer, _context.Get());

			BindConstantBufferVS(ConstantBufferRegister::InstancedStatics, _cbInstancedStaticMeshBuffer.get());
			BindConstantBufferPS(ConstantBufferRegister::InstancedStatics, _cbInstancedStaticMeshBuffer.get());

			for (const auto& bucket : mesh.Buckets)
			{
				if (bucket.NumVertices == 0 && bucket.BlendMode == BlendMode::Opaque)
					continue;

				SetBlendMode(bucket.BlendMode);

				BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
				BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::None);

				DrawIndexedInstancedTriangles(bucket.NumIndices, beetleCount, bucket.StartIndex, 0);

				_numStaticsDrawCalls++;
			}
		}
	}

	void Renderer::DrawLocusts(RenderView& view)
	{
		_context->VSSetShader(_vsStatics.Get(), NULL, 0);
		_context->PSSetShader(_psStatics.Get(), NULL, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		if (Objects[ID_LOCUSTS].loaded)
		{
			ObjectInfo* obj = &Objects[ID_LOCUSTS];
			RendererObject& moveableObj = *_moveableObjects[ID_LOCUSTS];
			
			_stStatic.LightMode = (int)moveableObj.ObjectMeshes[0]->LightMode;

			for (int i = 0; i < TEN::Entities::TR4::MAX_LOCUSTS; i++)
			{
				LOCUST_INFO* locust = &TEN::Entities::TR4::Locusts[i];

				if (locust->on)
				{
					RendererMesh* mesh = GetMesh(Objects[ID_LOCUSTS].meshIndex + (-locust->counter & 3));
					Matrix translation = Matrix::CreateTranslation(locust->pos.Position.x, locust->pos.Position.y, locust->pos.Position.z);
					Matrix rotation = locust->pos.Orientation.ToRotationMatrix();
					Matrix world = rotation * translation;

					_stStatic.World = world;
					_stStatic.Color = Vector4::One;
					_stStatic.AmbientLight = _rooms[locust->roomNumber].AmbientLight;
					_cbStatic.updateData(_stStatic, _context.Get());
					BindConstantBufferVS(ConstantBufferRegister::Static, _cbStatic.get());

					for (int b = 0; b < mesh->Buckets.size(); b++)
					{
						RendererBucket* bucket = &mesh->Buckets[b];

						if (bucket->Polygons.empty())
							continue;

						DrawIndexedTriangles(bucket->NumIndices, bucket->StartIndex, 0);

						_numMoveablesDrawCalls++;
					}
				}
			}
		}
	}

	void Renderer::DrawLines3D(RenderView& view)
	{
		SetBlendMode(BlendMode::Additive);
		SetCullMode(CullMode::None);

		_context->VSSetShader(_vsSolid.Get(), nullptr, 0);
		_context->PSSetShader(_psSolid.Get(), nullptr, 0);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_primitiveBatch->Begin();

		for (int i = 0; i < _lines3DToDraw.size(); i++)
		{
			RendererLine3D* line = &_lines3DToDraw[i];

			Vertex v1;
			v1.Position = line->Start;
			v1.Color = line->Color;

			Vertex v2;
			v2.Position = line->End;
			v2.Color = line->Color;
			_primitiveBatch->DrawLine(v1, v2);
		}

		_primitiveBatch->End();

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
	}

	void Renderer::AddLine3D(const Vector3& target, const Vector3& origin, const Vector4& color)
	{
		if (_isLocked)
			return;

		RendererLine3D line;

		line.Start = target;
		line.End = origin;
		line.Color = color;

		_lines3DToDraw.push_back(line);
	}

	void Renderer::AddReticle(const Vector3& center, float radius, const Vector4& color)
	{
		auto origin0 = center + Vector3(radius, 0.0f, 0.0f);
		auto target0 = center + Vector3(-radius, 0.0f, 0.0f);
		AddLine3D(origin0, target0, color);

		auto origin1 = center + Vector3(0.0f, radius, 0.0f);
		auto target1 = center + Vector3(0.0f, -radius, 0.0f);
		AddLine3D(origin1, target1, color);

		auto origin2 = center + Vector3(0.0f, 0.0f, radius);
		auto target2 = center + Vector3(0.0f, 0.0f, -radius);
		AddLine3D(origin2, target2, color);
	}

	void Renderer::AddDebugReticle(const Vector3& center, float radius, const Vector4& color, RendererDebugPage page)
	{
		if (!DebugMode || _debugPage != page)
			return;

		AddReticle(center, radius, color);
	}

	void Renderer::AddSphere(const Vector3& center, float radius, const Vector4& color)
	{
		constexpr auto AXIS_COUNT		 = 3;
		constexpr auto SUBDIVISION_COUNT = 32;
		constexpr auto STEP_COUNT		 = 4;
		constexpr auto STEP_ANGLE		 = PI / STEP_COUNT;

		if (_isLocked)
			return;

		auto prevPoints = std::array<Vector3, AXIS_COUNT>{};

		for (int i = 0; i < STEP_COUNT; i++)
		{
			float x = sin(STEP_ANGLE * i) * radius;
			float z = cos(STEP_ANGLE * i) * radius;
			float angle = 0.0f;

			for (int j = 0; j < SUBDIVISION_COUNT; j++)
			{
				auto points = std::array<Vector3, AXIS_COUNT>
				{
					center + Vector3(sin(angle) * abs(x), z, cos(angle) * abs(x)),
					center + Vector3(cos(angle) * abs(x), sin(angle) * abs(x), z),
					center + Vector3(z, sin(angle) * abs(x), cos(angle) * abs(x))
				};

				if (j > 0)
				{
					for (int k = 0; k < points.size(); k++)
						AddLine3D(prevPoints[k], points[k], color);
				}

				prevPoints = points;
				angle += PI_MUL_2 / (SUBDIVISION_COUNT - 1);
			}
		}
	}

	void Renderer::AddDebugSphere(const Vector3& center, float radius, const Vector4& color, RendererDebugPage page)
	{
		if (!DebugMode || _debugPage != page)
			return;

		AddSphere(center, radius, color);
	}

	void Renderer::AddBox(Vector3* corners, const Vector4& color)
	{
		if (_isLocked)
			return;

		for (int i = 0; i < 12; i++)
		{
			RendererLine3D line;

			switch (i)
			{
			case 0: line.Start = corners[0];
				line.End = corners[1];
				break;
			case 1: line.Start = corners[1];
				line.End = corners[2];
				break;
			case 2: line.Start = corners[2];
				line.End = corners[3];
				break;
			case 3: line.Start = corners[3];
				line.End = corners[0];
				break;

			case 4: line.Start = corners[4];
				line.End = corners[5];
				break;
			case 5: line.Start = corners[5];
				line.End = corners[6];
				break;
			case 6: line.Start = corners[6];
				line.End = corners[7];
				break;
			case 7: line.Start = corners[7];
				line.End = corners[4];
				break;

			case 8: line.Start = corners[0];
				line.End = corners[4];
				break;
			case 9: line.Start = corners[1];
				line.End = corners[5];
				break;
			case 10: line.Start = corners[2];
				line.End = corners[6];
				break;
			case 11: line.Start = corners[3];
				line.End = corners[7];
				break;
			}

			line.Color = color;
			_lines3DToDraw.push_back(line);
		}
	}

	void Renderer::AddBox(const Vector3 min, const Vector3& max, const Vector4& color)
	{
		if (_isLocked)
			return;

		for (int i = 0; i < 12; i++)
		{
			RendererLine3D line;

			switch (i)
			{
			case 0: line.Start = Vector3(min.x, min.y, min.z);
				line.End = Vector3(min.x, min.y, max.z);
				break;
			case 1: line.Start = Vector3(min.x, min.y, max.z);
				line.End = Vector3(max.x, min.y, max.z);
				break;
			case 2: line.Start = Vector3(max.x, min.y, max.z);
				line.End = Vector3(max.x, min.y, min.z);
				break;
			case 3: line.Start = Vector3(max.x, min.y, min.z);
				line.End = Vector3(min.x, min.y, min.z);
				break;

			case 4: line.Start = Vector3(min.x, max.y, min.z);
				line.End = Vector3(min.x, max.y, max.z);
				break;
			case 5: line.Start = Vector3(min.x, max.y, max.z);
				line.End = Vector3(max.x, max.y, max.z);
				break;
			case 6: line.Start = Vector3(max.x, max.y, max.z);
				line.End = Vector3(max.x, max.y, min.z);
				break;
			case 7: line.Start = Vector3(max.x, max.y, min.z);
				line.End = Vector3(min.x, max.y, min.z);
				break;

			case 8: line.Start = Vector3(min.x, min.y, min.z);
				line.End = Vector3(min.x, max.y, min.z);
				break;
			case 9: line.Start = Vector3(min.x, min.y, max.z);
				line.End = Vector3(min.x, max.y, max.z);
				break;
			case 10: line.Start = Vector3(max.x, min.y, max.z);
				line.End = Vector3(max.x, max.y, max.z);
				break;
			case 11: line.Start = Vector3(max.x, min.y, min.z);
				line.End = Vector3(max.x, max.y, min.z);
				break;
			}

			line.Color = color;
			_lines3DToDraw.push_back(line);
		}
	}

	void Renderer::AddDebugBox(const BoundingOrientedBox& box, const Vector4& color, RendererDebugPage page)
	{
		if (!DebugMode || _debugPage != page)
			return;

		Vector3 corners[8];
		box.GetCorners(corners);
		AddBox(corners, color);
	}

	void Renderer::AddDebugBox(const Vector3& min, const Vector3& max, const Vector4& color, RendererDebugPage page)
	{
		if (_debugPage != page)
			return;

		AddBox(min, max, color);
	}

	void Renderer::AddDynamicLight(int x, int y, int z, short falloff, byte r, byte g, byte b)
	{
		if (_isLocked)
			return;

		RendererLight dynamicLight = {};

		if (falloff >= 8)
		{
			dynamicLight.Color = Vector3(r / 255.0f, g / 255.0f, b / 255.0f) * 2.0f;
		}
		else
		{
			r = (r * falloff) >> 3;
			g = (g * falloff) >> 3;
			b = (b * falloff) >> 3;

			dynamicLight.Color = Vector3(r / 255.0f, g / 255.0f, b / 255.0f) * 2.0f;
		}

		dynamicLight.RoomNumber = NO_ROOM;
		dynamicLight.Intensity = 1.0f;
		dynamicLight.Position = Vector3(float(x), float(y), float(z));
		dynamicLight.Out = falloff * 256.0f;
		dynamicLight.Type = LightType::Point;
		dynamicLight.BoundingSphere = BoundingSphere(dynamicLight.Position, dynamicLight.Out);
		dynamicLight.Luma = Luma(dynamicLight.Color);

		_dynamicLights.push_back(dynamicLight);
	}

	void Renderer::ClearDynamicLights()
	{
		_dynamicLights.clear();
	}

	void Renderer::ClearScene()
	{
		ResetAnimations();

		ClearFires();
		ClearDynamicLights();
		ClearSceneItems();
		ClearShadowMap();

		_transparentFaces.clear();

		_currentCausticsFrame++;
		_currentCausticsFrame %= 32;

		CalculateFrameRate();
	}

	void Renderer::DrawSortedFaces(RenderView& view)
	{
		std::for_each(std::execution::par_unseq,
					  view.RoomsToDraw.begin(),
					  view.RoomsToDraw.end(),
					  [](RendererRoom* room)
					  {
						  std::sort(
							  room->TransparentFacesToDraw.begin(),
							  room->TransparentFacesToDraw.end(),
							  [](RendererTransparentFace& a, RendererTransparentFace& b)
							  {
								  return (a.distance > b.distance);
							  }
						  );
					  }
		);

		for (int r = (int)view.RoomsToDraw.size() - 1; r >= 0; r--)
		{
			RendererRoom& room = *view.RoomsToDraw[r];

			_transparentFacesVertices.clear();
			_transparentFacesIndices.clear();

			bool outputPolygons = false;
			bool resetPipeline = true;

			for (int f = 0; f < room.TransparentFacesToDraw.size(); f++)
			{
				RendererTransparentFace* face = &room.TransparentFacesToDraw[f];

				if (f > 0)
				{
					RendererTransparentFace* oldFace = &room.TransparentFacesToDraw[f - 1];

					// Check if it's time to output polygons
					if (face->type != oldFace->type)
					{
						outputPolygons = true;
						resetPipeline = true;
					}
					else
					{
						// If same type, check additional conditions
						if (face->type == TransparentFaceType::Room &&
							(oldFace->info.room != face->info.room
								|| oldFace->info.texture != face->info.texture
								|| oldFace->info.animated != face->info.animated
								|| oldFace->info.blendMode != face->info.blendMode
								|| _transparentFacesIndices.size() + (face->info.polygon->Shape ? 3 : 6) > MAX_TRANSPARENT_VERTICES))
						{
							outputPolygons = true;
							resetPipeline = oldFace->info.room != face->info.room;
						}
						else if (oldFace->type == TransparentFaceType::Moveable &&
							(oldFace->info.blendMode != face->info.blendMode
								|| oldFace->info.item->ItemNumber != face->info.item->ItemNumber
								|| _transparentFacesIndices.size() + (face->info.polygon->Shape ? 3 : 6) > MAX_TRANSPARENT_VERTICES))
						{
							outputPolygons = true;
							resetPipeline = oldFace->info.item->ItemNumber != face->info.item->ItemNumber;
						}
						else if (face->type == TransparentFaceType::Static &&
							(oldFace->info.blendMode != face->info.blendMode
								|| oldFace->info.staticMesh->IndexInRoom != face->info.staticMesh->IndexInRoom
								|| oldFace->info.staticMesh->RoomNumber != face->info.staticMesh->RoomNumber
								|| _transparentFacesIndices.size() + (face->info.polygon->Shape ? 3 : 6) > MAX_TRANSPARENT_VERTICES))
						{
							outputPolygons = true;
							resetPipeline = oldFace->info.staticMesh != face->info.staticMesh;
						}
						else if (face->type == TransparentFaceType::Sprite &&
							(oldFace->info.blendMode != face->info.blendMode
								|| oldFace->info.sprite->SoftParticle != face->info.sprite->SoftParticle
								|| oldFace->info.sprite->Sprite->Texture != face->info.sprite->Sprite->Texture
								|| _transparentFacesVertices.size() + 6 > MAX_TRANSPARENT_VERTICES))
						{
							outputPolygons = true;
							resetPipeline = oldFace->info.sprite->Sprite->Texture != face->info.sprite->Sprite->Texture;
						}
					}

					if (outputPolygons)
					{
						if (oldFace->type == TransparentFaceType::Room)
						{
							DrawRoomsSorted(&oldFace->info, resetPipeline, view);
						}
						else if (oldFace->type == TransparentFaceType::Moveable)
						{
							DrawItemsSorted(&oldFace->info, resetPipeline, view);
						}
						else if (oldFace->type == TransparentFaceType::Static)
						{
							DrawStaticsSorted(&oldFace->info, resetPipeline, view);
						}
						else if (oldFace->type == TransparentFaceType::Sprite)
						{
							DrawSpritesSorted(&oldFace->info, resetPipeline, view);
						}

						outputPolygons = false;
						_transparentFacesVertices.clear();
						_transparentFacesIndices.clear();
					}
				}
				else
				{
					resetPipeline = true;
				}

				// Accumulate vertices in the buffer
				if (face->type == TransparentFaceType::Room)
				{
					// For rooms, we already pass world coordinates, just copy vertices
					int numIndices = (face->info.polygon->Shape == 0 ? 6 : 3);
					_transparentFacesIndices.bulk_push_back(_roomsIndices.data(), face->info.polygon->BaseIndex, numIndices);
				}
				else if (face->type == TransparentFaceType::Moveable)
				{
					// For rooms, we already pass world coordinates, just copy vertices
					int numIndices = (face->info.polygon->Shape == 0 ? 6 : 3);
					_transparentFacesIndices.bulk_push_back(_moveablesIndices.data(), face->info.polygon->BaseIndex, numIndices);
				}
				else if (face->type == TransparentFaceType::Static)
				{
					// For rooms, we already pass world coordinates, just copy vertices
					int numIndices = (face->info.polygon->Shape == 0 ? 6 : 3);
					_transparentFacesIndices.bulk_push_back(_staticsIndices.data(), face->info.polygon->BaseIndex, numIndices);
				}
				else if (face->type == TransparentFaceType::Sprite)
				{
					// For sprites, we need to compute the corners of the quad and multiply 
					// by the world matrix that can be an identity (for 3D sprites) or 
					// a billboard matrix. We transform vertices on the CPU because 
					// CPUs nowadays are fast enough and we save draw calls, in fact 
					// each sprite would require a different world matrix and then 
					// we would fall in the case 1 poly = 1 draw call (worst case scenario).
					// For the same reason, we store also color directly there and we simply 
					// pass a Vector4::One as color to the shader.

					RendererSpriteToDraw* spr = face->info.sprite;

					Vector3 p0t;
					Vector3 p1t;
					Vector3 p2t;
					Vector3 p3t;

					Vector2 uv0;
					Vector2 uv1;
					Vector2 uv2;
					Vector2 uv3;

					if (spr->Type == SpriteType::ThreeD)
					{
						p0t = spr->vtx1;
						p1t = spr->vtx2;
						p2t = spr->vtx3;
						p3t = spr->vtx4;


					}
					else
					{
						p0t = Vector3(-0.5, 0.5, 0);
						p1t = Vector3(0.5, 0.5, 0);
						p2t = Vector3(0.5, -0.5, 0);
						p3t = Vector3(-0.5, -0.5, 0);
					}

					uv0 = spr->Sprite->UV[0];
					uv1 = spr->Sprite->UV[1];
					uv2 = spr->Sprite->UV[2];
					uv3 = spr->Sprite->UV[3];

					Vertex v0;
					v0.Position = Vector3::Transform(p0t, face->info.world);
					v0.UV = uv0;
					v0.Color = spr->c1;

					Vertex v1;
					v1.Position = Vector3::Transform(p1t, face->info.world);
					v1.UV = uv1;
					v1.Color = spr->c2;

					Vertex v2;
					v2.Position = Vector3::Transform(p2t, face->info.world);
					v2.UV = uv2;
					v2.Color = spr->c3;

					Vertex v3;
					v3.Position = Vector3::Transform(p3t, face->info.world);
					v3.UV = uv3;
					v3.Color = spr->c4;

					_transparentFacesVertices.push_back(v0);
					_transparentFacesVertices.push_back(v1);
					_transparentFacesVertices.push_back(v3);
					_transparentFacesVertices.push_back(v2);
					_transparentFacesVertices.push_back(v3);
					_transparentFacesVertices.push_back(v1);
				}

				if (f == room.TransparentFacesToDraw.size() - 1)
				{
					if (face->type == TransparentFaceType::Room)
					{
						DrawRoomsSorted(&face->info, true, view);
					}
					else if (face->type == TransparentFaceType::Moveable)
					{
						DrawItemsSorted(&face->info, true, view);
					}
					else if (face->type == TransparentFaceType::Static)
					{
						DrawStaticsSorted(&face->info, true, view);
					}
					else if (face->type == TransparentFaceType::Sprite)
					{
						DrawSpritesSorted(&face->info, true, view);
					}
				}
			}
		}

		SetCullMode(CullMode::CounterClockwise);
	}

	void Renderer::DrawRoomsSorted(RendererTransparentFaceInfo* info, bool resetPipeline, RenderView& view)
	{
		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		ROOM_INFO* nativeRoom = &g_Level.Rooms[info->room->RoomNumber];

		_context->IASetVertexBuffers(0, 1, _roomsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		// Set shaders
		if (!info->animated)
		{
			_context->VSSetShader(_vsRooms.Get(), nullptr, 0);
		}
		else
		{
			BindConstantBufferVS(ConstantBufferRegister::AnimatedTextures, _cbAnimated.get());
			_context->VSSetShader(_vsRoomsAnimatedTextures.Get(), nullptr, 0);
		}

		_context->PSSetShader(_psRooms.Get(), nullptr, 0);

		// Set texture
		if (resetPipeline)
		{
			_stRoom.Caustics = (int)(g_Configuration.EnableCaustics && (nativeRoom->flags & ENV_FLAG_WATER));
			_stRoom.AmbientColor = info->room->AmbientLight;
			_stRoom.Water = (nativeRoom->flags & ENV_FLAG_WATER) != 0 ? 1 : 0;
			BindRoomLights(view.LightsToDraw);
			_cbRoom.updateData(_stRoom, _context.Get());
			BindConstantBufferVS(ConstantBufferRegister::Room, _cbRoom.get());
			BindConstantBufferPS(ConstantBufferRegister::Room, _cbRoom.get());
		}

		// Draw geometry
		if (info->animated)
		{
			BindTexture(TextureRegister::ColorMap, &std::get<0>(_animatedTextures[info->texture]),
				SamplerStateRegister::AnisotropicClamp);
			BindTexture(TextureRegister::NormalMap, &std::get<1>(_animatedTextures[info->texture]),
				SamplerStateRegister::AnisotropicClamp);

			RendererAnimatedTextureSet& set = _animatedTextureSets[info->texture];
			_stAnimated.NumFrames = set.NumTextures;
			_stAnimated.Type = 0;
			_stAnimated.Fps = set.Fps;

			for (unsigned char i = 0; i < set.NumTextures; i++)
			{
				auto& tex = set.Textures[i];
				_stAnimated.Textures[i].topLeft = set.Textures[i].UV[0];
				_stAnimated.Textures[i].topRight = set.Textures[i].UV[1];
				_stAnimated.Textures[i].bottomRight = set.Textures[i].UV[2];
				_stAnimated.Textures[i].bottomLeft = set.Textures[i].UV[3];
			}
			_cbAnimated.updateData(_stAnimated, _context.Get());
		}
		else
		{
			BindTexture(TextureRegister::ColorMap, &std::get<0>(_roomTextures[info->texture]),
				SamplerStateRegister::AnisotropicClamp);
			BindTexture(TextureRegister::NormalMap, &std::get<1>(_roomTextures[info->texture]),
				SamplerStateRegister::AnisotropicClamp);
		}

		SetBlendMode(info->blendMode);
		SetAlphaTest(AlphaTestMode::None, 1.0f);
		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::CounterClockwise);

		_biggestRoomIndexBuffer = std::fmaxf(_biggestRoomIndexBuffer, (int)_transparentFacesIndices.size());

		int drawnVertices = 0;
		int size = (int)_transparentFacesIndices.size();

		while (drawnVertices < size)
		{
			int count = (drawnVertices + TRANSPARENT_BUCKET_SIZE > size
				? size - drawnVertices
				: TRANSPARENT_BUCKET_SIZE);

			_transparentFacesIndexBuffer.Update(_context.Get(), _transparentFacesIndices, drawnVertices, count);
			_context->IASetIndexBuffer(_transparentFacesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			DrawIndexedTriangles(count, 0, 0);

			_numTransparentDrawCalls++;
			_numRoomsTransparentDrawCalls++;

			drawnVertices += TRANSPARENT_BUCKET_SIZE;
		}

		SetCullMode(CullMode::CounterClockwise);
	}

	void Renderer::DrawStaticsSorted(RendererTransparentFaceInfo* info, bool resetPipeline, RenderView& view)
	{
		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		_context->IASetVertexBuffers(0, 1, _staticsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		// Set shaders
		_context->VSSetShader(_vsStatics.Get(), nullptr, 0);
		_context->PSSetShader(_psStatics.Get(), nullptr, 0);

		// Set texture
		BindTexture(TextureRegister::ColorMap, &std::get<0>(_staticTextures[info->bucket->Texture]),
					SamplerStateRegister::AnisotropicClamp);
		BindTexture(TextureRegister::NormalMap, &std::get<1>(_staticTextures[info->bucket->Texture]),
					SamplerStateRegister::AnisotropicClamp);

		if (resetPipeline)
		{
			_stStatic.World = info->world;
			_stStatic.Color = info->color;
			_stStatic.AmbientLight = info->room->AmbientLight;
			_stStatic.LightMode = (int)_staticObjects[info->staticMesh->ObjectNumber]->ObjectMeshes[0]->LightMode;
			BindStaticLights(info->staticMesh->LightsToDraw);
			_cbStatic.updateData(_stStatic, _context.Get());
			BindConstantBufferVS(ConstantBufferRegister::Static, _cbStatic.get());
			BindConstantBufferPS(ConstantBufferRegister::Static, _cbStatic.get());
		}

		SetBlendMode(info->blendMode);	
		SetAlphaTest(AlphaTestMode::None, 1.0f);
		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::CounterClockwise);

		int drawnVertices = 0;
		int size = (int)_transparentFacesIndices.size();

		while (drawnVertices < size)
		{
			int count = (drawnVertices + TRANSPARENT_BUCKET_SIZE > size
							 ? size - drawnVertices
							 : TRANSPARENT_BUCKET_SIZE);

			_transparentFacesIndexBuffer.Update(_context.Get(), _transparentFacesIndices, drawnVertices, count);
			_context->IASetIndexBuffer(_transparentFacesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			DrawIndexedTriangles(count, 0, 0);

			_numTransparentDrawCalls++;
			_numStaticsTransparentDrawCalls++;

			drawnVertices += TRANSPARENT_BUCKET_SIZE;
		}
	}

	void Renderer::RenderScene(ID3D11RenderTargetView* target, ID3D11DepthStencilView* depthTarget, RenderView& view)
	{
		using ns = std::chrono::nanoseconds;
		using get_time = std::chrono::steady_clock;

		ResetDebugVariables();
		_isLocked = false;
		 
		auto& level = *g_GameFlow->GetLevel(CurrentLevel);

		// Prepare scene to draw.
		auto time1 = std::chrono::high_resolution_clock::now();
		CollectRooms(view, false);
		auto time = std::chrono::high_resolution_clock::now();
		_timeRoomsCollector = (std::chrono::duration_cast<ns>(time - time1)).count() / 1000000;
		time1 = time;

		UpdateLaraAnimations(false);
		UpdateItemAnimations(view);

		_stBlending.AlphaTest = -1;
		_stBlending.AlphaThreshold = -1;

		CollectLightsForCamera();
		RenderItemShadows(view);

		auto time2 = std::chrono::high_resolution_clock::now();
		_timeUpdate = (std::chrono::duration_cast<ns>(time2 - time1)).count() / 1000000;
		time1 = time2;

		// Reset GPU state.
		SetBlendMode(BlendMode::Opaque, true);
		SetDepthState(DepthState::Write, true);
		SetCullMode(CullMode::CounterClockwise, true);

		// Bind and clear render target.
		_context->ClearRenderTargetView(_renderTarget.RenderTargetView.Get(), Colors::Black);
		_context->ClearDepthStencilView(_renderTarget.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		_context->ClearRenderTargetView(_depthMap.RenderTargetView.Get(), Colors::White);
		_context->ClearDepthStencilView(_depthMap.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		ID3D11RenderTargetView* pRenderViewPtrs[2]; 
		pRenderViewPtrs[0] = _renderTarget.RenderTargetView.Get(); 
		pRenderViewPtrs[1] = _depthMap.RenderTargetView.Get(); 
		_context->OMSetRenderTargets(2, &pRenderViewPtrs[0], _renderTarget.DepthStencilView.Get());

		_context->RSSetViewports(1, &view.Viewport);
		ResetScissor();

		// Camera constant buffer contains matrices, camera position, fog values, and other things that are shared for all shaders.
		CCameraMatrixBuffer cameraConstantBuffer;
		view.FillConstantBuffer(cameraConstantBuffer);
		cameraConstantBuffer.Frame = GlobalCounter;
		cameraConstantBuffer.CameraUnderwater = g_Level.Rooms[cameraConstantBuffer.RoomNumber].flags & ENV_FLAG_WATER;

		if (level.GetFogEnabled())
		{
			auto fogColor = level.GetFogColor();
			cameraConstantBuffer.FogColor = Vector4(fogColor.GetR() / 255.0f, fogColor.GetG() / 255.0f, fogColor.GetB() / 255.0f, 1.0f);
			cameraConstantBuffer.FogMinDistance = level.GetFogMinDistance();
			cameraConstantBuffer.FogMaxDistance = level.GetFogMaxDistance();
		}
		else
		{
			cameraConstantBuffer.FogMaxDistance = 0;
			cameraConstantBuffer.FogColor = Vector4::Zero;
		}

		cameraConstantBuffer.NumFogBulbs = (int)view.FogBulbsToDraw.size();

		for (int i = 0; i < view.FogBulbsToDraw.size(); i++)
		{
			cameraConstantBuffer.FogBulbs[i].Position = view.FogBulbsToDraw[i].Position;
			cameraConstantBuffer.FogBulbs[i].Density = view.FogBulbsToDraw[i].Density * (CLICK(1) / view.FogBulbsToDraw[i].Radius);
			cameraConstantBuffer.FogBulbs[i].SquaredRadius = SQUARE(view.FogBulbsToDraw[i].Radius);
			cameraConstantBuffer.FogBulbs[i].Color = view.FogBulbsToDraw[i].Color;
			cameraConstantBuffer.FogBulbs[i].SquaredCameraToFogBulbDistance = SQUARE(view.FogBulbsToDraw[i].Distance);
			cameraConstantBuffer.FogBulbs[i].FogBulbToCameraVector = view.FogBulbsToDraw[i].FogBulbToCameraVector;
		}
		
		_cbCameraMatrices.updateData(cameraConstantBuffer, _context.Get());
		BindConstantBufferVS(ConstantBufferRegister::Camera, _cbCameraMatrices.get());
		BindConstantBufferPS(ConstantBufferRegister::Camera, _cbCameraMatrices.get());
		
		// Draw horizon and sky.
		DrawHorizonAndSky(view, _renderTarget.DepthStencilView.Get());
		
		// Draw opaque and alpha faces.
		DrawRooms(view, RendererPass::Opaque);
		DrawItems(view, RendererPass::Opaque);
		DrawStatics(view, RendererPass::Opaque);
		DrawEffects(view, RendererPass::Opaque);
		DrawGunShells(view);
		DrawBats(view);
		DrawRats(view);
		DrawSpiders(view);
		DrawScarabs(view);
		DrawLocusts(view);
		DrawDebris(view, RendererPass::Opaque);

		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(), _renderTarget.DepthStencilView.Get());

		// Collect all sprites (all sorted like transparent faces)
		DrawFires(view);
		DrawSmokes(view);
		DrawSmokeParticles(view);
		DrawSimpleParticles(view);
		DrawSparkParticles(view);
		DrawExplosionParticles(view);
		DrawFootprints(view);
		DrawBlood(view);
		DrawWeatherParticles(view);
		DrawParticles(view);
		DrawBubbles(view);
		DrawDrips(view);
		DrawRipples(view);
		DrawUnderwaterBloodParticles(view);
		DrawSplashes(view);
		DrawShockwaves(view);
		DrawElectricity(view);
		DrawHelicalLasers(view);
		DrawRopes(view);
		DrawStreamers(view);
		DrawLaserBarriers(view);

		// Collect all transparent faces
		DrawRooms(view, RendererPass::CollectTransparentFaces);
		DrawStatics(view, RendererPass::CollectTransparentFaces);

		/*DrawItems(view, RendererPass::Transparent);
		DrawRooms(view, RendererPass::Transparent);
		DrawItems(view, RendererPass::Transparent);
		DrawStatics(view, RendererPass::Transparent);
		DrawEffects(view, RendererPass::Transparent);
		DrawDebris(view, RendererPass::Transparent);
		DrawGunFlashes(view);
		DrawBaddyGunflashes(view);*/

		// Draw transparent faces and lines
		DrawTransparentFaces(view);
		DrawLines3D(view);

		// Draw post-process effects (cinematic bars, fade, flash, HDR, tone mapping, etc.).
		DrawPostprocess(target, depthTarget, view);

		// Draw GUI elements at end.
		DrawLinesIn2DSpace();

		// Draw HUD.
		g_Hud.Draw(*LaraItem);

		// Draw display sprites sorted by priority.
		CollectDisplaySprites(view);
		DrawDisplaySprites(view);

		// Draw binoculars or lasersight.
		DrawOverlays(view); 

		time2 = std::chrono::high_resolution_clock::now();
		_timeFrame = (std::chrono::duration_cast<ns>(time2 - time1)).count() / 1000000;
		time1 = time2;
		
		DrawDebugInfo(view);
		DrawAllStrings();

		ClearScene();
	}

	void Renderer::RenderSimpleScene(ID3D11RenderTargetView* target, ID3D11DepthStencilView* depthTarget,
									   RenderView& view)
	{
		CollectRooms(view, true);
		// Draw shadow map

		// Reset GPU state
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);

		// Bind and clear render target

		_context->ClearRenderTargetView(target, Colors::Black);
		_context->ClearDepthStencilView(depthTarget, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		_context->OMSetRenderTargets(1, &target, depthTarget);

		_context->RSSetViewports(1, &view.Viewport);
		ResetScissor();

		// Opaque geometry
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);

		CCameraMatrixBuffer cameraConstantBuffer;
		view.FillConstantBuffer(cameraConstantBuffer);
		cameraConstantBuffer.CameraUnderwater = g_Level.Rooms[cameraConstantBuffer.RoomNumber].flags & ENV_FLAG_WATER;
		_cbCameraMatrices.updateData(cameraConstantBuffer, _context.Get());
		BindConstantBufferVS(ConstantBufferRegister::Camera, _cbCameraMatrices.get());
		DrawHorizonAndSky(view, depthTarget);
		DrawRooms(view, RendererPass::Opaque);
	}

	void Renderer::DumpGameScene()
	{
		RenderScene(_dumpScreenRenderTarget.RenderTargetView.Get(), _dumpScreenRenderTarget.DepthStencilView.Get(),
					_gameCamera);
	}

	void Renderer::DrawItems(RenderView& view, RendererPass rendererPass)
	{
		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		// Set shaders
		_context->VSSetShader(_vsItems.Get(), nullptr, 0);
		_context->PSSetShader(_psItems.Get(), nullptr, 0);

		for (auto room : view.RoomsToDraw)
		{
			for (auto itemToDraw : room->ItemsToDraw)
			{
				switch (itemToDraw->ObjectNumber)
				{
				case ID_LARA:
					DrawLara(view, rendererPass);
					break;

				case ID_WATERFALL1:
				case ID_WATERFALL2:
				case ID_WATERFALL3:
				case ID_WATERFALL4:
				case ID_WATERFALL5:
				case ID_WATERFALL6:
				case ID_WATERFALLSS1:
				case ID_WATERFALLSS2:
					DrawWaterfalls(itemToDraw, view, 10, rendererPass);
					continue;

				default:
					DrawAnimatingItem(itemToDraw, view, rendererPass);
					break;
				}
			}
		}
	}

	void Renderer::RenderItemShadows(RenderView& renderView)
	{
		RenderBlobShadows(renderView);

		if (g_Configuration.ShadowType != ShadowMode::None)
		{
			for (auto room : renderView.RoomsToDraw)
				for (auto itemToDraw : room->ItemsToDraw)
					RenderShadowMap(itemToDraw, renderView);
		}
	}

	void Renderer::DrawWaterfalls(RendererItem* item, RenderView& view, int fps, RendererPass rendererPass)
	{
		// Extremely hacky function to get first rendered face of a waterfall object mesh, calculate
		// its texture height and scroll all the textures according to that height.

		RendererObject& moveableObj = *_moveableObjects[item->ObjectNumber];

		// No mesh or bucket, abort
		if (!moveableObj.ObjectMeshes.size() || !moveableObj.ObjectMeshes[0]->Buckets.size())
			return;

		// Get first three vertices of a waterfall object, meaning the very first triangle
		const auto& v1 = _moveablesVertices[moveableObj.ObjectMeshes[0]->Buckets[0].StartVertex + 0];
		const auto& v2 = _moveablesVertices[moveableObj.ObjectMeshes[0]->Buckets[0].StartVertex + 1];
		const auto& v3 = _moveablesVertices[moveableObj.ObjectMeshes[0]->Buckets[0].StartVertex + 2];

		// Calculate height of the texture by getting min/max UV.y coords of all three vertices
		auto minY = std::min(std::min(v1.UV.y, v2.UV.y), v3.UV.y);
		auto maxY = std::max(std::max(v1.UV.y, v2.UV.y), v3.UV.y);
		auto minX = std::min(std::min(v1.UV.x, v2.UV.x), v3.UV.x);
		auto maxX = std::max(std::max(v1.UV.x, v2.UV.x), v3.UV.x);

		// Setup animated buffer
		_stAnimated.Fps = fps;
		_stAnimated.NumFrames = 1;
		_stAnimated.Type = 1; // UVRotate

		// We need only top/bottom Y coordinate for UVRotate, but we pass whole
		// rectangle anyway, in case later we may want to implement different UVRotate modes.
		_stAnimated.Textures[0].topLeft     = Vector2(minX, minY);
		_stAnimated.Textures[0].topRight    = Vector2(maxX, minY);
		_stAnimated.Textures[0].bottomLeft  = Vector2(minX, maxY);
		_stAnimated.Textures[0].bottomRight = Vector2(maxX, maxY);
		
		_cbAnimated.updateData(_stAnimated, _context.Get());
		BindConstantBufferPS(ConstantBufferRegister::AnimatedTextures, _cbAnimated.get());

		DrawAnimatingItem(item, view, rendererPass);

		// Reset animated buffer after rendering just in case
		_stAnimated.Fps = _stAnimated.NumFrames = _stAnimated.Type = 0;
		_cbAnimated.updateData(_stAnimated, _context.Get());
	}

	void Renderer::DrawAnimatingItem(RendererItem* item, RenderView& view, RendererPass rendererPass)
	{
		ItemInfo* nativeItem = &g_Level.Items[item->ItemNumber];
		RendererRoom* room = &_rooms[item->RoomNumber];
		RendererObject& moveableObj = *_moveableObjects[item->ObjectNumber];

		// Bind item main properties
		_stItem.World = item->World;
		_stItem.Color = item->Color;
		_stItem.AmbientLight = item->AmbientLight;
		memcpy(_stItem.BonesMatrices, item->AnimationTransforms, sizeof(Matrix) * MAX_BONES);

		for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
			_stItem.BoneLightModes[k] = (int)moveableObj.ObjectMeshes[k]->LightMode;

		BindMoveableLights(item->LightsToDraw, item->RoomNumber, item->PrevRoomNumber, item->LightFade);
		_cbItem.updateData(_stItem, _context.Get());
		BindConstantBufferVS(ConstantBufferRegister::Item, _cbItem.get());
		BindConstantBufferPS(ConstantBufferRegister::Item, _cbItem.get());

		for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
		{
			if (!(nativeItem->MeshBits & (1 << k)))
				continue;

			DrawMoveableMesh(item, GetMesh(item->MeshIndex[k]), room, k, rendererPass);
		}
	}

	void Renderer::DrawItemsSorted(RendererTransparentFaceInfo* info, bool resetPipeline, RenderView& view)
	{
		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		ItemInfo* nativeItem = &g_Level.Items[info->item->ItemNumber];
		RendererRoom& room = _rooms[nativeItem->RoomNumber];
		RendererObject& moveableObj = *_moveableObjects[nativeItem->ObjectNumber];

		// Set shaders
		_context->VSSetShader(_vsItems.Get(), nullptr, 0);
		_context->PSSetShader(_psItems.Get(), nullptr, 0);

		if (resetPipeline)
		{
			_stItem.World = info->item->World;
			_stItem.Color = info->color;
			_stItem.AmbientLight = info->item->AmbientLight;
			memcpy(_stItem.BonesMatrices, info->item->AnimationTransforms, sizeof(Matrix) * MAX_BONES);

			for (int k = 0; k < (int)moveableObj.ObjectMeshes.size(); k++)
				_stItem.BoneLightModes[k] = (int)moveableObj.ObjectMeshes[k]->LightMode;

			BindMoveableLights(info->item->LightsToDraw, info->item->RoomNumber, info->item->PrevRoomNumber, info->item->LightFade);
			_cbItem.updateData(_stItem, _context.Get());
			BindConstantBufferVS(ConstantBufferRegister::Item, _cbItem.get());
			BindConstantBufferPS(ConstantBufferRegister::Item, _cbItem.get());
		}

		// Set texture
		BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[info->bucket->Texture]),
					SamplerStateRegister::AnisotropicClamp);
		BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[info->bucket->Texture]),
					SamplerStateRegister::AnisotropicClamp);

		SetBlendMode(info->blendMode);
		SetDepthState(DepthState::Read);
		SetAlphaTest(AlphaTestMode::None, 1.0f);
		SetCullMode(CullMode::CounterClockwise);

		int drawnVertices = 0;
		int size = (int)_transparentFacesIndices.size();

		while (drawnVertices < size)
		{
			int count = (drawnVertices + TRANSPARENT_BUCKET_SIZE > size
							 ? size - drawnVertices
							 : TRANSPARENT_BUCKET_SIZE);

			_transparentFacesIndexBuffer.Update(_context.Get(), _transparentFacesIndices, drawnVertices, count);
			_context->IASetIndexBuffer(_transparentFacesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			DrawIndexedTriangles(count, 0, 0);

			_numTransparentDrawCalls++;
			_numStaticsTransparentDrawCalls++;

			drawnVertices += TRANSPARENT_BUCKET_SIZE;
		}
	}

	void Renderer::DrawStatics(RenderView& view, RendererPass rendererPass)
	{
		if (_staticTextures.size() == 0 || view.SortedStaticsToDraw.size() == 0)
		{
			return;
		}
		 
		if (rendererPass != RendererPass::CollectTransparentFaces)
		{
			_context->VSSetShader(_vsInstancedStaticMeshes.Get(), NULL, 0);
			_context->PSSetShader(_psInstancedStaticMeshes.Get(), NULL, 0);

			BindConstantBufferVS(ConstantBufferRegister::Static, _cbStatic.get());
			BindConstantBufferPS(ConstantBufferRegister::Static, _cbStatic.get());

			BindConstantBufferVS(ConstantBufferRegister::InstancedStatics, _cbInstancedStaticMeshBuffer.get());
			BindConstantBufferPS(ConstantBufferRegister::InstancedStatics, _cbInstancedStaticMeshBuffer.get());

			// Bind vertex and index buffer
			UINT stride = sizeof(Vertex);
			UINT offset = 0;

			_context->IASetVertexBuffers(0, 1, _staticsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_context->IASetInputLayout(_inputLayout.Get());
			_context->IASetIndexBuffer(_staticsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			for (auto it = view.SortedStaticsToDraw.begin(); it != view.SortedStaticsToDraw.end(); it++)
			{
				std::vector<RendererStatic*> statics = it->second;

				RendererStatic* refStatic = statics[0];
				RendererObject& refStaticObj = *_staticObjects[refStatic->ObjectNumber];
				if (refStaticObj.ObjectMeshes.size() == 0)
					continue;

				RendererMesh* refMesh = refStaticObj.ObjectMeshes[0];

				int staticsCount = (int)statics.size();
				int bucketSize = INSTANCED_STATIC_MESH_BUCKET_SIZE;
				int baseStaticIndex = 0;

				while (baseStaticIndex < staticsCount)
				{
					int k = 0;
					int instanceCount = std::min(bucketSize, staticsCount - baseStaticIndex);
					int max = std::min(baseStaticIndex + bucketSize, staticsCount);

					for (int s = baseStaticIndex; s < max; s++)
					{
						RendererStatic* current = statics[s];
						RendererRoom* room = &_rooms[current->RoomNumber];

						_stInstancedStaticMeshBuffer.StaticMeshes[k].World = current->World;
						_stInstancedStaticMeshBuffer.StaticMeshes[k].Color = current->Color;
						_stInstancedStaticMeshBuffer.StaticMeshes[k].Ambient = room->AmbientLight;
						_stInstancedStaticMeshBuffer.StaticMeshes[k].LightMode = (int)refMesh->LightMode;
						BindInstancedStaticLights(current->LightsToDraw, k);
						k++;
					}

					baseStaticIndex += bucketSize;

					for (auto& bucket : refMesh->Buckets)
					{
						if (bucket.NumVertices == 0)
						{
							continue;
						}

						if (bucket.BlendMode == BlendMode::Opaque || bucket.BlendMode == BlendMode::AlphaTest)
						{
							_cbInstancedStaticMeshBuffer.updateData(_stInstancedStaticMeshBuffer, _context.Get());

							int passes = bucket.BlendMode == BlendMode::AlphaTest ? 2 : 1;

							for (int pass = 0; pass < passes; pass++)
							{
								if (pass == 0)
								{
									SetBlendMode(bucket.BlendMode);
									SetAlphaTest(
										(bucket.BlendMode == BlendMode::AlphaTest) ? AlphaTestMode::GreatherThan : AlphaTestMode::None,
										ALPHA_TEST_THRESHOLD);
								}
								else
								{
									SetBlendMode(BlendMode::AlphaBlend);
									SetAlphaTest(AlphaTestMode::LessThan, FAST_ALPHA_BLEND_THRESHOLD);
								}

								BindTexture(TextureRegister::ColorMap,
									&std::get<0>(_staticTextures[bucket.Texture]),
									SamplerStateRegister::AnisotropicClamp);
								BindTexture(TextureRegister::NormalMap,
									&std::get<1>(_staticTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

								DrawIndexedInstancedTriangles(bucket.NumIndices, instanceCount, bucket.StartIndex, 0);

								_numStaticsDrawCalls++;
							}
						}
					}
				}
			}
		}
		else
		{
			// Collect sorted blend modes faces ordered by room, if transparent pass

			for (auto it = view.SortedStaticsToDraw.begin(); it != view.SortedStaticsToDraw.end(); it++)
			{
				std::vector<RendererStatic*> statics = it->second;

				RendererStatic* refStatic = statics[0];
				RendererObject& refStaticObj = *_staticObjects[refStatic->ObjectNumber];
				if (refStaticObj.ObjectMeshes.size() == 0)
					continue;

				RendererMesh* refMesh = refStaticObj.ObjectMeshes[0];

				for (int i = 0; i < statics.size(); i++)
				{
					for (int j = 0; j < refMesh->Buckets.size(); j++)
					{
						auto& bucket = refMesh->Buckets[j];

						if (bucket.NumVertices == 0)
						{
							continue;
						}

						if (bucket.BlendMode != BlendMode::Opaque && bucket.BlendMode != BlendMode::AlphaTest)
						{
							// Collect transparent bucket
							RendererSortableObject object;

							object.ObjectType = RendererObjectType::Static;
							object.Bucket = &bucket;
							object.Static = statics[i];
							object.Centre = Vector3::Transform(bucket.Centre, statics[i]->World);
							object.Distance = Vector3::Distance(object.Centre, view.Camera.WorldPosition);

							view.TransparentObjectsToDraw.push_back(object);
						}
					}
				}
			}
		}
	}

	void Renderer::DrawTransparentFaces(RenderView& renderView)
	{
		std::sort(
			renderView.TransparentObjectsToDraw.begin(),
			renderView.TransparentObjectsToDraw.end(),
			[](RendererSortableObject& a, RendererSortableObject& b)
			{
				return (a.Distance > b.Distance);
			}
		);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		// Bind vertex and index buffer.
		_context->IASetVertexBuffers(0, 1, _roomsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetIndexBuffer(_roomsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		// Bind pixel shaders.
		_context->PSSetShader(_psRooms.Get(), nullptr, 0);

		BindConstantBufferVS(ConstantBufferRegister::Room, _cbRoom.get());
		BindConstantBufferPS(ConstantBufferRegister::Room, _cbRoom.get());

		// Set shadow map data and bind shadow map texture.
		if (_shadowLight != nullptr)
		{
			BindTexture(TextureRegister::ShadowMap, &_shadowMap, SamplerStateRegister::ShadowMap);
		}

		/*short lastRoomNumber = -1;
		RendererRoom* room;
		ROOM_INFO* nativeRoom;
		int numTransparentFaces = 0;
		RendererObjectType lastObjectType = RendererObjectType::Unknown;

		for (int i = 0; i < renderView.TransparentObjectsToDraw.size(); i++)
		{
			auto& object = renderView.TransparentObjectsToDraw[i];

			if (object.ObjectType != lastObjectType || i == renderView.TransparentObjectsToDraw.size() - 1)
			{
				if (lastObjectType != RendererObjectType::Unknown)
				{
					// Output the polygons

				}
				// Reset the pipeline
				if ()
			}

			if (object.RoomNumber != lastRoomNumber)
			{
				room = &_rooms[object.RoomNumber];
				_cbShadowMap.updateData(_stShadowMap, _context.Get());

				BindConstantBufferPS(ConstantBufferRegister::ShadowLight, _cbShadowMap.get());
				BindConstantBufferVS(ConstantBufferRegister::ShadowLight, _cbShadowMap.get());

				nativeRoom = &g_Level.Rooms[room->RoomNumber];

				_stRoom.Caustics = (int)(g_Configuration.EnableCaustics && (nativeRoom->flags & ENV_FLAG_WATER));
				_stRoom.AmbientColor = room->AmbientLight;
				_stRoom.Water = (nativeRoom->flags & ENV_FLAG_WATER) != 0 ? 1 : 0;
				BindRoomLights(renderView.LightsToDraw);
				_cbRoom.updateData(_stRoom, _context.Get());

				SetScissor(room->ClipBounds);
			}

			RendererBucket& bucket = room->Buckets[object.BucketNumber];

			for (int animated = 0; animated < 2; animated++)
			{
				if (animated == 0)
				{
					_context->VSSetShader(_vsRooms.Get(), nullptr, 0);
				}
				else
				{
					_context->VSSetShader(_vsRoomsAnimatedTextures.Get(), nullptr, 0);
					BindConstantBufferVS(ConstantBufferRegister::AnimatedTextures, _cbAnimated.get());
				}

				if (bucket.Animated)
				{
					_context->VSSetShader(_vsRoomsAnimatedTextures.Get(), nullptr, 0);
					BindConstantBufferVS(ConstantBufferRegister::AnimatedTextures, _cbAnimated.get());
				}
				else
				{
					_context->VSSetShader(_vsRooms.Get(), nullptr, 0);
				}

				SetBlendMode(bucket.BlendMode);
				SetAlphaTest(AlphaTestMode::None, FAST_ALPHA_BLEND_THRESHOLD);

				// Draw geometry
				if (bucket.Animated)
				{
					BindTexture(TextureRegister::ColorMap,
						&std::get<0>(_animatedTextures[bucket.Texture]),
						SamplerStateRegister::AnisotropicClamp);
					BindTexture(TextureRegister::NormalMap,
						&std::get<1>(_animatedTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

					RendererAnimatedTextureSet& set = _animatedTextureSets[bucket.Texture];
					_stAnimated.NumFrames = set.NumTextures;
					_stAnimated.Type = 0;
					_stAnimated.Fps = set.Fps;

					for (unsigned char j = 0; j < set.NumTextures; j++)
					{
						if (j >= _stAnimated.Textures.size())
						{
							TENLog("Animated frame " + std::to_string(j) + " is out of bounds, too many frames in sequence.");
							break;
						}

						_stAnimated.Textures[j].topLeft = set.Textures[j].UV[0];
						_stAnimated.Textures[j].topRight = set.Textures[j].UV[1];
						_stAnimated.Textures[j].bottomRight = set.Textures[j].UV[2];
						_stAnimated.Textures[j].bottomLeft = set.Textures[j].UV[3];
					}
					_cbAnimated.updateData(_stAnimated, _context.Get());
				}
				else
				{
					BindTexture(TextureRegister::ColorMap, &std::get<0>(_roomTextures[bucket.Texture]),
						SamplerStateRegister::AnisotropicClamp);
					BindTexture(TextureRegister::NormalMap,
						&std::get<1>(_roomTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
				}

				DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

				_numRoomsDrawCalls++;

				numTransparentFaces += bucket.NumIndices / 3;
			}
		}*/

		ResetScissor();
	}

	void Renderer::DrawRooms(RenderView& view, RendererPass rendererPass)
	{
		if (rendererPass == RendererPass::CollectTransparentFaces)
		{
			for (int i = (int)view.RoomsToDraw.size() - 1; i >= 0; i--)
			{
				int index = i;
				RendererRoom* room = view.RoomsToDraw[index];

				for (int animated = 0; animated < 2; animated++)
				{
					for (int j = 0; j < room->Buckets.size(); j++)
					{
						auto& bucket = room->Buckets[j];

						if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
						{
							continue;
						}

						if (rendererPass == RendererPass::CollectTransparentFaces)
						{
							if (bucket.BlendMode != BlendMode::Opaque && bucket.BlendMode != BlendMode::AlphaTest)
							{
								RendererSortableObject object;
								object.ObjectType = RendererObjectType::Room;
								object.Centre = bucket.Centre;
								object.Distance = Vector3::Distance(view.Camera.WorldPosition, bucket.Centre);
								object.Bucket = &bucket;
								object.Room = room;

								view.TransparentObjectsToDraw.push_back(object);
							}
						}
					}
				}
			}
		}
		else
		{
			UINT stride = sizeof(Vertex);
			UINT offset = 0;

			// Bind vertex and index buffer.
			_context->IASetVertexBuffers(0, 1, _roomsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_context->IASetInputLayout(_inputLayout.Get());
			_context->IASetIndexBuffer(_roomsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			// Bind pixel shaders.
			_context->PSSetShader(_psRooms.Get(), nullptr, 0);

			BindConstantBufferVS(ConstantBufferRegister::Room, _cbRoom.get());
			BindConstantBufferPS(ConstantBufferRegister::Room, _cbRoom.get());

			// Bind caustics texture.
			if (!_sprites.empty())
			{
				int nmeshes = -Objects[ID_CAUSTICS_TEXTURES].nmeshes;
				int meshIndex = Objects[ID_CAUSTICS_TEXTURES].meshIndex;
				int causticsFrame = std::min(nmeshes ? meshIndex + ((GlobalCounter) % nmeshes) : meshIndex, (int)_sprites.size());
				BindTexture(TextureRegister::CausticsMap, _sprites[causticsFrame].Texture, SamplerStateRegister::AnisotropicClamp);

				// NOTE: Strange packing due to particular HLSL 16 bytes alignment requirements.
				RendererSprite* causticsSprite = &_sprites[causticsFrame];
				_stRoom.CausticsStartUV = causticsSprite->UV[0];
				_stRoom.CausticsScale = Vector2(causticsSprite->Width / (float)causticsSprite->Texture->Width, causticsSprite->Height / (float)causticsSprite->Texture->Height);
			}

			// Set shadow map data and bind shadow map texture.
			if (_shadowLight != nullptr)
			{
				memcpy(&_stShadowMap.Light, _shadowLight, sizeof(ShaderLight));
				_stShadowMap.ShadowMapSize = g_Configuration.ShadowMapSize;
				_stShadowMap.CastShadows = true;

				BindTexture(TextureRegister::ShadowMap, &_shadowMap, SamplerStateRegister::ShadowMap);
			}
			else
			{
				_stShadowMap.CastShadows = false;
			}

			_numRoomsTransparentPolygons = 0;
			for (int i = (int)view.RoomsToDraw.size() - 1; i >= 0; i--)
			{
				int index = i;
				RendererRoom* room = view.RoomsToDraw[index];
				_cbShadowMap.updateData(_stShadowMap, _context.Get());

				BindConstantBufferPS(ConstantBufferRegister::ShadowLight, _cbShadowMap.get());
				BindConstantBufferVS(ConstantBufferRegister::ShadowLight, _cbShadowMap.get());

				ROOM_INFO* nativeRoom = &g_Level.Rooms[room->RoomNumber];

				Vector3 cameraPosition = Vector3(Camera.pos.x, Camera.pos.y, Camera.pos.z);
				Vector3 roomPosition = Vector3(nativeRoom->x, nativeRoom->y, nativeRoom->z);

				_stRoom.Caustics = (int)(g_Configuration.EnableCaustics && (nativeRoom->flags & ENV_FLAG_WATER));
				_stRoom.AmbientColor = room->AmbientLight;
				_stRoom.Water = (nativeRoom->flags & ENV_FLAG_WATER) != 0 ? 1 : 0;
				BindRoomLights(view.LightsToDraw);
				_cbRoom.updateData(_stRoom, _context.Get());

				SetScissor(room->ClipBounds);

				for (int animated = 0; animated < 2; animated++)
				{
					if (animated == 0)
					{
						_context->VSSetShader(_vsRooms.Get(), nullptr, 0);
					}
					else
					{
						_context->VSSetShader(_vsRoomsAnimatedTextures.Get(), nullptr, 0);
						BindConstantBufferVS(ConstantBufferRegister::AnimatedTextures, _cbAnimated.get());
					}

					for (auto& bucket : room->Buckets)
					{
						if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
						{
							continue;
						}

						if (bucket.BlendMode == BlendMode::Opaque || bucket.BlendMode == BlendMode::AlphaTest)
						{
							int passes = bucket.BlendMode == BlendMode::AlphaTest ? 2 : 1;

							for (int pass = 0; pass < passes; pass++)
							{
								if (pass == 0)
								{
									SetBlendMode(bucket.BlendMode);
									SetAlphaTest(
										bucket.BlendMode == BlendMode::AlphaTest ? AlphaTestMode::GreatherThan : AlphaTestMode::None,
										FAST_ALPHA_BLEND_THRESHOLD
									);
								}
								else
								{
									SetBlendMode(BlendMode::AlphaBlend);
									SetAlphaTest(AlphaTestMode::LessThan, FAST_ALPHA_BLEND_THRESHOLD);
								}

								// Draw geometry
								if (animated)
								{
									BindTexture(TextureRegister::ColorMap,
										&std::get<0>(_animatedTextures[bucket.Texture]),
										SamplerStateRegister::AnisotropicClamp);
									BindTexture(TextureRegister::NormalMap,
										&std::get<1>(_animatedTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

									RendererAnimatedTextureSet& set = _animatedTextureSets[bucket.Texture];
									_stAnimated.NumFrames = set.NumTextures;
									_stAnimated.Type = 0;
									_stAnimated.Fps = set.Fps;

									for (unsigned char j = 0; j < set.NumTextures; j++)
									{
										if (j >= _stAnimated.Textures.size())
										{
											TENLog("Animated frame " + std::to_string(j) + " is out of bounds, too many frames in sequence.");
											break;
										}

										_stAnimated.Textures[j].topLeft = set.Textures[j].UV[0];
										_stAnimated.Textures[j].topRight = set.Textures[j].UV[1];
										_stAnimated.Textures[j].bottomRight = set.Textures[j].UV[2];
										_stAnimated.Textures[j].bottomLeft = set.Textures[j].UV[3];
									}
									_cbAnimated.updateData(_stAnimated, _context.Get());
								}
								else
								{
									BindTexture(TextureRegister::ColorMap, &std::get<0>(_roomTextures[bucket.Texture]),
										SamplerStateRegister::AnisotropicClamp);
									BindTexture(TextureRegister::NormalMap,
										&std::get<1>(_roomTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
								}

								DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

								_numRoomsDrawCalls++;
							}
						}
					}
				}
			}

			ResetScissor();
		}
	}
	
	void Renderer::DrawHorizonAndSky(RenderView& renderView, ID3D11DepthStencilView* depthTarget)
	{
		auto* levelPtr = g_GameFlow->GetLevel(CurrentLevel);

		bool anyOutsideRooms = false;
		for (int k = 0; k < renderView.RoomsToDraw.size(); k++)
		{
			const auto& nativeRoom = g_Level.Rooms[renderView.RoomsToDraw[k]->RoomNumber];
			if (nativeRoom.flags & ENV_FLAG_OUTSIDE)
			{
				anyOutsideRooms = true;
				break;
			}
		}

		if (!levelPtr->Horizon || !anyOutsideRooms)
			return;

		if (Lara.Control.Look.OpticRange != 0)
			AlterFOV(ANGLE(DEFAULT_FOV) - Lara.Control.Look.OpticRange, false);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		// Draw sky.
		auto rotation = Matrix::CreateRotationX(PI);

		_context->VSSetShader(_vsSky.Get(), nullptr, 0);
		_context->PSSetShader(_psSky.Get(), nullptr, 0);

		BindTexture(TextureRegister::ColorMap, &_skyTexture, SamplerStateRegister::AnisotropicClamp);

		_context->IASetVertexBuffers(0, 1, _skyVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetIndexBuffer(_skyIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		SetBlendMode(BlendMode::Additive);

		for (int s = 0; s < 2; s++)
		{
			for (int i = 0; i < 2; i++)
			{
				auto weather = TEN::Effects::Environment::Weather;

				auto translation = Matrix::CreateTranslation(Camera.pos.x + weather.SkyPosition(s) - i * SKY_SIZE,
					Camera.pos.y - 1536.0f, Camera.pos.z);
				auto world = rotation * translation;

				_stSky.World = (rotation * translation);
				_stSky.Color = weather.SkyColor(s);
				_stSky.ApplyFogBulbs = s == 0 ? 1 : 0;

				_cbSky.updateData(_stSky, _context.Get());
				BindConstantBufferVS(ConstantBufferRegister::Static, _cbSky.get());
				BindConstantBufferPS(ConstantBufferRegister::Static, _cbSky.get());

				DrawIndexedTriangles(SKY_INDICES_COUNT, 0, 0);
			}
		}

		_context->ClearDepthStencilView(depthTarget, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

		// Draw horizon.
		if (_moveableObjects[ID_HORIZON].has_value())
		{
			_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_context->IASetInputLayout(_inputLayout.Get());
			_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			auto& moveableObj = *_moveableObjects[ID_HORIZON];

			_stSky.World = Matrix::CreateTranslation(Camera.pos.x, Camera.pos.y, Camera.pos.z);
			_stSky.Color = Vector4::One;
			_stSky.ApplyFogBulbs = 1;

			_cbSky.updateData(_stSky, _context.Get());
			BindConstantBufferVS(ConstantBufferRegister::Static, _cbSky.get());
			BindConstantBufferPS(ConstantBufferRegister::Static, _cbSky.get());

			for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
			{
				auto* meshPtr = moveableObj.ObjectMeshes[k];

				for (auto& bucket : meshPtr->Buckets)
				{
					if (bucket.NumVertices == 0)
						continue;

					BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]),
						SamplerStateRegister::AnisotropicClamp);
					BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]),
						SamplerStateRegister::AnisotropicClamp);

					// Always render horizon as alpha-blended surface.
					SetBlendMode(bucket.BlendMode == BlendMode::AlphaTest ? BlendMode::AlphaBlend : bucket.BlendMode);
					SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

					// Draw vertices.
					DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

					_numMoveablesDrawCalls++;
				}
			}
		}

		// Clear just the Z-buffer to start drawing on top of horizon.
		_context->ClearDepthStencilView(depthTarget, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	void Renderer::Render()
	{
		//RenderToCubemap(reflectionCubemap, Vector3(LaraItem->pos.xPos, LaraItem->pos.yPos - 1024, LaraItem->pos.zPos), LaraItem->roomNumber);
		RenderScene(_backBuffer.RenderTargetView.Get(), _backBuffer.DepthStencilView.Get(), _gameCamera);
		_context->ClearState();
		_swapChain->Present(1, 0);
	}

	void Renderer::DrawMoveableMesh(RendererItem* itemToDraw, RendererMesh* mesh, RendererRoom* room, int boneIndex, RendererPass rendererPass)
	{
		auto cameraPos = Camera.pos.ToVector3();

		for (auto& bucket : mesh->Buckets)
		{
			if (bucket.NumVertices == 0)
				continue;

			if (rendererPass == RendererPass::ShadowMap)
			{
				SetBlendMode(BlendMode::Opaque);
				SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

				DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

				_numMoveablesDrawCalls++;
			}
			else if (rendererPass == RendererPass::CollectTransparentFaces)
			{
				if (DoesBlendModeRequireSorting(bucket.BlendMode))
				{
					// Collect transparent faces
					for (int j = 0; j < bucket.Polygons.size(); j++)
					{
						auto* polygonPtr = &bucket.Polygons[j];

						// Use averaged distance as polygon distance for moveables.
						auto centre = Vector3::Transform(
							polygonPtr->Centre, itemToDraw->AnimationTransforms[boneIndex] * itemToDraw->World);
						int distance = (centre - cameraPos).Length();

						RendererTransparentFace face;
						face.type = TransparentFaceType::Moveable;
						face.info.polygon = polygonPtr;
						face.distance = distance;
						face.info.animated = bucket.Animated;
						face.info.texture = bucket.Texture;
						face.info.room = room;
						face.info.item = itemToDraw;
						face.info.color = itemToDraw->Color;
						face.info.blendMode = bucket.BlendMode;
						face.info.bucket = &bucket;
						room->TransparentFacesToDraw.push_back(face);
					}
				}
			}
			else
			{
				if (!((bucket.BlendMode == BlendMode::Opaque || bucket.BlendMode == BlendMode::AlphaTest) ^
					(rendererPass == RendererPass::Transparent || rendererPass == RendererPass::CollectTransparentFaces)))
				{
					continue;
				}

				int passes = bucket.BlendMode == BlendMode::AlphaTest ? 2 : 1;

				BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]),
							SamplerStateRegister::AnisotropicClamp);
				BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]),
					SamplerStateRegister::AnisotropicClamp);

				for (int pass = 0; pass < passes; pass++)
				{
					if (pass == 0)
					{
						SetBlendMode(bucket.BlendMode);
						SetAlphaTest(
							bucket.BlendMode == BlendMode::AlphaTest ? AlphaTestMode::GreatherThan : AlphaTestMode::None,
							ALPHA_TEST_THRESHOLD);
					}
					else
					{
						SetBlendMode(BlendMode::AlphaBlend);
						SetAlphaTest(AlphaTestMode::LessThan, FAST_ALPHA_BLEND_THRESHOLD);
					}

					DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

					_numMoveablesDrawCalls++;
				}
			}
		}
	}
}
