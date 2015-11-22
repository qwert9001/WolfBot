// Copyright (C) 2003 Torben Könke. Licensed under GPL, see LICENSE.
//
// cg_waypoint.c -- funktionen für mapper um waypoints in maps zu setzen
// und zu editieren

#include "cg_local.h"

// Waypoints

WAYPOINT		Waypoints[MAX_NUM_WAYPOINTS];
BOOL			bRenderWaypoints	= FALSE;
BOOL			bRenderPaths		= TRUE;
BOOL			bRenderArrows		= FALSE;

 // next free waypoint slot in Waypoints[] array
int				Waypoint_index		= 0;

// debug variable to keep track of allocated memory
int memory;

// calculate nearest waypoint so we can color it green to highlight it
int				NearestIndex; 

// entity num of func_explosive in players crosshair else -1
int				iExplosiveNum;

// Menu Variables (only used for drawing and menu handling)
int iMenuState			= 0;
int iMenuObjType		= 0;
int iMenuObjTeam		= 0;
int iMenuObjPrior		= 0;
int iMenuObjTarget		= 0;
int iMenuObjNumber		= 0;

int iMenuPathFrom		= 0;
int iMenuPathTo			= 0;

int iMenuWpFlags		= 0;	// waypoint flags
int iMenuPtFlags		= 0;	// path travel flags
int iMenuWpUnreachTeam	= 0;

int iMenuWpUnreachEnts[4] = { 0, 0, 0, 0 };		// array of func_explosive entity nums for unreachable wps
int iMenuWpUnreachIndex;

int iMenuWpButtonIndex	= 0;

// WolfBot 1.5 : Waypoint Dragging
vec3_t	vWaypointOrigin;
int		iWpDragIndex;	// waypoint that is currently being dragged

//
// Reads in a WPS file
//

void Cmd_WP_LoadFile( void )
{
	char			arg[MAX_TOKEN_CHARS];
	int				i, c;
	fileHandle_t	hFile;
	WPSHEADER		Header;
	const char		*info;
	char			*mapname;

	if ( trap_Argc () < 2 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_load           [filename]\n");
		CG_Printf("Loads a Waypoint File\n\n");
		CG_Printf("[filename]        : File to load (Example : mp_beach.wps)\n");
		CG_Printf("------------------------\n");
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	
	trap_FS_FOpenFile( arg, &hFile, FS_READ );

	if( !hFile )
	{
		CG_Printf("Could not open %s\n", arg );
		return;
	}

	info = CG_ConfigString( CS_SERVERINFO );
	mapname = Info_ValueForKey( info, "mapname" );

	trap_FS_Read( &Header, sizeof(Header), hFile );

	if( strcmp( Header.szMagic, WPS_MAGIC ) )
	{
		CG_Printf("This is not a kajibot Waypoint File\n");
		return;
	}

	if( Header.nVersion != WPS_FILE_VERSION )
	{
		CG_Printf("Wrong Version %i (Should be %i)\n", Header.nVersion, WPS_FILE_VERSION );
		return;
	}

	if( Header.nWaypoints > MAX_NUM_WAYPOINTS )
	{
		CG_Printf("The file contains too many Waypoints. Maxium number of Waypoints is %i\n", MAX_NUM_WAYPOINTS);
		return;
	}

	if( strcmp( Header.szMapname, mapname ) )
	{
		CG_Printf("Mapname mismatch (%s)\n", Header.szMapname);
		return;
	}

	// Waypoints einlesen
	for( i = 0; i < Header.nWaypoints; i++ )
	{
		trap_FS_Read( &Waypoints[i], sizeof(WAYPOINT), hFile );

		Waypoints[i].paths = (WPPATH*)malloc( sizeof(WPPATH) * Waypoints[i].numPaths );

		// Paths für diesen Waypoint einlesen
		for( c = 0; c < Waypoints[i].numPaths; c++ )
		{
			trap_FS_Read( &Waypoints[i].paths[c], sizeof(WPPATH), hFile );
		}

	}

	Waypoint_index		= Header.nWaypoints;
	bRenderWaypoints	= TRUE;
	
	trap_FS_FCloseFile(hFile);

	CG_Printf("Successfully loaded %s\n", arg );

	return;
}

//
// Saves Waypoint data to a WPS file
//

void Cmd_WP_SaveFile( void )
{
	char			arg[MAX_TOKEN_CHARS];
	fileHandle_t	hFile;
	WPSHEADER		Header;
	int				i, c;
	const char		*info;
	char			*mapname;

	if ( trap_Argc () < 2 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_save           [filename]\n");
		CG_Printf("Saves a Waypoint File\n\n");
		CG_Printf("[filename]        : File to save to (Example : mp_beach.wps)\n");
		CG_Printf("------------------------\n");
		return;
	}

	trap_Argv( 1, arg, sizeof(arg) );

	trap_FS_FOpenFile( arg, &hFile, FS_WRITE );

	if( !hFile )
	{
		CG_Printf("Could not open %s\n", arg );
		return;
	}

	info = CG_ConfigString( CS_SERVERINFO );
	mapname = Info_ValueForKey( info, "mapname" );

	Header.nVersion		= WPS_FILE_VERSION;
	Header.nWaypoints	= Waypoint_index;

	strcpy( Header.szMagic, WPS_MAGIC );
	strcpy( Header.szMapname, mapname );

	trap_FS_Write( &Header, sizeof(Header), hFile );

	for( i = 0; i < Header.nWaypoints; i++ )
	{
		trap_FS_Write( &Waypoints[i], sizeof(WAYPOINT), hFile );

		for( c = 0; c < Waypoints[i].numPaths; c++ )
		{
			trap_FS_Write( &Waypoints[i].paths[c], sizeof(WPPATH), hFile );
		}
	}		

	trap_FS_FCloseFile(hFile);

	CG_Printf("Successfully saved %s\n", arg );

	return;
}

//
// Places a Waypoint at Player's current position
//

void Cmd_WP_PlaceWaypoint( void )
{
	WAYPOINT	*Waypoint = NULL;

	if( Waypoint_index == MAX_NUM_WAYPOINTS )
	{
		CG_Printf("Too many Waypoints placed already\n");
		return;
	}

	if( trap_Argc() > 1 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_place           [none]\n");
		CG_Printf("Places a normal Waypoint. Takes no parameters\n");
		CG_Printf("------------------------\n");
		return;
	}

	Waypoint = &Waypoints[Waypoint_index];

	Waypoint->index		= Waypoint_index;
	Waypoint->numPaths	= 0;
	Waypoint->paths		= NULL;
	Waypoint->flags		= 0;

	memset( &Waypoint->wpdata, 0, sizeof(Waypoint->wpdata) );

	Waypoint_index++;	// increment Waypoint index
	
	VectorCopy( cg.snap->ps.origin, Waypoint->origin );

	// CHECK FOR NEAR ENTITIES SUCH AS DOORS AND SET CORRESPONDING FLAGS

	CG_Printf("Waypoint (%i) placed\n", Waypoint->index);

	bRenderWaypoints = TRUE;
}

//
// Places an Objective Waypoint at Player's current position
//

void Cmd_WP_PlaceObjective( void )
{
	WAYPOINT	*Waypoint = NULL;
	char		buf[MAX_TOKEN_CHARS];
	int			obj_team;
	int			obj_type;
	int			obj_priority;
	int			obj_target;
	int			obj_num;

	if( trap_Argc() < 4 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_objective      [team] [type] [priority]\n");
		CG_Printf("Places an Objective Waypoint. Takes 4 parameters and 1 optional.\n\n");

		CG_Printf("[team]            : 1 for an Axis Objective\n");
		CG_Printf("                  : 2 for an Allies Objective\n\n");

		CG_Printf("[type]            : 0 for a Bomb Objective\n");
		CG_Printf("                  : 1 for a Steal Objective\n");
		CG_Printf("                  : 2 for a Capture Objective\n\n");

		CG_Printf("[priority]        : 0 for a High Priority Objective\n");
		CG_Printf("                  : 1 for a Low Priority Objective\n\n");

		CG_Printf("[number]          : Number of Wolfenstein Map Objective\n");
		CG_Printf("                    this Waypoint should be linked to.\n\n");

		CG_Printf("[target]          : Only needed when placing a Bomb Objective.\n");
		CG_Printf("                    In this case target is the Entity Number of the\n");
		CG_Printf("                    func_explosive this objective is linked to.\n");
		
		CG_Printf("------------------------\n");
		return;
	}

	if( Waypoint_index == MAX_NUM_WAYPOINTS )
	{
		CG_Printf("Too many Waypoints placed already\n");
		return;
	}

	trap_Argv( 1, buf, sizeof(buf) );
	obj_team = atoi(buf);

	if( obj_team < 1 || obj_team > 2 )
	{
		CG_Printf("Objective Team is out of Range ( 1 = TEAM_AXIS ; 2 = TEAM_ALLIES  )\n");
		return;
	}

	trap_Argv( 2, buf, sizeof(buf) );
	obj_type = atoi(buf);

	if( obj_type < 0 || obj_type > 2 )
	{
		CG_Printf("Objective Type is out of Range ( 0 = BOMB ; 1 = STEAL ; 2 = CAPTURE )\n");
		return;
	}

	trap_Argv( 3, buf, sizeof(buf) );
	obj_priority = atoi(buf);

	if( obj_priority < 0 || obj_priority > 1 )
	{
		CG_Printf("Objective Priority is out of Range ( 0 = HIGH ; 1 = LOW )\n");
		return;
	}

	trap_Argv( 4, buf, sizeof(buf) );
	obj_num = atoi(buf);

	if( obj_num < 1 || obj_num > MAX_OBJECTIVES )
	{
		CG_Printf("Objective Number is out of Range ( 0 - 6 )\n");
		return;
	}

	// Extra parameter for bomb objective
	if( obj_type == OBJECTIVE_BOMB )
	{
		trap_Argv( 5, buf, sizeof(buf) );
		obj_target = atoi(buf);

		// < 1 because 0 is worldspawn and must not be used also
		if( obj_target < 1 || obj_target > MAX_GENTITIES )
		{
			CG_Printf("Invalid Entity Number as Target for Bomb Objective!\n");
			return;
		}
	}

	Waypoint = &Waypoints[Waypoint_index];

	Waypoint->index		= Waypoint_index;
	Waypoint->numPaths	= 0;
	Waypoint->paths		= NULL;
	Waypoint->flags		= 0;

	memset( &Waypoint->wpdata, 0, sizeof(Waypoint->wpdata) );

	Waypoint_index++;	// increment Waypoint index

	VectorCopy( cg.snap->ps.origin, Waypoint->origin );

	Waypoint->flags |= FL_WP_OBJECTIVE;

	// pack information in wpdata field
	Waypoint->objdata.team		= obj_team;
	Waypoint->objdata.type		= obj_type;
	Waypoint->objdata.state		= STATE_UNACCOMPLISHED;
	Waypoint->objdata.prior		= obj_priority;
	Waypoint->objdata.objNum	= obj_num;

	if( obj_type == OBJECTIVE_BOMB )
		Waypoint->objdata.entNum = obj_target;

	CG_Printf("Objective Waypoint (%i) placed\n", Waypoint->index);

}

//
// Deletes a Waypoint
//

void Cmd_WP_DeleteWaypoint( void )
{
	char	arg[MAX_TOKEN_CHARS];
	int		index,c,j;

	if( trap_Argc() < 2 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_delete         [index]\n");
		CG_Printf("Deletes a Waypoint and all Paths to it. Takes 1 parameter\n\n");
		CG_Printf("[index]           : Index of the Waypoint\n");
		CG_Printf("------------------------\n");
		return;
	}

	trap_Argv( 1, arg, sizeof(arg) );

	index = atoi(arg);

	if( index < 0 || index > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint index\n");
		return;
	}

	// Alle Paths VON dem Wegpunkt löschen
	if( Waypoints[index].numPaths > 0 )
	{
		CG_Printf("Own Paths freed\n");
		free( Waypoints[index].paths );
	}

	// Alle Paths ZU dem Wegpunkt löschen
	for( c = 0; c < Waypoint_index; c++ )
	{
		if( c == index ) // skip self
			continue;

		for( j = 0; j < Waypoints[c].numPaths; j++ )
		{
			if( Waypoints[c].paths[j].indexTarget == index )
			{
				// dahinter liegende Paths nach vorne schieben und speicher verkleinern
				memmove( &Waypoints[c].paths[j], &Waypoints[c].paths[j + 1], (Waypoints[c].numPaths - j) * sizeof(WPPATH) );
					
				Waypoints[c].numPaths--;
				Waypoints[c].paths = (WPPATH*) realloc( Waypoints[c].paths, Waypoints[c].numPaths * sizeof(WPPATH) );

				CG_Printf("Path from Waypoint (%i) freed\n", Waypoints[c].index);
				break;
			}
		}
	}

	// Alle Wegpunkte dahinter nach vorne schieben 
	for( c = index + 1; c < Waypoint_index; c++ )
	{
		Waypoints[c].index--;
		Waypoints[c - 1] = Waypoints[c];
	}

	Waypoint_index--;

	// Alle links zu wegpunkten grösser als Index dekrementieren
	for( c = 0; c < Waypoint_index; c++ )
	{
		for( j = 0; j < Waypoints[c].numPaths; j++ )
		{
			if( Waypoints[c].paths[j].indexTarget > index )
				Waypoints[c].paths[j].indexTarget--;
		}

		// evtl müssen auch button index aktualisiert werden
		if( Waypoints[c].flags & FL_WP_DOOR )
		{
			if( Waypoints[c].wpdata.btnIndex > index )
				Waypoints[c].wpdata.btnIndex--;
		}
	}

}

//
// Sets Flags for a Waypoint
//

void Cmd_WP_SetFlags( void )
{
	int		index;
	int		bitflags;
	int		entitynum;
	int		buttonindex;
	char	szBuf[128];

	if( trap_Argc() < 3 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_setflags       [index] [flags] [data]\n");
		CG_Printf("Sets flags for a Waypoint. Takes 2 parameters and 5 conditional\n\n");
		CG_Printf("[index]           : Index of the Waypoint\n");
		CG_Printf("[flags]           : See wp_dumpflags for a List of Flags\n");
		CG_Printf("[cond1]           : If Waypoint has an unreachable flag set\n");
		CG_Printf("[cond2]           : You can add up to 4 entity numbers of\n");
		CG_Printf("[cond3]           : func_explosives here that block this\n");
		CG_Printf("[cond4]           : Waypoint for a Team.\n"); // fixme better desc.
		CG_Printf("[cond5]           : Index of the Waypoint nearest to a remote\n");
		CG_Printf("                  : button if this Waypoint is a door Waypoint\n");
		CG_Printf("------------------------\n");
		return;
	}

	trap_Argv( 1, szBuf, sizeof(szBuf) );
	index = atoi(szBuf);

	if( index < 0 || index > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint index\n");
		return;
	}

	trap_Argv( 2, szBuf, sizeof(szBuf) );
	bitflags = atoi(szBuf);

	if( (bitflags & FL_WP_AXIS_UNREACHABLE) || (bitflags & FL_WP_ALLIES_UNREACHABLE) )
	{
		trap_Argv( 3, szBuf, sizeof(szBuf) );
		entitynum = atoi(szBuf);

		if( entitynum < 1 || entitynum > MAX_GENTITIES )
		{
			CG_Printf("Invalid Entity Number\n");
			return;
		}

		Waypoints[index].wpdata.entNum1 = entitynum;

		trap_Argv( 4, szBuf, sizeof(szBuf) );
		entitynum = atoi(szBuf);
		
		if( entitynum > 0 && entitynum < MAX_GENTITIES )
		{
			Waypoints[index].wpdata.entNum2 = entitynum;
		}

		trap_Argv( 5, szBuf, sizeof(szBuf) );
		entitynum = atoi(szBuf);

		if( entitynum > 0 && entitynum < MAX_GENTITIES )
		{
			Waypoints[index].wpdata.entNum3 = entitynum;
		}

		trap_Argv( 6, szBuf, sizeof(szBuf) );
		entitynum = atoi(szBuf);

		if( entitynum > 0 && entitynum < MAX_GENTITIES )
		{
			Waypoints[index].wpdata.entNum4 = entitynum;
		}
	}

	if( bitflags & FL_WP_DOOR )
	{
		trap_Argv( 7, szBuf, sizeof(szBuf) );
		buttonindex = atoi(szBuf);

		if( buttonindex < 0 || buttonindex > Waypoint_index )
		{
			CG_Printf("Invalid Waypoint target index for Door Waypoint\n");
			return;
		}

		Waypoints[index].wpdata.btnIndex = buttonindex;
	}

	Waypoints[index].flags |= bitflags;
}

//
// Creats a Path between 2 Waypoints
//

void Cmd_WP_ConnectWaypoints( void )
{
	char		c_from[4], c_to[4], type[64];
	int			index_from, index_to, i;
	BOOL		bOneWay;
	WAYPOINT	*from, *to;
	vec3_t		tmp;

	if( trap_Argc() < 4 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_connect        [from] [to] [type]\n");
		CG_Printf("Creats a Path between 2 Waypoints. Takes 3 parameters\n\n");
		CG_Printf("[from]            : Index of first Waypoint\n");
		CG_Printf("[to]              : Index of second Waypoint\n");
		CG_Printf("[type]            : 1 to create a single Path\n");
		CG_Printf("                  : 2 to create Paths in both directions\n");
		CG_Printf("------------------------\n");
		return;
	}

	trap_Argv( 1, c_from, sizeof(c_from) );
	trap_Argv( 2, c_to, sizeof(c_to) );
	trap_Argv( 3, type, sizeof(type) );

	index_from	 = atoi(c_from);
	index_to	 = atoi(c_to);

	if( index_from < 0 || index_from > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint index\n");
		return;
	}

	if( index_to < 0 || index_to > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint index\n");
		return;
	}

	if( !Q_stricmp( type, "1" ) )
	{
		bOneWay = TRUE;
		CG_Printf("Creating One-Way Path from %i to %i\n", index_from, index_to );
	}
	else if( !Q_stricmp( type, "2" ) )
	{
		bOneWay = FALSE;
		CG_Printf("Creating Both-Way Path between %i and %i\n", index_from, index_to );
	}
	else
	{
		CG_Printf("Unknown Type passed\n");
		return;
	}

	from	= &Waypoints[index_from];
	to		= &Waypoints[index_to];

	for( i = 0; i < from->numPaths; i++ )
	{
		if( from->paths[i].indexTarget == index_to )
		{
			CG_Printf("There already is a Path from %i to %i!\n", index_from, index_to );
			return;
		}
	}

	for( i = 0; i < to->numPaths; i++ )
	{
		if( to->paths[i].indexTarget == index_from )
		{
			CG_Printf("There already is a Path from %i to %i!\n", index_to, index_from );
			return;
		}
	}

	from->numPaths++;
	from->paths = (WPPATH*) realloc( from->paths, sizeof(WPPATH) * from->numPaths );

	VectorSubtract( to->origin, from->origin, tmp );

	from->paths[ from->numPaths - 1 ].indexTarget	= to->index;
	from->paths[ from->numPaths - 1 ].flags			= 0;
	from->paths[ from->numPaths - 1 ].length		= VectorLength(tmp);

	if(!bOneWay)
	{
		to->numPaths++;
		to->paths = (WPPATH*) realloc( to->paths, sizeof(WPPATH) * to->numPaths );

		to->paths[ to->numPaths - 1 ].indexTarget	= from->index;
		to->paths[ to->numPaths - 1 ].flags			= 0;
		to->paths[ to->numPaths - 1 ].length		= VectorLength(tmp);
	}
	
	CG_Printf("Path created\n");
}

//
// Deletes a Path between 2 Waypoints
//

void Cmd_WP_DisconnectWaypoints( void )
{
	char c_from[4], c_to[4], c_type[4];
	int index_from, index_to, i;
	BOOL bOneWay;
	WAYPOINT *from, *to;

	if( trap_Argc() < 4 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_disconnect     [from] [to] [type]\n");
		CG_Printf("Deletes a Path between 2 Waypoints. Takes 3 parameters\n\n");
		CG_Printf("[from]            : Index of first Waypoint\n");
		CG_Printf("[to]              : Index of second Waypoint\n");
		CG_Printf("[type]            : 1 to delete a single Path\n");
		CG_Printf("                  : 2 to delete Paths in both directions\n");
		CG_Printf("------------------------\n");
		return;
	}

	trap_Argv( 1, c_from, sizeof(c_from) );
	trap_Argv( 2, c_to, sizeof(c_to) );
	trap_Argv( 3, c_type, sizeof(c_type) );

	index_from	= atoi(c_from);
	index_to	= atoi(c_to);

	if( index_from < 0 || index_from > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint index\n");
		return;
	}

	if( index_to < 0 || index_to > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint index\n");
		return;
	}

	if( !Q_stricmp( c_type, "1" ) )
	{
		bOneWay = TRUE;
		CG_Printf("Deleting Single Path\n");
	}
	else if( !Q_stricmp( c_type, "2" ) )
	{
		bOneWay = FALSE;
		CG_Printf("Deleting Paths in both directions\n");
	}
	else
	{
		CG_Printf("Invalid type passed\n");
		return;
	}

	from	= &Waypoints[index_from];
	to		= &Waypoints[index_to];

	// versuche Path zu löschen
	for( i = 0; i < from->numPaths; i++ )
	{
		if( from->paths[i].indexTarget == to->index )
		{
			memmove( &from->paths[i], &from->paths[ i + 1 ], (from->numPaths - i) * sizeof(WPPATH) );
					
			from->numPaths--;
			from->paths = (WPPATH*) realloc( from->paths, from->numPaths * sizeof(WPPATH) );

			CG_Printf("Path disconnected\n");
			break;
		}
	}
	
	if( i == from->numPaths + 1 )
		CG_Printf("Could not find a Path from %i to %i\n", index_from, index_to );

	if( bOneWay == FALSE )
	{
		for( i = 0; i < to->numPaths; i++ )
		{
			if( to->paths[i].indexTarget == from->index )
			{
				memmove( &to->paths[i], &to->paths[ i + 1 ], (to->numPaths - i) * sizeof(WPPATH) );

				to->numPaths--;
				to->paths = (WPPATH*) realloc( to->paths, to->numPaths * sizeof(WPPATH) );

				CG_Printf("Path disconnected\n");
				break;
			}
		}

		if( i == to->numPaths + 1 )
			CG_Printf("Could not find a Path from %i to %i\n", index_to, index_from );
	}
}

//
// Sets Flags for a Path between 2 Waypoints
//

void Cmd_WP_SetPathFlags( void )
{
	char c_from[4], c_to[4], szFlags[128];
	int index_from, index_to, bitflags;
	int i;
	WAYPOINT *from, *to;

	if( trap_Argc() < 4 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_setpathflags   [from] [to] [flags]\n");
		CG_Printf("Sets flags for a Path between 2 Waypoints. Takes 3 parameters\n\n");
		CG_Printf("[from]            : Index of first Waypoint\n");
		CG_Printf("[to]              : Index of second Waypoint\n");
		CG_Printf("[flags]           : See wp_dumpflags for a List of Flags\n");
		CG_Printf("------------------------\n");
		return;
	}

	trap_Argv( 1, c_from, sizeof(c_from) );
	trap_Argv( 2, c_to, sizeof(c_to) );
	trap_Argv( 3, szFlags, sizeof(szFlags) );

	index_from = atoi(c_from);
	index_to = atoi(c_to);

	bitflags = atoi(szFlags);

	from = &Waypoints[ index_from ];
	to = &Waypoints[ index_to ];

	if( index_from < 0 || index_from > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint Index\n");
		return;
	}

	if( index_to < 0 || index_to > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint Index\n");
		return;
	}

	// try to find the path
	for( i = 0; i < from->numPaths; i++ )
	{
		if( from->paths[i].indexTarget == to->index )
		{
			from->paths[i].flags |= bitflags;
			CG_Printf("Path flags set\n");
			return;
		}
	}

	CG_Printf("Could not find a Path from Waypoint %i to %i\n", index_from, index_to );
}

//
// Sets the display mode for Waypoint editing
//

void Cmd_WP_Rendermode( void )
{
	char rendermode[4];

	if( trap_Argc() < 2 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_rendermode     [mode]\n");
		CG_Printf("Sets the render mode for editing\n\n");
		CG_Printf("[mode]            : 1 to display Waypoints but no Paths\n");
		CG_Printf("                  : 2 to display Waypoints and Paths (default)\n");
		CG_Printf("                  : 3 to display Waypoints and Paths and Arrows\n");
		CG_Printf("------------------------\n");
		return;
	}

	trap_Argv( 1, rendermode, sizeof(rendermode) );

	switch( atoi(rendermode) )
	{
		case 1:
			bRenderWaypoints = TRUE;
			bRenderPaths = FALSE;
			bRenderArrows = FALSE;
			break;

		case 2:
			bRenderWaypoints = TRUE;
			bRenderPaths = TRUE;
			bRenderArrows = FALSE;
			break;

		case 3:
			bRenderWaypoints = TRUE;
			bRenderPaths = TRUE;
			bRenderArrows = TRUE; // for one way paths
			break;

		default:
			bRenderWaypoints = FALSE;
			bRenderPaths = FALSE;
			bRenderArrows = FALSE;
			break;
	}
}

//
// Deletes all Waypoints and frees Path memory
//

void Cmd_WP_Cleanup( void )
{
	int i;

	if( trap_Argc() > 1 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_cleanup        [none]\n");
		CG_Printf("Deletes all Waypoints and Paths\n\n");
		CG_Printf("------------------------\n");
		return;
	}

	// memory aufräumen
	for( i = 0; i < Waypoint_index; i++ )
	{
		if( Waypoints[i].numPaths > 0 )
		{
			CG_Printf("Waypoint (%i) has %i paths\n", Waypoints[i].index, Waypoints[i].numPaths );
			free(Waypoints[i].paths);
		}

		Waypoints[i].index			= -1;
		Waypoints[i].numPaths		= 0;
	}

	Waypoint_index = 0;
	bRenderWaypoints = FALSE;

	CG_Printf("Cmd_WP_Cleanup OK\n");
}

//
// Finds a Path between 2 Waypoints
//

void Cmd_WP_TestPath( void )
{
	char c_from[4], c_to[4];
	PATHNODE *Start, *Goal, *Path, *p, *q;
	localEntity_t *le;

	if( trap_Argc() < 3 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_testpath       [from] [to]\n");
		CG_Printf("Finds and displays a Path between 2 Waypoints\n\n");
		CG_Printf("[from]            : Index of first Waypoint\n");
		CG_Printf("[to]              : Index of second Waypoint\n");
		CG_Printf("------------------------\n");
		return;
	}

	trap_Argv( 1, c_from, sizeof(c_from) );
	trap_Argv( 2, c_to, sizeof(c_to) );

	if( atoi(c_from) < 0 || atoi(c_from) > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint Index\n");
		return;
	}

	if( atoi(c_to) < 0 || atoi(c_to) > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint Index\n");
		return;
	}

	Start = (PATHNODE*) malloc( sizeof(PATHNODE) );
	Goal = (PATHNODE*) malloc( sizeof(PATHNODE) );

	memory += sizeof(PATHNODE);
	memory += sizeof(PATHNODE);

	Start->f		= 0;
	Start->g		= 0;
	Start->h		= 0;
	Start->NextNode = NULL;
	Start->parent	= NULL;
	Start->prev		= 0;
	Start->wpIndex	= atoi(c_from);

	Goal->wpIndex	= atoi(c_to);

	CG_Printf("Looking for Path from %i to %i\n", atoi(c_from), atoi(c_to) );

	Path = AStarSearch( Start, Goal );
	
	free(Goal);
	memory -= sizeof(PATHNODE);

	if( !Path )
	{
		CG_Printf("Could not find a path from %s to %s\n", c_from, c_to);
//		free(Start); is already freed in AStarSearch()
		return;
	}

	p = Path;

	while( p != NULL )
	{
		q = (PATHNODE*) p->NextNode;

		if( q )
		{
			le = CG_AllocLocalEntity();

			le->leType		= LE_FADE_RGB;
			le->startTime	= cg.time;
			le->endTime		= cg.time + 15000;
			le->lifeRate	= 1.0 / (le->endTime - le->startTime);
			
			le->refEntity.shaderTime	= cg.time / 1000.0f;
			le->refEntity.reType		= RT_RAIL_CORE;
			le->refEntity.customShader	= cgs.media.railCoreShader;

			VectorCopy( Waypoints[p->wpIndex].origin, le->refEntity.origin );
			VectorCopy( Waypoints[q->wpIndex].origin, le->refEntity.oldorigin );

			le->refEntity.shaderRGBA[0] = 255;
			le->refEntity.shaderRGBA[1] = 0;
			le->refEntity.shaderRGBA[2] = 0;
			le->refEntity.shaderRGBA[3] = 255;

			le->color[0] = 1.0f;
			le->color[1] = 0.0f;
			le->color[2] = 0.0f;
			le->color[3] = 1.0f;

			AxisClear( le->refEntity.axis );
		}

		p = (PATHNODE*) p->NextNode;
	}

	while( Path != NULL )
	{
		p = (PATHNODE*) Path->NextNode;
		free(Path);
		memory -= sizeof(PATHNODE);
		Path = p;
	}

	CG_Printf("Nicht freigegebener Speicher : %i\n", memory);
}

//
// Lists all Waypoint and Path flags with short description
//

void Cmd_WP_DumpFlags( void )
{
	CG_Printf("Waypoint Flags :\n\n");

	CG_Printf("Flag         : %i (CAMP)\n", FL_WP_CAMP);
	CG_Printf("Description  : Marks Waypoint as a good Camp Spot for the Bot.\n\n");

	CG_Printf("Flag         : %i (DOOR)\n", FL_WP_DOOR);
	CG_Printf("Description  : Should be set if Waypoint is near to a door entity.\n\n");

	CG_Printf("Flag         : %i (MG42)\n", FL_WP_MG42);
	CG_Printf("Description  : Should be set if Waypoint is near a MG42 entity.\n\n");

	CG_Printf("Flag         : %i (Axis Docs Deliver)\n", FL_WP_AXIS_DOCS_DELIVER);
	CG_Printf("Description  : Bot will head towards this waypoint when having the allies' docs.\n\n");

	CG_Printf("Flag         : %i (Axis Docs Deliver)\n", FL_WP_ALLIES_DOCS_DELIVER);
	CG_Printf("Description  : Bot will head towards this waypoint when having the axis' docs.\n\n");
	
	CG_Printf("Path Flags :\n\n");

	CG_Printf("Flag         : %i (DUCK)\n", FL_PATH_DUCK);
	CG_Printf("Description  : Bot will duck when moving on a Path with this flag.\n\n");

	CG_Printf("Flag         : %i (JUMP)\n", FL_PATH_JUMP);
	CG_Printf("Description  : Bot will jump when moving on a Path with this flag.\n\n");

	CG_Printf("Flag         : %i (JUMP)\n", FL_PATH_WALK);
	CG_Printf("Description  : Bot will walk when moving on a Path with this flag.\n\n");

	CG_Printf("Flag         : %i (BLOCKED)\n", FL_PATH_BLOCKED);
	CG_Printf("Description  : Refer to the Map Manual.\n\n");

	CG_Printf("Flag         : %i (LADDER)\n", FL_PATH_LADDER);
	CG_Printf("               Ladder Paths need this flag.\n\n");
}

//
// Displays box between too Waypoints (first Waypoint mins , second Waypoint maxs)
//

void Cmd_WP_PathBox( void )
{
	int iIndexOne, iIndexTwo;
	char szBuf[256];

	if( trap_Argc() < 3 )
	{
		CG_Printf("------------------------\n");
		CG_Printf("wp_pathbox        [from] [to]\n");
		CG_Printf("Displays bounding box between 2 waypoints\n\n");
		CG_Printf("[from]            : Index of first Waypoint\n");
		CG_Printf("[to]              : Index of second Waypoint\n");
		CG_Printf("------------------------\n");
		return;
	}

	trap_Argv( 1, szBuf, sizeof(szBuf) );
	iIndexOne = atoi(szBuf);

	trap_Argv( 2, szBuf, sizeof(szBuf) );
	iIndexTwo = atoi(szBuf);

	if( iIndexOne < 0 || iIndexOne > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint Index\n");
		return;
	}

	if( iIndexTwo < 0 || iIndexTwo > Waypoint_index )
	{
		CG_Printf("Invalid Waypoint Index\n");
		return;
	}

	CG_RailTrail( NULL, Waypoints[ iIndexOne ].origin, Waypoints[ iIndexTwo ].origin, 1 );
}

//
// Displays Waypoint's Index using refEntities
//

void CG_RenderWaypoint( WAYPOINT Waypoint )
{
	refEntity_t one, two, three;

	memset( &one, 0, sizeof(one) );
	memset( &two, 0, sizeof(two) );
	memset( &three, 0, sizeof(three) );

	// render objective waypoints rot
	if( Waypoint.flags & FL_WP_OBJECTIVE )
	{
		one.shaderRGBA[0] = two.shaderRGBA[0] = three.shaderRGBA[0] = 255;
		one.shaderRGBA[1] = two.shaderRGBA[1] = three.shaderRGBA[1] = 0;
		one.shaderRGBA[2] = two.shaderRGBA[2] = three.shaderRGBA[2] = 0;
		one.shaderRGBA[3] = two.shaderRGBA[3] = three.shaderRGBA[3] = 255;
	}
	else if( Waypoint.index == NearestIndex )
	{
		one.shaderRGBA[0] = two.shaderRGBA[0] = three.shaderRGBA[0] = 0;
		one.shaderRGBA[1] = two.shaderRGBA[1] = three.shaderRGBA[1] = 255;
		one.shaderRGBA[2] = two.shaderRGBA[2] = three.shaderRGBA[2] = 0;
		one.shaderRGBA[3] = two.shaderRGBA[3] = three.shaderRGBA[3] = 255;
	}
	else
	{
		memset( one.shaderRGBA, 255, sizeof(one.shaderRGBA) );
		memset( two.shaderRGBA, 255, sizeof(two.shaderRGBA) );
		memset( three.shaderRGBA, 255, sizeof(three.shaderRGBA) );
	}

	if( Waypoint.index >= 100 )
	{
		char index[4]; int test, test2, test3;

		one.reType = two.reType = three.reType = RT_SPRITE;
		one.radius = two.radius = three.radius = 4;

		Com_sprintf(index, sizeof(index), "%i", Waypoint.index);

		test = atoi(&index[2]);
		index[2] = '\0';
		test2 = atoi(&index[1]);
		index[1] = '\0';
		test3 = atoi(index);

		one.customShader	= cgs.media.numberShaders[ test ];
		two.customShader	= cgs.media.numberShaders[ test2 ];
		three.customShader	= cgs.media.numberShaders[ test3 ];

		VectorCopy( Waypoint.origin, two.origin );
		VectorMA( two.origin,-6, cg.refdef.viewaxis[1], one.origin );
		VectorMA( two.origin, 6, cg.refdef.viewaxis[1], three.origin );

		one.origin[2] += 20;
		two.origin[2] += 20;
		three.origin[2] += 20;

		trap_R_AddRefEntityToScene(&one);
		trap_R_AddRefEntityToScene(&two);
		trap_R_AddRefEntityToScene(&three);
	}
	else if( Waypoint.index >= 10 )
	{
		char index[4]; int test, test2;

		one.reType = two.reType = RT_SPRITE;
		one.radius = two.radius = 4;

		Com_sprintf(index, sizeof(index), "%i", Waypoint.index);

		test = atoi(&index[1]);
		index[1] = '\0';
		test2 = atoi(index);
	
		one.customShader	= cgs.media.numberShaders[ test ];
		two.customShader	= cgs.media.numberShaders[ test2 ];
		
		VectorMA( Waypoint.origin, 4, cg.refdef.viewaxis[1], two.origin );
		VectorMA( Waypoint.origin,-4, cg.refdef.viewaxis[1], one.origin );

		one.origin[2] += 20;
		two.origin[2] += 20;

		trap_R_AddRefEntityToScene(&one);
		trap_R_AddRefEntityToScene(&two);
	}
	else
	{

		one.reType			= RT_SPRITE;
		one.radius			= 4;
		one.customShader	= cgs.media.numberShaders[ Waypoint.index ];
		
		VectorCopy( Waypoint.origin, one.origin );

		one.origin[2] += 20;

		trap_R_AddRefEntityToScene(&one);
		
		return;
	}

}

void CG_AddWaypoints( void )
{
	int i, c;
	vec3_t tmp;

	if( !bRenderWaypoints )
		return;

	CG_CrosshairBModelBBox( ET_EXPLOSIVE, &iExplosiveNum );

	for( i = 0; i < Waypoint_index; i++ )
	{
		VectorSubtract( Waypoints[i].origin, cg.snap->ps.origin, tmp );

		if( VectorLength(tmp) > 1000 )
			continue;

		CG_RenderWaypoint(Waypoints[i]);

		// Paths rendern
		if( bRenderPaths )
		{
			for( c = 0; c < Waypoints[i].numPaths; c++ )
			{
				localEntity_t *path = CG_AllocLocalEntity();

				path->leType	= LE_FADE_RGB;
				path->startTime	= cg.time;
				path->endTime	= cg.time + 1;
				path->lifeRate	= 1.0 / ( path->endTime - path->startTime );
				
				path->refEntity.shaderTime = cg.time / 1000.0f;
				path->refEntity.reType = RT_RAIL_CORE;
				path->refEntity.customShader = cgs.media.railCoreShader;

				VectorCopy( Waypoints[i].origin, path->refEntity.origin );
				VectorCopy( Waypoints[ Waypoints[i].paths[c].indexTarget ].origin, path->refEntity.oldorigin );

				AxisClear( path->refEntity.axis );

				path->color[0] = 1.0f;
				path->color[1] = 1.0f;
				path->color[2] = 1.0f;
				path->color[3] = 1.0f;

				// Jump Paths rot
				if( Waypoints[i].paths[c].flags & FL_PATH_JUMP )
				{
					path->color[0] = 1.0f;
					path->color[1] = 0;
					path->color[2] = 0;
				}
				// duck Paths blau
				else if( Waypoints[i].paths[c].flags & FL_PATH_DUCK )
				{
					path->color[0] = 0;
					path->color[1] = 0;
					path->color[2] = 1.0f;
				}
				// blocked Paths grün
				else if( Waypoints[i].paths[c].flags & FL_PATH_BLOCKED )
				{
					path->color[0] = 0;
					path->color[1] = 1.0f;
					path->color[2] = 0;
				}
				// walk paths gelb
				else if( Waypoints[i].paths[c].flags & FL_PATH_WALK )
				{
					path->color[0] = (int) 255.0f / 255.0f;
					path->color[1] = (int) 241.0f / 255.0f;
					path->color[2] = (int) 17 / 255.0f;
				}
				// ladder paths orange
				else if( Waypoints[i].paths[c].flags & FL_PATH_LADDER )
				{
					path->color[0] = (int) 250 / 255.0f;
					path->color[1] = (int) 135 / 255.0f;
					path->color[2] = (int) 0 / 255.0f;
				}

				// WolfBot 1.5 : Check if this is a one way path. If so render it as an
				// arrow indicating the direction
				if( bRenderArrows )
				{
					int y;
					BOOL bOneWay = TRUE;

					for( y = 0; y < Waypoints[ Waypoints[i].paths[c].indexTarget ].numPaths; y++ )
					{
						if( Waypoints[ Waypoints[i].paths[c].indexTarget ].paths[y].indexTarget == Waypoints[i].index )
						{
							bOneWay = FALSE;
							break;
						}
					}

					// this isn't implemented very well
					if( bOneWay == TRUE )
					{
						vec3_t vDir, vRight, vForward, vAngles;
						vec3_t vLeftPoint, vRightPoint;
						localEntity_t *pLeftPoint, *pRightPoint;

						// get a vector pointing from the target to the start
						VectorSubtract( Waypoints[i].origin, Waypoints[ Waypoints[i].paths[c].indexTarget ].origin, vDir );
						VectorNormalizeFast(vDir);

						vectoangles( vDir, vAngles );
						AngleVectors( vAngles, vForward, vRight, NULL );

						VectorMA( Waypoints[ Waypoints[i].paths[c].indexTarget ].origin, 25, vRight, vLeftPoint );
						VectorMA( Waypoints[ Waypoints[i].paths[c].indexTarget ].origin, -25, vRight, vRightPoint );

						VectorMA( vLeftPoint, 25, vForward, vLeftPoint );
						VectorMA( vRightPoint, 25, vForward, vRightPoint );

						pLeftPoint = CG_AllocLocalEntity();

						pLeftPoint->leType	= LE_FADE_RGB;
						pLeftPoint->startTime	= cg.time;
						pLeftPoint->endTime	= cg.time + 1;
						pLeftPoint->lifeRate	= 1.0 / ( pLeftPoint->endTime - pLeftPoint->startTime );
				
						pLeftPoint->refEntity.shaderTime = cg.time / 1000.0f;
						pLeftPoint->refEntity.reType = RT_RAIL_CORE;
						pLeftPoint->refEntity.customShader = cgs.media.railCoreShader;

						VectorCopy( Waypoints[ Waypoints[i].paths[c].indexTarget ].origin, pLeftPoint->refEntity.origin );
						VectorCopy( vLeftPoint, pLeftPoint->refEntity.oldorigin );

						AxisClear( pLeftPoint->refEntity.axis );

						pLeftPoint->color[0] = 1.0f;
						pLeftPoint->color[1] = 0.0f;
						pLeftPoint->color[2] = 1.0f;
						pLeftPoint->color[3] = 1.0f;

						pRightPoint = CG_AllocLocalEntity();

						pRightPoint->leType	= LE_FADE_RGB;
						pRightPoint->startTime	= cg.time;
						pRightPoint->endTime	= cg.time + 1;
						pRightPoint->lifeRate	= 1.0 / ( pRightPoint->endTime - pRightPoint->startTime );
				
						pRightPoint->refEntity.shaderTime = cg.time / 1000.0f;
						pRightPoint->refEntity.reType = RT_RAIL_CORE;
						pRightPoint->refEntity.customShader = cgs.media.railCoreShader;

						VectorCopy( Waypoints[ Waypoints[i].paths[c].indexTarget ].origin, pRightPoint->refEntity.origin );
						VectorCopy( vRightPoint, pRightPoint->refEntity.oldorigin );

						AxisClear( pRightPoint->refEntity.axis );

						pRightPoint->color[0] = 1.0f;
						pRightPoint->color[1] = 0.0f;
						pRightPoint->color[2] = 1.0f;
						pRightPoint->color[3] = 1.0f;

					}
				}
			}
		}
	}
}

//
// Pathfinding 
//

// Thanks to Count Floyd from PODBot
PATHNODE *AStarSearch( PATHNODE *Start, PATHNODE *Goal )
{
	PATHNODE *OpenList		= NULL;
	PATHNODE *ClosedList	= NULL;
	PATHNODE *Current;
	PATHNODE *ChildList;
	PATHNODE *CurChild;
	PATHNODE *Path;
	PATHNODE *p, *q;

	Start->NextNode = NULL;
	Start->prev		= NULL;

	OpenList = Start;

	while( OpenList != NULL )
	{
		Current = OpenList;
		OpenList = (PATHNODE*) OpenList->NextNode;

		if(OpenList != NULL)
			OpenList->prev = NULL;

		// Pfad gefunden
		if( IsGoalNode( Current, Goal ) )
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
				memory -= sizeof(PATHNODE);
				OpenList = p;
			}

			// ClosedList aufräumen
			while( ClosedList != NULL )
			{
				p = (PATHNODE*) ClosedList->NextNode;
				free(ClosedList);
				memory -= sizeof(PATHNODE);
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
			CurChild->h			= GoalDistanceEstimate( CurChild, Goal );
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
							memory -= sizeof(PATHNODE);
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
							memory -= sizeof(PATHNODE);
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
							memory -= sizeof(PATHNODE);
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

							// bugfix -> count floyd forgot to actually delete the node
							free(p);
							memory -= sizeof(PATHNODE);

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
		memory -= sizeof(PATHNODE);
		ClosedList = p;
	}

	return NULL;
}

BOOL IsGoalNode( PATHNODE *Compare, PATHNODE *Goal )
{
	if( Compare->wpIndex == Goal->wpIndex )
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

		memory += sizeof(PATHNODE);
		
		p->wpIndex	= wp->paths[i].indexTarget;
		p->parent	= Parent;
		p->NextNode	= q;

		// g berechnen (Distanz zwischen child node und parent node)
		p->g = Parent->g + wp->paths[i].length;

		q = p;
	}

	return q;
}

float GoalDistanceEstimate( PATHNODE *Current, PATHNODE *Goal )
{
	vec3_t tmp;

	VectorSubtract( Waypoints[Goal->wpIndex].origin, Waypoints[Current->wpIndex].origin, tmp );

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

//
// Waypoint Menu Stuff
//

//
// Outlines Bounding Box of brush model in crosshair
//

void CG_CrosshairBModelBBox( int eType, int *pEntNum )
{
	trace_t			tr;
	vec3_t			start, end;
	vec3_t			mins, maxs;
	clipHandle_t	cModel;
	vec3_t			diff, v1, v2, v3, v4, v5, v6;
	localEntity_t	*le; 
	
	// there was some problem with using ref ents directly so use le system for now

	VectorCopy( cg.refdef.vieworg, start );
	VectorMA( start, 800, cg.refdef.viewaxis[0], end );

	// WolfBot 1.5 : added playerclip contents check
	CG_Trace( &tr, start, NULL, NULL, end, cg.snap->ps.clientNum, MASK_SOLID|CONTENTS_PLAYERCLIP );

	if( (eType != 1) && (cg_entities[tr.entityNum].currentState.eType != eType) )
	{
		if( pEntNum )
			*pEntNum = -1;
	
		return;
	}

	if( pEntNum )
		*pEntNum = cg_entities[tr.entityNum].currentState.number;

	cModel = cgs.inlineDrawModel[ cg_entities[tr.entityNum].currentState.modelindex ];

	trap_R_ModelBounds( cModel, mins, maxs );

	VectorSubtract( maxs, mins, diff );

	// 1
	le = CG_AllocLocalEntity();

	VectorCopy( maxs, v1 );
	v1[0] -= diff[0];

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( maxs, le->refEntity.origin );
	VectorCopy( v1, le->refEntity.oldorigin );

	// 2
	le = CG_AllocLocalEntity();

	VectorCopy( maxs, v2 );
	v2[1] -= diff[1];

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( maxs, le->refEntity.origin );
	VectorCopy( v2, le->refEntity.oldorigin );

	// 3
	le = CG_AllocLocalEntity();

	VectorCopy( maxs, v3 );
	v3[2] -= diff[2];

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( maxs, le->refEntity.origin );
	VectorCopy( v3, le->refEntity.oldorigin );

	// 4
	le = CG_AllocLocalEntity();

	VectorCopy( mins, v4 );
	v4[0] += diff[0];

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( mins, le->refEntity.origin );
	VectorCopy( v4, le->refEntity.oldorigin );

	// 5
	le = CG_AllocLocalEntity();

	VectorCopy( mins, v5 );
	v5[1] += diff[1];

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( mins, le->refEntity.origin );
	VectorCopy( v5, le->refEntity.oldorigin );

	// 6
	le = CG_AllocLocalEntity();

	VectorCopy( mins, v6 );
	v6[2] += diff[2];

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( mins, le->refEntity.origin );
	VectorCopy( v6, le->refEntity.oldorigin );

	// build other lines

	le = CG_AllocLocalEntity();

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( v2, le->refEntity.origin );
	VectorCopy( v6, le->refEntity.oldorigin );

	le = CG_AllocLocalEntity();

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( v6, le->refEntity.origin );
	VectorCopy( v1, le->refEntity.oldorigin );

	le = CG_AllocLocalEntity();

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( v1, le->refEntity.origin );
	VectorCopy( v5, le->refEntity.oldorigin );

	le = CG_AllocLocalEntity();

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( v2, le->refEntity.origin );
	VectorCopy( v4, le->refEntity.oldorigin );

	le = CG_AllocLocalEntity();

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( v4, le->refEntity.origin );
	VectorCopy( v3, le->refEntity.oldorigin );

	le = CG_AllocLocalEntity();

	le->leType		= LE_FADE_RGB;
	le->startTime	= cg.time;
	le->endTime		= cg.time + 1;
	le->lifeRate	= 1.0 / ( le->endTime - le->startTime );
	le->color[0]	= 1.0f;
	le->color[1]	= 0.0f;
	le->color[2]	= 0.0f;
	le->color[3]	= 1.0f;

	le->refEntity.shaderTime	= cg.time / 1000.0f;
	le->refEntity.reType		= RT_RAIL_CORE;
	le->refEntity.customShader	= cgs.media.railCoreShader;

	AxisClear( le->refEntity.axis );

	VectorCopy( v3, le->refEntity.origin );
	VectorCopy( v5, le->refEntity.oldorigin );
}

//
// Displays a Menu on the right with Information about the nearest Waypoint
//

void CG_DrawWaypointInfo( void )
{
	int i, y;
	vec3_t vDist;
	float fLength;
	float fMin;
	WAYPOINT *pWaypoint;
	char paths[128];
	char *str;
	vec4_t vColorBack = { 0, 0, 0.15f, 0.5f };

	fMin = 99999;
	NearestIndex = -1;

	if( !bRenderWaypoints )
		return;

	for( i = 0; i < Waypoint_index; i++ )
	{
		VectorSubtract( Waypoints[i].origin, cg.snap->ps.origin, vDist );
		fLength = VectorLength(vDist);

		if( fLength < fMin )
		{
			fMin = fLength;
			NearestIndex = i;
		}
	}

	if( NearestIndex != -1 )
	{
		pWaypoint = &Waypoints[NearestIndex];

		CG_DrawRect( 300, 100, 300, 250, 1 , colorMdGrey );
		CG_FillRect( 301, 101, 298, 248, vColorBack );

		if( pWaypoint->flags & FL_WP_OBJECTIVE )
			CG_DrawStringExt( 310, 110, va("Waypoint %i Objective", NearestIndex), colorRed, qtrue, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		else
			CG_DrawStringExt( 310, 110, va("Waypoint %i", NearestIndex), NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		
		CG_DrawStringExt( 310, 130, "Flags :", NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

		y = 145;

		if( pWaypoint->flags & FL_WP_CAMP )
		{
			CG_DrawStringExt( 310, y, "FL_WP_CAMP", NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;
		}

		if( pWaypoint->flags & FL_WP_AXIS_UNREACHABLE )
		{
			// kotz
			if( pWaypoint->wpdata.entNum4 > 0 )
			{
				str = va( "AXIS_UNREACHABLE %i ,%i ,%i ,%i", pWaypoint->wpdata.entNum1, pWaypoint->wpdata.entNum2, pWaypoint->wpdata.entNum3, pWaypoint->wpdata.entNum4 );
			}
			else if( pWaypoint->wpdata.entNum3 > 0 )
			{
				str = va( "AXIS_UNREACHABLE %i ,%i ,%i", pWaypoint->wpdata.entNum1, pWaypoint->wpdata.entNum2, pWaypoint->wpdata.entNum3 );
			}
			else if( pWaypoint->wpdata.entNum2 > 0 )
			{
				str = va( "AXIS_UNREACHABLE %i ,%i", pWaypoint->wpdata.entNum1, pWaypoint->wpdata.entNum2 );
			}
			else
			{
				str = va( "AXIS_UNREACHABLE %i", pWaypoint->wpdata.entNum1 );
			}

			CG_DrawStringExt( 310, y, str, NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;
		}

		if( pWaypoint->flags & FL_WP_ALLIES_UNREACHABLE )
		{
			// kotz
			if( pWaypoint->wpdata.entNum4 > 0 )
			{
				str = va( "ALLIES_UNREACHABLE %i ,%i ,%i ,%i", pWaypoint->wpdata.entNum1, pWaypoint->wpdata.entNum2, pWaypoint->wpdata.entNum3, pWaypoint->wpdata.entNum4 );
			}
			else if( pWaypoint->wpdata.entNum3 > 0 )
			{
				str = va( "ALLIES_UNREACHABLE %i ,%i ,%i", pWaypoint->wpdata.entNum1, pWaypoint->wpdata.entNum2, pWaypoint->wpdata.entNum3 );
			}
			else if( pWaypoint->wpdata.entNum2 > 0 )
			{
				str = va( "ALLIES_UNREACHABLE %i ,%i", pWaypoint->wpdata.entNum1, pWaypoint->wpdata.entNum2 );
			}
			else
			{
				str = va( "ALLIES_UNREACHABLE %i", pWaypoint->wpdata.entNum1 );
			}

			CG_DrawStringExt( 310, y, str, NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;
		}

		if( pWaypoint->flags & FL_WP_DOOR )
		{
			CG_DrawStringExt( 310, y, va("FL_WP_DOOR (Target %i)", pWaypoint->wpdata.btnIndex), NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;
		}

		if( pWaypoint->flags & FL_WP_OBJECTIVE )
		{
			CG_DrawStringExt( 310, y, "FL_WP_OBJECTIVE", NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;
		}

		if( pWaypoint->flags & FL_WP_MG42 )
		{
			CG_DrawStringExt( 310, y, "FL_WP_MG42", NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;
		}

		if( pWaypoint->flags & FL_WP_AXIS_DOCS_DELIVER )
		{
			CG_DrawStringExt( 310, y, "FL_WP_AXIS_DOCS_DELIVER", NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;
		}

		if( pWaypoint->flags & FL_WP_ALLIES_DOCS_DELIVER )
		{
			CG_DrawStringExt( 310, y, "FL_WP_ALLIES_DOCS_DELIVER", NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;
		}

		strcpy( paths, "Paths to : ");

		for( y = 0; y < pWaypoint->numPaths; y++ )
			strcat( paths, va("%i,", pWaypoint->paths[y].indexTarget) );

		CG_DrawStringExt( 310, 210, paths, NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

		if( pWaypoint->flags & FL_WP_OBJECTIVE )
		{
			int iObjTeam;
			int iObjType;
			int iObjPriority;
			int iObjTarget;
			int iObjNum;

			iObjTeam = pWaypoint->objdata.team;
			iObjType = pWaypoint->objdata.type;
			iObjPriority = pWaypoint->objdata.prior;
			iObjNum = pWaypoint->objdata.objNum;

			CG_DrawStringExt( 310, 240, "Objective Info :", NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

			if( iObjTeam == TEAM_AXIS )
				CG_DrawStringExt( 310, 255, "Team : 1 (Axis)", NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else if( iObjTeam == TEAM_ALLIES )
				CG_DrawStringExt( 310, 255, "Team : 2 (Allies)", NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

			if( iObjType == OBJECTIVE_BOMB )
				CG_DrawStringExt( 310, 265, va("Type : %i (Bomb)", iObjType), NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else if( iObjType == OBJECTIVE_STEAL )
				CG_DrawStringExt( 310, 265, va("Type : %i (Steal)", iObjType), NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else if( iObjType == OBJECTIVE_CAPTURE )
				CG_DrawStringExt( 310, 265, va("Type : %i (Capture)", iObjType), NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

			if( iObjPriority == PRIORITY_HIGH )
				CG_DrawStringExt( 310, 275, va("Priority : %i (High)", PRIORITY_HIGH), NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else if( iObjPriority == PRIORITY_LOW )
				CG_DrawStringExt( 310, 275, va("Priority : %i (Low)", PRIORITY_LOW), NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

			CG_DrawStringExt( 310, 285, va("Linked Obj. : %i", iObjNum), NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

			if( iObjType == OBJECTIVE_BOMB )
			{
				iObjTarget = pWaypoint->objdata.entNum;
				CG_DrawStringExt( 310, 295, va("func_explosive : %i", iObjTarget), NULL, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			}
		}
	}
}

//
// called to display waypoint placement menu
//

void Cmd_WP_Menu( void )
{
	iMenuState = WP_MENU_MAIN;
}

//
// displays the actual menu
//

void CG_DrawWaypointMenu( void )
{
	int y;
	const char *info;
	char *mapname;
	vec4_t vColorBack = { 0, 0, 0.15f, 0.5f };

	if( iMenuState == WP_MENU_NONE )
		return;

	// outline box never changes
	CG_DrawRect( 60, 100, 160, 250, 1 , colorMdGrey );
	CG_FillRect( 61, 101, 158, 248, vColorBack );

	CG_DrawStringExt( TEXT_X_OFFSET, 110, "Waypoint Menu", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

	y = 130;

	switch( iMenuState )
	{
		case WP_MENU_MAIN:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Place Waypoint", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Place Objective", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Connect", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "4. Disconnect", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "5. Set Flag", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "6. Reset Flags", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "7. Delete Waypoint", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10,

			CG_DrawStringExt( TEXT_X_OFFSET, y, "8. Save File", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10,

			// WolfBot 1.5
			CG_DrawStringExt( TEXT_X_OFFSET, y, "9. Drag Waypoint", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "0. Close Menu", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_OBJ_TYPE:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Objective Type", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Bomb", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Steal", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Capture", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "4. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_OBJ_TEAM:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Objective Team", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Axis", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Allies", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_OBJ_PRIORITY:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Objective Rank", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. High", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Low", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_OBJ_TARGET:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Objective Target", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, va("func_explosive  %i", iExplosiveNum), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. OK", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_OBJ_NUMBER:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Objective Number", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. One", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Two", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Three", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "4. Four", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "5. Five", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "6. Six", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "7. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_CONNECT:

			CG_DrawStringExt( TEXT_X_OFFSET, y, va("Connecting %i", iMenuPathFrom), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, va("To %i", NearestIndex), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. OK", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_CONNECT_TYPE:

			CG_DrawStringExt( TEXT_X_OFFSET, y, va("Path from %i to %i", iMenuPathFrom, iMenuPathTo), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Both Way Path", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. One Way Path", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_DISCONNECT:

			CG_DrawStringExt( TEXT_X_OFFSET, y, va("Disconnecting %i", iMenuPathFrom), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, va("From %i", NearestIndex), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. OK", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_DISCONNECT_TYPE:

			CG_DrawStringExt( TEXT_X_OFFSET, y, va("Path from %i to %i", iMenuPathFrom, iMenuPathTo), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Both Way Path", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. One Way Path", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_SET_WAYPOINT_FLAGS:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Set Flags", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Waypoint Flags", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			if( iMenuWpFlags & FL_WP_CAMP )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Camp", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Camp", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			if( iMenuWpFlags & FL_WP_MG42 )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "2. MG42", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "2. MG42", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			if( iMenuWpFlags & FL_WP_DOOR )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Door", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Door", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			if( iMenuWpFlags & FL_WP_AXIS_DOCS_DELIVER )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "4. Axis Deliver", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "4. Axis Deliver", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			if( iMenuWpFlags & FL_WP_ALLIES_DOCS_DELIVER )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "5. Allies Deliver", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "5. Allies Deliver", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			if( (iMenuWpFlags & FL_WP_AXIS_UNREACHABLE) || (iMenuWpFlags & FL_WP_ALLIES_UNREACHABLE) )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "6. Unreachable", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "6. Unreachable", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "7. Set Path Flags", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "8. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_SET_PATH_FLAGS:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Set Flags", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Path Flags", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			if( iMenuPtFlags & FL_PATH_JUMP )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Jump", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Jump", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			if( iMenuPtFlags & FL_PATH_DUCK )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Duck", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Duck", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			if( iMenuPtFlags & FL_PATH_BLOCKED )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Blocked", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Blocked", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			if( iMenuPtFlags & FL_PATH_WALK )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "4. Walk", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "4. Walk", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			if( iMenuPtFlags & FL_PATH_LADDER )
				CG_DrawStringExt( TEXT_X_OFFSET, y, "5. Ladder", colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, "5. Ladder", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "6. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_SET_FLAGS_UNREACHABLE_TEAM:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Team", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Axis", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Allies", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_SET_FLAGS_UNREACHABLE_ENTITIES:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Entity Num", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			if( iMenuWpUnreachIndex == 0 )
				CG_DrawStringExt( TEXT_X_OFFSET, y, va("One     : %i", iExplosiveNum), colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, va("One     : %i", iMenuWpUnreachEnts[0]), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

			y += 10;

			if( iMenuWpUnreachIndex == 1 )
				CG_DrawStringExt( TEXT_X_OFFSET, y, va("Two     : %i", iExplosiveNum), colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, va("Two     : %i", iMenuWpUnreachEnts[1]), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

			y += 10;

			if( iMenuWpUnreachIndex == 2 )
				CG_DrawStringExt( TEXT_X_OFFSET, y, va("Three   : %i", iExplosiveNum), colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, va("Three   : %i", iMenuWpUnreachEnts[2]), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

			y += 10;

			if( iMenuWpUnreachIndex == 3 )
				CG_DrawStringExt( TEXT_X_OFFSET, y, va("Four    : %i", iExplosiveNum), colorGreen, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			else
				CG_DrawStringExt( TEXT_X_OFFSET, y, va("Four    : %i", iMenuWpUnreachEnts[3]), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Set", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Done", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_SET_FLAGS_BUTTON_INDEX:

			CG_DrawStringExt( TEXT_X_OFFSET, y, va("Target %i", NearestIndex), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Set", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_SAVE:

			info = CG_ConfigString( CS_SERVERINFO );
			mapname = Info_ValueForKey( info, "mapname" );

			CG_DrawStringExt( TEXT_X_OFFSET, y, "Overwrite File ?", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Yes", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case WP_MENU_DRAG:

			CG_DrawStringExt( TEXT_X_OFFSET, y, va("Dragging %i", iWpDragIndex), NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Set", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Go Back", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			// this shouldn't be done here but what the hell
			if( iWpDragIndex != -1 )
			{
				VectorCopy( cg.predictedPlayerState.origin, Waypoints[iWpDragIndex].origin );
			}

			break;
	}
	
}

//
// Handles all Input to Menu
//

void CG_WP_MenuInput( int iKey )
{
	const char	*info = CG_ConfigString( CS_SERVERINFO );
	char *mapname = Info_ValueForKey( info, "mapname" );

	switch( iMenuState )
	{
		case WP_MENU_MAIN:

			if( iKey == 1 )
			{
				trap_SendConsoleCommand( "wp_place" );

				// now add the global flags to this waypoint
				if( iMenuWpFlags != 0 )
				{
					CG_Printf("Trying to adding some flags...\n");

					// we can pass Waypoint_index instead of Waypoint_index - 1 since all console commands are buffered and not executed immediately
					trap_SendConsoleCommand( va(" ; wp_setflags %i %i %i %i %i %i %i", Waypoint_index , iMenuWpFlags, iMenuWpUnreachEnts[0], iMenuWpUnreachEnts[1], iMenuWpUnreachEnts[2], iMenuWpUnreachEnts[3], iMenuWpButtonIndex) );
				}
			}
			else if( iKey == 2 )
			{
				iMenuState = WP_MENU_OBJ_TYPE;
			}
			else if( iKey == 3 )
			{
				if( NearestIndex != -1 )
				{
					iMenuPathFrom = NearestIndex;
					iMenuState = WP_MENU_CONNECT;
				}
			}
			else if( iKey == 4 )
			{
				iMenuPathFrom = NearestIndex;
				iMenuState = WP_MENU_DISCONNECT;
			}
			else if( iKey == 5 )
			{
				iMenuState = WP_MENU_SET_WAYPOINT_FLAGS;
			}
			else if( iKey == 6 )
			{
				iMenuWpFlags		= 0;
				iMenuPtFlags		= 0;
				iMenuWpUnreachTeam	= 0;

				iMenuWpUnreachIndex = 0;
				memset( iMenuWpUnreachEnts, 0, sizeof(iMenuWpUnreachEnts) );
			}
			else if( iKey == 7 )
			{
				if( NearestIndex != -1 )
				{
					trap_SendConsoleCommand( va("wp_delete %i", NearestIndex) );
				}
			}
			else if( iKey == 8 )
			{
				// try to open mapname.wps. If no valid handle assume it doesn't exist.
				fileHandle_t f;

				trap_FS_FOpenFile( va("%s.wps", mapname), &f, FS_READ );

				if(!f)
				{
					trap_SendConsoleCommand( va("wp_save %s.wps", mapname) );
				}
				else
				{
					iMenuState = WP_MENU_SAVE;
					trap_FS_FCloseFile(f);
				}
			}
			// WolfBot 1.5
			else if( iKey == 9 )
			{
				if( NearestIndex != -1 )
				{
					iMenuState = WP_MENU_DRAG;

					// safe the origin so we can restore it
					VectorCopy( Waypoints[NearestIndex].origin, vWaypointOrigin );
					iWpDragIndex = NearestIndex;
				}
			}
			else if( iKey == 10 )
			{
				iMenuState = WP_MENU_NONE;
			}

			break;

		case WP_MENU_OBJ_TYPE:

			if( iKey == 1 )
			{
				iMenuObjType = OBJECTIVE_BOMB;
				iMenuState = WP_MENU_OBJ_TARGET;
			}
			else if( iKey == 2 )
			{
				iMenuObjType = OBJECTIVE_STEAL;
				iMenuState = WP_MENU_OBJ_TEAM;
			}
			else if( iKey == 3 )
			{
				iMenuObjType = OBJECTIVE_CAPTURE;
				iMenuState = WP_MENU_OBJ_TEAM;
			}
			else if( iKey == 4 )
			{
				iMenuState = WP_MENU_MAIN;
			}

			break;

		case WP_MENU_OBJ_TEAM:

			if( iKey == 1 )
			{
				iMenuObjTeam = TEAM_AXIS;
				iMenuState = WP_MENU_OBJ_PRIORITY;
			}
			else if( iKey == 2 )
			{
				iMenuObjTeam = TEAM_ALLIES;
				iMenuState = WP_MENU_OBJ_PRIORITY;
			}
			else if( iKey == 3 )
			{
				iMenuState = WP_MENU_OBJ_TYPE;
			}

			break;

		case WP_MENU_OBJ_PRIORITY:

			if( iKey == 1 )
			{
				iMenuObjPrior = PRIORITY_HIGH;
				iMenuState = WP_MENU_OBJ_NUMBER;
			}
			else if( iKey == 2 )
			{
				iMenuObjPrior = PRIORITY_LOW;
				iMenuState = WP_MENU_OBJ_NUMBER;
			}
			else if( iKey == 3 )
			{
				iMenuState = WP_MENU_OBJ_TEAM;
			}

			break;

		case WP_MENU_OBJ_TARGET:

			if( iKey == 1 && iExplosiveNum != -1 ) // valid crosshair entity
			{
				iMenuObjTarget = iExplosiveNum;
				iMenuState = WP_MENU_OBJ_TEAM;
			}
			else if( iKey == 2 )
			{
				iMenuState = WP_MENU_OBJ_TYPE;
			}

			break;

		case WP_MENU_OBJ_NUMBER:

			// WolfBot 1.5 : should be iKey < 7 not iKey < 6
			if( iKey > 0 && iKey < 7 )
			{
				iMenuObjNumber = iKey;
				iMenuState = WP_MENU_MAIN;

				trap_SendConsoleCommand( va("wp_objective %i %i %i %i %i", iMenuObjTeam, iMenuObjType, iMenuObjPrior, iMenuObjNumber, iMenuObjTarget) );

				// now add the global flags to this waypoint
				if( iMenuWpFlags != 0 )
				{
					// we can pass Waypoint_index instead of Waypoint_index - 1 since all console commands are buffered and not executed immediately
					trap_SendConsoleCommand( va(" ; wp_setflags %i %i %i %i %i %i %i", Waypoint_index , iMenuWpFlags, iMenuWpUnreachEnts[0], iMenuWpUnreachEnts[1], iMenuWpUnreachEnts[2], iMenuWpUnreachEnts[3], iMenuWpButtonIndex) );
				}

			}
			else if( iKey == 7 )
			{
				iMenuState = WP_MENU_OBJ_PRIORITY;
			}

		case WP_MENU_CONNECT:

			if( iKey == 1 )
			{
				iMenuPathTo = NearestIndex;
				iMenuState = WP_MENU_CONNECT_TYPE;
			}
			else if( iKey == 2 )
			{
				iMenuState = WP_MENU_MAIN;
			}

			break;

		case WP_MENU_CONNECT_TYPE:

			if( iKey == 1 )
			{
				iMenuState = WP_MENU_MAIN;

				trap_SendConsoleCommand( va("wp_connect %i %i %i", iMenuPathFrom, iMenuPathTo, 2) );

				// now add the global path flags to this path
				if( iMenuPtFlags != 0 )
				{
					trap_SendConsoleCommand( va(" ; wp_setpathflags %i %i %i", iMenuPathFrom, iMenuPathTo, iMenuPtFlags) );
					trap_SendConsoleCommand( va(" ; wp_setpathflags %i %i %i", iMenuPathTo, iMenuPathFrom, iMenuPtFlags) );
				}
			}
			else if( iKey == 2 )
			{
				iMenuState = WP_MENU_MAIN;

				trap_SendConsoleCommand( va("wp_connect %i %i %i", iMenuPathFrom, iMenuPathTo, 1) );

				// now add the global path flags to this path
				if( iMenuPtFlags != 0 )
				{
					trap_SendConsoleCommand( va(" ; wp_setpathflags %i %i %i", iMenuPathFrom, iMenuPathTo, iMenuPtFlags) );
				}
			}
			else if( iKey == 3 )
			{
				iMenuState = WP_MENU_CONNECT;
			}

			break;

		case WP_MENU_DISCONNECT:

			if( iKey == 1 )
			{
				iMenuPathTo = NearestIndex;
				iMenuState = WP_MENU_DISCONNECT_TYPE;
			}
			else if( iKey == 2 )
			{
				iMenuState = WP_MENU_MAIN;
			}

			break;

		case WP_MENU_DISCONNECT_TYPE:

			if( iKey == 1 )
			{
				iMenuState = WP_MENU_MAIN;

				trap_SendConsoleCommand( va("wp_disconnect %i %i %i", iMenuPathFrom, iMenuPathTo, 2) );
			}
			else if( iKey == 2 )
			{
				iMenuState = WP_MENU_MAIN;

				trap_SendConsoleCommand( va("wp_disconnect %i %i %i", iMenuPathFrom, iMenuPathTo, 1) );
			}
			else if( iKey == 3 )
			{
				iMenuState = WP_MENU_DISCONNECT;
			}

			break;

		case WP_MENU_SET_WAYPOINT_FLAGS:

			if( iKey == 1 )
			{
				iMenuWpFlags ^= FL_WP_CAMP;
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 2 )
			{
				iMenuWpFlags ^= FL_WP_MG42;
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 3 )
			{
				if( iMenuWpFlags & FL_WP_DOOR )
				{
					iMenuWpFlags &= ~FL_WP_DOOR;
					iMenuState = WP_MENU_MAIN;
				}
				else
				{
					iMenuState = WP_MENU_SET_FLAGS_BUTTON_INDEX;
				}
			}
			else if( iKey == 4 )
			{
				iMenuWpFlags ^= FL_WP_AXIS_DOCS_DELIVER;
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 5 )
			{
				iMenuWpFlags ^= FL_WP_ALLIES_DOCS_DELIVER;
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 6 )
			{
				iMenuState = WP_MENU_SET_FLAGS_UNREACHABLE_TEAM;
			}
			else if( iKey == 7 )
			{
				iMenuState = WP_MENU_SET_PATH_FLAGS;
			}
			else if( iKey == 8 )
			{
				iMenuState = WP_MENU_MAIN;
			}

			break;

		case WP_MENU_SET_PATH_FLAGS:

			if( iKey == 1 )
			{
				iMenuPtFlags ^= FL_PATH_JUMP;
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 2 )
			{
				iMenuPtFlags ^= FL_PATH_DUCK;
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 3 )
			{
				iMenuPtFlags ^= FL_PATH_BLOCKED;
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 4 )
			{
				iMenuPtFlags ^= FL_PATH_WALK;
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 5 )
			{
				iMenuPtFlags ^= FL_PATH_LADDER;
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 6 )
			{
				iMenuState = WP_MENU_SET_WAYPOINT_FLAGS;
			}

			break;

		case WP_MENU_SET_FLAGS_UNREACHABLE_TEAM:

			if( iKey == 1 )
			{
				iMenuWpUnreachTeam = TEAM_AXIS;
				iMenuState = WP_MENU_SET_FLAGS_UNREACHABLE_ENTITIES;
			}
			else if( iKey == 2 )
			{
				iMenuWpUnreachTeam = TEAM_ALLIES;
				iMenuState = WP_MENU_SET_FLAGS_UNREACHABLE_ENTITIES;
			}
			else if( iKey == 3 )
			{
				iMenuState = WP_MENU_SET_WAYPOINT_FLAGS;
			}

			break;

		case WP_MENU_SET_FLAGS_UNREACHABLE_ENTITIES:

			if( iKey == 1 ) // 'Set'
			{
				// all 4 entities set ? then go on as if 'Done' was pressed
				if( iMenuWpUnreachIndex >= 3 )
				{
					iMenuState = WP_MENU_MAIN;
					iMenuWpUnreachIndex = 0;

					// set the unreachable flag
					iMenuWpFlags &= ~FL_WP_AXIS_UNREACHABLE;
					iMenuWpFlags &= ~FL_WP_ALLIES_UNREACHABLE;

					if( iMenuWpUnreachTeam == TEAM_AXIS )
					{
						iMenuWpFlags |= FL_WP_AXIS_UNREACHABLE;
					}
					else if( iMenuWpUnreachTeam == TEAM_ALLIES )
					{
						iMenuWpFlags |= FL_WP_ALLIES_UNREACHABLE;
					}
				}
				else
				{
					if( iExplosiveNum != -1 )
					{
						iMenuWpUnreachEnts[ iMenuWpUnreachIndex ] = iExplosiveNum;

						iMenuWpUnreachIndex++;
					}
				}
			}
			else if( iKey == 2 ) // 'Done'
			{
				iMenuWpUnreachIndex = 0;
				iMenuState = WP_MENU_MAIN;

				// set the unreachable flag
				iMenuWpFlags &= ~FL_WP_AXIS_UNREACHABLE;
				iMenuWpFlags &= ~FL_WP_ALLIES_UNREACHABLE;

				if( iMenuWpUnreachTeam == TEAM_AXIS )
				{
					iMenuWpFlags |= FL_WP_AXIS_UNREACHABLE;
				}
				else if( iMenuWpUnreachTeam == TEAM_ALLIES )
				{
					iMenuWpFlags |= FL_WP_ALLIES_UNREACHABLE;
				}
			}
			else if( iKey == 3 ) // 'Go Back'
			{
				iMenuState = WP_MENU_SET_FLAGS_UNREACHABLE_TEAM;
			}

			break;

		case WP_MENU_SET_FLAGS_BUTTON_INDEX:

			if( iKey == 1 && NearestIndex != -1 )
			{
				iMenuWpFlags |= FL_WP_DOOR;
				iMenuWpButtonIndex = NearestIndex;
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 2 )
			{
				iMenuState = WP_MENU_SET_WAYPOINT_FLAGS;
			}

			break;

		case WP_MENU_SAVE:

			if( iKey == 1 )
			{
				// user said overwrite OK so just do it now
				trap_SendConsoleCommand( va("wp_save %s.wps", mapname) );
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 2 )
			{
				iMenuState = WP_MENU_MAIN;
			}
			
			break;

		case WP_MENU_DRAG:

			if( iKey == 1 ) // confirmed position
			{
				iMenuState = WP_MENU_MAIN;
			}
			else if( iKey == 2 ) // want to restore origin and go back
			{
				VectorCopy( vWaypointOrigin, Waypoints[iWpDragIndex].origin );
				iMenuState = WP_MENU_MAIN;
			}

			break;
	}
}

//
// Add Bot Menu Stuff (should move this to anouther file)
//

int iAddBotMenuState = 0;
int iBotClass = 0;
int iBotTeam = 0;

void Cmd_Bot_Menu( void )
{
	iAddBotMenuState = BOT_MENU_MAIN;

	// make sure waypoint menu is not active
	iMenuState = 0;
}

void CG_DrawBotMenu( void )
{
	int y;
	vec4_t vColorBack = { 0, 0, 0.15f, 0.5f };

	if( iAddBotMenuState == BOT_MENU_NONE )
		return;

	// outline box never changes
	CG_DrawRect( 60, 100, 160, 250, 1 , colorMdGrey );
	CG_FillRect( 61, 101, 158, 248, vColorBack );

	CG_DrawStringExt( TEXT_X_OFFSET, 110, "Bot Menu", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

	y = 130;

	switch( iAddBotMenuState )
	{
	
		case BOT_MENU_MAIN:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Add Bot", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Remove Bot", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Add Random Bot", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "4. Remove all Bots", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 20;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "5. Fill Server", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case BOT_MENU_CLASS:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Soldier", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Medic", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Engineer", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "4. Lieutenant", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case BOT_MENU_TEAM:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. Team Axis", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Team Allies", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			break;

		case BOT_MENU_WEAPON:

			CG_DrawStringExt( TEXT_X_OFFSET, y, "1. MP40", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "2. Thompson", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			CG_DrawStringExt( TEXT_X_OFFSET, y, "3. Sten", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			y += 10;

			if( (iBotClass - 1) == PC_SOLDIER )
			{
				CG_DrawStringExt( TEXT_X_OFFSET, y, "4. Mauser", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
				y += 10;

				CG_DrawStringExt( TEXT_X_OFFSET, y, "5. Panzerfaust", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
				y += 10;

				CG_DrawStringExt( TEXT_X_OFFSET, y, "6. Flamethrower", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
				y += 10;

				CG_DrawStringExt( TEXT_X_OFFSET, y, "7. Venom", NULL, FALSE, FALSE, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
				y += 10;
			}

			break;
	}


}

void CG_Bot_MenuInput( int iKey )
{
	char szTeam[64];
	char szWeapon[64];

	switch( iAddBotMenuState )
	{
		case BOT_MENU_MAIN:

			if( iKey == 1 )
			{
				iAddBotMenuState = BOT_MENU_CLASS;
			}
			else if( iKey == 2 )
			{
				// remove random bot
				iAddBotMenuState = 0;

				trap_SendConsoleCommand( "bot_remove" );
			}
			else if( iKey == 3 )
			{
				trap_SendConsoleCommand( "bot_connect" );
				iAddBotMenuState = 0;
			}
			else if( iKey == 4 )
			{
				// remove all bots
				iAddBotMenuState = 0;

				trap_SendConsoleCommand( "bot_removeall" );
			}
			else if( iKey == 5 )
			{
				// fill map with bots
				iAddBotMenuState = 0;

				trap_SendConsoleCommand( "bot_fillserver" );
			}
			else
			{
				iAddBotMenuState = 0;
			}

			break;

		case BOT_MENU_CLASS:

			if( (iKey > 0) && (iKey < 5) )
			{
				iBotClass = iKey;
				iAddBotMenuState = BOT_MENU_TEAM;
			}
			else
			{
				iAddBotMenuState = 0;
			}

			break;

		case BOT_MENU_TEAM:

			if( (iKey > 0) && (iKey < 3) )
			{
				iBotTeam = iKey;
				iAddBotMenuState = 0;

				if( (iBotClass - 1) == PC_SOLDIER || (iBotClass - 1) == PC_LT ) // lame
				{
					iAddBotMenuState = BOT_MENU_WEAPON;
				}
				else
				{

					if( iBotTeam == TEAM_AXIS )
					{
						strcpy( szTeam, "red" );
					}
					else
					{
						strcpy( szTeam, "blue" );
					}

					// using skill normal
					trap_SendConsoleCommand( va("bot_connect %s %i normal", szTeam, iBotClass - 1) );
				}
			}
			else
			{
				iAddBotMenuState = 0;
			}

			break;

		case BOT_MENU_WEAPON:

			if( (iKey > 0) && (iKey < 8) )
			{
				if( (iBotClass - 1) == PC_LT )
				{
					if( iKey > 3 )
					{
						iAddBotMenuState = 0;
						break;
					}
				}

				iAddBotMenuState = 0;

				if( iBotTeam == TEAM_AXIS )
				{
					strcpy( szTeam, "red" );
				}
				else
				{
					strcpy( szTeam, "blue" );
				}

				// mp40
				if( iKey == 1 )
				{
					strcpy( szWeapon, "MP40" );
				}
				else if( iKey == 2 )
				{
					strcpy( szWeapon, "Thompson" );
				}
				else if( iKey == 3 )
				{
					strcpy( szWeapon, "Sten" );
				}
				else if( iKey == 4 )
				{
					strcpy( szWeapon, "Mauser" );
				}
				else if( iKey == 5 )
				{
					strcpy( szWeapon, "Panzerfaust" );
				}
				else if( iKey == 6 )
				{
					strcpy( szWeapon, "Flamethrower" );
				}
				else if( iKey == 7 )
				{
					strcpy( szWeapon, "Venom" );
				}

				trap_SendConsoleCommand( va("bot_connect %s %i normal %s", szTeam, iBotClass - 1, szWeapon) );

			}
			else
			{
				iAddBotMenuState = 0;
			}

			break;
	}
}
