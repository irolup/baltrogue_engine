-- Melee: short-range hit that damages enemies

return {
    fire = function(cfg, cameraName, getEnemyRootName)
        if not cfg or not cameraName or not getEnemyRootName then return end
        local range = cfg.range or 3.0

        local camX, camY, camZ = getActiveCameraPosition()
        if not camX then camX, camY, camZ = getNodePosition(cameraName) end
        if not camX then return end

        local fx, fy, fz = getNodeForward(cameraName)
        local len = math.sqrt(fx * fx + fy * fy + fz * fz)
        if len < 0.0001 then fx, fy, fz = 0, 0, -1 else fx, fy, fz = fx / len, fy / len, fz / len end

        local hitNode, hitX, hitY, hitZ, hitDist = physicsRaycast(camX, camY, camZ, fx, fy, fz, range)
        if not hitNode or type(hitNode) ~= "string" or hitNode == "" or not hitDist or hitDist > range then return end

        local rootName = getEnemyRootName(hitNode)
        if not rootName then return end
        local damage = cfg.damage or 25
        if type(callNodeScriptFunctionWithParam) == "function" then
            callNodeScriptFunctionWithParam(rootName, "takeDamage", damage)
        end
    end
}
