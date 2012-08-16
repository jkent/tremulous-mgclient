--[[
queue convenience functions
]]--
do
	local stash = {}
	local stashing = false

	local normal_read = queue.read
	local stashing_read = function()
		if stashing then
			message = normal_read()
			table.insert(stash, message)
			return message
		else
			if #stash >= 1 then
				return table.remove(stash, 1)
			else
				queue.read = normal_read
				return normal_read()
			end
		end
	end

	queue.stash_begin = function()
		stashing = true
		queue.read = stashing_read
	end
	
	queue.stash_end = function()
		table.remove(stash)
		stashing = false
	end

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

queue.wait = function(condition)
	queue.stash_begin()
	while not stopped() do
		local message = queue.read()
		if message then
			if condition(message) then
				queue.stash_end()
				return message
			end
		end
	end
end

queue.wait_response = function(id)
	condition = function(message)
		return message.type == "response" and message.id == id
	end
		 
	return queue.wait(condition)
end


--[[
tremulous 'library'
]]--
tremulous = {hook={}}

tremulous.execute = function(text)
	queue.send_command("execute", {text=text})
end

tremulous.get_cvar = function(name)
	return queue.wait_response(queue.send_command("get_cvar", {name=name})).result
end

tremulous.set_cvar = function(name, value)
	queue.send_command("set_cvar", {name=name, value=value})
end

do
	local command = {}
	local mt = {__index=command}
	mt.__newindex = function(t, key, value)
		if type(value) == "function" then
			queue.send_command("register_command", {name=key})
		else
			queue.send_command("unregister_command", {name=key})
			value = nil
		end
		t.__t[key] = value
	end
	tremulous.command = setmetatable({__t=command}, mt)
end

tremulous.restrict_output = false

do
	local mt = {__index=tremulous}
	mt.__newindex = function(t, key, value)
		if key == "restrict_output" then
			value = value and true or false
			queue.send_command("set_restrict_output", {value=value})
		end
		t.__t[key] = value
	end
	tremulous = setmetatable({__t=tremulous}, mt)
end


--[[
hook functions
]]--
local hook = {}
hook.command = function(arg)
	if tremulous.hook.command then
		local ok, result = pcall(tremulous.hook.command, arg.raw, arg.arg)
		if not ok then
			tremulous.hook.command = nil
			print('Command hook: '..result)
		end
	end
	local name = arg.arg[1]
	if tremulous.command[name] then
		local ok, result = pcall(tremulous.command[name], arg.raw, arg.arg)
		if not ok then
			tremulous.command[name] = nil
			print("\""..name.."\" command hook: "..result)
		end
	end
end

hook.connect = function(arg)
	if tremulous.hook.connect then
		local ok, result = pcall(tremulous.hook.connect, arg.addr)
		if not ok then
			tremulous.command[name] = nil
			print("\""..name.."\" command hook: "..result)
		end
	end
end

hook.disconnect = function(arg)
	if tremulous.hook.disconnect then
		local ok, result = pcall(tremulous.hook.disconnect)
		if not ok then
			tremulous.command[name] = nil
			print("\""..name.."\" command hook: "..result)
		end
	end
end

do
	local startup = false
	hook.frame = function(arg)
		if not startup then
			startup = true
			if tremulous.hook.startup then
				local ok, result = pcall(tremulous.hook.startup)
				if not ok then
					tremulous.hook.startup = nil
					print("Startup hook: "..result)
				end
			end
		end

		if tremulous.hook.frame then
			local ok, result = pcall(tremulous.hook.frame)
			if not ok then
				tremulous.hook.frame = nil
				print("Frame hook: "..result)
			end
		end
	end
end

hook.print = function(arg)
	if tremulous.hook.print then
		local ok, result = pcall(tremulous.hook.print, arg.text)
		if not ok then
			tremulous.hook.print = nil
			print("Print hook: "..result)
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
		home.."/lua/lib/?.lua;"..home.."/lua/lib/?/init.lua;"..
		base.."/lua/?.lua;"..base.."/lua/?/init.lua;"..
		base.."/lua/lib/?.lua;"..base.."/lua/lib/?/init.lua"
	package.cpath = home.."/lua/?."..ext..";"..home.."/lua/loadall."..ext..";"..
		home.."/lua/lib/?."..ext..";"..home.."/lua/lib/loadall."..ext..";"..
		base.."/lua/?."..ext..";"..base.."/lua/loadall."..ext..";"..
		base.."/lua/lib/?."..ext..";"..base.."/lua/lib/loadall."..ext
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
