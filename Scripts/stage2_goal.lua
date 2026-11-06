local nextSpawn = FVector(2000, 0, 0.5)
local state = script_state

function BeginPlay()
    state.used = false
end

function OnBeginOverlap(other)
    if state.used or other.Name ~= "IcePlayer" then
        return
    end

    state.used = true
    _G.RespawnLocation = nextSpawn

    other:ResetMovement()
    other.Velocity = FVector(0, 0, 0)
    other.Location = nextSpawn
end
