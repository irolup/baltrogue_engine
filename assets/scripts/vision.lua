local M = {}

local function vec3_len(x, y, z)
  return math.sqrt(x * x + y * y + z * z)
end

local function vec3_normalize(x, y, z)
  local len = vec3_len(x, y, z)
  if len < 1e-6 then return 0, 0, 0 end
  return x / len, y / len, z / len
end

function M.can_see(observerName, targetName, opts)
  if not getNodePosition or not raycastFromToWorld then return false end
  opts = opts or {}
  local maxDist = opts.max_distance or 20.0
  local fovDeg = opts.fov_degrees or 360.0

  local ox, oy, oz = getNodePosition(observerName)
  local tx, ty, tz = getNodePosition(targetName)
  if not ox or not tx then return false end

  local dx = tx - ox
  local dy = ty - oy
  local dz = tz - oz
  local dist = vec3_len(dx, dy, dz)
  if dist < 1e-6 then return true end
  if dist > maxDist then return false end

  if fovDeg < 360.0 and getNodeForward then
    local fx, fy, fz = getNodeForward(observerName)
    if fx then
      local toX, toY, toZ = vec3_normalize(dx, dy, dz)
      local dot = fx * toX + fy * toY + fz * toZ
      dot = math.max(-1, math.min(1, dot))
      local angleDeg = math.deg(math.acos(dot))
      if angleDeg > fovDeg * 0.5 then return false end
    end
  end

  local hitNodeName = raycastFromToWorld(ox, oy, oz, tx, ty, tz, observerName)
  if hitNodeName == nil then return true end
  if hitNodeName == targetName then return true end
  local allowNames = opts.target_node_names
  if allowNames and type(allowNames) == "table" then
    for _, name in ipairs(allowNames) do
      if hitNodeName == name then return true end
    end
  end
  return false
end

function M.debug_vision(observerName, targetName, opts)
  local out = { can_see = false, in_range = false, in_fov = false, ray_clear = false, distance = 0, angle_deg = 0, hit_node = "" }
  if not getNodePosition or not raycastFromToWorld then return out end
  opts = opts or {}
  local maxDist = opts.max_distance or 20.0
  local fovDeg = opts.fov_degrees or 360.0

  local ox, oy, oz = getNodePosition(observerName)
  local tx, ty, tz = getNodePosition(targetName)
  if not ox or not tx then return out end

  local dx = tx - ox
  local dy = ty - oy
  local dz = tz - oz
  local dist = vec3_len(dx, dy, dz)
  out.distance = dist
  out.in_range = (dist <= maxDist)

  local angleDeg = 0
  if dist >= 1e-6 and getNodeForward then
    local fx, fy, fz = getNodeForward(observerName)
    if fx then
      local toX, toY, toZ = vec3_normalize(dx, dy, dz)
      local dot = fx * toX + fy * toY + fz * toZ
      dot = math.max(-1, math.min(1, dot))
      angleDeg = math.deg(math.acos(dot))
      out.angle_deg = angleDeg
    end
  end
  out.in_fov = (fovDeg >= 360.0 or angleDeg <= fovDeg * 0.5)

  local hitNodeName = raycastFromToWorld(ox, oy, oz, tx, ty, tz, observerName)
  out.hit_node = (hitNodeName and tostring(hitNodeName)) or ""
  out.ray_clear = (hitNodeName == nil or hitNodeName == targetName)
  if opts.target_node_names and type(opts.target_node_names) == "table" and hitNodeName then
    for _, name in ipairs(opts.target_node_names) do
      if hitNodeName == name then out.ray_clear = true break end
    end
  end

  out.can_see = out.in_range and out.in_fov and out.ray_clear
  return out
end

return M
