local base = trem.cvar_get("fs_basepath").string
local home = trem.cvar_get("fs_homepath").string
local ext = string.match(package.cpath, "%.(%a+)$")

package.path = home.."/lua/?.lua;"..home.."/lua/?/init.lua;"..
	base.."/lua/?.lua;"..base.."/lua/?/init.lua"
package.cpath = home.."/lua/?."..ext..";"..home.."/lua/loadall."..ext..";"..
	base.."/lua/?."..ext..";"..base.."/lua/loadall."..ext

base, home, ext = nil

trem.hook.frame = function()
	while queue.readable() do
		t = queue.read()
		if t.cmd == "print" then
			print(t.str)
		end
	end
end

local fn = package.searchpath("autoexec", package.path)
if fn then
	return dofile(fn)
end

print("CL_LuaInit: autoexec not found")
print("CL_LuaInit: path: "..package.path)
