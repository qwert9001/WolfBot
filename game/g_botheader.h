// Copyright (C) 2003 Torben Könke. Licensed under GPL, see LICENSE.
//
// g_botheader.h -- various function declarations and type definitions

#include "bg_botshared.h"

typedef struct gentity_s gentity_t;

typedef struct
{
	PATHNODE	*pWaypointNodes;	// linked list of waypoints from start to goal node
	PATHNODE	*pStartNode;		// so we can easily free solution nodes

	int			iCurWaypoint;		// index of current waypoing bot is trying to reach
	int			iGoalWaypoint;		// index of goal waypoint
	int			iCurWaypointTime;	// time how long bot's been tryng to reach iCurWaypoint
	int			iLastWaypoint;		// last waypoint bot reached

	int			iGoalHistory[5];	// Last 5 nodes bot has reached. This is ONLY used for
									// Objective Waypoints at the moment so bots won't head
									// for the same objectives over and over again.
									// Mainly an issue if they come across a bomb spot where
									// another player has already planted a bomb but the
									// objective is not yet completed.

	int			iLongTermGoal;		// This is actually only used to store the bots goal
									// If bot needs to interrupt its route to find and
									// press a button to open a remote door.

	int			iLastCampTime;		// time bot last camped

	int			iPathFlags;			// flags of Path bot is currently on

	gentity_t	*pEnemy;
	int			iEnemyUpdateTime;	// is it time to look for/update enemy ?
	int			iEnemyLastSeen;		// time current enemy was last seen
	int			iEnemyFirstSeen;	// time current enemy was first seen

	int			iConditionFlags;	// Condition flags
	
	gentity_t	*pUser;				// accompanying a teammate ?
	int			iUseTime;			// last time user was seen

	gentity_t	*pItem;				// pointer to item bot wishes to pick up
	int			iLookForItemsTime;	// is it time to look for dropped weapons or other items ?
	int			iItemPickupTime;	// how long has bot been trying to pick up an item ?

	int			iRadioOrder;		// Radio Command bot has received this frame
	gentity_t	*pRadioEmitter;		// Client who sent the radio command
	int			iRadioDelayTimer;	// for delayed replying
	int			iRadioSendCmd;
	BOOL		bRadioTeam;
	int			iRadioLastUseTime;	// Last time bot issued a radio command

	vec3_t		vNoiseOrigin;		// Origin of Noise bot heard
	int			iLastNoiseTime;		// time when bot heard noise

	gentity_t	*pDynamite;			// Pointer to dynamite entity if bot planted it
	gentity_t	*pDocuments;
	gentity_t	*pCheckpoint;		// move these 3 into one single pointer...

	int			iCampTurnTime;		// Turn when camping

	int			iDuckJumpTime;		// need to perform a duckjump ?

	gentity_t	*pButton;

	int			iLastMG42Time;		// Last time bot operated an MG42 Machinegun
	int			iMG42StartTime;		// Time bot activated the MG42 Machinegun
	gentity_t	*pMG42;

	int			iEntityEventTime[MAX_GENTITIES];

	float		ideal_yaw;
	float		ideal_pitch;
	BOOL		bInitialized;	
	BOOL		bChangedClass;
	usercmd_t	ucmd;

	bottask_t	*pTasks;			// Pointer to linked list of waiting tasks

	int			iWeapon;			// bots current weapon (need to store since usercommand is cleared every frame)
	BOOL		bReloading;

	int			iFightStyle;
	int			iFSUpdateTime;		// fight style update time
	int			iJumpTime;

	BOOL		bShutdown;			// this is set to TRUE on a GAME_SHUTDOWN event to stop
									// bot from allocating new pathnodes

	gentity_t	*pFriend;			// Used by Medics and Lieutenant to drop health and ammo
	int			iFriendHelpTime;	// How long bot has been trying to help a friend

	int			iCheckMedicTime;	// If bot is low on health or wounded on ground look for medics
									// every once in a while and yell medic if one is nearby

	int			iCheckBackupTime;	// same as iCheckMedicTime for bots carrying the enemy docs
									// "Cover me" or "Follow me"

	// keep track of movement so we can figure out if bot is stuck somewhere
	vec3_t		vPrevOrigin;
	int			iPrevSpeed;
	int			iSpeedCheckTime;	// WolfBot 1.5 : Make check sv_fps independent

	int			iLastMoveTime;
	int			iStrafeTime;
	int			iStrafeDir;
	int			iLastJumpTime;

	BOOL		bWounded;			// TRUE if bot is laying on the ground wounded

	// these fields remain throughout the game and are not cleared on Bot's death
	int			playerClass;
	int			playerWeapon;
	int			clientNum;
	botSkill_t	*pSkill;		// pointer to a skill in botSkills array MUST never be NULL

} botClient_t;

#define SPAWNFLAG_DYNO_ONLY		64
#define SPAWNFLAG_EXPLO_ONLY	32

#define MAX_BOT_NAMES			32

//
// g_botclient.c
//

void Bot_StartFrame( int time );
void Bot_GetServerCommands( botClient_t *pBot );
void Bot_CheckSnapshot( gentity_t *ent );
int Bot_RetrieveSnapshotEntity( int clientNum, int sequence, entityState_t *state );
void Svcmd_Bot_Connect( gentity_t *ent, BOOL onMapRestart, int client );
void Svcmd_Bot_Remove( void );
void Svcmd_Bot_RemoveAll( void );
void Svcmd_Bot_FillServer( void );
void Cmd_Bot_MoveTo( gentity_t *ent );
void Cmd_Bot_ChangeYaw( gentity_t *ent );
void Cmd_Bot_ChangePitch( gentity_t *ent );
void Cmd_Bot_Follow( gentity_t *ent );
void Cmd_Bot_Debug_GoWounded( gentity_t *ent );
int CountTeamPlayers( int team, int playerType );
BOOL FlagAtHome( int iTeam );

int G_CountHumanPlayers( int team );
int G_CountBotPlayers( int team );
void Bot_CheckMinimumPlayers( void );

void Bot_LogInitSession( void );
void Bot_Log( const char *fmt, ... );
void Bot_LogEndSession( void );

void Bot_LoadNames( void );
void Bot_LoadSkills( void );
botSkill_t *Bot_FindSkillByName( const char *skill );

extern int mem_amount;

//
// g_botwaypoint.c
//

void WaypointLoadFile( void );
void WaypointCleanUp( void );
void WaypointInit( void );
void WaypointCheckWolfObjectives( void );
void WaypointUpdateBombWaypoints( void );
int WaypointFindCampSpot( gentity_t *ent, float fRadius );
int	WaypointFindNearest( gentity_t *ent );
int WaypointFindObjective( gentity_t *ent, int iType );
int WaypointFindRandomGoal( gentity_t *ent, int iFlags );
int WaypointFindVisible( vec3_t vPosition, int iFlags, float fMinRange, float fMaxRange );


PATHNODE *AStarSearch( int iIndexStart, int iIndexGoal );
BOOL IsGoalNode( PATHNODE *Compare, int iIndexGoal );
PATHNODE *MakeChildren( PATHNODE *Parent );
float GoalDistanceEstimate( PATHNODE *Current, int iIndexGoal );
BOOL SameState( PATHNODE *a, PATHNODE *b );
WAYPOINT *GetWaypoint( int index );

BOOL Bot_FreeSolutionNodes( botClient_t *pBot );
BOOL Bot_MapNavigation( gentity_t *ent );
int Bot_FindGoal( gentity_t* ent );

//
// g_botmain.c
//

void Bot_Think( gentity_t *ent, int time );
void Bot_PushTask( botClient_t *pBot, bottask_t *pTask );
bottask_t *Bot_GetTask( botClient_t *pBot );
void Bot_TaskComplete( botClient_t *pBot );
void Bot_RemoveTasks( botClient_t *pBot );
float Bot_ChangeYaw ( gentity_t *ent );
float Bot_ChangePitch( gentity_t *ent );
BOOL Bot_CheckPlayerClass( gentity_t *ent );
void Bot_Initialize( botClient_t *pBot );
gentity_t *Bot_FindRadius( gentity_t *from, vec3_t org, float rad );
BOOL Bot_EntityIsVisible( gentity_t *ent, gentity_t *other );
BOOL Bot_EntityInFOV( gentity_t *bot, gentity_t *other );
BOOL Bot_FindItem( gentity_t *ent );
BOOL Bot_LookForMedic( gentity_t *ent );
BOOL Bot_LookForTeammates( gentity_t *ent );
BOOL Bot_FollowUser( gentity_t *ent );
BOOL Bot_HelpingFriend( gentity_t *ent );
BOOL Bot_OperatingMG42( gentity_t *ent );
void Bot_CheckRadioMessages( gentity_t *ent );
void Bot_SendRadioCommand( gentity_t *ent, int iRadioCommand, BOOL bTeam );
void Bot_SendDelayedRadioCommand( gentity_t *ent, int iRadioCommand, BOOL bTeam );
int Bot_SelectWeapon( gentity_t *ent );
BOOL Bot_CanStrafeRight( gentity_t *ent );
BOOL Bot_CanStrafeLeft( gentity_t *ent );
BOOL Bot_CanJumpOnObstacle( gentity_t *ent, BOOL *bDuckJump );
BOOL Bot_HasEnemyFlag( gentity_t *ent );
BOOL Bot_HasHeavyWeapon( gentity_t *ent );

//
// g_botcombat.c
//

BOOL Bot_FindEnemy( gentity_t *ent );
void Bot_AttackMovement( gentity_t *ent );
void Bot_ShootAtEnemy( gentity_t *ent );
int Bot_GetSecondaryWeapon( gentity_t *ent );
BOOL Bot_UsingSecondaryWeapon( gentity_t *ent );
void Bot_TakeDamage( gentity_t *self, gentity_t *attacker, int damage, vec3_t point );
void Bot_ShootAtEnemyWithMG42( gentity_t *ent );
BOOL Bot_FindEnemyWithMG42( gentity_t *ent );

//
// g_botchat.c
//

void Bot_LoadChatFile( void );
void Bot_UnloadChatFile( void );
void Bot_ProcessChatMessage( gentity_t *ent, char *output, const char *input );
char *Bot_ClientName (int client, char *name, int size );
char *Bot_CleanClientName( int client, char *buf, int size );
char *Bot_GetRandomChatRecord( gentity_t *ent );

