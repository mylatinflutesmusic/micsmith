local sourceProject = os.getenv ("QUELL_DAW_SOURCE")
local savedProject = os.getenv ("QUELL_DAW_SAVED")
local logPath = os.getenv ("QUELL_DAW_LOG")

local function log (...)
    if not logPath then return end

    local f = io.open (logPath, "a")
    if f then
        local parts = {}
        for i = 1, select ("#", ...) do parts[i] = tostring (select (i, ...)) end
        f:write (os.date ("%H:%M:%S "), table.concat (parts, " "), "\n")
        f:close()
    end
end

if not sourceProject or not savedProject then
    log ("RESULT FAIL: missing project paths")
    return
end

local expected = {
    { "Depth", 0.55 },
    { "Sensitivity", 0.60 },
    { "Selectivity", 0.60 },
    { "Speed", 0.55 },
    { "Low Tame", 0.20 },
    { "High Tame", 0.15 },
    { "Mix", 1.00 },
}

local function getParam (track, fx, name)
    for p = 0, reaper.TrackFX_GetNumParams (track, fx) - 1 do
        local _, parameterName = reaper.TrackFX_GetParamName (track, fx, p, "")
        if parameterName == name then
            return reaper.TrackFX_GetParamNormalized (track, fx, p)
        end
    end
    return -1
end

local function verify (stage)
    local track = reaper.GetTrack (0, 0)
    local fx = track and reaper.TrackFX_GetByName (track, "Quell", false) or -1

    if track == nil or fx < 0 then
        log (stage, "FAIL: Quell track not found")
        return false
    end

    local ok = true

    for _, pair in ipairs (expected) do
        local value = getParam (track, fx, pair[1])
        local pass = math.abs (value - pair[2]) < 0.01
        log (stage, pair[1], value, pass and "PASS" or "FAIL")
        ok = ok and pass
    end

    return ok
end

log ("starting")
reaper.Main_openProject (sourceProject)

local openingOk = verify ("open")

reaper.Main_SaveProjectEx (0, savedProject, 0)
log ("saved", savedProject)

reaper.Main_openProject (savedProject)

local reopenOk = verify ("reopen")

log ("RESULT", (openingOk and reopenOk) and "PASS" or "FAIL")
