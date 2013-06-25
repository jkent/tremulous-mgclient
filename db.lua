local sqlite3 = require("sqlite3")
local db

function toboolean(value)
	if type(value) == "nil" then
		return false
	elseif type(value) == "number" and value == 1 then
		return true
	elseif type(value) == "string" then
		value = value:lower()
		return (value == "1") or (value == "true") or (value == "yes") or
			(value == "y") or (value == "t")
	elseif type(value) == "boolean" then
		return value
	elseif type(value) == "table" and #value > 0 then
		return true
	else
		return true
	end
	return false
end

local function query(sql, ...)
	local results = {}
	local vm = assert(db:prepare(sql), db:errmsg())
	if #{...} > 0 then
		assert(vm:bind_values(...) == sqlite3.OK, db:errmsg())
	end
	while true do
		local state = vm:step()
		if state == sqlite3.ROW then
			table.insert(results, vm:get_values())
		elseif state == sqlite3.DONE then
			break
		else
			error(db:errmsg())
		end
	end
	assert(vm:finalize() == sqlite3.OK, db:errmsg())
	return results
end

local function query_one(sql, ...)
	local results = query(sql, ...)
	if #results == 0 then
		return nil
	end
	assert(#results == 1, "query returned more than one row")
	return results[1]
end

local function schema_version()
	query([=[
		CREATE TABLE IF NOT EXISTS schema_history (
			from_version INTEGER,
			to_version INTEGER
		);
	]=])
	local result = query_one([=[
		SELECT to_version
			FROM schema_history
			ORDER BY rowid DESC
			LIMIT 1;
	]=])
	return result and tonumber(result[1]) or nil
end

local LATEST_SCHEMA = 0
local function database_init()
	print("Creating database tables...")
	assert(db:exec([=[
		CREATE TABLE server (
			id INTEGER
				PRIMARY KEY,
			name TEXT
				NOT NULL
				UNIQUE,
			abbr TEXT
				DEFAULT NULL
				UNIQUE,
			addr TEXT
				NOT NULL
				UNIQUE,
			auto_namelog BOOLEAN
				DEFAULT FALSE
		);
		CREATE TABLE player (
			id INTEGER
				PRIMARY KEY,
			server_id INTEGER
				DEFAULT NULL
				REFERENCES server(id)
					ON DELETE SET NULL,
			guid TEXT
				DEFAULT NULL,
			ip TEXT
				NOT NULL,
			lang TEXT
				DEFAULT NULL,
			UNIQUE (server_id, guid, ip)
		);
		CREATE TABLE player_name (
			id INTEGER
				PRIMARY KEY,
			server_id INTEGER
				DEFAULT NULL
				REFERENCES server(id)
					ON DELETE SET NULL,
			player_id INTEGER
				DEFAULT NULL
				REFERENCES player(id)
					ON DELETE CASCADE,
			name TEXT
				NOT NULL,
			stripped_name TEXT
				NOT NULL,
			connected_time INTEGER
				DEFAULT (DATETIME('now')),
			disconnected_time INTEGER
				DEFAULT NULL
		);
	]=]) == sqlite3.OK, db:errmsg())
	query("INSERT INTO schema_history VALUES (0, ?);", LATEST_SCHEMA)
end

local function database_upgrade()
	local version = schema_version()
	if version == LATEST_SCHEMA then
		return
	elseif not version then
		database_init()
	elseif version > LATEST_SCHEMA then
		error("Database schema is newer than software schema")
	elseif version < LATEST_SCHEMA then
		--print("Performing database upgrade...")
		error("schema update is not implemented")
	end
	assert(schema_version() == LATEST_SCHEMA, "database_upgrade failed")
end


do
	local dbfile = tremulous.cvar["fs_homepath"].."/db.sqlite3"
	db = sqlite3.open(dbfile)
end
database_upgrade()

local function db_server_cmd(arg)
	local subcmds = {
		["list"] = function(arg)
			local results = query("SELECT name, abbr, addr, auto_namelog FROM server ORDER BY name;")
			if #results == 0 then
				print("No servers found")
				return
			end
			local fmt = "%-19s %-24s %-12s"
			print(fmt:format("name [abbr]", "addr", "auto_namelog"))
			print(fmt:format(("-"):rep(19), ("-"):rep(24), ("-"):rep(12)))
			for _,row in pairs(results) do
				local name
				if row[2] then
					name = string.format("%s [%s]", table.unpack(row, 1, 2))
				else
					name = row[1]
				end
				local addr, auto_namelog = table.unpack(row, 3)
				auto_namelog = (auto_namelog == 1) and "true" or "false"
				print(fmt:format(name, addr, auto_namelog))
			end
		end,
		["set"] = function(arg)
			local subcmds = {
				["abbr"] = function()
					query("UPDATE server SET abbr=? WHERE name=?;", arg[3], arg[1])
				end,
				["auto_namelog"] = function()
					query("UPDATE server SET auto_namelog=? WHERE name=?;", toboolean(arg[3]), arg[1])
				end,
			}
		
			local f = subcmds[arg[2]]
			if not f and #arg ~= 3 then
				local keys = {}
				for key,_ in pairs(subcmds) do
					table.insert(keys, key)
				end
				print("usage: db server set <name> ["..table.concat(keys, "|").."] <value>")
				return
			end
			f()		
		end, 
		["del"] = function(arg)
			if #arg ~= 1 then
				print("usage: db server del <name>")
				return
			end
			query("DELETE FROM server WHERE name=?", arg[1])
		end,
		["add"] = function(arg)
			if #arg ~= 2 then
				print("usage: db server add <name> <addr>")
				return
			end
			query("INSERT INTO server (name, addr) VALUES(?, ?)", table.unpack(arg))
		end,
	}

	local f = subcmds[arg[1]]
	if not f then
		local keys = {}
		for key,_ in pairs(subcmds) do
			table.insert(keys,key)
		end
		print("usage: db server ["..table.concat(keys, "|").."]")
		return
	end
	arg = table.pack(table.unpack(arg, 2)) 
	f(arg)
end

tremulous.command.db = function(raw, arg)
	local subcmds = {
		['server'] = db_server_cmd,
		['player'] = db_player_cmd,
		['name'] = db_name_cmd,
	}

	local f = subcmds[arg[2]]
	if not f then
		local keys = {}
		for key,_ in pairs(subcmds) do
			table.insert(keys,key)
		end
		print("usage: db ["..table.concat(keys, "|").."]")
		return
	end
	arg = table.pack(table.unpack(arg, 3)) 
	f(arg)
end

tremulous.command.connect = function(raw, arg)
	if #arg ~= 2 then
		print("usage: connect [address|abbr]")
		return
	end

	local results = query_one("SELECT addr FROM server WHERE abbr=?", arg[2])
	if results then
		tremulous.execute("connect "..results[1])
		return
	end
	
	tremulous.execute("connect "..arg[2])
end	
