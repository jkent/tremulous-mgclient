local commands = {}

commands.print = function(command)
	print(command.text)
end

commands.get_cvar = function(command)
	command.value = trem.get_cvar(command.key)
	queue.write(command)
end

commands.set_cvar = function(command)
	local result = trem.set_cvar(command.key, command.value)
	queue.write(command)
end

function command_hook(args, raw)
end

function console_hook(text)
end

function frame_hook()
	while queue.readable() do
		local command = queue.read()
		if commands[command.name] then
			commands[command.name](command)
		end
	end
end
