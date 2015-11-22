// Copyright (C) 2003 Torben Könke. Licensed under GPL, see LICENSE.
//
// bg_botshared.h -- header file shared by game and cgame
//

#define BOOL qboolean
#define TRUE qtrue
#define FALSE qfalse

#define WPS_FILE_VERSION 0x00000001
#define MAX_NUM_WAYPOINTS 1024
#define MAX_NUM_LINKS 12
#define WPS_MAGIC "kajibot"

typedef struct tagWPSHEADER
{
	char	szMagic[8];
	int		nVersion;
	int		nWaypoints;
	int		nObjectives;
	char	szMapname[32];
} WPSHEADER, *LPWPSHEADER;

typedef struct
{
	int		indexTarget;
	float	length;
	byte	flags;
} WPPATH;

// important for waypoints that have the FL_WP_OBJECTIVE flag set
typedef struct objdata_s
{
	unsigned int team 	: 2;
	unsigned int type 	: 2;
	unsigned int state	: 2;
	unsigned int prior	: 1;

	// holds entity num of linked func_explosive for bomb objective
	unsigned int entNum	: 10;

	// holds wolf map objective num so state can be changed
	unsigned int objNum	: 3;

} objdata_t;

// c does not allow bitfield arrays
typedef struct wpdata_s
{
	unsigned int btnIndex	: 10;

	unsigned int entNum1	: 10;	// only relevant to waypoints that have an unreachable
	unsigned int entNum2	: 10;	// flag set. 
	unsigned int entNum3	: 10;	// Hold entity nums of blocking func_entities.
	unsigned int entNum4	: 10;	// 0 if not used , worldspawn anyway.

} wpdata_t;

typedef struct
{
	vec3_t		origin;
	int			index;
	int			flags;

	wpdata_t	wpdata;
	objdata_t	objdata;

	int			numPaths;
	WPPATH		*paths;

} WAYPOINT;

/* kaji : no longer used
typedef struct
{
	vec3_t	origin;
	BOOL	priority;		// Primary important or Secondary unimportant Objective 
	int		team;			// which teams objective is it
	int		type;			// what type of objective is it
	int		state;			// what state is the objective currently in
} MAPOBJECTIVE;
*/

typedef struct pathnode
{
	int		wpIndex;
	double	g;
	double	h;
	double	f;
	struct	pathnode *parent;
	struct	pathnode *NextNode;
	struct	pathnode *prev;

} PATHNODE;

#define FL_PATH_DUCK	0x00000001
#define FL_PATH_JUMP	0x00000004
#define FL_PATH_WALK	0x00000008
#define FL_PATH_BLOCKED	0x00000010
#define FL_PATH_LADDER	0x00000020

#define WAYPOINT_RADIUS 80

#define FL_WP_CAMP					0x00000001
#define FL_WP_OBJECTIVE				0x00000004
#define FL_WP_DOOR					0x00000008
#define FL_WP_AXIS_UNREACHABLE		0x00000010		// a waypoint marked as unreachable
#define FL_WP_ALLIES_UNREACHABLE	0x00000020		// can first be reached if the objective linked to it has been accomplished
#define FL_WP_MG42					0x00000040		// Waypoint is near a MG42
#define FL_WP_AXIS_DOCS_DELIVER		0x00000080
#define FL_WP_ALLIES_DOCS_DELIVER	0x00000100

enum
{
	STATE_UNACCOMPLISHED,
	STATE_ACCOMPLISHED,
	STATE_INPROGRESS
};

enum
{
	OBJECTIVE_BOMB,
	OBJECTIVE_STEAL,
	OBJECTIVE_CAPTURE
};

enum
{
	PRIORITY_HIGH,
	PRIORITY_LOW
};

#define TEAM_AXIS 1
#define TEAM_ALLIES 2

#define BOT_YAW_SPEED 400	// 400 Grad pro Sekunde
#define BOT_PITCH_SPEED 360

typedef struct
{
	int		iCommand;
	char	*sId;
} radioCommand_t;

typedef enum
{
	RADIO_NONE,

	RADIO_PATHCLEARED,
	RADIO_ENEMYWEAKENED,
	RADIO_ALLCLEAR,
	RADIO_INCOMING,
	RADIO_FIREINTHEHOLE,
	RADIO_DEFENSE,
	RADIO_OFFENSE,
	RADIO_TAKINGFIRE,

	RADIO_MEDIC,
	RADIO_NEEDAMMO,
	RADIO_NEEDBACKUP,
	RADIO_NEEDENGINEER,
	RADIO_COVERME,
	RADIO_HOLDFIRE,
	RADIO_WHERETO,

	RADIO_FOLLOWME,
	RADIO_LETSGO,
	RADIO_MOVE,
	RADIO_CLEARPATH,
	RADIO_DEFENDOBJECTIVE,
	RADIO_DISARMDYNAMITE,

	RADIO_YES,
	RADIO_NO,
	RADIO_THANKS,
	RADIO_WELCOME,
	RADIO_SORRY,

	RADIO_HI,
	RADIO_BYE,
	RADIO_GREATSHOT,
	RADIO_CHEER,
	RADIO_OOPS,
	RADIO_GOODGAME,

	RADIO_IAMSOLDIER,
	RADIO_IAMMEDIC,
	RADIO_IAMENGINEER,
	RADIO_IAMLIEUTENANT

} enumRadioCommands_t;
	
typedef enum
{
	TASK_ROAM,
	TASK_PLANTDYNAMITE,
	TASK_DISARMDYNAMITE,
	TASK_ATTACK,
	TASK_RETREAT,
	TASK_ACCOMPANY,
	TASK_THROWGRENADE,
	TASK_CAMP,
	TASK_MOVETOPOSITION,		// WolfBot 1.5 : finally got rid of this
	TASK_PICKUPOBJECT,
	TASK_CAPTURE,
	TASK_DESTROYBARRIER,
	TASK_PRESSBUTTON,
	TASK_ACTIVATEMG42

} botTasks_t;

#define DYNAMITE_PLANTTIME	1800

// inspired by podbot http://podbot.nuclearbox.com/
typedef struct bottask_s bottask_t;

struct bottask_s
{
	bottask_t *pPrevTask;
	bottask_t *pNextTask;
	int iTask;
	int iExpireTime;
	int iIndex;			// some tasks include moving to waypoints

};

#define MAX_NUM_BOTSKILLS 32

typedef struct
{
	int		iCombatMoveSkill;	// Range : 0 - 100
	int		iReactionTime;		// In Miliseconds
	int		iMaxYawSpeed;		// Maximum YAW Speed / Second
	int		iMaxPitchSpeed;		// Maximum PITCH Speed / Second
	float	fAimSkill;			// Range : 0 - 2
	BOOL	bPickupWeapons;		//
	BOOL	bInstantTurn;		//

	char	szIdent[64];		// skill name identifier

} botSkill_t;

#define FOOTSTEP_HEARING_DISTANCE	700.0f