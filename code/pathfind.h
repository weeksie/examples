#include <float.h>
#include "lua.h"


#if defined(WIN32)
 #if defined(libpathfind_EXPORTS)
  #define PATHFIND_API __declspec(dllexport)
 #else
  #define PATHFIND_API
 #endif
 typedef int _Bool;
 #define bool _Bool
 #define true 1
 #define false 0
#else
 #include <stdbool.h>
 #define PATHFIND_API extern
#endif



#ifndef MAX_X
#define MAX_X 320
#ifndef WIN32
#warning "MAX_X not specified, defined as 320"
#endif
#endif

#ifndef MAX_Y
#define MAX_Y 320
#ifndef WIN32
#warning "MAX_Y not specified, defined as 320"
#endif
#endif


/* MAX_Z is not a real Z index as it can refer to other floors or maps
   on the same plane */
#ifndef MAX_Z
#define MAX_Z 6
#ifndef WIN32
#warning "MAX_Z not specified, defined as 6"
#endif
#endif

#define UNPASSABLE 255
#define SQ2 1.41421356f


#define INFINITE_COST FLT_MAX
typedef float cost;
typedef int coord;

typedef struct  _Point {
  coord x,y,z;
} Point;

typedef struct _JumpNode {
  Point exit;
  cost weight;
  struct _JumpNode *next;
} JumpNode;

typedef JumpNode* JumpList;

typedef unsigned int Mobility;

// Mobility is defined as inaccessible to 
#define WHEELS 0x02      // wheelchairs
#define SIGHT  0x04      // blind dudes
#define UNRESTRICTED 0x0 // or completely unrestricted

// D8  D1  D2 
//
// D7      D3
//   
// D6  D5  D4
typedef unsigned int Direction;
#define D1 0x002 
#define D2 0x004
#define D3 0x008
#define D4 0x010
#define D5 0x020
#define D6 0x040
#define D7 0x080
#define D8 0x100
#define D0 0x000

/* 
 lookup table for directions based on xdelta and ydelta
 DIRS[y][x]
*/
static unsigned int DIRS[3][3] = {
  {D8,D1,D2},
  {D7,D0,D3},
  {D6,D5,D4}
};


enum VerticeState { CLOSED, OPEN, UNTOUCHED };

#define VOPEN(x)      ((x)->state == OPEN)
#define VCLOSED(x)    ((x)->state == CLOSED)
#define VUNTOUCHED(x) ((x)->state == UNTOUCHED)

/*  
 xdelta and ydelta are transposed (*-1) from the actual traveling
 direction, e.g. if we're traveling up and to the right we're entering
 the new vertice from the bottom left. -1 0 => 1 0, we then add 1 to
 the total to get the Direction value from the DIRS lookup table.
*/
#define VENTRANCE(x,xd,yd) (((x)->direction & DIRS[((yd)*-1)+1][((xd)*-1)+1]) == 0)
#define VMOBILE(x,m) (((m) & (x)->mobility) == UNRESTRICTED)
#define VPASSABLE(x,m) ((x)->weight < UNPASSABLE && VMOBILE(x,m))

typedef struct _Vertice {
  cost weight; /* 1 - 255 */
  cost total_cost;
  JumpList jumps;
  Mobility mobility;
  Direction direction;
  enum VerticeState  state;  
  Point parent;
  Point prev;
  Point next;
} Vertice;

// once again MAX_Z is NOT a real z index
typedef Vertice* Map[MAX_X][MAX_Y][MAX_Z];

typedef struct _Graph {
  Mobility mobility;
  coord levels;
  coord rows;
  coord cols;
  Map map;  
} Graph;

// checks bounds for a point on a graph
#define VALID(graph,x,y,z)			\
  !( (x) >= (graph)->cols			\
  || (x) < 0					\
  || (y) >= (graph)->rows			\
  || (y) < 0					\
  || (z) >= (graph)->levels			\
  || (z) < 0)


#define VERT(graph,pt) (graph)->map[(pt).x][(pt).y][(pt).z]
#define MKPT(x,y,z) (struct Point){(coord)(x),(coord)(y),(coord)(z)}

#define SETPT(pt,_x,_y,_z)  (pt).x = (coord)(_x); \
			    (pt).y = (coord)(_y); \
			    (pt).z = (coord)(_z)

#define SETINVALID(pt) SETPT((pt),-99,-99,-99)
#define INVALID_POINT MKPT(-99,-99,-99)
#define VALID_PT(p) ((p).x != -99)

/* C API */

Vertice* lua_toverticep(lua_State *L);

void free_vertice(Vertice *p);
void free_graph(Graph *g);

cost cartesian(Point a, Point b);
bool pt_eq(Point a,Point b);

Point open_vertice(Graph *graph, Point open, Point point);
Point min_open_vertice(Graph *graph, Point open);
Point close_vertice(Graph *graph, Point open, Point close);
Point check_adjacent(Graph *graph, Point open, Point goal, Point p, int xd, int yd);
void reset_graph(Graph *graph);
void shortest_path(Graph *graph, Point start, Point goal);

/* LUA API */

#define LUA_PF_FUNC(name) static int name(lua_State *L)

