local M = {}

local vision = nil
local function ensureVision()
  if vision then return vision end
  local ok, mod = pcall(dofile, "assets/scripts/vision.lua")
  if ok and mod and mod.can_see then
    vision = mod
    return vision
  end
  return nil
end

local lastPlayerCell = {}
local hasSeenPlayer = {}
local debugTick = 0

function M.updateChase(enemyNodeName, playerNodeName, deltaTime, opts)
  if not Nav or not Nav.get_agent or not getNodePosition then return end
  local v = ensureVision()
  if not v or type(v.can_see) ~= "function" then return end

  opts = opts or {}
  local maxDistance = opts.maxDistance or 50.0

  local agent = Nav.get_agent(enemyNodeName)
  if not agent then return end

  local px, py, pz = getNodePosition(playerNodeName)
  local ex, ey, ez = getNodePosition(enemyNodeName)
  local dx = px - ex
  local dz = pz - ez
  local distSq = dx * dx + dz * dz
  local maxDistSq = maxDistance * maxDistance

  if distSq > maxDistSq then
    hasSeenPlayer[enemyNodeName] = false
    if agent.clear_destination and agent:has_path() then
      agent:clear_destination()
    end
    return
  end

  if not hasSeenPlayer[enemyNodeName] then
    local optsVision = {
      max_distance = maxDistance,
      fov_degrees = opts.fov_degrees or 360,
      target_node_names = { playerNodeName, "PlayerCollision" }
    }
    local canSee = v.can_see(enemyNodeName, playerNodeName, optsVision)
    if v.debug_vision then
      debugTick = (debugTick or 0) + 1
      if debugTick % 60 == 0 then
        local dbg = v.debug_vision(enemyNodeName, playerNodeName, optsVision)
      end
    end
    if not canSee then
      if agent.clear_destination and agent:has_path() then
        agent:clear_destination()
      end
      return
    end
    hasSeenPlayer[enemyNodeName] = true
  end

  if not lastPlayerCell[enemyNodeName] then lastPlayerCell[enemyNodeName] = {} end

  local hasPath = agent.has_path and agent:has_path()
  local playerCellX, playerCellZ = Nav.world_to_cell(px, pz)
  local last = lastPlayerCell[enemyNodeName]
  local cellChanged = (last[1] ~= playerCellX or last[2] ~= playerCellZ)

  if not hasPath or cellChanged then
    agent:set_destination(px, py, pz)
    last[1], last[2] = playerCellX, playerCellZ
  end

end

return M
