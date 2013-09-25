#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "pathfind.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

/* debug functions          */
static void stackDump (lua_State *L) {
      int i;
      int top = lua_gettop(L);
      for (i = 1; i <= top; i++) {  /* repeat for each level */
        int t = lua_type(L, i);
	printf("%d:",i);
        switch (t) {
    
          case LUA_TSTRING:  /* strings */
            printf("`%s'", lua_tostring(L, i));
            break;
    
          case LUA_TBOOLEAN:  /* booleans */
            printf(lua_toboolean(L, i) ? "true" : "false");
            break;
    
          case LUA_TNUMBER:  /* numbers */
            printf("%g", lua_tonumber(L, i));
            break;
    
          default:  /* other values */
            printf("%s", lua_typename(L, t));
            break;
    
        }
        printf("  ");  /* put a separator */
      }
      printf("\n");  /* end the listing */
    }


/* Constructors */

/* Assumes that there is a mobility list on top of the stack,
   e.g. {"WHEELS","SIGHT"}. If there isn't then the function fails
   quietly and does nothing 
*/
Mobility lua_tomobility(lua_State *L) {
  Mobility m = 0x0;
  if(lua_istable(L,-1)) {
    lua_pushnil(L);
    while(lua_next(L,-2) != 0) {
      const char* mob = lua_tostring(L,-1);
      if(strcmp("WHEELS",mob) == 0) {
	m |= WHEELS;
      } else if (strcmp("SIGHT",mob) == 0) {
	m |= SIGHT;
      }
      lua_pop(L,1);
    }    
  }
  return m;
}


JumpList lua_tojumplist(lua_State *L) {
  JumpList head;
  JumpNode *tmp, *prv;
  int i,x,y,z;
  
  // stack: 
  // [{cost = n, exit = {x=n,y=n}, map=s},...]  
  if(lua_isnil(L,-1)) {
    head    = NULL;
  } else {
    head = NULL;
    tmp  = NULL;
    prv  = NULL;
    lua_pushnil(L);
    i    = 0;
    while(lua_next(L,-2) != 0) {
      tmp        = (JumpNode *)malloc(sizeof(JumpNode));
      if(prv != NULL) {
	prv->next = tmp;
      }
      // set the root of the list on the first cycle
      if(head == NULL) {
	head  = tmp;
      } 
      
      lua_getfield(L,-1,"exit");

      lua_getfield(L,-1,"x");
      lua_getfield(L,-2,"y");
      lua_getfield(L,-3,"z");

                  
      x     = (int)lua_tonumber(L,-3);
      y     = (int)lua_tonumber(L,-2);
      z     = (int)lua_tonumber(L,-1);
      
      //for some reason this doesn't compile on Windows
      //tmp->exit  = MKPT(x,y,z);
      tmp->exit.x = x;
      tmp->exit.y = y;
      tmp->exit.z = z;      
      
      lua_pop(L,4);      
                            
      lua_getfield(L,-1,"cost");
      tmp->weight   = (cost)lua_tonumber(L,-1);
      lua_pop(L,1);
      
      tmp->next     = NULL;
      prv           = tmp;
      lua_pop(L,1);
    }
  }
  return head;
}

Direction lua_todirection(lua_State *L) {
  Direction direction = D0;
  if(!lua_isnil(L,-1)) {
    lua_pushnil(L);
    while(lua_next(L,-2) != 0) {
      unsigned int d = 0;
      switch((unsigned int)lua_tonumber(L,-1)) {		
      case 1: d = D1; break;			
      case 2: d = D2; break;			
      case 3: d = D3; break;			
      case 4: d = D4; break;			
      case 5: d = D5; break;			
      case 6: d = D6; break;			
      case 7: d = D7; break;			
      case 8: d = D8; break;			
      default:
	// TODO make this into a real error
	printf("ERROR! invalid direction passed %d\n", (int) lua_tonumber(L,-1));
      }
      direction |= d;
      lua_pop(L,1);
    }
  }
  return direction;
}

Vertice* lua_toverticep(lua_State *L) {  
  cost weight;
  Vertice *v    = (Vertice *)malloc(sizeof(Vertice));
  
  /* 
     The stack: 
     table -> {cost = n, jump = table, mobility = list, direction = list}
  */
  
  lua_getfield(L,-1,"cost");
  weight   = (cost)lua_tonumber(L,-1);
  lua_pop(L,1);
    
  lua_getfield(L,-1,"jump");
  v->jumps = lua_tojumplist(L);
  lua_pop(L,1);  

  lua_getfield(L,-1,"direction");
  v->direction = lua_todirection(L);
  lua_pop(L,1);
  
  lua_getfield(L,-1,"mobility");
  v->mobility = lua_tomobility(L);
  lua_pop(L,1);
  
  v->weight     = weight == 0 ? 1 : weight;
  v->total_cost = 0;
  v->state      = UNTOUCHED;
  
  SETINVALID(v->parent);
  SETINVALID(v->prev);
  SETINVALID(v->next);
/*   v->parent     = INVALID_POINT; */
/*   v->prev       = INVALID_POINT; */
/*   v->next       = INVALID_POINT; */
  return v;
}

void free_jumplist(JumpList jn) {
  JumpNode *nxt;
  while(jn) {
    nxt = jn->next;
    free(jn);
    jn = nxt;
  }
}

void free_vertice(Vertice *v) {
  if(v) {
    free_jumplist(v->jumps);
    free(v);
  }
}

void free_graph(Graph *graph) {
  int i,j,k;
  if(graph) {
    for(i=0;i<graph->rows;i++) {
      for(j=0;j<graph->cols;j++) {
	for(k=0;k<graph->levels;k++) {
	  free_vertice(graph->map[i][j][k]);
	}
      }
    }
    free(graph);
    graph = NULL;
  }
}

/* End of Constructors/Destructors */




/* Path finding  */

// TODO: if this gets used again make sure that floats are used in the
// correct places
cost cartesian(Point a, Point b) {
  cost dx   = (cost) a.x - b.x;
  cost dy   = (cost) a.y - b.y;
  cost dist = (cost) sqrt(dx*dx + dy*dy);
  // force a positive
  dist      = dist < 0.0f ? dist * -1.0f : dist;
  return dist;
}


bool pt_eq(Point a, Point b) {
  return (a.x == b.x && a.y == b.y && a.z == b.z);
}


/* Open list operations */
Point open_vertice(Graph *graph, Point open, Point point) {
  if(VALID_PT(open)) {
    VERT(graph,open)->prev   = point;
    VERT(graph,point)->next  = open;
  }
  VERT(graph,point)->state  = OPEN;
  return point;
}

Point min_open_vertice(Graph *graph, Point open) {
  cost c     = INFINITE_COST;
  Point tmp  = open;
  while(VALID_PT(open)) {
    Vertice *v = VERT(graph,open);
    if(v->total_cost < c) {
      c = v->total_cost;
      tmp = open;
    }
    open = v->next;
  }
  return tmp;
}

// returns the head of the new open list
Point close_vertice(Graph *graph, Point open, Point close) {
  Point head = open;
  Vertice *v  = VERT(graph,close);
  v->state    = CLOSED;
  if(VALID_PT(v->prev)) {
    VERT(graph,v->prev)->next = v->next;
  } else {
    head = v->next;
  }
  
  if(VALID_PT(v->next)) {
    VERT(graph,v->next)->prev = v->prev;
  }
  
  SETINVALID(v->next);
  SETINVALID(v->prev);
  return head;
}

Point check_adjacent(Graph *graph, Point open, Point goal, Point p, int xd, int yd) {
  Vertice *adj; Point adj_pt;
  int x,y,z; cost weight;
  
  x        = p.x + xd;
  y        = p.y + yd;
  z        = p.z; // not traversing levels, so no z delta
  
  // GTFO if it's not even on the map
  if(VALID(graph,x,y,z)) 
    adj = graph->map[x][y][z];
  else
    return open;
  
  if(   VUNTOUCHED(adj)                // hasn't been visited before
     && VPASSABLE(adj,graph->mobility) // can be traversed
	&& VENTRANCE(adj,xd,yd))           // can enter from this direction
  {
    SETPT(adj->parent,p.x,p.y,p.z);
    SETPT(adj_pt,x,y,z);
    open            = open_vertice(graph, open, adj_pt);
    
    // adjust weight according to straight or diagonal movements
    weight     = (xd == 0 || yd == 0) ? adj->weight : adj->weight * SQ2;
    
    /* 
     note that I've commented out the heuristic, degenerating this to
     a dijkstra pathfind, a useful heuristic for multilevel might aim
     at jump nodes but it's too early to start fucking about with that
     and the performance is too good to worry about it at this point
    */
    adj->total_cost = VERT(graph,p)->total_cost + weight;// + cartesian(adj_pt,goal);
  }
  
  return open;
}

Point check_jumps(Graph *graph, Point open, Point goal, Point p) {
  JumpNode *jn = VERT(graph,p)->jumps;
  
  if(jn == NULL) 
    return open;  
  
  while(jn) {
    Vertice *adj    = VERT(graph,jn->exit); 
    
    if(VPASSABLE(adj,graph->mobility) 
       && VUNTOUCHED(adj))
      {
	SETPT(adj->parent,p.x,p.y,p.z);
	open            = open_vertice(graph, open, jn->exit);
	adj->total_cost = VERT(graph,p)->total_cost + jn->weight;
      }
    jn              = jn->next;
  }
  
  return open;
}

/* no need to return anything. After calling this function you can
   start from VERT(graph, goal) and work backward through the parent
   chain */
void shortest_path(Graph *graph, Point start, Point goal) {
  Point invalid, current, open;  
  SETINVALID(invalid);
  open = open_vertice(graph,invalid,start);
  while(VALID_PT(open)) {
    current = min_open_vertice(graph, open);    
    if(pt_eq(current, goal)) {
      // finished
      break;
    } else {
      open = close_vertice(graph, open, current);
      

      // examine adjacent vertices
      open = check_adjacent(graph, open, goal, current, -1,0); // top 
      open = check_adjacent(graph, open, goal, current, -1,1); // top right
      open = check_adjacent(graph, open, goal, current, 0,1);  // right
      open = check_adjacent(graph, open, goal, current, 1,1);  // bottom right
      open = check_adjacent(graph, open, goal, current, 1,0);  // bottom
      open = check_adjacent(graph, open, goal, current, 1,-1); // bottom left      
      open = check_adjacent(graph, open, goal, current, 0,-1); // left
      open = check_adjacent(graph, open, goal, current, -1,-1);// top left
      
      open = check_jumps(graph, open, goal, current);      
    }
  }
}


void reset_graph(Graph *graph) {
  int x,y,z;
  graph->mobility = UNRESTRICTED;
  for(x=0;x<graph->rows;x++) {
    for(y=0;y<graph->cols;y++) {
      for(z=0;z<graph->levels;z++) {
	SETINVALID(graph->map[x][y][z]->next);
	SETINVALID(graph->map[x][y][z]->prev);
	SETINVALID(graph->map[x][y][z]->parent);
	graph->map[x][y][z]->state      = UNTOUCHED;
	graph->map[x][y][z]->total_cost = 0.0;
      }
    }
  }
}

/* LUA API FUNCTIONS */

// helper for building the return value of the path
void add_path(lua_State *L, int node_number, Point node) {
  /* push node index */
  lua_pushnumber(L,node_number);
  
  lua_createtable(L,0,2);
  /* push x coord */
  lua_pushstring(L,"x");
  lua_pushnumber(L,node.x);
  lua_settable(L,-3);
  
  /* push y coord */
  lua_pushstring(L,"y");
  lua_pushnumber(L,node.y);
  lua_settable(L,-3);

  /* push z coord */
  lua_pushstring(L,"z");
  lua_pushnumber(L,node.z);
  lua_settable(L,-3);

  /* push Point entry onto the table */
  lua_settable(L,-3);
}


LUA_PF_FUNC(graph_gc) {
  Graph **graph = (Graph **)lua_touserdata(L,1);
  if(*graph) free_graph(*graph);
  lua_pop(L,1);
  return 0;
}

/* pf_init(data, rows, cols, levels) -> (Graph **) userdata */
LUA_PF_FUNC(pf_init) {
  int i,j,k;
  int levels    = (int)luaL_checknumber(L,2);
  int rows      = (int)luaL_checknumber(L,3);
  int cols      = (int)luaL_checknumber(L,4);

  Graph **graph = (Graph **)lua_newuserdata(L,sizeof(Graph*));
  // define meta table for graph
  luaL_newmetatable(L, "MapData");
  // push the meta garbage collection
  lua_pushstring(L,"__gc");
  // add the current graph to the c function closure
  lua_pushvalue(L,-3);
  lua_pushcclosure(L,graph_gc,1);
  // set __gc = function(graph) ... end
  lua_settable(L,-3);
  // setmetatable(graph,mt);
  lua_setmetatable(L,-2);
    
  // populate graph
  *graph           = (Graph *)malloc(sizeof(Graph));
  (*graph)->rows   = rows;
  (*graph)->cols   = cols;
  (*graph)->levels = levels;
  
  for(i=0;i<levels;i++) {
    lua_rawgeti(L,1,i+1);    /* index 1 is the map data table */
    for(j=0;j<rows;j++) {
      lua_rawgeti(L,-1,j+1); /* floor data is on top of the stack */
      for(k=0;k<cols;k++) {
	lua_rawgeti(L,-1,k+1); /* vertice data */
	(*graph)->map[k][j][i] = lua_toverticep(L);
	lua_pop(L,1);
      }
      lua_pop(L,1);
    }
    lua_pop(L,1);
  }
  lua_settop(L,-1);
  return 1;
}


LUA_PF_FUNC(pf_shortest_path) {
  int n, vert_idx;
  Graph *graph;
  Point start, goal;
  // argument check
  if(!lua_isuserdata(L,1)) {
    lua_pushnil(L);
    lua_pushfstring(L,"[pf_shortest_path] Userdata expected, passed %s as first argument",lua_typename(L,lua_type(L,1)));
    return 2;
  }
  
  n;
  for(n=2;n<=7;n++) {
    if(!lua_isnumber(L,n)) {
      lua_pushnil(L);
      lua_pushfstring(L,"[pf_shortest_path] Number expected, passed %s as argument %d", lua_typename(L,lua_type(L,n)),n);
      return 2;
    }
  }
  // now we get to the goods
  graph    = *(Graph **)lua_touserdata(L,1);
  SETPT(start,lua_tonumber(L,2), lua_tonumber(L,3), lua_tonumber(L,4));
  SETPT(goal, lua_tonumber(L,5), lua_tonumber(L,6), lua_tonumber(L,7));
  
  reset_graph(graph);
  graph->mobility = lua_tomobility(L);
  
  // sanity check to make sure we're not walking into walls
  if(!VPASSABLE(VERT(graph,start),graph->mobility)
     || !VPASSABLE(VERT(graph,goal),graph->mobility)) {
    lua_pushnil(L);
    lua_pushfstring(L,
		    "Unpassable goal (x:%d,y:%d,z:%d) or destination (x:%d,y:%d,z:%d)",
		    start.x,
		    start.y,
		    start.z,
		    goal.x,
		    goal.y,
		    goal.z);
    return 2;
  }
  
  shortest_path(graph,start,goal);
  
  vert_idx = 1;
  lua_newtable(L);
  add_path(L,vert_idx++,goal);
  
  // TODO, check for un traversable paths (from a walled off area or whatnot)
  while(VALID_PT(VERT(graph,goal)->parent)) {
    goal = VERT(graph,goal)->parent;
    add_path(L,vert_idx++,goal);
  }
  
  return 1;
}

LUA_PF_FUNC(pf_weight) {
  Graph **graph = (Graph **)lua_touserdata(L,1);
  coord x, y, z;
  x = (coord)luaL_checknumber(L,2);
  y = (coord)luaL_checknumber(L,3);
  z = (coord)luaL_checknumber(L,4);
  lua_pushnumber(L,(**graph).map[x][y][z]->weight);
  lua_settop(L,-1);
  return 1;
}

LUA_PF_FUNC(pf_free_graph) {
  Graph *graph = *(Graph **)lua_touserdata(L,-1);
  free_graph(graph);
  return 0;
}
// quickie, only here as a util for pf_walkable below
void swap(int *a,int *b) {
  int temp = *a;
  *a       = *b;
  *b       = temp;
}

/*
  Bresenham's line of sight algorithm. This function assumes that
  mobility is still set on the graph userdata and is a potential
  GOTCHA.
 */
LUA_PF_FUNC(pf_walkable) {
  coord x0,y0,x1,y1, z;
  bool walkable, steep;
  int dx, dy, y, ystep, x;
  float derr, err;  
  Graph *graph;
  Vertice *last, *v;
  
  walkable = true;
  graph    = *(Graph **)lua_touserdata(L,1);  
  z        = (coord)lua_tonumber(L,2);
  x0       = (coord)lua_tonumber(L,3);
  y0       = (coord)lua_tonumber(L,4);
  x1       = (coord)lua_tonumber(L,5);
  y1       = (coord)lua_tonumber(L,6);

  // steep if delta(y) is greater than delta(x)
  steep   = abs(y1 - y0) > abs(x1 - x0);
  
  if(steep) {   // rotate the grid 90 degrees
    swap(&x0,&y0);
    swap(&x1,&y1);
  }
  
  if(x0 > x1) { // start looking from the point to the left
    swap(&x0,&x1);
    swap(&y0,&y1);
  }
  
  dx     = x1 - x0;
  dy     = abs(y1 - y0);
  derr   = (float)dy / (float)dx;
  err    = 0;
  
     y   = y0;
  ystep  = y0 < y1 ? 1 : -1;


  last = steep ? graph->map[y][x0][z] : graph->map[x0][y][z];
  for(x = x0; x <= x1;) {
    v = steep ? graph->map[y][x][z] : graph->map[x][y][z];
    
    if(!VPASSABLE(v,graph->mobility)
       || last->weight < v->weight
       || v->jumps
       || v->direction
       ) {
      walkable = false;
      break;
    }
    
    last = v;    
    err += derr;
    
    if(err >= 0.5) {
      y += ystep;
      err--;
    }
    x++;
  }

  lua_pushboolean(L,walkable);
  return 1;
}

LUA_PF_FUNC(pf_perf) {
#ifdef PROFILE
  chudInitialize();  
  chudAcquireRemoteAccess();
  char lbl_str[8192];
  chudStartRemotePerfMonitor(lbl_str);
  lua_call(L,0,0); // call a passed closure
  chudStopRemotePerfMonitor();
  chudReleaseRemoteAccess();
#else
  printf("(Mac only)Profiling not enabled, to switch this on add -DPROFILE to the command line when compiling\n");
#endif
  return 0;
}

static const struct luaL_reg pathfind [] = {
  {"pf_init", pf_init},
  {"pf_weight", pf_weight},
  {"pf_shortest_path", pf_shortest_path},
  {"pf_free_graph", pf_free_graph},
  {"pf_walkable", pf_walkable},
  {"pf_perf", pf_perf},
  {NULL,NULL}
};

PATHFIND_API int luaopen_libpathfind(lua_State *L) {
  luaL_register(L,"libpathfind",pathfind);
  return 1;
}
