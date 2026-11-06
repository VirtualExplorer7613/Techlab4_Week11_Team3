local elapsed = 0

function BeginPlay()
    print("BeginPlay A")
end

function Tick(dt)
    elapsed = elapsed + dt;
    if elapsed >= 1.0 then
        print("A 1 sec")
        elapsed = 0
    end
end

function EndPlay()
    print("EndPlay A")
end