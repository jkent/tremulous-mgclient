local description = [=[
Usage: lua bin2c.lua filename

Compile lua source, turn it into a c source, and write to standard output.
]=]

if not arg or not arg[1] then
  io.stderr:write(description)
  return
end

local filename = arg[1]

local content = string.dump(assert(loadfile(filename)))

local symbol do
  function symbol(filename)
    sym = filename:match("[\\/]([%w_]+)[^\\/]*$")
	i = sym:find("_", 1, true)
	while i do
      i = sym:find("_", i+1, true)
      if not i then break end
	  sym = sym:sub(1,i-1)..sym:sub(i+1,i+1):upper()..sym:sub(i+2,-1)
	end
	return sym
  end
end

local data_header do
  function data_header(filename)
    sym = filename:match("[\\/]([%w_]+)[^\\/]*$")
	i = sym:find("_", 1, true)
	while i do
      i = sym:find("_", i+1, true)
      if not i then break end
	  sym = sym:sub(1,i-1)..sym:sub(i+1,i+1):upper()..sym:sub(i+2,-1)
	end
	return "const unsigned char "..sym.."[] = {\n"
  end
end

local dump do
  local numtab={}; for i=0,255 do numtab[string.char(i)]=("%3d,"):format(i) end
  function dump(str)
    return (str:gsub(".", numtab):gsub(("."):rep(80), "%0\n").."\n")
  end
end

io.write([=[
#include <stddef.h>

const size_t ]=],symbol(filename),"_size = ",content:len(),[=[;
const unsigned char ]=],symbol(filename),"[",content:len(),[=[] = {
]=],dump(content),[=[
};
]=])

