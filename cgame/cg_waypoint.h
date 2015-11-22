// Copyright (C) 2003 Torben Könke. Licensed under GPL, see LICENSE.
//

#include "../game/bg_botshared.h"

//
// cg_waypoint.c
//

void Cmd_WP_LoadFile( void );
void Cmd_WP_SaveFile( void );
void Cmd_WP_PlaceWaypoint( void );
void Cmd_WP_PlaceObjective( void );
void Cmd_WP_DeleteWaypoint( void );
void Cmd_WP_SetFlags( void );
void Cmd_WP_ConnectWaypoints( void );
void Cmd_WP_DisconnectWaypoints( void );
void Cmd_WP_SetPathFlags( void );
void Cmd_WP_Rendermode( void );
void Cmd_WP_Cleanup( void );
void Cmd_WP_TestPath( void );
void Cmd_WP_DumpFlags( void );
void Cmd_WP_PathBox( void );
void Cmd_WP_Menu( void );

void CG_AddWaypoints( void );
WAYPOINT *CG_GetWaypointFromId( int id );

void CG_CrosshairBModelBBox( int eType, int *pEntNum );

void CG_DrawWaypointInfo( void );
void CG_DrawWaypointMenu( void );
void CG_WP_MenuInput( int iKey );

void Cmd_Bot_Menu( void );
void CG_DrawBotMenu( void );
void CG_Bot_MenuInput( int iKey );

PATHNODE *AStarSearch( PATHNODE *Start, PATHNODE *Goal );
PATHNODE *MakeChildren( PATHNODE *Parent );
float GoalDistanceEstimate( PATHNODE *Current, PATHNODE *Goal );
BOOL SameState( PATHNODE *a, PATHNODE *b );
BOOL IsGoalNode( PATHNODE *Compare, PATHNODE *Goal );

// waypoint menu stuff
#define TEXT_X_OFFSET 70

extern int iMenuState;
extern int iAddBotMenuState;

typedef enum
{
	WP_MENU_NONE,

	WP_MENU_MAIN,
	
	// objective menus
	WP_MENU_OBJ_TYPE,
	WP_MENU_OBJ_TEAM,
	WP_MENU_OBJ_PRIORITY,
	WP_MENU_OBJ_TARGET,			// for bomb objectives
	WP_MENU_OBJ_NUMBER,
	
	// path menus
	WP_MENU_CONNECT,
	WP_MENU_CONNECT_TYPE,

	WP_MENU_DISCONNECT,
	WP_MENU_DISCONNECT_TYPE,

	WP_MENU_SET_WAYPOINT_FLAGS,
	WP_MENU_SET_PATH_FLAGS,

	WP_MENU_RESET_FLAGS,		// resets both waypoint and path flags

	WP_MENU_SET_FLAGS_UNREACHABLE_TEAM,
	WP_MENU_SET_FLAGS_UNREACHABLE_ENTITIES,

	WP_MENU_SET_FLAGS_BUTTON_INDEX,	// for door waypoints

	WP_MENU_SAVE,

	WP_MENU_DRAG

} menuState_t;

typedef enum
{
	BOT_MENU_NONE,

	BOT_MENU_MAIN,

	BOT_MENU_CLASS,
	BOT_MENU_TEAM,
	BOT_MENU_WEAPON		// for soldier and lt

} botmenuState_t;