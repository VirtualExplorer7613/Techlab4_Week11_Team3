function BeginPlay()
    StartCoroutine(function()
        wait(1.0)

        -- (0, 0, 0)에서 (5, 0, 0) 방향으로 이동
        move_to(obj.Location + FVector(5, 0, 0), 1.5)
        wait_until_move_done()

        wait(1.0)

        -- 원래 자리로 복귀
        move_to(obj.Location - FVector(5, 0, 0), 1.5)
        wait_until_move_done()
    end)
end

function OnBeginOverlap(OtherActor)
    print("[BEGIN] " .. obj.Name .. " touched " .. OtherActor.Name)
end

function OnEndOverlap(OtherActor)
    print("[END] " .. obj.Name .. " left " .. OtherActor.Name)
end