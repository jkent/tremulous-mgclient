require("logging").register()

function inspect(t, depth)
	f = require "inspect"
	for line in f(t, depth):gmatch("[^\r\n]+") do
		print(line)
	end
end

function string.startswith(s, prefix)
	return s:sub(1, #prefix) == prefix
end

function string.endswith(s, suffix)
	return suffix == '' or s:sub(-#suffix) == suffix
end

tremulous.command.eval = function(raw, arg)
	local f, err, _, ok, result

	s = raw:sub(6)
	
	f, err = load("return "..s)
	if not f then
		f, _ = load(s)
		if not f then
			print("load: "..err)
			return
		end
	end

	ok, result = pcall(f)
	if not ok then
		print("pcall: "..result)
		return
	end

	if result ~= nil then
		print(result)
	end
end

tremulous.command.cinematic = function(raw, arg)
	tremulous.command.cinematic = nil
end

tremulous.restrict_output = true
tremulous.add_hook("print", 
	function(text)
		if #text ~= 0 then
			text, _ = text:gsub("\n","\n    ")
			print(text)
		end
	end
)
