local commands = {}

commands.print = function(command)
	print(command.text)
end

commands.exec = function(command)
	command.value = trem.exec(command.text)
	queue.write(command)
end

commands.get_cvar = function(command)
	command.value = trem.get_cvar(command.key)
	queue.write(command)
end

commands.set_cvar = function(command)
	trem.set_cvar(command.key, command.value)
	queue.write(command)
end

function command_hook(args, raw)
	-- TODO: need to implement a command registry
	queue.write({name="command", args=args, raw=raw})
end

function console_hook(text)
	queue.write({name="console", text=text})
end

function frame_hook()
	while queue.readable() do
		local command = queue.read()
		if commands[command.name] then
			commands[command.name](command)
		end
	end
end
