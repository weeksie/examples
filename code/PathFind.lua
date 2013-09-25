module("PathFind",package.seeall)
local stdlib      = assert(require_framework"LuaStdLib")
assert(require_framework"Json")
local log = require_framework"Logger"
assert(require"lfs")
assert(require"libpathfind")

local logger    = log.Logger.new{}

PathFinder        = {}
local prototype   = {}

-- API methods
local pf_inst     = nil

function add_map(args) 
  pf_inst = pf_inst or PathFinder.new()
  pf_inst:add_map(args.map)
  return "SUCCESS"
end

-- returns
-- { distance = n, route = [{level = "FNORD", path = [{x,y}...] }]  }
-- 
function find(args)
  local result = nil
  local route,dist = nil, nil

  if not pf_inst then
    local mdir   = Application.application_support .. "/Maps"
    local maps   = {}
    for mapname in lfs.dir(mdir) do
      if not string.match(mapname,"^%.") then
	maps[string.gsub(mapname,"%.json","")] = mdir .. "/" .. mapname
      end
    end    
    pf_inst = PathFinder.new{maps = maps}
    route, dist = pf_inst:find(args)
  elseif (args.from and args.to) then
    route, dist = pf_inst:find(args)
  elseif (args.current and args.targets) then
    route, dist = pf_inst:find(args)    
  else
    return nil, {error="missing required args: PathFind.find(from, to)"}
  end
  logger:debug("route: %s dist: %s",route, tostring(dist))
  if route then 
    return {route = route,distance = dist}
  else
    logger:debug("Caught error in pathfind")
    return nil, dist
  end
end


function PathFinder.new(...)
  local args = ... or {}
  local instance   = {
    inverse     = {},  -- reverse lookup table "map name" -> index
    level_names = {},  -- lookup table index -> "map name"
    levels      = {},  -- a list of index -> coords
    data        = nil, -- C data for map 3d array
    tile_size   = nil, -- rows / height
    height      = nil, -- the height in pixels of the map set
    width       = nil, -- the width in pixels of the map set
    jumps       = {},  -- an index of all of the jump points in the map
  }  
  instance   =  stdlib.instantiate(instance,prototype)
  instance:add_maps(args.maps)
  logger:debug("Added maps in PathFinder.new")
  return instance
end

function prototype:add_maps(maps)
  -- initialisation of libpathfind map data
  local first_loop = true
  for k,v in pairs(maps or {}) do    
    logger:info("Looping in PathFind:add_maps")
    local map
    if v.data then
      map = v
    else
      local mh = assert(io.open(v))
      local ms = mh:read("*a")
      assert(mh:close())      
      map      = Json.decode(ms)      
    end
    
    if first_loop then
      self.rows   = #map.data
      self.cols   = #map.data[1]
      self.width  = map.width
      self.height = map.height
    end
    first_loop  = false
    
    -- either update or add new map data
    if self.inverse[k] then
      self.levels[self.inverse[k]] = map.data
      self.level_names[self.inverse[k]] = k
    else
      table.insert(self.levels,map.data)
      table.insert(self.level_names,k)
      self.inverse[k] = #self.levels - 1
    end
    
  end
  
  -- update the jumps for z indexes
  for z,lvl in ipairs(self.levels) do
    for y, row in ipairs(lvl) do
      for x, cell in ipairs(row) do
	if cell.jump then
	  for _,jumpto in ipairs(cell.jump) do
	    jumpto.exit.z = self.inverse[jumpto.exit.level]
	  end
	  table.insert(self.jumps, {jump = cell.jump, coords = {x=x-1,y=y-1,level=self.level_names[z]}})
	end
      end
    end
  end

  self.data = libpathfind.pf_init(self.levels,#self.levels,self.rows,self.cols)
  --self:print_map{{level="G", path={}}}
end

function prototype:add_map(map)
  self:add_maps{map}
end


function prototype:closest(args)
  if not self.data then 
    local msg = "Error, PathFinder not initialized" 
    logger:error(msg)
    return nil, {error = msg}
  end
  
  local current, targets = args.current, args.targets
  
  if #current == 0 then
    return nil, {error = "No current path given"}
  end
  
  if #targets == 0 then
    local path = {}
    for pt1, pt2 in spaniter(current) do
      local p = self:find{from=pt1, to=pt2}
      path    = stdlib.cat(path,p)
    end
    return path, distance(path)
  end
  
  local pathsorter       = function(p1,p2) 
			     if not p1 then return p2    end
			     if not p2 then return false end
			     return p1.dist < p2.dist
			   end
  
  if #current == 1 then
    local detours = {}
    for _,target in ipairs(targets) do
      path,dist = self:find{from = current[1], to = target}
      table.insert(detours, {path = path, dist = dist})
    end
    table.sort(detours, pathsorter)
    return detours[1].path, detours[1].dist
  end
  
  
  local final_path       = {}
    
  -- gather list of detours
  for pt1,pt2 in spaniter(current) do
    local detours = {}
    for _,target in ipairs(targets) do
      path1,dist1 = self:find{from = pt1, to = target}
      path2,dist2 = self:find{from = target, to = pt2}
      table.insert(detours,{path = stdlib.cat(path1,path2), dist = dist1+dist2})
    end
    table.sort(detours,pathsorter)
    normal_path, normal_dist = self:find{from = pt1, to = pt2}
    table.insert(final_path,{
		   detour      = detours[1],
		   path        = normal_path,
		   dist        = normal_dist
		 })
  end
  
  -- find the shortest total path
  local shortest = { dist = 999999, index = 0 }
  for is, segment in ipairs(final_path) do
    local d = 0
    for ic, check in ipairs(final_path) do
      if ic == is then
	d = d + check.detour.dist
      else
	d = d + check.dist
      end
    end
    if d < shortest.dist then
      shortest = {dist = d, index = is}
    end
  end
  
  -- build final path
  local path = {}
  for i, segment in ipairs(final_path) do
    if shortest.index == i then
      path = stdlib.cat(path,segment.detour.path)
    else
      path = stdlib.cat(path,segment.path)
    end
  end
  return path, distance(path)
end

function prototype:find(args)    
  
  local from, to = args.from, args.to
  if not self.data then 
    local msg = "Error, PathFinder not initialized" 
    logger:error(msg)
    return nil, {error = msg}
  end
  if not self.inverse[from.level] then
    local msg = "Level " .. tostring(from.level) .. " isn't in the available map set"
    logger:error(msg)
    return nil, {error = msg}
  end
  if not self.inverse[to.level] then
    local msg = "Level " .. tostring(to.level) .. " isn't in the available map set"
    logger:error(msg)
    return nil, {error = msg}    
  end
  
  local path,err = libpathfind.pf_shortest_path(self.data,					    
						from.x,
						from.y,
						self.inverse[from.level],
						to.x,
						to.y,
						self.inverse[to.level],
						args.mobility)
  if not path then
    logger:error(err)
    return nil, {error = err}
  end

  reverse_path(path)

  local floors   = self:split_floors(path)

  local smoothed     = {}
  local smoothed_idx = 0
  for i,v in ipairs(floors) do   
    -- tricky, a jump_break is when we have a same-level-jump
    local path, jump_break = self:smooth(v.path)
    -- if this is the case we split off the smoothed portion of the
    -- path and then add the rest to the floors table to be processed
    -- on the next iteration.
    if jump_break then
      local tmp_path = {}
      for j=1,jump_break do
	table.insert(tmp_path,j, table.remove(path,1))
      end      
      smoothed[i] = {level = v.level, path = tmp_path}
      logger:info("tmp len: %d, rest len: %d",#tmp_path, #path)
      if #path > 0 then
	table.insert(floors,i,path)
      end
    else
      smoothed[i] = {level = v.level, path = path}      
    end
  end


  return smoothed, distance(smoothed)
end

-- returns the path split into a hash of paths by map name, TODO:
-- change this so that it's an ordered list of {level = ..., path = ...}
function prototype:split_floors(path)
  local floors  = {}
  local current_name = self.level_names[path[1].z+1]
  local current_path = {}
  for i,v in ipairs(path) do
    if current_name ~= self.level_names[v.z+1] then
      table.insert(floors,{level = current_name, path = current_path})
      current_name = self.level_names[v.z+1]
      current_path = {}
    end
    table.insert(current_path,v)
  end
  table.insert(floors,{level = current_name, path = current_path})
  return floors
end

function prototype:smooth(path)
  if #path == 2 then return path end
  
  local map      = self.levels[path[1].z+1]
  local from     = 1
  local to       = 2
  local last     = nil
  local sanity   = 1
  
  while to < #path do
    sanity = sanity + 1
    if self:walkable(map,path[from],path[to]) then
      last = table.remove(path,to)
    elseif map[path[to].y+1][path[to].x+1].jump then
      -- there is a jump point to a location on the same map, like a
      -- travelator or a mezzenine
      return path, to
    else
      from = to
      to   = to + 1
      table.insert(path,from,last)
    end
    
    if not path[from] then
      table.remove(path,from)      
    end
    
  end
  table.insert(path,to,last)  

  return path
end

function prototype:print_map(whole_path)
  for level,floor in pairs(whole_path) do
    print("=====",level,"=====")
    local map = self.levels[self.inverse[floor.level]+1]
    if floor then
      for _,cell in ipairs(floor.path) do
	map[cell.y+1][cell.x+1].cost = "*"
      end
    end  
  
    for i,rows in ipairs(map) do
      if i == 1 then
	local hdr = "      "
	for c=1,#rows do
	  hdr = hdr .. string.format("% 3d",c)
	end
	print(hdr)
      end
      local row = string.format("% 4d - ",i)
      for _, col in ipairs(rows) do
	if col.jump then 
	  col.cost = "#"
	end
	if col.cost == 255 then 
	  col.cost = "x" 
	elseif col.cost == 0 then
	  col.cost = " "
	end
	row = row .. string.format("% 1s",col.cost)
      end
      print(row)
    end
  end
end


-- Bresenham's line of sight algorithm, 
-- Moved to C
function prototype:walkable(map,from,to)
  return libpathfind.pf_walkable(self.data, from.z, from.x,from.y,to.x,to.y)
end

-- returns the centre pixel of a tile
function coord_to_pixel(tile_size,c)  
  return { x = tile_size * (c.x+1) - (tile_size/2),	   
	   y = tile_size * (c.y+1) - (tile_size/2) }
end

function pixel_to_coord(tile_size,p)  
  return { x = math.floor(p.x / tile_size),
	   y = math.floor(p.y / tile_size) }
end

function prototype:sample(map,x,y)
  pt = pixel_to_coord(map.tile_size,{x=x,y=y})
--  print("sampling...",x,y, pt.x,pt.y)
  x = math.ceil(x/map.tile_size)
  y = math.ceil(y/map.tile_size)
  return x > 0 and 
    y > 0 and 
    y < self.rows and 
    x < self.cols and 
    map[y+1][x+1] and
    (map[y+1][x+1].cost ~= 255)
end

function reverse_path(path)
  local tmp;
  for i=1,#path/2 do
    tmp             = path[i]
    path[i]         = path[#path - (i - 1)]
    path[#path - (i - 1)] = tmp
  end  
end

function distance(whole_path)
  local dist = 0
  for _,v in ipairs(whole_path) do
    local path = {}
    for _,coord in ipairs(v.path) do table.insert(path,coord) end
    local from = table.remove(path,1)
    while #path > 0 do
      local to = table.remove(path,1)
      local dx = from.x - to.x
      local dy = from.y - to.y
      local d  = math.sqrt(dx*dx + dy*dy)
      if d < 0 then d = d*-1 end
      dist     = dist + d
      from     = to
    end    
  end
  return dist
end

function pttos(pt)
  return "(x:" .. pt.x .. ",y:" .. pt.y .. ")"
end

-- iterates between spans in a path, e.g. {pt1,pt2,pt3} would yield
-- pt1,pt2
-- pt2,pt3
function spaniter(path)
  local i = 0
  local n = #path
  return function()
	   i = i+1
	   if i < n then
	     return path[i],path[i+1]
	   end
	 end
end