#pragma once

#include "Hydro.h"
#include "Hydro/Scene/UI/UI.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/BrushRules.h"
#include "Rules/FarmRules.h"
#include "Rules/HeightRules.h"
#include "Rules/MountainRules.h"
#include "Rules/SeasonRules.h"
#include "Rules/SoilRules.h"
#include "State/HearthlandsSession.h"
#include "Views/BrushCursorView.h"

#include <vector>

namespace Hydro
{
	// The player's terrain brush on the Terrain entity: the left mouse paints the session's brush in a round stamp along the
	// drag (a ring shows where); Raise (the rock type) lifts a dome per press, Dig lowers raised land, then digs riverbed.
	HY_SCRIPT()
	class BrushScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultTerrainTypes = 4392505292887402202ull; // Assets/Data/TerrainTypes.hytable

		HY_PROPERTY(Asset(DataTable, TerrainTypeDefinition), Tooltip("Which type raises (Rock) and the levels rock and snow start at"))
		AssetHandle TerrainTypes = DefaultTerrainTypes;
		HY_PROPERTY(Range(1, 32), Tooltip("Most levels between neighbouring land tiles: steeper strokes slump outwards"))
		int MaxStep = 2;

	protected:
		void OnCreate() override
		{
			m_Cursor.Bind(*GetEntity().GetScene());
			std::vector<TerrainTypeDefinition> types;
			const DataTableView<TerrainTypeDefinition> table = Data::GetTable<TerrainTypeDefinition>(TerrainTypes);
			for (const auto& [id, type] : table.Rows())
			{
				types.push_back(type);
			}
			m_Mountains = MountainRules::FromTypes(types, MountainRules::Settings{});
			m_Soil = SoilRules::FromTypes(types, SoilRules::Settings{});
			m_Heights.MaxStep = MaxStep;
		}

		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			HearthlandsSession& session = HearthlandsSession::Get();
			const Scene::UIPointer& pointer = GetEntity().GetScene()->GetUIPointer();
			glm::vec2 point(0.0f);
			float pointZ = 0.0f;
			const bool overUI = session.PointerOverHud || session.PointerOverMenu;
			const bool overLand = pointer.Inside && overUI == false && PointUnderPointer(session, point, pointZ) && IsLand(session, point);
			// A settlement's centre under the pointer: a small ring, and a new press there selects it instead of painting.
			const int hovered = overLand ? SettlementAt(session, point) : 0;
			const float radius = hovered != 0 ? 0.75f : session.Brush.Radius;
			m_Cursor.Present(overLand, GroundPoint(session.Map, point) + glm::vec3(0.0f, 0.0f, pointZ), radius);
			const bool press = overLand && pointer.Down && m_WasDown == false && UI::CapturesInput(*GetEntity().GetScene()) == false;
			m_WasDown = pointer.Down;
			if (press && hovered != 0)
			{
				session.SelectedSettlement = hovered;
				m_HeldSinceSelect = true;
			}
			m_HeldSinceSelect = m_HeldSinceSelect && pointer.Down; // the press that selected a settlement paints nothing
			const bool painting = overLand && pointer.Down && UI::CapturesInput(*GetEntity().GetScene()) == false
				&& m_HeldSinceSelect == false;
			session.PointerOnLand = overLand;
			session.PointerTile = point;
			session.Painting = painting && session.UnlockedTiles.empty() == false;
			if (painting == false || session.UnlockedTiles.empty())
			{
				m_Stroking = false;
				m_ToldReach = false;
				return;
			}

			const uint8_t tileId = BrushRules::BrushTile(session.Brush.TileId, session.UnlockedTiles);
			m_Mountains.SnowShift = SeasonRules::Step(session.Season.SnowShift, 4.0f); // as SimulationScript settles it
			const bool raise = (int)tileId == m_Mountains.RockTileId;
			const bool dig = tileId == session.Water.BedTileId;
			if (m_Stroking == false)
			{
				m_Stroke.Begin(session.Map.GetTiles().size()); // a new press: every tile moves at most its bump from here
				m_OutOfReach = false;
				m_PaintedInReach = false;
			}
			m_Heights.WaterTileId = session.Water.WaterTileId;
			m_Heights.BedTileId = session.Water.BedTileId;
			std::vector<uint32_t> moved;
			const glm::vec2 from = m_Stroking ? m_LastPoint : point;
			for (const glm::vec2& step : BrushRules::Stroke(from, point, 0.5f))
			{
				for (const glm::ivec2& at : BrushRules::Stamp(step, radius, session.Map.GetWidth(), session.Map.GetHeight(), session.Land))
				{
					if (raise || dig)
					{
						Shape(session, at, step, radius, raise, moved);
					}
					else if (FarmRules::MayPaint(session.FarmReach, session.FieldTileId, tileId, at.x, at.y, session.Map.GetWidth())
						== false)
					{
						m_OutOfReach = true; // farmland only near a settlement that farms
					}
					else if (MountainRules::LevelTile(session.Heights.Get(at.x, at.y), at.x, at.y, m_Mountains) < 0
						&& BrushRules::CanPaint(session.Map.Get(at.x, at.y), tileId, session.Water.WaterTileId, session.Water.BedTileId))
					{
						// Rock and snow are the height's: painting doesn't cover them. A new type mixes its soil in.
						if (session.Map.Set(at.x, at.y, tileId))
						{
							m_PaintedInReach = true;
							SoilRules::Paint(session.Soil, (size_t)at.y * (size_t)session.Map.GetWidth() + (size_t)at.x, tileId, m_Soil);
						}
					}
				}
			}
			if (moved.empty() == false)
			{
				std::vector<uint32_t> slumped = HeightRules::Slump(session.Heights, session.Map, session.Land, moved, raise, m_Heights);
				moved.insert(moved.end(), slumped.begin(), slumped.end());
				MountainRules::SettleTiles(session.Map, session.Heights, moved, m_Mountains); // rock and snow at once, even paused
			}
			if (m_OutOfReach && m_PaintedInReach == false && m_ToldReach == false)
			{
				session.Notices.push_back("Farmland can only be laid near a settlement that farms");
				m_ToldReach = true; // once a stroke; told again on the next press
			}
			m_LastPoint = point;
			m_Stroking = true;
		}

		void OnDestroy() override
		{
			m_Cursor.Release();
		}

	private:
		// The settlement whose centre is within a tile of `point` (its Id; 0 none).
		static int SettlementAt(const HearthlandsSession& session, const glm::vec2& point)
		{
			for (const Settlement& settlement : session.Settlements.List)
			{
				const float dx = point.x - ((float)settlement.X + 0.5f);
				const float dy = point.y - ((float)settlement.Y + 0.5f);
				if (dx * dx + dy * dy <= 1.2f * 1.2f)
				{
					return settlement.Id;
				}
			}
			return 0;
		}

		// Raise: water and riverbed fill with bare land, then the tile rises its bump. Dig: raised land (at the press) sinks
		// its bump, ground-level land becomes riverbed.
		void Shape(HearthlandsSession& session, const glm::ivec2& at, const glm::vec2& centre, float radius, bool raise,
			std::vector<uint32_t>& moved)
		{
			const glm::vec2 offset = glm::vec2((float)at.x + 0.5f, (float)at.y + 0.5f) - centre;
			const int bump = HeightRules::Bump(glm::length(offset), radius);
			const uint32_t index = (uint32_t)(at.y * session.Map.GetWidth() + at.x);
			const uint8_t tile = session.Map.Get(at.x, at.y);
			if (raise)
			{
				if ((tile == session.Water.WaterTileId || tile == session.Water.BedTileId) && m_Mountains.BareTileId >= 0)
				{
					session.Map.Set(at.x, at.y, (uint8_t)m_Mountains.BareTileId);
				}
				if (HeightRules::Move(session.Heights, m_Stroke, at.x, at.y, bump, m_Heights))
				{
					moved.push_back(index);
				}
				return;
			}
			if (HeightRules::StartLevel(session.Heights, m_Stroke, at.x, at.y) > 0)
			{
				if (HeightRules::Move(session.Heights, m_Stroke, at.x, at.y, -bump, m_Heights))
				{
					moved.push_back(index);
				}
			}
			else if (BrushRules::CanPaint(tile, session.Water.BedTileId, session.Water.WaterTileId, session.Water.BedTileId))
			{
				session.Map.Set(at.x, at.y, session.Water.BedTileId);
			}
		}

		static bool IsLand(const HearthlandsSession& session, const glm::vec2& point)
		{
			const size_t index = (size_t)point.y * (size_t)session.Map.GetWidth() + (size_t)point.x;
			return session.Land.empty() || (index < session.Land.size() && session.Land[index] != 0);
		}

		// A map point back in the world, on this entity's ground.
		glm::vec3 GroundPoint(const TileMap& map, const glm::vec2& point)
		{
			const glm::vec3 centre = GetWorldPosition();
			const glm::vec2 offset(point.x - (float)map.GetWidth() * 0.5f, (float)map.GetHeight() * 0.5f - point.y);
			return glm::vec3(centre.x + offset.x, centre.y + offset.y, centre.z);
		}

		// The map point under the pointer (BrushRules::MapPoint) and the height there: the primary camera's ray met with the
		// raised land, else the ground (this z).
		bool PointUnderPointer(const HearthlandsSession& session, glm::vec2& point, float& z)
		{
			const TileMap& map = session.Map;
			Scene& scene = *GetEntity().GetScene();
			Entity camera = scene.GetPrimaryCameraEntity();
			if ((bool)camera == false || scene.GetViewportWidth() == 0 || scene.GetViewportHeight() == 0 || map.GetWidth() == 0)
			{
				return false;
			}

			// The pointer is in the view's pixels with Y up; the projection's Y flip puts the image's top at -1.
			const glm::vec2 position = scene.GetUIPointer().Position;
			const glm::vec2 ndc(position.x / (float)scene.GetViewportWidth() * 2.0f - 1.0f,
				1.0f - position.y / (float)scene.GetViewportHeight() * 2.0f);
			const glm::mat4 cameraTransform = scene.GetWorldTransform(camera);
			const glm::mat4 projection = camera.GetComponent<CameraComponent>().Camera.GetProjection(scene.GetViewportAspectRatio());
			const glm::mat4 inverse = glm::inverse(projection * glm::inverse(cameraTransform));
			glm::vec4 nearPoint = inverse * glm::vec4(ndc, 0.0f, 1.0f);
			glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
			nearPoint /= nearPoint.w;
			farPoint /= farPoint.w;

			const glm::vec3 centre = GetWorldPosition();
			if (session.Heights.GetLevels().size() == map.GetTiles().size())
			{
				return HeightRules::RayToLand(glm::vec3(nearPoint) - centre, glm::vec3(farPoint - nearPoint), session.Heights,
					session.LevelHeight, m_Heights.MaxLevel, point, z);
			}
			glm::vec3 hit(0.0f);
			if (BrushRules::RayToGround(glm::vec3(nearPoint), glm::vec3(farPoint - nearPoint), centre.z, hit) == false)
			{
				return false;
			}
			z = 0.0f;
			point = BrushRules::MapPoint(glm::vec2(hit.x - centre.x, hit.y - centre.y), map.GetWidth(), map.GetHeight());
			return BrushRules::OnMap(point, map.GetWidth(), map.GetHeight());
		}

		BrushCursorView m_Cursor;
		MountainRules::Settings m_Mountains;
		SoilRules::Settings m_Soil; // what each type's soil is, for mixing
		HeightRules::Settings m_Heights;
		HeightRules::Stroke m_Stroke;
		bool m_Stroking = false;
		bool m_OutOfReach = false;     // this press touched farmland out of any farming settlement's reach
		bool m_PaintedInReach = false; // this press painted something
		bool m_ToldReach = false;      // the out-of-reach notice was shown for this press
		bool m_HeldSinceSelect = false; // since a press that selected a settlement
		bool m_WasDown = false;
		glm::vec2 m_LastPoint{ 0.0f };
	};
}
