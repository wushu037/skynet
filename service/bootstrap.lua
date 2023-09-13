local skynet = require "skynet"
local harbor = require "skynet.harbor"
local service = require "skynet.service"
require "skynet.manager"    -- import skynet.launch, ...

skynet.start(function()
    local standalone = skynet.getenv "standalone"

    --skynet.launch 用于启动一个 C 模块的服务。主要意思是 第一个参数是 c 模块的名字 可以是snlua logger...
    local launcher = assert(skynet.launch("snlua", "launcher"))
    skynet.name(".launcher", launcher) -- 给服务取一个名字叫做 .launcher

    local harbor_id = tonumber(skynet.getenv "harbor" or 0)
    if harbor_id == 0 then
        assert(standalone == nil)
        standalone = true
        skynet.setenv("standalone", "true")

        local ok, slave = pcall(skynet.newservice, "cdummy")
        if not ok then
            skynet.abort()
        end
        skynet.name(".cslave", slave)

    else
        if standalone then
            if not pcall(skynet.newservice, "cmaster") then
                skynet.abort()
            end
        end

        local ok, slave = pcall(skynet.newservice, "cslave")
        if not ok then
            skynet.abort()
        end
        skynet.name(".cslave", slave)
    end

    if standalone then
        local datacenter = skynet.newservice "datacenterd"
        skynet.name("DATACENTER", datacenter)
    end
    skynet.newservice "service_mgr"

    local enablessl = skynet.getenv "enablessl"
    if enablessl then
        service.new("ltls_holder", function()
            local c = require "ltls.init.c"
            c.constructor()
        end)
    end

    pcall(skynet.newservice, skynet.getenv "start" or "main")
    skynet.exit()
end)
