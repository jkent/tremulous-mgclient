local function loop(wait_id)
	while not stopped() do
		local message = queue.read()
		if message then
			if message.type == "response" and message.id == wait_id then
				return message
			else
				--TODO: stash discarded messages
			end
		end
	end
end


--[[
queue convenience functions
]]--
do
	local id = 0
	queue.send_command = function(name, arg)
		id = id + 1
		local message = {type="command", name=name, arg=arg, id=id}
		queue.write(message)
		return id
	end
end

queue.send_response = function(message, result)
	local response = {type="response", result=result, id=message.id}
	queue.write(response)
end


--[[
tremulous 'library'
]]--
tremulous = {hook={}}

tremulous.execute = function(text)
	queue.send_command("execute", {text=text})
end

tremulous.get_cvar = function(name)
	return loop(queue.send_command("get_cvar", {name=name})).result
end

tremulous.set_cvar = function(name, value)
	queue.send_command("set_cvar", {name=name, value=value})
end

do
	local mt = {}
	tremulous.command = setmetatable({__callback={}}, mt)
	mt.__newindex = function(t, key, value)
		if value then
			queue.send_command("register_command", {name=key})
		else
			queue.send_command("unregister_command", {name=key})
		end
		t.__callback[key] = value
	end
	mt.__index = tremulous.command.__callback
end


--[[
hook functions
]]--
local hook = {}
hook.command = function(arg)
	if tremulous.hook.command then
		local ok, result = pcall(tremulous.hook.command, arg.arg, arg.raw)
		if not ok then
			tremulous.hook.command = nil
			print('Command hook: '..result)
		end
	end
	local name = arg.arg[1]
	if tremulous.command[name] then
		local ok, result = pcall(tremulous.command[name], arg.arg, arg.raw)
		if not ok then
			tremulous.command[name] = nil
			print("\""..name.."\" command hook: "..result)
		end
	end
end

hook.console = function(arg)
	if tremulous.hook.console then
		local ok, result = pcall(tremulous.hook.console, arg.text)
		if not ok then
			tremulous.hook.console = nil
			print("Console hook: "..result)
		end
	end
end

hook.frame = function(arg)
	if tremulous.hook.frame then
		local ok, result = pcall(tremulous.hook.frame)
		if not ok then
			tremulous.hook.frame = nil
			print("Frame hook: "..result)
		end
	end
end


--[[
startup stuff
]]--
local function configure_paths()
	local base = tremulous.get_cvar("fs_basepath")
	local home = tremulous.get_cvar("fs_homepath")
	local ext = package.cpath:match("%.(%a+)$")

	package.path = home.."/lua/?.lua;"..home.."/lua/?/init.lua;"..
		base.."/lua/?.lua;"..base.."/lua/?/init.lua"
	package.cpath = home.."/lua/?."..ext..";"..home.."/lua/loadall."..ext..";"..
		base.."/lua/?."..ext..";"..base.."/lua/loadall."..ext
end

local function run_autoexec()
	local path, err = package.searchpath("autoexec", package.path)
	if not path then
		print("Lua autoexec was not found: "..err:gsub("\t", "    "))
		return
	end

	local f, err = loadfile(path)
	if not f then
		print(err)
		return
	end
	
	local ok, result = pcall(f)
	if not ok then
		print(result)
	end
end

return function()
	configure_paths()
	run_autoexec()

	while not stopped() do
		local message = queue.read()
		if message then
			if message.type == "hook" and hook[message.name] then
				hook[message.name](message.arg)
			end
		end
	end
end
