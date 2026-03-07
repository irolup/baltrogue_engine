local M = {}

local lastPlayerCell = {}
local hasSeenPlayer = {}
local debugTick = 0

function M.updateChase(enemyNodeName, playerNodeName, deltaTime, opts)
  if not Nav or not Nav.get_agent or not getNodePosition then return end
  if not Vision or type(Vision.can_see) ~= "function" then return end

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
    local canSee = Vision.can_see(enemyNodeName, playerNodeName, { max_distance = maxDistance, fov_degrees = 90 })
    if Vision.debug_vision then
      debugTick = (debugTick or 0) + 1
      if debugTick % 60 == 0 then
        local dbg = Vision.debug_vision(enemyNodeName, playerNodeName)
        local hit = (dbg and dbg.hit_node) and dbg.hit_node or "none"
        --print(string.format("[vision] can_see=%s ray_hit=%s", tostring(canSee), hit))
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
