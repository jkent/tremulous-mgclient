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
queue.send_hook = function(name, arg)
	local message = {type="hook", name=name, arg=arg}
	queue.write(message)
end


--[[
queue command functions
]]--
local commands = {}
commands.execute = function(message)
	tremulous.execute(message.arg.text)
end

commands.get_cvar = function(message)
	local result = 
	queue.send_response(message, tremulous.get_cvar(message.arg.name))
end

commands.set_cvar = function(message)
	tremulous.set_cvar(message.arg.name, message.arg.value)
end

local command_registry = {}
commands.register_command = function(message)
	command_registry[message.arg.name] = true
end

commands.unregister_command = function(message)
	command_registry[message.arg.name] = nil
end

local retain_console = false
commands.set_retain_console = function(message)
	retain_console = message.arg.value
end


--[[
hook functions
]]--
function command_hook(arg, raw)
	queue.send_hook("command", {arg=arg, raw=raw})
	return command_registry[arg[1]]
end

function console_hook(text)
	queue.send_hook("console", {text=text})
	return retain_console
end

function frame_hook()
	queue.send_hook("frame")
	while queue.readable() do
		local message = queue.read()
		if message.type == "print" then
			print(message.text)
		elseif message.type == "command" and commands[message.name] then
			commands[message.name](message)
		end
	end
end
