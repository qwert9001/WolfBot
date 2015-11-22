// Copyright (C) 2003 Torben Könke. Licensed under GPL, see LICENSE.
//
// g_botwaypoint.c -- Waypoint and Pathfinding routines 

#include "g_local.h"

WAYPOINT		Waypoints[MAX_NUM_WAYPOINTS];
int				numWaypoints = 0;

int				ObjWaypoints[32];		// holds all Objective Waypoint Indices
int				numObjWaypoints;

int				numStealObjectives		= 0;
int				numCaptureObjectives	= 0;
int				numBombObjectives		= 0;

int				ObjectiveStatus[MAX_OBJECTIVES];
int				funcExplosives[128];

BOOL			bWaypointsLoaded		= FALSE;

// WolfBot 1.5
int				CampWaypoints[128];
int				numCampWaypoints;
int				DeliverWaypoints[128];
int				numDeliverWaypoints;
int				numMG42Waypoints;

//
// Reads in the Waypoint File
//

void WaypointLoadFile( void )
{
	char			map[MAX_QPATH];
	fileHandle_t	hFile;
	WPSHEADER		Header;
	int				i, c;

	trap_Cvar_VariableStringBuffer( "mapname", map, sizeof(map) );

	trap_FS_FOpenFile( va("maps/%s.wps", map) , &hFile, FS_READ );

	if( !hFile )
	{
		G_Printf("Couldn't find WPS File for map %s\n", map);
		return;
	}

	trap_FS_Read( &Header, sizeof(Header), hFile );

	if( strcmp( Header.szMagic, WPS_MAGIC ) )
	{
		G_Printf("This is not a WolfBot Waypoint file\n");
		return;
	}

	if( Header.nVersion != WPS_FILE_VERSION )
	{
		G_Printf("Wrong WPS File Version %i (Should be %i)\n", Header.nVersion, WPS_FILE_VERSION );
		return;
	}

	if( Header.nWaypoints > MAX_NUM_WAYPOINTS )
	{
		G_Printf("The file contains too many Waypoints. Maximum number of Waypoints is %i\n", MAX_NUM_WAYPOINTS);
		return;
	}

	if( strcmp( Header.szMapname, map ) )
	{
		G_Printf("WPS Mapname mismatch (%s) (%s)\n", Header.szMapname, map);
		return;
	}

	// Waypoints einlesen
	for( i = 0; i < Header.nWaypoints; i++ )
	{
		trap_FS_Read( &Waypoints[i], sizeof(WAYPOINT), hFile );

		Waypoints[i].paths = (WPPATH*)malloc( sizeof(WPPATH) * Waypoints[i].numPaths );

		mem_amount += (sizeof(WPPATH) * Waypoints[i].numPaths);

		// Paths für diesen Waypoint einlesen
		for( c = 0; c < Waypoints[i].numPaths; c++ )
		{
			trap_FS_Read( &Waypoints[i].paths[c], sizeof(WPPATH), hFile );
		}

	}

	numWaypoints	= Header.nWaypoints;

	trap_FS_FCloseFile(hFile);

	bWaypointsLoaded = TRUE;
	G_Printf("Successfully read Waypoint file for %s\n", map);
}

//
// Unloads Waypoint File and also frees up all allocated memory from bots
//

void WaypointCleanUp( void )
{
	int i;

	// memory aufräumen
	for( i = 0; i < numWaypoints; i++ )
	{
		if( Waypoints[i].numPaths > 0 )
		{
			if( g_debugBots.integer )
			{
				G_Printf("Waypoint (%i) has %i paths\n", Waypoints[i].index, Waypoints[i].numPaths );
			}

			free(Waypoints[i].paths);
			mem_amount -= (sizeof(WPPATH) * Waypoints[i].numPaths);
		}

		Waypoints[i].index			= -1;
		Waypoints[i].numPaths		= 0;
	}

	numWaypoints	= 0;

	// free memory allocated by bot clients
	for( i = 0; i < level.numConnectedClients; i++ )
	{
		if( g_entities[i].botClient )
		{
			Bot_Initialize(g_entities[i].botClient);		// this does what we want
			Bot_Log( "WaypointCleanUp calling Bot_Initialize for client %i", i );

			// shut down AI
			// WolfBot 1.5 : should be g_entities[i] NOT g_entities[1]
			g_entities[i].botClient->bShutdown = TRUE;
		}
	}

	bWaypointsLoaded = FALSE;

	if( g_debugBots.integer )
	{
		G_Printf("WaypointCleanUp -> OK\n");
	}
}

//
// This initializes the ObjectiveStatus[] Array. Also initializes an Array containing
// all func_explosives that have a targetname set and could possibly trigger objectives.
//

void WaypointInit( void )
{
	char	cs[MAX_STRING_CHARS];
	int		iNumObjectives;
	int		iNumFuncs;
	int		i;

	trap_GetConfigstring( CS_MULTI_INFO, cs, sizeof(cs) );

	iNumObjectives = atoi( Info_ValueForKey( cs, "numobjectives" ) );

	for( i = 0; i < iNumObjectives; i++ )
	{
		trap_GetConfigstring( CS_MULTI_OBJ1_STATUS + i , cs, sizeof(cs) );
		ObjectiveStatus[i] = atoi( Info_ValueForKey( cs, "status" ) );
	}

	iNumFuncs = 0;

	// now initialize funcExplosives[] Array.
	for( i = MAX_CLIENTS; i < level.num_entities; i++ )
	{
		if( g_entities[i].s.eType == ET_EXPLOSIVE )
		{
			funcExplosives[iNumFuncs] = g_entities[i].s.number;
			iNumFuncs++;

			if( g_debugBots.integer )
			{
				G_Printf("Added entity %i to funcExplosives Array\n", g_entities[i].s.number);
			} 
		}
	}

	// count all objective waypoints in an array for faster referencing
	for( i = 0; i < numWaypoints; i++ )
	{
		if( Waypoints[i].flags & FL_WP_OBJECTIVE )
		{
			ObjWaypoints[ numObjWaypoints ] = Waypoints[i].index;

			if( g_debugBots.integer )
			{
				G_Printf("Added Waypoint %i to ObjWaypoints Array\n", Waypoints[i].index);
			}

			numObjWaypoints++;

			if( Waypoints[i].objdata.type == OBJECTIVE_STEAL )
			{
				numStealObjectives++;
			}
			else if( Waypoints[i].objdata.type == OBJECTIVE_CAPTURE )
			{
				numCaptureObjectives++;
			}
			else if( Waypoints[i].objdata.type == OBJECTIVE_BOMB )
			{
				numBombObjectives++;
			}
		}

		if( Waypoints[i].flags & FL_WP_CAMP )
		{
			CampWaypoints[ numCampWaypoints ] = Waypoints[i].index;

			numCampWaypoints++;
		}

		if( (Waypoints[i].flags & FL_WP_AXIS_DOCS_DELIVER) || (Waypoints[i].flags & FL_WP_ALLIES_DOCS_DELIVER) )
		{
			DeliverWaypoints[ numDeliverWaypoints ] = Waypoints[i].index;
			
			numDeliverWaypoints++;
		}

		if( Waypoints[i].flags & FL_WP_MG42 )
		{
			numMG42Waypoints++;
		}
	}
}

//
// Called once a frame to check if we need to clear either FL_WP_AXIS_UNREACHABLE or
// FL_WP_ALLIES_UNREACHABLE flags for Waypoints
// This is done by saving the Status of each Objective on Round Start and check
// for changes here.
//
// This updates Waypoint reachability!
//
// Also changes state of Steal and Capture Objective Waypoints.
//

void WaypointCheckWolfObjectives( void )
{
	int		i;
	int		y;
	int		iNumObjectives;
	int		iStatus;
	int		iWpObj;
	int		iObjType;
	int		iObjState;
	char	cs[MAX_STRING_CHARS];

	trap_GetConfigstring( CS_MULTI_INFO, cs, sizeof(cs) );

	iNumObjectives = atoi( Info_ValueForKey( cs, "numobjectives" ) );

	for( i = 0; i < iNumObjectives; i++ )
	{
		trap_GetConfigstring( CS_MULTI_OBJ1_STATUS + i, cs, sizeof(cs) );
		iStatus = atoi( Info_ValueForKey( cs, "status" ) );

		// an objective has changed status i.e. is completed
		if( ObjectiveStatus[i] != iStatus )
		{
			ObjectiveStatus[i] = iStatus;

			for( y = 0; y < numWaypoints; y++ )
			{
				if( Waypoints[y].flags & FL_WP_OBJECTIVE )
				{
					iObjType = Waypoints[y].objdata.type;

					// check for steal objective...
					if( iObjType == OBJECTIVE_STEAL )
					{
						iWpObj = Waypoints[y].objdata.objNum;

						// check if Objective Waypoint is "linked" to this Wolf Map Objective
						if( iWpObj == (i + 1) )
						{
							iObjState = Waypoints[y].objdata.state;

							// object is being stolen
							if( iObjState == STATE_UNACCOMPLISHED )
							{
								if( g_debugBots.integer )
								{
									G_Printf("Documents are being stolen. Objective Accomplished\n");
								}

								Waypoints[y].objdata.state = STATE_ACCOMPLISHED;
							}
							// object is being returned
							else if( iObjState == STATE_ACCOMPLISHED )
							{
								if( g_debugBots.integer )
								{
									G_Printf("Documents are being returned. Objective Unaccomplished\n");
								}

								Waypoints[y].objdata.state = STATE_UNACCOMPLISHED;
							}
							
						}
					}
					// check for capture objective...
					else if( iObjType == OBJECTIVE_CAPTURE )
					{
						iWpObj = Waypoints[y].objdata.objNum;

						if( iWpObj == (i + 1) )
						{
							iObjState = Waypoints[y].objdata.state;

							// WolfBot 1.5 If captured just flip the objective team.
							// This way the other will automatically head for this
							// objective.
							if( Waypoints[y].objdata.team == TEAM_AXIS )
							{
								Waypoints[y].objdata.team = TEAM_ALLIES;

								if( g_debugBots.integer )
								{
									G_Printf("Axis capture Waypoint %i\n", y );
								}
							}
							else if( Waypoints[y].objdata.team == TEAM_ALLIES )
							{
								Waypoints[y].objdata.team = TEAM_AXIS;

								if( g_debugBots.integer )
								{
									G_Printf("Allies capture Waypoint %i\n", y );
								}
							}
						}
					}
				}
			}
		}
	}

}

//
// Called once a frame to check if a Bomb Waypoint should be marked as accomplished.
// This is done by going through all func_explosives (previously init in WaypointInit).
// We must do it this way since 1 Map Objective can have several Bomb spot Waypoints
// Sea Wall and Sea Door in mp_beach for example, so we couldn't just check if the 
// wolf objective was accomplished since we wouldn't know which spot was breached.
//
// This also checks blocked Paths
//

void WaypointUpdateBombWaypoints( void )
{
	int i;
	int c;
	int d;
	gentity_t *pEnt;
	WAYPOINT *pWaypoint;
	trace_t tr;

	for( i = 0; i < 128; i++ )
	{
		// no more entries
		if( funcExplosives[i] == 0 )
			break;

		if( funcExplosives[i] == -1 )
			continue;

		pEnt = &g_entities[ funcExplosives[i] ];

		// if about to explode , think points to BecomeExplosion()
		if( pEnt->think == BecomeExplosion )
		{
			vec3_t size;
			gentity_t *bbox;

			bbox = G_Spawn();

			// setup temporary bbox entity
			bbox->classname = "BBOX";

			VectorCopy( pEnt->r.absmin, bbox->r.absmin );
			VectorCopy( pEnt->r.absmax, bbox->r.absmax );

			VectorSubtract( pEnt->r.absmax, pEnt->r.absmin, size );
			VectorScale( size, 0.5, size );
			VectorAdd( pEnt->r.absmin, size, bbox->s.origin );

			VectorSubtract( pEnt->r.absmax, bbox->s.origin, bbox->r.maxs );
			VectorSubtract( pEnt->r.absmin, bbox->s.origin, bbox->r.mins );

			VectorCopy( bbox->s.origin, bbox->r.currentOrigin );
			VectorCopy( bbox->s.origin, bbox->s.pos.trBase );

			bbox->clipmask = CONTENTS_SOLID;
			bbox->r.contents = CONTENTS_SOLID;

			trap_LinkEntity(bbox);

			// outline the bbox
			if( g_debugBots.integer )
			{
				gentity_t *tent = G_TempEntity( bbox->r.absmin, EV_RAILTRAIL );

				VectorCopy( bbox->r.absmax, tent->s.origin2 );
				tent->s.dmgFlags = 1;
			}

			// now check out blocked paths...
			for( c = 0; c < numWaypoints; c++ )
			{
				for( d = 0; d < Waypoints[c].numPaths; d++ )
				{
					if( Waypoints[c].paths[d].flags & FL_PATH_BLOCKED )
					{
						trap_Trace( &tr, Waypoints[c].origin, NULL, NULL, Waypoints[ Waypoints[c].paths[d].indexTarget ].origin, ENTITYNUM_NONE, MASK_SOLID );

						if( tr.entityNum == bbox->s.number )
						{
							// this entity was the blocker for this path , path is now free to use for bots
							if( g_debugBots.integer )
							{
								G_Printf("Path between %i and %i was blocked by the exploding entity\n", Waypoints[c].index, Waypoints[ Waypoints[c].paths[d].indexTarget ].index );
							}

							Bot_Log("Path between Waypoint %i and %i is no longer blocked!", Waypoints[c].index, Waypoints[c].paths[d].indexTarget);

							Waypoints[c].paths[d].flags &= ~FL_PATH_BLOCKED;
						}
					}
				}
			}

			G_FreeEntity(bbox);

			// now go through all bomb objective waypoints
			for( c = 0; c < numObjWaypoints; c++ )
			{
				int iObjType;
				int iObjState;
				int iObjTarget;

				pWaypoint = &Waypoints[ ObjWaypoints[c] ];

				iObjType = pWaypoint->objdata.type;
				iObjState = pWaypoint->objdata.state;
				iObjTarget = pWaypoint->objdata.entNum;

				if( iObjType == OBJECTIVE_BOMB && iObjState != STATE_ACCOMPLISHED )
				{
					// Objective Waypoint is linked to this func_explosive entity
					if( iObjTarget == pEnt->s.number )
					{
						Bot_Log("Bomb Waypoint %i is linked to entity %i", pWaypoint->index, pEnt->s.number );
						Bot_Log("Bomb Objective accomplished");

						pWaypoint->objdata.state = STATE_ACCOMPLISHED;

						// now go through all waypoints and check for reachability updates
						for( d = 0; d < numWaypoints; d++ )
						{
							if( (Waypoints[d].flags & FL_WP_AXIS_UNREACHABLE) || (Waypoints[d].flags & FL_WP_ALLIES_UNREACHABLE) )
							{
								if( Waypoints[d].wpdata.entNum1 == pEnt->s.number ||
									Waypoints[d].wpdata.entNum2 == pEnt->s.number ||
									Waypoints[d].wpdata.entNum3 == pEnt->s.number ||
									Waypoints[d].wpdata.entNum4 == pEnt->s.number )
								{
									// this waypoint is now reachable
									Waypoints[d].flags &= ~FL_WP_AXIS_UNREACHABLE;
									Waypoints[d].flags &= ~FL_WP_ALLIES_UNREACHABLE;

									if( g_debugBots.integer )
									{
										G_Printf("Waypoint %i is now reachable because func_explosive %i was destroyed!\n", d, pEnt->s.number );
									}
								}
							}
						}
					}

				}
			}

			// "delete" entity from array
			funcExplosives[i] = -1;
		}
	}
}


//
// Pathfinding
//

PATHNODE *AStarSearch( int iIndexStart, int iIndexGoal )
{
	PATHNODE *OpenList		= NULL;
	PATHNODE *ClosedList	= NULL;
	PATHNODE *Current;
	PATHNODE *ChildList;
	PATHNODE *CurChild;
	PATHNODE *Path;
	PATHNODE *p, *q;
	PATHNODE *Start;

	Start = (PATHNODE*) malloc( sizeof(PATHNODE) );

	mem_amount += sizeof(PATHNODE);

	Start->wpIndex	= iIndexStart;
	Start->parent	= NULL;
	Start->NextNode = NULL;
	Start->prev		= NULL;
	Start->g		= 0;
	Start->h		= 0;
	Start->f		= 0;

	OpenList = Start;

	while( OpenList != NULL )
	{
		Current = OpenList;
		OpenList = (PATHNODE*) OpenList->NextNode;

		if(OpenList != NULL)
			OpenList->prev = NULL;

		// Pfad gefunden
		if( IsGoalNode( Current, iIndexGoal ) )
		{
			Current->NextNode = NULL;

			Path = Current;
			p = (PATHNODE*) Current->parent;

			while( p != NULL )
			{
				if( p->prev != NULL )
					((PATHNODE*) p->prev)->NextNode = p->NextNode;
				if( p->NextNode != NULL )
					((PATHNODE*) p->NextNode)->prev = p->prev;

				if( p == ClosedList )
					ClosedList = (PATHNODE*) p->NextNode;

				p->NextNode = Path;
				Path = p;

				p = (PATHNODE*) p->parent;
			}

			// Openlist aufräumen
			while( OpenList != NULL )
			{
				p = (PATHNODE*) OpenList->NextNode;
				free(OpenList);
				mem_amount -= sizeof(PATHNODE);
				OpenList = p;
			}

			// ClosedList aufräumen
			while( ClosedList != NULL )
			{
				p = (PATHNODE*) ClosedList->NextNode;
				free(ClosedList);
				mem_amount -= sizeof(PATHNODE);
				ClosedList = p;
			}

			return Path;
		}

		ChildList = MakeChildren(Current);

		while( ChildList != NULL )
		{
			CurChild = ChildList;
			ChildList = (PATHNODE*) ChildList->NextNode;

			CurChild->parent	= Current;
			CurChild->NextNode	= NULL;
			CurChild->prev		= NULL;

			// g is calculated in MakeChildren
		//	CurChild->g			= Current->g + 1;
			CurChild->h			= GoalDistanceEstimate( CurChild, iIndexGoal );
			CurChild->f			= CurChild->g + CurChild->h;

			if( ClosedList != NULL )
			{
				p = ClosedList;

				while( p != NULL )
				{
					if( SameState( p, CurChild) )
					{
						if( p->f <= CurChild->f )
						{
							// better node is already on closedlist , delete CurChild
							free(CurChild);
							mem_amount -= sizeof(PATHNODE);
							CurChild = NULL;
							break;
						}
						else
						{
							// CurChild is better than node on ClosedList so delete old node
							if( p->prev != NULL )
								((PATHNODE*) p->prev)->NextNode = p->NextNode;

							if( p->NextNode != NULL )
								((PATHNODE*) p->NextNode)->prev = p->prev;

							if ( p == ClosedList )
								ClosedList = (PATHNODE*)p->NextNode;

							free(p);
							mem_amount -= sizeof(PATHNODE);
							break;
						}
					}

					p = (PATHNODE*) p->NextNode;
				}
			}

			// is CurChild already on the OpenList ?
			if ( CurChild != NULL )
			{
				p = OpenList;

				while ( p != NULL ) 
				{
					if( SameState( p, CurChild ) )
					{						
						if ( p->f <= CurChild->f )
						{
							// better node is already on the OpenList so delete CurChild							
							free(CurChild);
							mem_amount -= sizeof(PATHNODE);
							CurChild = NULL;
							break;
						}
						else
						{
							// CurChild has shorter path than old node so delete the old node							
							if( p->prev != NULL )
								((PATHNODE*) p->prev)->NextNode = p->NextNode;

							if( p->NextNode != NULL )
								((PATHNODE*) p->NextNode)->prev = p->prev;

							if( p == OpenList )
								OpenList = (PATHNODE*)p->NextNode;

							// bugfix -> count floyd forgot to actually delete the node in podbot
							free(p);
							mem_amount -= sizeof(PATHNODE);

							break;
						}
					}

					p = (PATHNODE*)p->NextNode;
				}
				
				// insert CurChild into the OpenList
				if ( CurChild != NULL )
				{
					p = OpenList;
					q = p;

					while( p != NULL ) 
					{
						if( p->f >= CurChild->f )
						{	
							if( p == OpenList )
								OpenList = CurChild;
							
							CurChild->NextNode = p;
							CurChild->prev = p->prev;
							
							p->prev = CurChild;

							if( CurChild->prev != NULL )
								((PATHNODE*) CurChild->prev)->NextNode = CurChild;

							break;
						}

						q = p;
						p = (PATHNODE*)p->NextNode;
					}
					if( p == NULL )
					{		
						if( q != NULL )
						{
							q->NextNode = CurChild;
							CurChild->prev = q;
						}
						else
							OpenList = CurChild;
					}
				}				       
				
			}	
			
		}
	
		// Current node has been expanded so put it on the ClosedList now
		Current->NextNode = ClosedList;
    
		if ( ClosedList != NULL )
			ClosedList->prev = Current;

		ClosedList		= Current;
		Current->prev	= NULL;
	}
  
	// no nodes on the openlist anymore which means no path could be found
	while ( ClosedList != NULL )
	{
		p = (PATHNODE*) ClosedList->NextNode;
		free(ClosedList);
		mem_amount -= sizeof(PATHNODE);
		ClosedList = p;
	}

	return NULL;
}

BOOL IsGoalNode( PATHNODE *Compare, int iIndexGoal )
{
	if( Compare->wpIndex == iIndexGoal )
		return TRUE;

	return FALSE;
}

PATHNODE *MakeChildren( PATHNODE *Parent )
{
	int i;
	WAYPOINT *wp;
	PATHNODE *p, *q = NULL;

	wp = &Waypoints[Parent->wpIndex];

	for( i = 0; i < wp->numPaths; i++ )
	{
		// ignore blocked paths
		if( wp->paths[i].flags & FL_PATH_BLOCKED )
			continue;

		p = (PATHNODE*) malloc( sizeof(PATHNODE) * 1 );

		mem_amount += sizeof(PATHNODE);
		
		p->wpIndex	= wp->paths[i].indexTarget;
		p->parent	= Parent;
		p->NextNode	= q;

		// g berechnen (Distanz zwischen child node und parent node)
		p->g = Parent->g + wp->paths[i].length;

		q = p;
	}

	return q;
}

float GoalDistanceEstimate( PATHNODE *Current, int iIndexGoal )
{
	vec3_t tmp;

	VectorSubtract( Waypoints[iIndexGoal].origin, Waypoints[Current->wpIndex].origin, tmp );

	return VectorLength(tmp);
}

BOOL SameState( PATHNODE *a, PATHNODE *b )
{
	if( a == NULL && b == NULL )
		return TRUE;
	else if( a == NULL )
		return FALSE;
	else if( b == NULL )
		return FALSE;

	return a->wpIndex == b->wpIndex;
}

WAYPOINT *GetWaypoint( int index )
{
	return &Waypoints[index];
}

//
// Frees all Solution Nodes ; Returns TRUE if any nodes were freed
//

BOOL Bot_FreeSolutionNodes( botClient_t *pBot )
{
	PATHNODE *tmp;
	PATHNODE *next;

	tmp = pBot->pStartNode;

	while( tmp )
	{
		next = tmp->NextNode;
		free(tmp);
		mem_amount -= sizeof(PATHNODE);
		tmp = next;
	}

	pBot->pStartNode = pBot->pWaypointNodes = NULL;

	return TRUE;
}

//
// BOT NAVIGATING FUNCTIONS
//

//
// Maintains Waypoint Movement for Bot ; called whenever bot is not in combat mode
// Returns TRUE if Goal node was reached
//

BOOL Bot_MapNavigation( gentity_t *ent )
{
	int			iIndex;
	int			iGoal;
	int			i;
	int			iLadderDir;
	int			iWaypointReachTime;	// varies
	float		fLength;
	float		fTouchDist;
	float		fMinMove;
	vec3_t		vDistance;
	vec3_t		vDirection;
	vec3_t		vAngles;
	vec3_t		vViewOrigin;
	vec3_t		vBModelOrg, vDiff;
	BOOL		bTouching;
	BOOL		bWaitingForDoorToOpen;
	BOOL		bDuck;
	bottask_t	botTask;
	trace_t		tr;
	gentity_t	*pHit;
	botClient_t *pBot = ent->botClient;

	iWaypointReachTime = 5000;

	// some extra time if its a walk or crouch path or bot is carrying a 'slow down' weapon
	if( (pBot->iPathFlags & FL_PATH_WALK) || (pBot->iPathFlags & FL_PATH_DUCK) || (Bot_HasHeavyWeapon(ent) == TRUE) )
	{
		iWaypointReachTime += 2500;
	}

	if( pBot->iPathFlags & FL_PATH_LADDER )
	{
		iWaypointReachTime += 5000; // 10 seconds timeout because ladder paths can be LONG
									// I will change this once I have written better ladder
									// handling code...
	}

	// bot couldn't reach waypoint within time
	if( level.time > (pBot->iCurWaypointTime + iWaypointReachTime) )
	{
		Bot_FreeSolutionNodes(pBot);

		if( g_debugBots.integer )
		{
			G_Printf("Bot (%i) : Could not reach Waypoint %i.\n", ent->client->ps.clientNum, pBot->iCurWaypoint);
		}

		pBot->iCurWaypoint = -1;
		pBot->iGoalWaypoint = -1;

		// delete all tasks ; why ?
		Bot_RemoveTasks(pBot);
	}

	bWaitingForDoorToOpen = FALSE;

	// need to find a near waypoint
	if( pBot->iCurWaypoint == -1 )
	{
		iIndex = WaypointFindNearest(ent);

		if( iIndex == -1 )
		{
			if( g_debugBots.integer )
			{
				G_Printf("Bot (%i) : Bot_MapNavigation failed, could not find a visible waypoint\n", ent->client->ps.clientNum);
			}
			return FALSE;
		}

		pBot->iCurWaypoint = iIndex;
		pBot->iCurWaypointTime = level.time;
	}
	else
	{
		// check if a door is blocking the way...
		if( Waypoints[ pBot->iCurWaypoint ].flags & FL_WP_DOOR )
		{
			VectorCopy( ent->client->ps.origin, vViewOrigin );
			vViewOrigin[2] += ent->client->ps.viewheight;

			trap_Trace( &tr, vViewOrigin, ent->r.mins, ent->r.maxs, Waypoints[ pBot->iCurWaypoint ].origin, ENTITYNUM_NONE, MASK_SOLID );

			pHit = &g_entities[ tr.entityNum ];

			// only need to activate rotating doors
			if( (!Q_stricmp( pHit->classname, "func_door_rotating" )) || (!Q_stricmp( pHit->classname, "func_door") && pHit->targetname == NULL ) )
			{
				// WolfBot 1.5 : bugfix func_door is only remote if targetname set
				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) : func_door_rotating is blocking my path\n", ent->client->ps.clientNum);
				}

				if( pHit->r.bmodel )
				{
					VectorSubtract( pHit->r.absmax, pHit->r.absmin, vDiff );
					VectorScale( vDiff, 0.5, vDiff );
					VectorAdd( pHit->r.absmin, vDiff, vBModelOrg );

					VectorSubtract( vBModelOrg, ent->client->ps.origin, vDistance );
				}
				else
				{
					VectorSubtract( pHit->r.currentOrigin, ent->client->ps.origin, vDistance );
				}

				if( VectorLength(vDistance) < 50 )
				{
					pBot->ucmd.buttons |= BUTTON_ACTIVATE;
				}
			}
			else if( !Q_stricmp( pHit->classname, "func_door" ) || !Q_stricmp( pHit->classname, "script_mover" ) )
			{
				// need to press a button to open the door
				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) : Remote door is blocking my path.\n", ent->client->ps.clientNum);
				}

				// door is closed and is NOT opening
				if( pHit->s.pos.trType == TR_STATIONARY )
				{
					// get last waypoint bot reached to get the button index of it
					if( pBot->iLastWaypoint != -1 )
					{
						botTask.iExpireTime = -1;
						botTask.iIndex = -1;
						botTask.iTask = TASK_PRESSBUTTON;
						botTask.pNextTask = NULL;
						botTask.pPrevTask = NULL;

						Bot_PushTask( pBot, &botTask );

						// push move task on stack
						botTask.iExpireTime = -1;
						botTask.iIndex = Waypoints[ pBot->iLastWaypoint ].wpdata.btnIndex;
						botTask.iTask = TASK_ROAM;
						botTask.pNextTask = NULL;
						botTask.pPrevTask = NULL;

						G_Printf("Pushing TASK_ROAM , destination %i\n", botTask.iIndex);

						Bot_PushTask( pBot, &botTask );

						// save off the actual goal so we can continue once door is open
						pBot->iLongTermGoal = pBot->iGoalWaypoint;

						if( g_debugBots.integer )
						{
							G_Printf("Saving GOAL WP %i\n", pBot->iLongTermGoal);
						}

						// free old route now
						Bot_FreeSolutionNodes(pBot);

						// set goal node
						pBot->iGoalWaypoint = botTask.iIndex;

						// ugly hack
						pBot->iCurWaypoint = pBot->iLastWaypoint;

						return FALSE;
					}
				}
				else
				{
					if( g_debugBots.integer )
					{
						G_Printf("Bot (%i) : Waiting for door to open.\n", ent->client->ps.clientNum);
					}

					bWaitingForDoorToOpen = TRUE;

					// reset iCurWaypointTime since bot is waiting for something
					pBot->iCurWaypointTime = level.time;
				}
			}
		}
	}

	// now need to check if bot is in "touching zone" of the waypoint
	VectorSubtract( Waypoints[ pBot->iCurWaypoint ].origin, ent->client->ps.origin, vDistance );
	fLength = VectorLength(vDistance);

	fTouchDist = 20.0f;
	bTouching = FALSE;

	// another UGLY hack : trace a line to waypoint and see if a camping teammate is blocking it
	if( fLength < 60.0f )
	{
		if( pBot->iCurWaypoint != -1 )
		{
			trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, Waypoints[ pBot->iCurWaypoint ].origin, ent->client->ps.clientNum, MASK_PLAYERSOLID );

			// WolfBot 1.5 : Consider MG42 Waypoint reached if used by anouther player
			if( g_entities[tr.entityNum].client )
			{
				if( g_entities[tr.entityNum].client->ps.eFlags & EF_MG42_ACTIVE )
				{
					bTouching = TRUE;
				}
			}

			if( g_entities[tr.entityNum].botClient )
			{
				// just consider reached
				if( Bot_GetTask(g_entities[tr.entityNum].botClient)->iTask == TASK_CAMP )
				{
					if( g_debugBots.integer )
					{
						G_Printf("Bot (%i) : ugly hack, camping teammate blocking waypoint.\n", ent->client->ps.clientNum);
					}

					bTouching = TRUE;
				}
			}
		}
	}

	if( fTouchDist > fLength )
		bTouching = TRUE;

	if( bTouching )
	{
		iGoal = -1;

		// goal node reached ?
		if( pBot->iCurWaypoint == pBot->iGoalWaypoint )
		{
			// put on top of Goal History Array
			pBot->iGoalHistory[0] = pBot->iGoalHistory[1];
			pBot->iGoalHistory[1] = pBot->iGoalHistory[2];
			pBot->iGoalHistory[2] = pBot->iGoalHistory[3];
			pBot->iGoalHistory[3] = pBot->iGoalHistory[4];
			pBot->iGoalHistory[4] = pBot->iGoalWaypoint;

			if( g_debugBots.integer )
			{
				Bot_Log("Bot (%i) : Goal Node (%i) reached. History : %i %i %i %i %i", ent->client->ps.clientNum, pBot->iGoalWaypoint, pBot->iGoalHistory[4], pBot->iGoalHistory[3], pBot->iGoalHistory[2], pBot->iGoalHistory[1], pBot->iGoalHistory[0]);
			}

			pBot->iGoalWaypoint = -1;
			return TRUE;
		}

		// if the bot doesn't have a goal waypoint pick a new goal
		if( pBot->iGoalWaypoint == -1 )
		{
			// look for stuff here then decide what to do
			iGoal = Bot_FindGoal(ent);

			if( iGoal != -1 )
			{
				// free old path
				Bot_FreeSolutionNodes(pBot);

				// bugfix -> Only search path if found Goal is NOT the current waypoint anyway
				if( pBot->iCurWaypoint != iGoal )
				{
					// found goal try to calculate a path to it
					pBot->pStartNode		= AStarSearch( pBot->iCurWaypoint, iGoal );
					pBot->pWaypointNodes	= pBot->pStartNode;

					if( pBot->pWaypointNodes == NULL )
					{
						if( g_debugBots.integer )
						{
							G_Printf("Bot (%i) : Path Calculation failed from %i to %i.\n", ent->client->ps.clientNum, ent->botClient->iCurWaypoint, iGoal);
						}
					}
					else
					{
						// assign goal
						pBot->iGoalWaypoint = iGoal;
					}
				}

				// NOT GOOD, moved into if statement above
		//		pBot->iGoalWaypoint = iGoal;
			}
		}
		else if( pBot->pWaypointNodes == NULL )
		{
			// bot might have been given a goal , for instance from a TASK_MOVETOPOSITION
			// but path has not yet been calculated so do it now

			pBot->pStartNode		= AStarSearch( pBot->iCurWaypoint, pBot->iGoalWaypoint );
			pBot->pWaypointNodes	= pBot->pStartNode;

			// If AStar failed Goal Node CANNOT be reached so find a new goal
			if( pBot->pWaypointNodes == NULL )
				pBot->iGoalWaypoint = -1;
		}

		if( pBot->iGoalWaypoint != -1 )
		{
			// bugfix -> make sure pWaypointNodes is not NULL
			if( pBot->pWaypointNodes )
				pBot->pWaypointNodes = pBot->pWaypointNodes->NextNode;

			if( pBot->pWaypointNodes )
			{
				// get the path flags
				for( i = 0; i < Waypoints[ pBot->iCurWaypoint ].numPaths; i++ )
				{
					// If there is a barrier near this entity and the bot
					// can and wants to blow it up , do it
					if( Waypoints[ pBot->iCurWaypoint ].paths[i].flags & FL_PATH_BLOCKED )
					{
						int			num;
						int			touch[MAX_GENTITIES];
						int			y;
						int			botWantsTo;
						gentity_t	*pBlocker = NULL;

						botWantsTo = rand() % 100;

						// does the bot even have grenades ?
						if( COM_BitCheck( ent->client->ps.weapons, WP_GRENADE_PINEAPPLE ) || COM_BitCheck( ent->client->ps.weapons, WP_GRENADE_LAUNCHER ) )
						{
							// randomize a bit
							if( botWantsTo > 50 )
							{
								num = trap_EntitiesInBox( Waypoints[ pBot->iCurWaypoint ].origin, Waypoints[ Waypoints[ pBot->iCurWaypoint ].paths[i].indexTarget ].origin, touch, MAX_GENTITIES );

								for( y = 0; y < num; y++ )
								{
									if( g_entities[ touch[y] ].s.eType == ET_EXPLOSIVE )
									{
										pBlocker = &g_entities[ touch[y] ];

										if( g_debugBots.integer )
										{
											G_Printf("Bot (%i) : Found Entity as the blocker %i\n", ent->client->ps.clientNum, pBlocker->s.number);
										}
										break;
									}	
								}

								if( pBlocker )
								{
									// need to check out if Bot can blow this entity up
									// we don't care for Dynamite Only entities since they're
									// most likely objectives anyway...
									if( !(pBlocker->spawnflags & SPAWNFLAG_DYNO_ONLY) )
									{
										botTask.iExpireTime = level.time + 5000;
										botTask.iIndex		= pBlocker->s.number;
										botTask.iTask		= TASK_DESTROYBARRIER;
										botTask.pNextTask	= NULL;
										botTask.pPrevTask	= NULL;

										Bot_PushTask( pBot, &botTask );
									}
								}
							}
						}
					}

					if( Waypoints[ pBot->iCurWaypoint ].paths[i].indexTarget == pBot->pWaypointNodes->wpIndex )
						pBot->iPathFlags = Waypoints[ pBot->iCurWaypoint ].paths[i].flags;
				}

				pBot->iLastWaypoint = pBot->iCurWaypoint;
				pBot->iCurWaypoint = pBot->pWaypointNodes->wpIndex;
				pBot->iCurWaypointTime = level.time;
			}
		}

	}

	// turn towards waypoint
	VectorSubtract( Waypoints[ pBot->iCurWaypoint ].origin, ent->client->ps.origin, vDirection );
	vectoangles( vDirection, vAngles );

	pBot->ideal_yaw = vAngles[YAW];
	pBot->ideal_pitch = vAngles[PITCH];

	pBot->ucmd.forwardmove = bWaitingForDoorToOpen ? 0 : 127;

	if( level.time >= pBot->iSpeedCheckTime )
	{
		VectorSubtract( ent->client->ps.origin, pBot->vPrevOrigin, vDistance );
		VectorCopy( ent->client->ps.origin, pBot->vPrevOrigin );

		// if not on ladder zero out vertical speed
		if( !(ent->client->ps.pm_flags & PMF_LADDER) )
			vDistance[2] = 0;

		fMinMove = (pBot->iPathFlags & FL_PATH_WALK) ? 3.0f : 5.0f;

		// adapt for venom panzerfaust and flamethrower since move speed is slowed down
		if( Bot_HasHeavyWeapon(ent) )
			fMinMove = 3.0f;

		fMinMove = (pBot->iPathFlags & FL_PATH_DUCK) ? 2.0f : fMinMove;

		if( VectorLength(vDistance) >= fMinMove )
			pBot->iLastMoveTime = level.time;

		// check again 50 msecs later ,maximum 20 times a second
		pBot->iSpeedCheckTime = level.time + 50;
	}

	// bot is probably stuck , try to get unstuck by jumping and strafing vigorously
	if( (pBot->iPrevSpeed > 0) && (!(pBot->iPathFlags & FL_PATH_LADDER)) && (level.time > (pBot->iLastMoveTime + 100)) )
	{
		BOOL bStrafe = TRUE;
		int	iStuckTime;

		if( wb_allowSuicide.integer )
		{
			iStuckTime = level.time - pBot->iLastMoveTime;

			// stuck longer than 10 seconds ? suicide!
			if( iStuckTime >= 10000 )
			{
				Cmd_Kill_f(ent);
			}
		}

		// can potentially jump
		if( level.time - ent->client->ps.jumpTime > 1000 )
		{
			if( level.time > pBot->iLastJumpTime )
			{
				if( Bot_CanJumpOnObstacle( ent, &bDuck ) )
				{
					pBot->ucmd.upmove = 127;
					pBot->iLastJumpTime = level.time;

					// we need to duckjump here
					if( bDuck == TRUE )
					{
						pBot->iDuckJumpTime = level.time + 2000;
					}

					bStrafe = FALSE;
				}
				else
				{
					// jump at random times
					if( (rand() % 100) > 70 )
						pBot->ucmd.upmove = 127;
						
					pBot->iLastJumpTime = level.time;
				}
			}
		}

		if( level.time > pBot->iStrafeTime )
		{
			BOOL bRightOK, bLeftOK;

			pBot->iStrafeTime = level.time + 100 + rand() % 50;

			if( (rand() % 100) > 40 && bStrafe == TRUE )
			{

				bRightOK = Bot_CanStrafeRight(ent);
				bLeftOK = Bot_CanStrafeLeft(ent);

				if( (rand() % 100) > 50 )
				{
					if( bRightOK )
						pBot->iStrafeDir = 1;
					else
						pBot->iStrafeDir = -1;
				}
				else
				{
					if( bLeftOK )
						pBot->iStrafeDir = -1;
					else
						pBot->iStrafeDir = 1;
				}

				if( !bRightOK && !bLeftOK )
					pBot->iStrafeDir = 0;
			}
		}
	}

	// check out path
	if( pBot->iPathFlags & FL_PATH_JUMP )
	{
		if( level.time - ent->client->ps.jumpTime > 1000 )
		{
			pBot->ucmd.upmove = 127;
			pBot->iPathFlags &= ~FL_PATH_JUMP;
		}
	}
	else if( pBot->iPathFlags & FL_PATH_DUCK )
	{
		pBot->ucmd.upmove = -127;
	}
	else if( pBot->iPathFlags & FL_PATH_WALK )
	{
		// if slow walking already just ignore
		if( Bot_HasHeavyWeapon(ent) == FALSE )
		{
			if( pBot->ucmd.forwardmove > 64 )
				pBot->ucmd.forwardmove = 64;
			else if( pBot->ucmd.forwardmove < -64 )
				pBot->ucmd.forwardmove = -64;
		}
	}
	else if( pBot->iPathFlags & FL_PATH_LADDER )
	{
		// figure out whether bot needs to climb up or down
		if( Waypoints[ pBot->iCurWaypoint ].origin[2] > ent->client->ps.origin[2] )
			iLadderDir = 1;
		else
			iLadderDir = -1;

		if( iLadderDir == 1 )
		{
			// look straight up while moving forward
			pBot->ideal_pitch = -80;
		}
		else
		{
			// turn 180 degrees around so we face the exact opposite direction
			pBot->ideal_yaw -= 180.0f;

			pBot->ucmd.forwardmove = -30;

			// We're on the ladder
			if( ent->client->ps.pm_flags & PMF_LADDER )
			{
				// look up and move down
				pBot->ideal_pitch = -20;
				pBot->ucmd.forwardmove = -127;
			}
		}
		
	}

	return FALSE;

}

//
// Finds a good Goal for ent and returns index of waypoint found
//

int Bot_FindGoal( gentity_t* ent )
{
	int iRandom;
	int iGoal;
	int i;
	vec3_t vVector;
	float fDistance;
	float fBest;
	WAYPOINT *pWaypoint;
	botClient_t *pBot;

	iGoal	= -1;
	pBot	= ent->botClient;

	iRandom = rand() % 100;

	if( g_debugBots.integer )
	{
		G_Printf("Bot (%i) : Bot_FindGoal Random : %i\n", ent->client->ps.clientNum, iRandom);
	}

	fBest = 99999;

	// If bot has enemy docs find deliver waypoint shortest from position
	if( Bot_HasEnemyFlag(ent) )
	{
		for( i = 0; i < numDeliverWaypoints; i++ )
		{
			pWaypoint = &Waypoints[ DeliverWaypoints[ i ] ];

			if( (pWaypoint->flags & FL_WP_AXIS_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_AXIS) )
				continue;

			if( (pWaypoint->flags & FL_WP_ALLIES_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_ALLIES) )
				continue;

			if( (pWaypoint->flags & FL_WP_AXIS_DOCS_DELIVER) && (ent->client->sess.sessionTeam != TEAM_AXIS) )
				continue;

			if( (pWaypoint->flags & FL_WP_ALLIES_DOCS_DELIVER) && (ent->client->sess.sessionTeam != TEAM_ALLIES) )
				continue;

			VectorSubtract( pWaypoint->origin, ent->client->ps.origin, vVector );
			fDistance = VectorLength(vVector);

			if( fDistance < fBest )
			{
				fBest = fDistance;
				iGoal = pWaypoint->index;
			}
		}

		if( iGoal != -1 )
		{
			if( g_debugBots.integer )
			{
				G_Printf("Bot_FindGoal : Bot is heading for deliver waypoint %i!\n", iGoal);
			}

			return iGoal;
		}
		else
		{
			if( g_debugBots.integer )
			{
				G_Printf("Warning : Bot has enemy documents but could not find a deliver waypoint\n");
			}

			Bot_Log("Bot (%i) : Have docs but could not find a deliver waypoint", ent->client->ps.clientNum);
		}
	}

	// FIXME : this should be rewritten so that bots that dont have objectives dont bother
	if( iRandom > 55 )
	{
		// only engineers can arm bombs
		if( pBot->playerClass == PC_ENGINEER )
		{
			iGoal = WaypointFindObjective( ent, -1 );
		}
		else
		{
			if( (numStealObjectives > 0) && (numCaptureObjectives > 0) )
			{
				if( crandom() > 0 )
					iGoal = WaypointFindObjective( ent, OBJECTIVE_STEAL );
				else
					iGoal = WaypointFindObjective( ent, OBJECTIVE_CAPTURE );
			}
			else if( numStealObjectives > 0 )
			{
				iGoal = WaypointFindObjective( ent, OBJECTIVE_STEAL );
			}
			else if( numCaptureObjectives > 0 )
			{
				iGoal = WaypointFindObjective( ent, OBJECTIVE_CAPTURE );
			}
		}
	}

	if( iRandom > 40 && iGoal == -1 )
	{
		// WolfBot 1.5 : If enemy team has stolen bot teams documents camp at a
		// enemy deliver waypoint for about half of the time
		 if( (FlagAtHome(ent->client->sess.sessionTeam) == FALSE) && ((rand() % 100) > 30) )
		{
			if( ent->client->sess.sessionTeam == TEAM_AXIS )
			{
				iGoal = WaypointFindRandomGoal( ent, FL_WP_ALLIES_DOCS_DELIVER );
			}
			else if( ent->client->sess.sessionTeam == TEAM_ALLIES )
			{
				iGoal = WaypointFindRandomGoal( ent, FL_WP_AXIS_DOCS_DELIVER );
			}
		}
		else
		{
			if( numMG42Waypoints > 0 )
			{
				if( (rand() % 100) > 40 )
				{
					iGoal = WaypointFindRandomGoal( ent, FL_WP_MG42 );
				}
				else
				{
					iGoal = WaypointFindRandomGoal( ent, FL_WP_CAMP );
				}
			}
			else
			{
				iGoal = WaypointFindRandomGoal( ent, FL_WP_CAMP );
			}
		}
	}

	if( iRandom >= 0 && iGoal == -1 )
	{
		iGoal = WaypointFindRandomGoal( ent, -1 );
	}

	// if bot failed to find a waypoint give it a random one
	if( iGoal == -1 )
		iGoal = WaypointFindRandomGoal( ent, -1 );

	if( g_debugBots.integer )
	{
		G_Printf("Bot (%i) : Found %i as a goal.\n", ent->client->ps.clientNum, iGoal);
	}

	return iGoal;
}

//
// Tries to find closest unaccomplished objective for bots team
//

int WaypointFindObjective( gentity_t *ent, int iType )
{
	int				index;
	int				i;
	vec3_t			vDistance;
	float			fLength;
	float			fTemp;

	WAYPOINT		*pWaypoint;

	int				obj_team;
	int				obj_type;
	int				obj_state;

	fLength = 99999;
	index = -1;

	// go through all objective Waypoints
	for( i = 0; i < numObjWaypoints; i++ )
	{
		pWaypoint = &Waypoints[ ObjWaypoints[ i ] ];

		// check if waypoint is currently reachable. Maybe another objective has to be
		// accomplished first. Think of mp_beach where allies have to breach the sea
		// wall first in order to advance to the second objective.
		if( (pWaypoint->flags & FL_WP_AXIS_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_AXIS) )
			continue;

		if( (pWaypoint->flags & FL_WP_ALLIES_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_ALLIES) )
			continue;

		// extract the Objective Information
		obj_team = pWaypoint->objdata.team;

		// objective is not for bots team so skip it
		if( obj_team != ent->client->sess.sessionTeam )
			continue;

		obj_type = pWaypoint->objdata.type;

		// not objective type we're looking for
		if( (iType != -1) && (obj_type != iType) )
			continue;

		obj_state = pWaypoint->objdata.state;

		// objective already accomplished (FIXME : react to STATE_INPROGRESS ?)
		if( obj_state != STATE_UNACCOMPLISHED )
			continue;

		// don't bother if bot just has been there recently
		if( ent->botClient->iGoalHistory[0] == pWaypoint->index || ent->botClient->iGoalHistory[1] == pWaypoint->index || ent->botClient->iGoalHistory[2] == pWaypoint->index || ent->botClient->iGoalHistory[3] == pWaypoint->index || ent->botClient->iGoalHistory[4] == pWaypoint->index )
		{
			continue;
		}


		VectorSubtract( pWaypoint->origin, ent->client->ps.origin, vDistance );
		fTemp = VectorLength(vDistance);

		if( fTemp < fLength )
		{
			fLength = fTemp;
			index = pWaypoint->index;
		}
	}

	return index;
}

//
// Finds a random Waypoint Goal with the matching flags (pass -1 for no specific flags)
//

int WaypointFindRandomGoal( gentity_t *ent, int iFlags )
{
	int i,c;
	int indices[MAX_NUM_WAYPOINTS];
	BOOL bFlags = (iFlags < 0) ? FALSE : TRUE;

	c = 0;

	for( i = 0; i < numWaypoints; i++ )
	{
		// if waypoint is currently not reachable for this bot , skip it
		if( (Waypoints[i].flags & FL_WP_AXIS_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_AXIS) )
			continue;

		if( (Waypoints[i].flags & FL_WP_ALLIES_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_ALLIES) )
			continue;

		// waypoint is not what we want
		if( (bFlags) && ((Waypoints[i].flags & iFlags) != iFlags) )
			continue;

		if( c < MAX_NUM_WAYPOINTS )
		{
			indices[c] = i;
			c++;
		}
	}

	// rand() % 0 .... not good
	if( c == 0 )
		return -1;

	return indices[ rand() % c ];
}

//
// Finds all reachable camp waypoints within fRadius and returns a random one
//

int WaypointFindCampSpot( gentity_t *ent, float fRadius )
{
	int i;
	int indices[128];
	int iIndex;
	vec3_t vVector;
	WAYPOINT *pWaypoint;

	iIndex = 0;

	for( i = 0; i < numCampWaypoints; i++ )
	{
		pWaypoint = &Waypoints[ CampWaypoints[ i ] ];

		if( (pWaypoint->flags & FL_WP_AXIS_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_AXIS) )
			continue;

		if( (pWaypoint->flags & FL_WP_ALLIES_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_ALLIES) )
			continue;

		VectorSubtract( pWaypoint->origin, ent->client->ps.origin, vVector );

		if( VectorLength(vVector) > fRadius )
			continue;

		if( iIndex < 128 )
		{
			indices[ iIndex ] = pWaypoint->index;
			iIndex++;
		}
	}

	if( iIndex == 0 )
		return -1;

	return indices[ rand() % iIndex ];

}

//
// Find Nearest Waypoint from bot
//

int WaypointFindNearest( gentity_t *ent )
{
	int i;
	int index = -1;
	float dist = 99999;
	vec3_t tmp;
	trace_t tr;

	for( i = 0; i < numWaypoints; i++ )
	{
		VectorSubtract( Waypoints[i].origin, ent->client->ps.origin, tmp );

		if( VectorLength(tmp) < dist )
		{
			// check for obstacles
			trap_Trace( &tr, Waypoints[i].origin, NULL, NULL, ent->client->ps.origin, ent->client->ps.clientNum, MASK_SOLID );

			if( tr.fraction == 1.0f )
			{
				dist = VectorLength(tmp);
				index = i;
			}
		}
	}

	if( index == -1 )
	{
		if( g_debugBots.integer )
		{
			G_Printf("Bot (%i) : WaypointFindNearest failed.\n", ent->client->ps.clientNum);
		}
		return -1;
	}

	return index;
}

//
// Finds nearest Waypoint with matching flags that is visible from vPosition
//

int WaypointFindVisible( vec3_t vPosition, int iFlags, float fMinRange, float fMaxRange  )
{
	int i;
	float fLength;
	float fBest;
	int iBest;
	vec3_t tmp;
	trace_t tr;
	BOOL bFlags = (iFlags < 0) ? FALSE : TRUE;

	fBest = 99999;
	iBest = -1;

	for( i = 0; i < numWaypoints; i++ )
	{
		VectorSubtract( Waypoints[i].origin, vPosition, tmp );
		
		fLength = VectorLength(tmp);

		if( (fLength > fMaxRange) || (fLength < fMinRange) )
			continue;

		if( (bFlags) && ((Waypoints[i].flags && iFlags) != iFlags) )
			continue;

		trap_Trace( &tr, Waypoints[i].origin, NULL, NULL, vPosition, ENTITYNUM_NONE, MASK_SOLID );

		if( tr.fraction != 1.0f )
			continue;

		if( fLength < fBest )
		{
			fBest = fLength;
			iBest = i;
		}
	}

	return iBest;
}