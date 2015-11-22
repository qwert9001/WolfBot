// Copyright (C) 2003 Torben Könke. Licensed under GPL, see LICENSE.
//
// g_botclient.c -- code to maintain fake client 

#include "g_local.h"

botClient_t		botClients[MAX_CLIENTS];
fileHandle_t	f;							// for logging
int				mem_amount = 0;				// keep track of memory allocation

char			botNames[MAX_BOT_NAMES][32 + 5];
int				numNames = 0;

botSkill_t		botSkills[MAX_NUM_BOTSKILLS];
int				numSkills = 0;

extern BOOL		bWaypointsLoaded;			// TRUE if wps file was successfully read in

extern WAYPOINT			Waypoints[MAX_NUM_WAYPOINTS];
extern int				numWaypoints;
extern int				ObjWaypoints[32];
extern int				numObjWaypoints;	

// Radio Command Array
radioCommand_t radioCommands[] =
{
	{ RADIO_NONE,				""					},

	{ RADIO_PATHCLEARED,		"PathCleared"		},
	{ RADIO_ENEMYWEAKENED,		"EnemyWeakened"		},
	{ RADIO_ALLCLEAR,			"AllClear"			},
	{ RADIO_INCOMING,			"Incoming"			},
	{ RADIO_FIREINTHEHOLE,		"FireInTheHole"		},
	{ RADIO_DEFENSE,			"OnDefense"			},
	{ RADIO_OFFENSE,			"OnOffense"			},
	{ RADIO_TAKINGFIRE,			"TakingFire"		},
	{ RADIO_MEDIC,				"Medic"				},
	{ RADIO_NEEDAMMO,			"NeedAmmo"			},
	{ RADIO_NEEDBACKUP,			"NeedBackup"		},
	{ RADIO_NEEDENGINEER,		"NeedEngingeer"		},
	{ RADIO_COVERME,			"CoverMe"			},
	{ RADIO_HOLDFIRE,			"HoldYourFire"		},
	{ RADIO_WHERETO,			"WhereTo"			},
	{ RADIO_FOLLOWME,			"FollowMe"			},
	{ RADIO_LETSGO,				"LetsGo"			},
	{ RADIO_MOVE,				"Move"				},
	{ RADIO_CLEARPATH,			"ClearPath"			},
	{ RADIO_DEFENDOBJECTIVE,	"DefendObjective"	},
	{ RADIO_DISARMDYNAMITE,		"DisarmDynamite"	},
	{ RADIO_YES,				"Affirmative"		},
	{ RADIO_NO,					"Negative"			},
	{ RADIO_THANKS,				"Thanks"			},
	{ RADIO_WELCOME,			"Welcome"			},
	{ RADIO_SORRY,				"Sorry"				},
	{ RADIO_HI,					"Hi"				},
	{ RADIO_BYE,				"Bye"				},
	{ RADIO_GREATSHOT,			"GreatShot"			},
	{ RADIO_CHEER,				"Cheer"				},
	{ RADIO_OOPS,				"Oops"				},
	{ RADIO_GOODGAME,			"GoodGame"			},
	{ RADIO_IAMSOLDIER,			"IamSoldier"		},
	{ RADIO_IAMMEDIC,			"IamMedic"			},
	{ RADIO_IAMENGINEER,		"IamEngineer"		},
	{ RADIO_IAMLIEUTENANT,		"IamLieutenant"		},

	{ 0, NULL }
};

//
// Server Command functions to maintain bots
//

//
// Can be called directly to add a bot and is also called from vmMain
//

void Svcmd_Bot_Connect( gentity_t *ent, BOOL onMapRestart, int client )
{
	char		userinfo[MAX_INFO_STRING];
	int			clientNum;
	char		szTeam[64];
	char		szClass[64];
	char		szWeapon[64];
	int			iRand;
	char		szNetname[128];
	char		szSkill[64];
	char		*pValue;
	botSkill_t	*pSkill;
	char		*pClientConnect;

	if( bWaypointsLoaded == FALSE )
	{
		G_Printf("Could not load WPS file for this map.\n");
		return;
	}

	// Aufruf aus vmMain durch einen map_restart command
	if( onMapRestart )
		clientNum = client;
	else
		clientNum = trap_BotAllocateClient();

	if( clientNum == -1 )
	{
		G_Printf("Failed to allocate a client slot. Try setting sv_maxclients to a higher value.\n");
		return;
	}

	userinfo[0] = '\0';

	if( onMapRestart )
	{
		trap_GetUserinfo( client, userinfo, sizeof(userinfo) );
	}
	else
	{
		strcpy( szNetname, "[Bot]" );
		strcat( szNetname, botNames[ rand() % numNames ] );

		Info_SetValueForKey( userinfo, "name", va("%s\0", szNetname) );
		Info_SetValueForKey( userinfo, "rate", "25000" );
		Info_SetValueForKey( userinfo, "snaps", "20" );
		Info_SetValueForKey( userinfo, "model", "visor/default" );
		Info_SetValueForKey( userinfo, "headmodel", "visor/default" );
		Info_SetValueForKey( userinfo, "sex", "male" );
		Info_SetValueForKey( userinfo, "cg_autoactivate" ,"1" ); // PICKUP_TOUCH
		Info_SetValueForKey( userinfo, "botskill", "normal" ); // may be overwritten by user
	}
	
	trap_SetUserinfo( clientNum, userinfo );

	memset( &botClients[clientNum], 0, sizeof(botClient_t) );

	g_entities[clientNum].botClient = &botClients[clientNum];
	g_entities[clientNum].botClient->clientNum = clientNum;

	// set the bot skill
	pValue = Info_ValueForKey( userinfo, "botskill" );
	pSkill = Bot_FindSkillByName( pValue );

	// this should NEVER happen
	if( pSkill == NULL )
	{
		G_Printf("Warning : pSkill is NULL in Svcmd_Bot_Connect\n");

		g_entities[clientNum].botClient->pSkill = botSkills;
	}
	else
	{
		g_entities[clientNum].botClient->pSkill = pSkill;
	}

	// init some stuff
	Bot_Initialize(&botClients[clientNum]);

	// apparently the engine checks for this flag. This is really annoying but I could
	// not figure out a work around to this. If this flag is not set and a bot client is being
	// dropped the engine spills out a hell of Net_CompareAddr Warnings.
	// Also If this flag is not set several bots will cause the server to crash with the error
	// "SV_NetChan_TransmitNextFragment : netchan queue not properly intialized."
	if( g_svBotFlag.integer )
		g_entities[clientNum].r.svFlags |= SVF_BOT;

	// is it a firsttime connect (i.e. not called from within vmMain) ?
	// If so, let ClientConnect know so it can initialize the session
	// data.
	if( !onMapRestart )
	{
		pClientConnect = ClientConnect( clientNum, qtrue, qfalse );
	}
	else
	{
		pClientConnect = ClientConnect( clientNum, qfalse, qfalse );

		// make sure playerClass var is set if reading from session data
		// once i have got some sort of bot character files working i need to also
		// hook weapon type over here
		g_entities[clientNum].botClient->playerClass = g_entities[clientNum].client->sess.playerType;
		g_entities[clientNum].botClient->playerWeapon = g_entities[clientNum].client->sess.playerWeapon;
	}

	// WolfBot 1.5 : Log if ClientConnect failed for some reason
	if( pClientConnect != NULL )
	{
		if( g_debugBots.integer )
		{
			Bot_Log("Warning : ClientConnect failed : %s", pClientConnect);
		}

		G_Printf("Warning : ClientConnect failed : %s\n", pClientConnect);
	}

	ClientBegin( clientNum );

	// Map Restart (bei Rundenende oder map change) benutzt Session Data
	if( !onMapRestart )
	{
		if( trap_Argc() > 1 )
		{
			G_Printf("Syntax : bot_connect [team] [class] [skill] [weapon]\n");

			trap_Argv( 1, szTeam, sizeof(szTeam) );

			// spezielles team
			if( Q_stricmp( szTeam, "red" ) && Q_stricmp( szTeam, "blue" ) && Q_stricmp( szTeam, "r" ) && Q_stricmp( szTeam, "blue" ) )
			{
				G_Printf("Invalid Team. Adding Bot to random team\n");

				if( (rand() % 100) > 50 )
				{
					strcpy( szTeam, "red" );
				}
				else
				{
					strcpy( szTeam, "blue" );
				}
			}
		}

		// spezielle player class
		if( trap_Argc() > 2 )
		{
			trap_Argv( 2, szClass, sizeof(szClass) );

			if( !Q_stricmp( szClass, "soldier" ) )
			{
				strcpy( szClass, "0" );
			}
			else if( !Q_stricmp( szClass, "medic" ) )
			{
				strcpy( szClass, "1" );
			}
			else if( !Q_stricmp( szClass, "engineer" ) )
			{
				strcpy( szClass, "2" );
			}
			else if( !Q_stricmp( szClass, "lieutenant" ) )
			{
				strcpy( szClass, "3" );
			}

			// check for the value now
			if( Q_stricmp( szClass, "0" ) && Q_stricmp( szClass, "1" ) && Q_stricmp( szClass, "2" ) && Q_stricmp( szClass, "3" ) )
			{
				G_Printf("Invalid Player Class. Choosing a random class.\n");

				iRand = rand() % 100;

				if( iRand > 75 )
				{
					strcpy( szClass, "3" );
				}
				else if( iRand > 50 )
				{
					strcpy( szClass, "2" );
				}
				else if( iRand > 25 )
				{
					strcpy( szClass, "1" );
				}
				else
				{
					strcpy( szClass, "0" );
				}
			}
		}

		// spezielles skill level
		if( trap_Argc() > 3 )
		{
			trap_Argv( 3, szSkill, sizeof(szSkill) );

			pSkill = Bot_FindSkillByName(szSkill);

			if( pSkill )
			{
				g_entities[clientNum].botClient->pSkill = pSkill;

				// update the userinfo
				Info_SetValueForKey( userinfo, "botskill", pSkill->szIdent );
				trap_SetUserinfo( clientNum, userinfo );
			}
			else
			{
				G_Printf("Could not find Skill, using default.\n");
			}
		}

		// spezielle waffe für soldier oder lieutenant ?
		if( trap_Argc() > 4 )
		{
			if( !Q_stricmp( szClass, "0" ) || !Q_stricmp( szClass, "3" ) )
			{
				trap_Argv( 4, szWeapon, sizeof(szWeapon) );

				// Das ist dumm gemacht in Wolfenstein.
				// Man benutzt nicht die WP_ Konstanten sondern wieder eigene Werte
				// d.h. wir können nicht einfach BG_FindItem aufrufen.
				// Siehe auch SetWolfSpawnWeapons().
				if( !Q_stricmp( szWeapon, "MP40" ) )
				{
					strcpy( szWeapon, "3" );
				}
				else if( !Q_stricmp( szWeapon, "Thompson" ) )
				{
					strcpy( szWeapon, "4" );
				}
				else if( !Q_stricmp( szWeapon, "Sten" ) )
				{
					strcpy( szWeapon, "5" );
				}
				else if( !Q_stricmp( szWeapon, "Mauser" ) )
				{
					// Wir müssen hier nicht extra prüfen ob die Klasse
					// Lieutenant ist , das wird ohnehin in SetWolfSpawnVars gemacht.
					strcpy( szWeapon, "6" );
				}
				else if( !Q_stricmp( szWeapon, "Panzerfaust" ) )
				{
					strcpy( szWeapon, "8" );
				}
				else if( !Q_stricmp( szWeapon, "Venom" ) )
				{
					strcpy( szWeapon, "9" );
				}
				else if( !Q_stricmp( szWeapon, "Flamethrower" ) )
				{
					strcpy( szWeapon, "10" );
				}

				if( Q_stricmp( szWeapon, "3" ) && Q_stricmp( szWeapon, "4" ) && Q_stricmp( szWeapon, "5" ) && Q_stricmp( szWeapon, "6" ) && Q_stricmp( szWeapon, "8" ) && Q_stricmp( szWeapon, "9" ) && Q_stricmp( szWeapon, "10" ) )
				{
					G_Printf("Could not find weapon. Giving Bot default Weapon.\n");

					// axis get mp40 by default , allies thompson
					if( !Q_stricmp( "red", szTeam ) )
					{
						strcpy( szWeapon, "3" );
					}
					else
					{
						strcpy( szWeapon, "4" );
					}
				}
			}
		}


		if( trap_Argc() == 1 || trap_Argc() == 2 )
		{
			// i.e. bot_connect without params
			if( trap_Argc() == 1 )
			{
				// join team with less players
				if( CountTeamPlayers(TEAM_RED, -1) >= CountTeamPlayers(TEAM_BLUE, -1) )
				{
					strcpy( szTeam, "blue" );
				}
				else
				{
					strcpy( szTeam, "red" );
				}

				G_Printf("Joining Team with less Players!\n");
			}

			// set random class
			iRand = rand() % 100;

			if( iRand > 75 )
			{
				strcpy( szClass, "3" );
			}
			else if( iRand > 50 )
			{
				strcpy( szClass, "2" );
			}
			else if( iRand > 25 )
			{
				strcpy( szClass, "1" );
			}
			else
			{
				strcpy( szClass, "0" );
			}

			// set weapon to 1
			strcpy( szWeapon, "1" );

			G_Printf("Random Class for Bot : %s\n", szClass );
		}

		if( g_debugBots.integer )
		{
			G_Printf("Setting Wolfdata!\n");

			G_Printf("Weapon Num : %i\n", atoi(szWeapon) );
			G_Printf("Class num : %i\n", atoi(szClass) );
		}

		SetWolfData( &g_entities[clientNum], szClass, szWeapon, "0", "1" );

		g_entities[clientNum].botClient->playerClass = atoi(szClass);
		g_entities[clientNum].botClient->playerWeapon = atoi(szWeapon);

		SetTeam( &g_entities[clientNum], szTeam );
	}

	if( g_debugBots.integer )
	{
		G_Printf("Bot %i has skill %s\n", clientNum, g_entities[clientNum].botClient->pSkill->szIdent );
	}
}

//
// Removes random Bot in Entity Array from Game
//

void Svcmd_Bot_Remove( void )
{
	int i;
	int numBots = 0;
	int bots[MAX_CLIENTS];
	int random;
	gentity_t *ent;

	for( i = 0; i < MAX_CLIENTS; i++ )
	{
		ent = &g_entities[i];

		if( ent->botClient )
		{
			bots[ numBots ] = ent->client->ps.clientNum;
			numBots++;
		}
	}

	if( numBots == 0 )
	{
		G_Printf("There are no bot clients on this server!\n");
		return;
	}

	random = rand() % numBots;

	g_entities[ bots[ random ] ].botClient->bShutdown = TRUE;

	trap_DropClient( bots[ random ], "Bot kicked" );
}

//
// Removes all Bots on Server
//

void Svcmd_Bot_RemoveAll( void )
{
	int i;
	gentity_t *ent;

	for( i = 0; i < MAX_CLIENTS; i++ )
	{
		ent = &g_entities[i];

		if( ent->botClient )
			trap_DropClient( i, "Bot kicked" );
	}
}

//
// Fills server with bots
//

void Svcmd_Bot_FillServer( void )
{
	int i;
	int max;

	max = g_maxclients.integer;

	if( max > 6 )
	{
		max = 6;
	}

	// this should go in a queue
	for( i = 0; i < max; i++ )
	{
		// be sure to add a dynamite guy to each team
		if( i == 0 )
		{
			trap_SendConsoleCommand( EXEC_APPEND, "bot_connect blue engineer\n" );
		}
		else if( i == 1 )
		{
			trap_SendConsoleCommand( EXEC_APPEND, "bot_connect red engineer\n" );
		}
		else
		{
			trap_SendConsoleCommand( EXEC_APPEND, "bot_connect\n" );
		}
	}
}

//
// Reads in the botnames.cfg file
//

void Bot_LoadNames( void )
{
	fileHandle_t f;
	char szData[8196];
	char *token;
	char *pString;

	trap_FS_FOpenFile( "botnames.cfg", &f, FS_READ );

	if( !f )
	{
		G_Printf("Could not open botnames.cfg\n");
		return;
	}

	trap_FS_Read( szData, sizeof(szData), f );

	pString = szData;

	while(1)
	{
		token = COM_ParseExt( &pString, TRUE );

		if( !token[0] )
			break;

		if( numNames >= MAX_BOT_NAMES )
			break;

		strcpy( botNames[ numNames ], token );

		numNames++;

		if( g_debugBots.integer )
		{
			G_Printf("Name Token : %s\n", token );
		}
	}

	trap_FS_FCloseFile(f);

	G_Printf("Successfully read in botnames.cfg\n");
}

//
// Reads in the botskills.cfg file
//

void Bot_LoadSkills( void )
{
	fileHandle_t f;
	BOOL bAborted;
	char szData[4096];
	char *pFile;
	char *pToken;

	char Variable[64];
	char Value[64];

	trap_FS_FOpenFile( "botskills.cfg", &f, FS_READ );

	memset( &botSkills[0], 0, sizeof(botSkill_t)  * MAX_NUM_BOTSKILLS );

	if( !f )
	{
		G_Printf("Warning : Could not open botskills.cfg\n");

		// create a default skill so we can still add bots
		botSkills[0].bInstantTurn		= FALSE;
		botSkills[0].bPickupWeapons		= TRUE;
		botSkills[0].fAimSkill			= 1.0f;
		botSkills[0].iMaxPitchSpeed		= BOT_PITCH_SPEED;
		botSkills[0].iMaxYawSpeed		= BOT_YAW_SPEED;
		botSkills[0].iCombatMoveSkill	= 60;
		botSkills[0].iReactionTime		= 250;

		strcpy( botSkills[0].szIdent, "normal" );

		numSkills = 1;

		return;
	}

	trap_FS_Read( szData, sizeof(szData), f );
	pFile = szData;

	bAborted = FALSE;
	numSkills = 0;

	// parse the botskills file
	while(1)
	{
		if( numSkills >= MAX_NUM_BOTSKILLS )
		{
			G_Printf("MAX_NUM_BOTSKILLS exceeded in botskills.cfg\n");
			break;
		}

		// parse the 'skill' keyword
		pToken = COM_ParseExt( &pFile, TRUE );

		if( !pToken[0] )
			break;

		if( strcmp( pToken, "skill" ) )
		{
			G_Printf("Parse Error : Invalid token %s (expected 'skill')\n", pToken);
			bAborted = TRUE;
			break;
		}

		// parse the skill name
		pToken = COM_ParseExt( &pFile, TRUE );

		if( !pToken[0] )
		{
			G_Printf("Parse Error : Unexpected end of file in botskills.cfg\n");
			bAborted = TRUE;
			break;
		}

		strcpy( botSkills[ numSkills ].szIdent, pToken );

		// parse the opening bracket
		pToken = COM_ParseExt( &pFile, TRUE );

		if( strcmp( pToken, "{" ) )
		{
			G_Printf("Parse Error : Missing { in botskills.cfg\n");
			bAborted = TRUE;
			break;
		}

		while(1)
		{
			pToken = COM_ParseExt( &pFile, TRUE );

			if( !pToken[0] )
			{
				G_Printf("Parse Error : Unexpected end of file in botskills.cfg\n");
				bAborted = TRUE;
				break;
			}
			
			if( !strcmp( pToken, "}" ) )
			{
				// assume everything went OK and increment numSkills
				numSkills++;

				break;
			}

			// parse a variable/value set
			strcpy( Variable, pToken );

			// next token should be a '='
			pToken = COM_ParseExt( &pFile, TRUE );

			if( !pToken[0] )
			{
				G_Printf("Parse Error : Unexpected end of file in botskills.cfg\n");
				bAborted = TRUE;
				break;
			}

			if( pToken[0] != '=' )
			{
				G_Printf("Parse Error : Expected '=' in botskills.cfg\n");
				bAborted = TRUE;
				break;
			}

			// next token is the value
			pToken = COM_ParseExt( &pFile, TRUE );

			if( !pToken[0] )
			{
				G_Printf("Parse Error : Unexpected end of file in botskills.cfg\n");
				bAborted = TRUE;
				break;
			}

			strcpy( Value, pToken );

			if( !Q_stricmp( Variable, "bot_fightmove_skill" ) )
			{
				botSkills[ numSkills ].iCombatMoveSkill = atoi(Value);

				if( g_debugBots.integer )
					G_Printf("Parsing Move Skill (%i) for Skill %s\n", atoi(Value), botSkills[ numSkills ].szIdent );
			}
			else if( !Q_stricmp( Variable, "bot_reaction_time" ) )
			{
				botSkills[ numSkills ].iReactionTime = atoi(Value);

				if( g_debugBots.integer )
					G_Printf("Parsing Reaction Time (%i) for Skill %s\n", atoi(Value), botSkills[ numSkills ].szIdent );
			}
			else if( !Q_stricmp( Variable, "bot_yaw_speed" ) )
			{
				botSkills[ numSkills ].iMaxYawSpeed = atoi(Value);

				if( botSkills[ numSkills ].iMaxYawSpeed < 200 )
					botSkills[ numSkills ].iMaxYawSpeed = 200;

				if( g_debugBots.integer )
					G_Printf("Parsing YAW Speed (%i) for Skill %s\n", atoi(Value), botSkills[ numSkills ].szIdent );
			}
			else if( !Q_stricmp( Variable, "bot_pitch_speed" ) )
			{
				botSkills[ numSkills ].iMaxPitchSpeed = atoi(Value);

				if( botSkills[ numSkills ].iMaxPitchSpeed < 200 )
					botSkills[ numSkills ].iMaxPitchSpeed = 200;

				if( g_debugBots.integer )
					G_Printf("Parsing PITCH Speed (%i) for Skill %s\n", atoi(Value), botSkills[ numSkills ].szIdent );
			}
			else if( !Q_stricmp( Variable, "bot_aim_skill" ) )
			{
				botSkills[ numSkills ].fAimSkill = atof(Value);

				if( botSkills[ numSkills ].fAimSkill > 5.0f )
					botSkills[ numSkills ].fAimSkill = 5.0f;

				if( g_debugBots.integer )
					G_Printf("Parsing Aim Skill (%f) for Skill %s\n", atof(Value), botSkills[ numSkills ].szIdent );
			}
			else if( !Q_stricmp( Variable, "bot_pickup_weapons" ) )
			{
				botSkills[ numSkills ].bPickupWeapons = atoi(Value);

				if( g_debugBots.integer )
					G_Printf("Parsing Pickup Weapons (%i) for Skill %s\n", atoi(Value), botSkills[ numSkills ].szIdent );
			}
			else if( !Q_stricmp( Variable, "bot_instant_turn" ) )
			{
				botSkills[ numSkills ].bInstantTurn = atoi(Value);

				if( g_debugBots.integer )
					G_Printf("Parsing Instant Turn (%i) for Skill %s\n", atoi(Value), botSkills[ numSkills ].szIdent );
			}

		}
	}

	if( !bAborted )
		G_Printf("Successfully read in botskills.cfg\n");

	trap_FS_FCloseFile(f);
}

//
// Finds a Skill in the botskills array by the name identifier
//

botSkill_t *Bot_FindSkillByName( const char *skill )
{
	int i;

	for( i = 0; i < numSkills; i++ )
	{
		if( !Q_stricmp( skill, botSkills[i].szIdent ) )
			return &botSkills[i];
	}

	return NULL;
}

//
// Debug function for testing Pathfinding
//

void Cmd_Bot_MoveTo( gentity_t *ent )
{
	WAYPOINT *Target = NULL;
	int		 Nearest;
	PATHNODE *Path;
	char string[4];
	int index;

	if( trap_Argc() < 2 )
	{
		G_Printf("Syntax : bot_moveto Waypoint_id\n");
		return;
	}

	trap_Argv( 1, string, sizeof(string) );
	index = atoi(string);

	if( index < 0 )
	{
		G_Printf("Invalid Waypoint index\n");
		return;
	}

	Target = GetWaypoint(index);

	Nearest = WaypointFindNearest(&g_entities[1]);

	if( !Nearest )
		return;

	Path = AStarSearch( Nearest, Target->index );

	if( !Path )
	{
		G_Printf("AStarSearch failed\n");
		return;
	}

	// Botcode frees up solution nodes
//	g_entities[1].botClient->Goal = Path;
}

//
// Debug function 
//

void Cmd_Bot_ChangeYaw( gentity_t *ent )
{
	char arg[32];

	if( trap_Argc() < 2 )
		return;

	trap_Argv( 1, arg, sizeof(arg) );

	g_entities[1].botClient->ideal_yaw = atof(arg);
}

//
// Debug function
//

void Cmd_Bot_ChangePitch( gentity_t *ent )
{
	char arg[32];

	if( trap_Argc() < 2 )
		return;

	trap_Argv( 1, arg, sizeof(arg) );

	g_entities[1].botClient->ideal_pitch = atof(arg);
}

//
// Debug function
//

void Cmd_Bot_Follow( gentity_t *ent )
{
	bottask_t tmp;

	if( Bot_EntityIsVisible( &g_entities[1], ent ) == TRUE )
	{
		g_entities[1].botClient->pUser = ent;
		g_entities[1].botClient->iUseTime = level.time;

		tmp.iExpireTime = level.time + 7000 + rand() % 5000;
		tmp.iIndex = -1;
		tmp.iTask = TASK_ACCOMPANY;
		tmp.pNextTask = NULL;
		tmp.pPrevTask = NULL;

		Bot_PushTask( g_entities[1].botClient, &tmp );
	}
}

//
// Debug function to go into "wounded mode"
//

void Cmd_Bot_Debug_GoWounded( gentity_t *ent )
{
	G_Damage( ent, ent, ent, NULL, NULL, 100, 0, 0 );
}

//
// Returns number of players on team
//

int CountTeamPlayers( int team, int playerType )
{
	int i, num = 0;
	gclient_t	*cl;
	
	for ( i = 0; i < g_maxclients.integer; i++ ) 
	{
		cl = level.clients + i;
	
		if ( cl->pers.connected != CON_CONNECTED )
			continue;

		if ( team >= 0 && cl->sess.sessionTeam != team )
			continue;

		// looking for a specific class 
		if( playerType >= 0 && cl->sess.playerType != playerType )
			continue;
	
		num++;
	}

	return num;
}

//
// Returns TRUE if Teams flag is safe at home
//

BOOL FlagAtHome( int iTeam )
{
	int i;
	gentity_t *ent;

	for( i = 0; i < level.numConnectedClients; i++ )
	{
		ent = &g_entities[i];

		if( ent->client->sess.sessionTeam == iTeam )
			continue;

		if( ent->client->sess.sessionTeam == TEAM_AXIS )
		{
			if( ent->client->ps.powerups[PW_BLUEFLAG] )
			{
				return FALSE;
			}
		}
		else if( ent->client->sess.sessionTeam == TEAM_ALLIES )
		{
			if( ent->client->ps.powerups[PW_REDFLAG] )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

//
// Called every once in a while to check if we need to remove or add bots to the game
//

void Bot_CheckMinimumPlayers( void )
{
	int minplayers;
	int humanplayers, botplayers;
	static int checkminimumplayers_time;

	if( level.intermissiontime ) 
		return;

	// give players some time to re-spawn after a map_restart
	if( level.startTime + 10000 > level.time )
		return;

	//only check once each 10 seconds
	if( checkminimumplayers_time > level.time - 10000 )
		return;

	checkminimumplayers_time = level.time;
	minplayers = wb_minPlayers.integer;

	if( minplayers <= 0 )
		return;

	if( minplayers >= g_maxclients.integer / 2 )
	{
		minplayers = (g_maxclients.integer / 2) - 1;
	}

	humanplayers = G_CountHumanPlayers( TEAM_RED );
	botplayers = G_CountBotPlayers( TEAM_RED );

	if( humanplayers + botplayers < minplayers )
	{
		trap_SendConsoleCommand( EXEC_APPEND, "bot_connect red\n" );
	}
	else if( humanplayers + botplayers > minplayers && botplayers )
	{
		trap_SendConsoleCommand( EXEC_APPEND, "bot_remove red\n" );
	}

	humanplayers = G_CountHumanPlayers( TEAM_BLUE );
	botplayers = G_CountBotPlayers( TEAM_BLUE );

	if( humanplayers + botplayers < minplayers )
	{
		trap_SendConsoleCommand( EXEC_APPEND, "bot_connect blue\n" );
	}
	else if( humanplayers + botplayers > minplayers && botplayers )
	{
		trap_SendConsoleCommand( EXEC_APPEND, "bot_remove blue\n" );
	}
}

//
// Called every server frame
//

void Bot_StartFrame( int time )
{
	int i;

	// check for waypoint flag updates
	WaypointCheckWolfObjectives();
	WaypointUpdateBombWaypoints();

	// check for min players stuff
	Bot_CheckMinimumPlayers();

	for( i = 0; i < MAX_CLIENTS; i++ )
	{
		if( g_entities[i].botClient && g_entities[i].inuse )
		{
			Bot_Think( &g_entities[i], time );

			// must be called to by-pass engine timeout
//			while( trap_BotGetServerCommand( i, buf, sizeof(buf)) ) 
//			{
//				G_Printf("Receiving : %s\n", buf);
//			}
		}
	}
}

//
// Called every frame from Bot_Think to receive Server Commands.
// This checks for Radio Commands and Chat Phrases and sets corresponding bot variables.
// trap_BotGetServerCommand MUST be called to by-pass engine timeout.
//

void Bot_GetServerCommands( botClient_t *pBot )
{
	char			buf[4096];
	char			*p;
	char			*token;
	int				iClientNum;
	radioCommand_t	*r;
	
	// radio messages format : vchat bVoice iClientNum, iColor, sId, vOrigin

	while( trap_BotGetServerCommand( pBot->clientNum, buf, sizeof(buf)) )
	{
		// parse the command
		p = buf;
		token = COM_Parse( &p );

		// receiving a radio message
		if( !(Q_stricmp( token, "vtchat" )) || !(Q_stricmp( token, "vchat" ))  )
		{
			// skip voiceOnly variable
			token = COM_Parse( &p );

			// keep track of who triggered the radio command
			token = COM_Parse( &p );
			iClientNum = atoi(token);

			// don't react on own radio commands
			if( iClientNum == pBot->clientNum )
				continue;

			// skip the color
			token = COM_Parse( &p );

			// extract the radio id string
			token = COM_Parse( &p );

			for( r = radioCommands; r->sId; r++ )
			{
				if( !strcmp( r->sId, token ) )
				{
					pBot->iRadioOrder	= r->iCommand;
					pBot->pRadioEmitter	= &g_entities[ iClientNum ];
				}
			}
		}
	}
}

//
// Checks out the latest snapshot for sound events, death events etc.
//

void Bot_CheckSnapshot( gentity_t *ent )
{
	entityState_t	state;
	int				entityNum;
	int				event;
	vec3_t			vDistance;
	float			fDistance;
	int				iKiller, iVictim;
	int				iSoundIndex;
	int				i;
	int				iMatches[64];
	int				iFound = 0;
	WAYPOINT		*pWaypoint;
	botClient_t		*pBot;

	pBot = ent->botClient;

	entityNum = 0;

	while( (entityNum = Bot_RetrieveSnapshotEntity( ent->s.number, entityNum, &state ) ) != -1 )
	{
		// so we don't get them more than once
		if( pBot->iEntityEventTime[state.number] == g_entities[state.number].eventTime )
			continue;

		pBot->iEntityEventTime[state.number] = g_entities[state.number].eventTime;

		if( state.eType > ET_EVENTS )
		{
			event = (state.eType - ET_EVENTS) & ~EV_EVENT_BITS;
		}
		else
		{
			event = state.event & ~EV_EVENT_BITS;
		}

		switch(event)
		{
			// make bot go into "suspicious mode" if hearing enemy footsteps
			case EV_FOOTSTEP:
			case EV_FOOTSTEP_METAL:
			case EV_FOOTSTEP_WOOD:
			case EV_FOOTSTEP_GRASS:
			case EV_FOOTSTEP_GRAVEL:
			case EV_FOOTSTEP_ROOF:
			case EV_FOOTSTEP_SNOW:
			case EV_FOOTSTEP_CARPET:
			case EV_FOOTSPLASH:
			case EV_FOOTWADE:

				// only care for enemies
				if( !OnSameTeam( ent, &g_entities[state.number] ) )
				{
					// still need to check for distance
					VectorSubtract( ent->r.currentOrigin, g_entities[state.number].r.currentOrigin, vDistance );
					fDistance = VectorLength(vDistance);

					if( fDistance < FOOTSTEP_HEARING_DISTANCE )
					{
						if( g_debugBots.integer )
							G_Printf("Bot (%i) : Hearing Footsteps from %i\n", ent->client->ps.clientNum, state.number);

						pBot->iLastNoiseTime = level.time;
						VectorCopy( g_entities[state.number].r.currentOrigin, pBot->vNoiseOrigin );
					}
				}
					
				break;

			// say sorry or oops if killed teammate by accident
			case EV_OBITUARY:

				iKiller = state.otherEntityNum2;
				iVictim = state.otherEntityNum;

				if( iKiller == ent->s.number && iKiller != iVictim )
				{
					if( OnSameTeam( ent, &g_entities[iVictim] ) )
					{
						if( (rand() % 100) > 50 )
						{
							Bot_SendDelayedRadioCommand( ent, RADIO_SORRY, TRUE );
						}
						else
						{
							Bot_SendDelayedRadioCommand( ent, RADIO_OOPS, TRUE );
						}
					}
				}

				break;

			// check for some important announcements
			case EV_GLOBAL_SOUND:

				iSoundIndex = state.eventParm;

				// Allies have planted the dynamite
				if( iSoundIndex == G_SoundIndex( "sound/multiplayer/allies/a-dynamite_planted.wav" ) )
				{
					if( g_debugBots.integer )
					{
						G_Printf("Allies have planted the dynamite\n");
					}

					// roaming Axis engineers go look for the dynamite !
					if( ent->client->sess.sessionTeam == TEAM_AXIS )
					{
						if( pBot->playerClass == PC_ENGINEER )
						{
							if( Bot_GetTask(pBot)->iTask == TASK_ROAM )
							{
								if( (rand() % 100) > 40 )
								{
									if( pBot->iGoalWaypoint != -1 )
									{
										// don't bother if already heading for a bomb waypoint
										if( (Waypoints[pBot->iGoalWaypoint].flags & FL_WP_OBJECTIVE) && (Waypoints[pBot->iGoalWaypoint].objdata.type == OBJECTIVE_BOMB) )
										{
											continue;
										}

										iFound = 0;

										// find random enemy bomb spot
										for( i = 0; i < numObjWaypoints; i++ )
										{
											pWaypoint = &Waypoints[ ObjWaypoints[ i ] ];

											// only enemy spots are important
											if( pWaypoint->objdata.team == ent->client->sess.sessionTeam )
												continue;

											// only bomb objectives are important
											if( pWaypoint->objdata.type != OBJECTIVE_BOMB )
												continue;

											// already blown up
											if( pWaypoint->objdata.state == STATE_ACCOMPLISHED )
												continue;

											// unreachable ?
											if( pWaypoint->flags & FL_WP_AXIS_UNREACHABLE )
												continue;

											iMatches[ iFound ] = pWaypoint->index;
											iFound++;
										}

										// if there are ANY reachable enemy bomb spots go for one
										// this is not always the case (mp_beach)
										if( iFound > 0 )
										{
											bottask_t botTask;

											Bot_RemoveTasks(pBot);

											botTask.iExpireTime = -1;
											botTask.iIndex = iMatches[ rand() % iFound ];
											botTask.iTask = TASK_ROAM;
											botTask.pNextTask = NULL;
											botTask.pPrevTask = NULL;

											Bot_PushTask( pBot, &botTask );
											Bot_FreeSolutionNodes(pBot);

											pBot->iGoalWaypoint = botTask.iIndex;

											// issue chat message
											if( g_debugBots.integer )
											{
												G_Printf("Bot (%i) : Going to look for the allied dynamite at %i\n", ent->client->ps.clientNum, pBot->iGoalWaypoint );
											}
										}
									}
								}
							}
						}
					}
				}
				else if( iSoundIndex == G_SoundIndex( "sound/multiplayer/axis/g-dynamite_planted.wav" ) )
				{
					// Axis have planted the dynamite
					if( g_debugBots.integer )
					{
						G_Printf("Axis have planted the dynamite\n");
					}

					// roaming Allies engineers go look for the dynamite !
					if( ent->client->sess.sessionTeam == TEAM_ALLIES )
					{
						if( pBot->playerClass == PC_ENGINEER )
						{
							if( Bot_GetTask(pBot)->iTask == TASK_ROAM )
							{
								if( (rand() % 100) > 40 )
								{
									if( pBot->iGoalWaypoint != -1 )
									{
										// don't bother if already heading for a bomb waypoint
										if( (Waypoints[pBot->iGoalWaypoint].flags & FL_WP_OBJECTIVE) && (Waypoints[pBot->iGoalWaypoint].objdata.type == OBJECTIVE_BOMB) )
										{
											continue;
										}

										iFound = 0;

										// find random enemy bomb spot
										for( i = 0; i < numObjWaypoints; i++ )
										{
											pWaypoint = &Waypoints[ ObjWaypoints[ i ] ];

											// only enemy spots are important
											if( pWaypoint->objdata.team == ent->client->sess.sessionTeam )
												continue;

											// only bomb objectives are important
											if( pWaypoint->objdata.type != OBJECTIVE_BOMB )
												continue;

											// already blown up
											if( pWaypoint->objdata.state == STATE_ACCOMPLISHED )
												continue;

											// unreachable ?
											if( pWaypoint->flags & FL_WP_ALLIES_UNREACHABLE )
												continue;

											iMatches[ iFound ] = pWaypoint->index;
											iFound++;
										}

										// if there are ANY reachable enemy bomb spots go for one
										// this is not always the case (mp_beach)
										if( iFound > 0 )
										{
											bottask_t botTask;

											Bot_RemoveTasks(pBot);

											botTask.iExpireTime = -1;
											botTask.iIndex = iMatches[ rand() % iFound ];
											botTask.iTask = TASK_ROAM;
											botTask.pNextTask = NULL;
											botTask.pPrevTask = NULL;

											Bot_PushTask( pBot, &botTask );
											Bot_FreeSolutionNodes(pBot);

											pBot->iGoalWaypoint = botTask.iIndex;

											// issue chat message
											if( g_debugBots.integer )
											{
												G_Printf("Bot (%i) : Going to look for the allied dynamite at %i\n", ent->client->ps.clientNum, pBot->iGoalWaypoint );
											}
										}
									}
								}
							}
						}
					}
				}
				else if( iSoundIndex == G_SoundIndex( "sound/multiplayer/allies/a-objective_taken.wav" ) )
				{
					// Allies have stolen the axis' documents
					if( g_debugBots.integer )
					{
						G_Printf("Allies have stolen the axis' documents\n");
					}

					// Set off some Axis bots to camp at allied deliver spots
					if( ent->client->sess.sessionTeam == TEAM_AXIS )
					{
						if( Bot_GetTask(pBot)->iTask == TASK_ROAM )
						{
							if( (rand() % 100) > 55 )
							{
								// camp at enemy deliver spot about half of the time
								// or move to the enemy steal objective waypoint
								if( (rand() % 100) > 50 )
								{
									// find a random enemy deliver waypoint
									iFound = WaypointFindRandomGoal( ent, FL_WP_ALLIES_DOCS_DELIVER );

									// can reach an enemy deliver spot
									if( iFound != -1 )
									{
										bottask_t botTask;

										if( g_debugBots.integer )
										{
											G_Printf("Bot (%i) : Going to camp at allied deliver spot %i\n", ent->client->ps.clientNum, iFound );
										}

										Bot_RemoveTasks(pBot);

										botTask.iExpireTime = -1;
										botTask.iIndex = iFound;
										botTask.iTask = TASK_ROAM;
										botTask.pNextTask = NULL;
										botTask.pPrevTask = NULL;

										Bot_PushTask( pBot, &botTask );
										Bot_FreeSolutionNodes(pBot);

										pBot->iGoalWaypoint = iFound;
									}
								}
								else
								{
									pWaypoint = NULL;

									// head to the enemy steal waypoint
									for( i = 0; i < numObjWaypoints; i++ )
									{
										pWaypoint = &Waypoints[ ObjWaypoints[ i ] ];

										if( pWaypoint->objdata.team == ent->client->sess.sessionTeam )
											continue;

										if( pWaypoint->objdata.type == OBJECTIVE_STEAL )
											break;
									}

									if( pWaypoint != NULL )
									{
										bottask_t botTask;

										if( g_debugBots.integer )
										{
											G_Printf("Bot (%i) : Heading to allied steal objective %i\n", ent->client->ps.clientNum, pWaypoint->index);
										}

										Bot_RemoveTasks(pBot);
										
										botTask.iExpireTime = -1;
										botTask.iIndex = pWaypoint->index;
										botTask.iTask = TASK_ROAM;
										botTask.pNextTask = NULL;
										botTask.pPrevTask = NULL;

										Bot_PushTask( pBot, &botTask );
										Bot_FreeSolutionNodes(pBot);

										pBot->iGoalWaypoint = pWaypoint->index;
									}
									
								}
							}
						}
					}
				}
				else if( iSoundIndex == G_SoundIndex( "sound/multiplayer/axis/g-objective_taken.wav" ) )
				{
					// Axis have stolen the allied documents
					if( g_debugBots.integer )
					{
						G_Printf("Axis have stolen the allied documents\n");
					}

					// Set off some Allies bots to camp at axis deliver spots
					if( ent->client->sess.sessionTeam == TEAM_ALLIES )
					{
						if( Bot_GetTask(pBot)->iTask == TASK_ROAM )
						{
							if( (rand() % 100) > 55 )
							{
								// camp at enemy deliver spot about half of the time
								// or move to the enemy steal objective waypoint
								if( (rand() % 100) > 50 )
								{
									// find a random enemy deliver waypoint
									iFound = WaypointFindRandomGoal( ent, FL_WP_AXIS_DOCS_DELIVER );

									// can reach an enemy deliver spot
									if( iFound != -1 )
									{
										bottask_t botTask;

										if( g_debugBots.integer )
										{
											G_Printf("Bot (%i) : Going to camp at allied deliver spot %i\n", ent->client->ps.clientNum, iFound );
										}

										Bot_RemoveTasks(pBot);

										botTask.iExpireTime = -1;
										botTask.iIndex = iFound;
										botTask.iTask = TASK_ROAM;
										botTask.pNextTask = NULL;
										botTask.pPrevTask = NULL;

										Bot_PushTask( pBot, &botTask );
										Bot_FreeSolutionNodes(pBot);

										pBot->iGoalWaypoint = iFound;
									}
								}
								else
								{
									pWaypoint = NULL;

									// head to the enemy steal waypoint
									for( i = 0; i < numObjWaypoints; i++ )
									{
										pWaypoint = &Waypoints[ ObjWaypoints[ i ] ];

										if( pWaypoint->objdata.team == ent->client->sess.sessionTeam )
											continue;

										if( pWaypoint->objdata.type == OBJECTIVE_STEAL )
											break;
									}

									if( pWaypoint != NULL )
									{
										bottask_t botTask;

										if( g_debugBots.integer )
										{
											G_Printf("Bot (%i) : Heading to axis steal objective %i\n", ent->client->ps.clientNum, pWaypoint->index);
										}

										Bot_RemoveTasks(pBot);
										
										botTask.iExpireTime = -1;
										botTask.iIndex = pWaypoint->index;
										botTask.iTask = TASK_ROAM;
										botTask.pNextTask = NULL;
										botTask.pPrevTask = NULL;

										Bot_PushTask( pBot, &botTask );
										Bot_FreeSolutionNodes(pBot);

										pBot->iGoalWaypoint = pWaypoint->index;
									}
									
								}
							}
						}
					}
				}

				break;
		}
	}
}

//
// Retrieves an entity from a snapshot
//

int Bot_RetrieveSnapshotEntity( int clientNum, int sequence, entityState_t *state )
{
	int entNum;
	gentity_t *pEnt;

	entNum = trap_BotGetSnapshotEntity( clientNum, sequence );

	if( entNum == -1 )
	{
		memset(state, 0, sizeof(entityState_t));
		return -1;
	}

	pEnt = &g_entities[entNum];
	memset(state, 0, sizeof(entityState_t));

	if( pEnt->inuse )
	{
		if( pEnt->r.linked )
		{
			if( !(pEnt->r.svFlags & SVF_NOCLIENT) )
			{
				memcpy( state, &pEnt->s, sizeof(entityState_t) );
			}
		}
	}

	return sequence + 1;
}

//
// Logging stuff
//

void Bot_LogInitSession( void )
{
	trap_FS_FOpenFile( "bot_log.txt", &f, FS_APPEND );

	if( !f )
	{
		G_Printf("Bot_LogInitSession : failed to open bot_log.txt\n");
		return;
	}

	trap_FS_Write( "[Session started]\r\n", strlen( "[Session started]\r\n" ), f );
	
}

void Bot_Log( const char *fmt, ... )
{
	va_list		argptr;
	char		string[1024];
	int			min, tens, sec;

	if( !f )
	{
		if( g_debugBots.integer )
		{
			G_Printf("Bot_Log : invalid handle\n");
		}

		return;
	}

	sec = level.time / 1000;

	min = sec / 60;
	sec -= min * 60;
	tens = sec / 10;
	sec -= tens * 10;

	Com_sprintf( string, sizeof(string), "%3i:%i%i ", min, tens, sec );

	va_start( argptr, fmt );
	Q_vsnprintf( string+7, sizeof(string)-7, fmt, argptr );
	va_end( argptr );

	trap_FS_Write( string, strlen( string ), f );
	trap_FS_Write( "\r\n", strlen("\r\n"), f );
}

void Bot_LogEndSession( void )
{
	char szString[256];

	if( !f )
		return;

	// memory leaks?
	if( g_debugBots.integer )
	{
		Com_sprintf( szString, sizeof(szString), "Bytes of allocated memory : %i\r\n", mem_amount );

		trap_FS_Write( szString, strlen( szString ), f );
	}

	trap_FS_Write( "[Session closed]\r\n\r\n", strlen( "[Session closed]\r\n\r\n" ), f );

	trap_FS_FCloseFile(f);
}
