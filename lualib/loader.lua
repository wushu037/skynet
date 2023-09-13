local args = {}
for word in string.gmatch(..., "%S+") do
    table.insert(args, word)
end

SERVICE_NAME = args[1] -- lua服务指定的文件名字 比如 launcher.lua 对应的名字是 launcher

local main, pattern

local err = {}
-- LUA_SERVICE 是存放lua服务对应文件的所有路径 类似 "./service/?.lua;./skynet-1.5.0/test/?.lua;"
for pat in string.gmatch(LUA_SERVICE, "([^;]+);*") do
    local filename = string.gsub(pat, "?", SERVICE_NAME) -- 用 launcher 替换掉路径配置中的 "?"
    local f, msg = loadfile(filename)
    if not f then
        table.insert(err, msg)
    else
        pattern = pat
        main = f
        break -- 这里表示从其中一条路径找到一个文件就够了
    end
end

if not main then
    error(table.concat(err, "\n"))
end

-- LUA_PATH LUA_CPATH 这些都是启动skynet时在配置文件里设置的 不过配置文件中使用小写 比如lua_path
LUA_SERVICE = nil
package.path, LUA_PATH = LUA_PATH -- 等价于 package.path = LUA_PATH ;LUA_PATH = nil
package.cpath, LUA_CPATH = LUA_CPATH -- 等价于 package.cpath = LUA_CPATH ;LUA_CPATH = nil

-- 如果pattern类似 ./aaa/bbb/?/main.lua 那么下面返回值service_path是 ./aaa/bbb/?/
-- 一般情况下pattern类似 ./aaa/bbb/?.lua 那么下面的返回值service_path是 nil
local service_path = string.match(pattern, "(.*/)[^/?]+$")

if service_path then
    service_path = string.gsub(service_path, "?", args[1])
    package.path = service_path .. "?.lua;" .. package.path
    SERVICE_PATH = service_path
else
    local p = string.match(pattern, "(.*/).+$")
    SERVICE_PATH = p
end

if LUA_PRELOAD then
    local f = assert(loadfile(LUA_PRELOAD))
    f(table.unpack(args))
    LUA_PRELOAD = nil
end

_G.require = (require "skynet.require").require -- 注意这里首先加载了 skynet.require 然后替换了lua原本的require

main(select(2, table.unpack(args))) -- 从这里开始执行我们指定的lua服务文件
