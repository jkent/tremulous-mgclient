--[[
queue command functions
]]--
local commands = {}
commands.execute = function(message)
	tremulous.execute(message.arg.text)
end

commands.get_cvar = function(message)
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

local restrict_output = false
commands.set_restrict_output = function(message)
	restrict_output = message.arg.value
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

queue.send_hook = function(name, arg)
	local message = {type="hook", name=name, arg=arg}
	queue.write(message)
end

queue.process = function(forced)
	while forced or queue.readable() do
		local message = queue.read()
		if not message then
			return
		end
		if message.type == "print" then
			print(message.text)
		elseif message.type == "command" and commands[message.name] then
			commands[message.name](message)
		end
	end
end


--[[
hook functions
]]--
function command_hook(raw, arg)
	queue.send_hook("command", {raw=raw, arg=arg})
	return command_registry[arg[1]]
end

function connect_hook(addr)
	queue.send_hook("connect", {addr=addr})
end

function disconnect_hook()
	queue.send_hook("disconnect")
end

function frame_hook()
	queue.send_hook("frame")
	queue.process()
end

do
	local buffer = ""
	function print_hook(text)
		if text:sub(-1) ~= "\n" then
			buffer = buffer..text
		else
			text = buffer..text:sub(1,-2)
			buffer = ""
			queue.send_hook("print", {text=text})
		end
		return restrict_output
	end
end


--[[
startup stuff
]]--
queue.process(true)
