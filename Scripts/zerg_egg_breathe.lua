-- The mesh origin is at its base. Only the visual component breathes.
local MESH = "EggMesh"
local PERIOD = 2.5
local HORIZONTAL = 0.02
local VERTICAL = 0.015
local elapsed = 0
local baseScale = nil

function BeginPlay()
    baseScale = obj:GetComponentRelativeScale(MESH)
    if not baseScale then
        print("[Egg] EggMesh component not found")
        return
    end
    elapsed = 0
end

function Tick(dt)
    if not baseScale then return end
    elapsed = (elapsed + dt) % PERIOD
    local breath = math.sin(elapsed * 2 * math.pi / PERIOD)
    obj:SetComponentRelativeScale(MESH, FVector(
        baseScale.x * (1 + HORIZONTAL * breath),
        baseScale.y * (1 + HORIZONTAL * breath),
        baseScale.z * (1 + VERTICAL * breath)
    ))
end

function EndPlay()
    if baseScale then obj:SetComponentRelativeScale(MESH, baseScale) end
end
