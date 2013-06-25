local os = require("os")
local io = require("io")
local lfs = require("lfs")

local DATE_FORMAT = "%F"
local TIME_FORMAT = "%T"

local servers = {
	["208.100.0.130:30720"] = "a_server",
	["75.126.181.231:8002"] = "z_server"
}

local logfile
local last_date
local server = tremulous.server

local function open_log()
	if logfile then
		logfile:close()
	end
	last_date = os.date(DATE_FORMAT)
	
	local path = tremulous.cvar["fs_homepath"].."/logs"
	lfs.mkdir(path)
	if server then
		path = path.."/"..server
		lfs.mkdir(path)
	end
	local filename = path.."/"..last_date..".log"
	logfile = io.open(filename, "a+")
end

local function write_log(text)
	local date = os.date(DATE_FORMAT)
	if date ~= last_date then
		open_log()
	end

	if logfile then
		local time = "["..os.date(TIME_FORMAT).."] "
		text = text:gsub("\n", "\n"..string.rep(" ", #time)) 
		logfile:write(time..text.."\n")
		logfile:flush()
	end
end

local function connect_hook(addr)
	server = nil
	if servers[addr] then
		server = servers[addr]
	end
	open_log()

	text = "*** Connected to "..addr
	if server then
		text = text.." ["..server.."]"
	end
	write_log(text)
end

local function disconnect_hook()
	write_log("*** Disconnected")
	server = nil
	open_log()
end

local function print_hook(text)
	write_log(text)
end

open_log()

local logging = {}

logging.register = function()
	tremulous.add_hook("connect", connect_hook)
	tremulous.add_hook("disconnect", disconnect_hook)
	tremulous.add_hook("print", print_hook)
end

logging.unregister = function()
	tremulous.remove_hook("connect", connect_hook)
	tremulous.remove_hook("disconnect", disconnect_hook)
	tremulous.remove_hook("print", print_hook)
end

return logging
