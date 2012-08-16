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
	queue.send_command = function(name, ...)
		id = id + 1
		local message = {type="command", name=name, arg={...}, id=id}
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

tremulous.add_hook = function(name, fn)
	local hooks = tremulous.hook[name] 
	if not hooks then
		hooks = {}
		tremulous.hook[name] = hooks
	end
	table.insert(hooks, fn)
end 

tremulous.remove_hook = function(name, fn)
	local hooks = tremulous.hook[name]
	if not hooks then
		return
	end
	for i,hook in ipairs(hooks) do
		if hook == fn then
			table.remove(hooks, i)
			return
		end
	end
end

tremulous.call_hook = function(name, ...)
	local hooks = tremulous.hook[name]
	if not hooks then
		return
	end
	for i, hook in ipairs(hooks) do
		local ok, result = pcall(hook, ...)
		if not ok then
			table.remove(hooks, i)
			print("name hook: "..result)
		end
	end
end

tremulous.execute = function(text)
	queue.send_command("execute", text)
end

do
	local cacheable = { 
		["arch"] = true, 
		["fs_basepath"] = true, 
		["fs_homepath"] = true,
	}
	local mt = {}
	mt.__newindex = function(t, key, value)
		if type(value) == "boolean" then
			value = value and "1" or "0"
		elseif type(value) == "number" then
			value = tostring(value)
		elseif type(value) ~= "string" then
			return
		end
		queue.send_command("set_cvar", key, value)
	end
	mt.__index = function(t, key)
		local id = queue.send_command("get_cvar", key)
		local value = queue.wait_response(id).result
		if cacheable[key] then
			rawset(t, key, value)
		end
		return value
	end
	tremulous.cvar = setmetatable({}, mt)
end

do
	local command = {}
	local mt = {__index=command}
	mt.__newindex = function(t, key, value)
		if type(value) == "function" then
			queue.send_command("register_command", key)
		else
			queue.send_command("unregister_command", key)
			value = nil
		end
		t.__t[key] = value
	end
	tremulous.command = setmetatable({__t=command}, mt)
end

tremulous.restrict_output = false

do
	local mt = {}
	mt.__newindex = function(t, key, value)
		if key == "restrict_output" then
			value = value and true or false
			queue.send_command("set_restrict_output", value)
		end
		t.__t[key] = value
	end
	mt.__index = function(t, key)
		if key == "server" then
			local id = queue.send_command("get_server")
			return queue.wait_response(id).result
		end
		return t.__t[key]
	end
	tremulous = setmetatable({__t=tremulous}, mt)
end


--[[
startup stuff
]]--
local function configure_paths()
	local base = tremulous.cvar["fs_basepath"]
	local home = tremulous.cvar["fs_homepath"]
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
			if message.type == "hook" then
				tremulous.call_hook(message.name, table.unpack(message.arg))
				if message.name == "command" then
					local name = message.arg[2][1]
					if tremulous.command[name] then
						local ok, result = pcall(tremulous.command[name], table.unpack(message.arg))
						if not ok then
							tremulous.command[name] = nil
							print("\""..name.."\" command hook: "..result)
						end
					end
				end
			end
		end
	end
end
