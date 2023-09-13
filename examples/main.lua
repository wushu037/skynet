local skynet = require "skynet"
local sprotoloader = require "sprotoloader"

local max_client = 64

-- 传递的匿名函数可以认为是lua服务的入口函数
skynet.start(function()
    skynet.error("Server start")
    skynet.uniqueservice("protoloader")
    if not skynet.getenv "daemon" then
        local console = skynet.newservice("console") -- 启动一个console服务
    end
    skynet.newservice("debug_console", 8000)
    skynet.newservice("simpledb")
    local watchdog = skynet.newservice("watchdog") -- 启动一个watchdog服务
    local addr, port = skynet.call(watchdog, "lua", "start", { -- 发送 lua类型 的请求给watchdog服务，然后等待对方回应
        port = 8888,
        maxclient = max_client,
        nodelay = true,
    })
    skynet.error("Watchdog listen on " .. addr .. ":" .. port)
    skynet.exit()
end)
