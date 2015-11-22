// Copyright (C) 2003 Torben Könke. Licensed under GPL, see LICENSE.
//
// g_botmain.c -- core bot AI 

#include "g_local.h"

extern WAYPOINT			Waypoints[MAX_NUM_WAYPOINTS];
extern int				numWaypoints;

extern radioCommand_t	radioCommands[];

extern int				ObjWaypoints[32];
extern int				numObjWaypoints;

//
// Called every ai frame. This is the core function which calls all subroutines
//

void Bot_Think( gentity_t *ent, int time )
{
	float		yaw_degrees;
	float		pitch_degrees;
	BOOL		bHasEnemy;
	BOOL		bFollowMate;
	BOOL		bHelping;
	BOOL		bMG42;
	vec3_t		vViewOrigin;
	vec3_t		vDir;
	vec3_t		vAngles;
	vec3_t		vSize, vOrigin;
	int			iIndex;
	int			i;
	int			iReinforceTime;
	bottask_t	botTask;
	WAYPOINT	*pWaypoint;
	gentity_t	*pEntity;
	trace_t		tr;
	botClient_t *pBot = ent->botClient;

	if( pBot->bShutdown )
		return;

	// clear out last user command
	memset( &pBot->ucmd, 0, sizeof(pBot->ucmd) );

	// receive all waiting server commands
	Bot_GetServerCommands(pBot);

	// check out the snapshot
	Bot_CheckSnapshot(ent);

	pBot->ucmd.serverTime = time;

	pBot->ucmd.mpSetup |= 1 << MP_TEAM_OFFSET;
	pBot->ucmd.mpSetup |= pBot->playerClass	<< MP_CLASS_OFFSET;
	pBot->ucmd.mpSetup |= 2 << MP_TEAM_OFFSET;
	pBot->ucmd.mpSetup |= pBot->playerWeapon << MP_WEAPON_OFFSET;

	// does bot want to respond to a radio command this frame ?
	if( pBot->iRadioSendCmd != -1 )
	{
		if( level.time > pBot->iRadioDelayTimer )
		{
			Bot_SendRadioCommand( ent, pBot->iRadioSendCmd, pBot->bRadioTeam );
			pBot->iRadioSendCmd = -1;
			pBot->bRadioTeam = FALSE;
		}
	}

	// if bot is dead don't do much (or in intermission)
	if( ent->client->ps.stats[STAT_HEALTH] <= 0 || ent->client->ps.pm_type == PM_DEAD || (ent->client->ps.pm_flags & PMF_FOLLOW) || (ent->client->ps.pm_flags & PMF_LIMBO) || (level.intermissiontime) )
	{
		// FIXME : its a waste to call this every frame , really
		// Check player class only if REALLY dead (i.e. in limbo)
		if( ent->client->ps.pm_flags & PMF_LIMBO )
		{
			Bot_CheckPlayerClass(ent);
			pBot->bWounded = FALSE;
		}

		if( pBot->bInitialized == FALSE )
		{
			Bot_Initialize(ent->botClient);

			// WolfBot 1.5 : If round is over and in intermission let bots cheer or nag a bit
			if( level.intermissiontime )
			{
				// is cheering allowed ?
				if( wb_allowRadio.integer != 2 )
				{
					// this sucks since this is called at the same time for every bot
					if( (rand() % 100) > 20 )
					{
						char cs[MAX_STRING_CHARS];
						int winner;

						trap_GetConfigstring( CS_MULTI_MAPWINNER, cs, sizeof(cs) );
						winner = atoi( Info_ValueForKey( cs, "winner" ) );

						// 0 == AXIS , 1 == ALLIES
						// TEAM_RED == 1, TEAM_BLUE == 2
						if( (ent->client->sess.sessionTeam - 1) == winner )
						{
							Bot_SendDelayedRadioCommand( ent, RADIO_CHEER, FALSE );
						}
						else
						{
							Bot_SendDelayedRadioCommand( ent, RADIO_NO, FALSE );
						}

						// maybe use bots score to vary the delay instead ?
						pBot->iRadioDelayTimer += rand() % 4000;
					}
				}
			}

			// if kill was a headshot pretend to be impressed by the players skills
			if( ent->client->ps.eFlags & EF_HEADSHOT )
			{	
				if( (ent->enemy) && (ent->enemy->client) && (ent->enemy->botClient == NULL) )
				{
					if( !OnSameTeam( ent, ent->enemy ) )
					{
						if( (rand() % 100) > 60 ) // don't spam too much
						{
							Bot_SendDelayedRadioCommand( ent, RADIO_GREATSHOT, FALSE );
						}
					}
				}
			}
		}

		// WolfBot 1.5 : If Wounded don't go into limbo mode immediately. Rather stay
		// laying on the ground yelling for a medic from time to time and drop out just a 
		// few seconds before reinforcements arrive.
		if( (ent->client->ps.pm_type == PM_DEAD) && !(ent->client->ps.pm_flags & PMF_LIMBO) )
		{
			pBot->bWounded = TRUE;

			if( ent->client->sess.sessionTeam == TEAM_AXIS )
			{
				iReinforceTime = g_redlimbotime.integer - level.time % g_redlimbotime.integer;
			}
			else if( ent->client->sess.sessionTeam == TEAM_BLUE )
			{
				iReinforceTime = g_bluelimbotime.integer - level.time % g_bluelimbotime.integer;
			}

			if( level.time > pBot->iCheckMedicTime )
			{
				pBot->iCheckMedicTime = level.time + 7000 + rand() % 3000;

				if( Bot_LookForMedic(ent) == TRUE )
				{
					Bot_SendDelayedRadioCommand( ent, RADIO_MEDIC, TRUE );
				}
			}
	
			if( iReinforceTime < 2000 )
			{
				// time to drop out now
				pBot->ucmd.upmove = 127;
				pBot->bWounded = FALSE;

				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) : Reinforcements in 2 seconds, dropping out!!\n", ent->client->ps.clientNum);
				}
			}
		}
		// just reset this so bot doesnt complain next time it spawns
		pBot->iCurWaypointTime = level.time;

		if( g_debugBots.integer )
			G_Printf("Bot (%i) is dead and is waiting to respawn\n", pBot->clientNum);

		trap_BotUserCommand( pBot->clientNum, &pBot->ucmd );
		return;
	}

	if( pBot->bChangedClass )
	{
		Bot_SendDelayedRadioCommand( ent, RADIO_IAMENGINEER, TRUE );
		pBot->bChangedClass = FALSE;
	}

	// make sure bot re-initializes stuff next time it dies
	if( pBot->bInitialized )
		pBot->bInitialized = FALSE;

	if( pBot->bWounded )
	{
		// bot was wounded so he must have been given a syringe by a teammate.
		// issue a thanks radio command
		Bot_SendDelayedRadioCommand( ent, RADIO_THANKS, TRUE );
	}

	pBot->bWounded = FALSE;

	Bot_CheckRadioMessages(ent);

	bHasEnemy = Bot_FindEnemy(ent);
	bFollowMate = Bot_FollowUser(ent);
	bHelping = Bot_HelpingFriend(ent);
	bMG42 = Bot_OperatingMG42(ent);

	yaw_degrees = Bot_ChangeYaw(ent);
	pitch_degrees = Bot_ChangePitch(ent);

	if( bHasEnemy )
	{
		if( level.time > pBot->iEnemyFirstSeen + pBot->pSkill->iReactionTime )
		{
			if( Bot_GetTask(pBot)->iTask != TASK_ATTACK )
			{
				Bot_RemoveTasks(pBot);

				botTask.iExpireTime = -1;
				botTask.iIndex = -1;
				botTask.iTask = TASK_ATTACK;
				botTask.pNextTask = NULL;
				botTask.pPrevTask = NULL;

				Bot_PushTask( pBot, &botTask );

				// delete all nodes and also current index
				Bot_FreeSolutionNodes(pBot);
				pBot->iCurWaypoint = -1;
			}
		}
	}

	switch( Bot_GetTask(pBot)->iTask )
	{
		case TASK_ROAM:

			// set iIndex to current goal if not already
			if( Bot_GetTask(pBot)->iIndex != pBot->iGoalWaypoint )
				Bot_GetTask(pBot)->iIndex = pBot->iGoalWaypoint;

			// it's time to check for nearby dropped items
			if( level.time >= pBot->iLookForItemsTime )
			{
				pBot->iLookForItemsTime = level.time + 2500;
			
				if( Bot_FindItem(ent) )
				{
					if( g_debugBots.integer )
						G_Printf("Bot (%i) : found an interesting item\n", pBot->clientNum);
				}
			}

			// Bot is NOT heading for an item and NOT heading for a friend and NOT operating
			// an MG42 so lets find/move to a waypoint
			if( (pBot->pItem == NULL || pBot->pItem->inuse == FALSE) && (bHelping == FALSE) && (bMG42 == FALSE) )
			{
				// bot reached its goal
				if( Bot_MapNavigation(ent) == TRUE )
				{
					Bot_Log("Bot (%i) : reached Navigation Goal (%i)", pBot->clientNum, pBot->iCurWaypoint);
					
					// WolfBot 1.5 : added
					Bot_TaskComplete(pBot);

					// check out the reached waypoint
					if( pBot->iCurWaypoint != -1 )
					{
						pWaypoint = &Waypoints[ pBot->iCurWaypoint ];

						// objective Waypoint
						if( pWaypoint->flags & FL_WP_OBJECTIVE )
						{
							int iObjTeam, iObjType, iObjState;

							iObjTeam  = pWaypoint->objdata.team;
							iObjType  = pWaypoint->objdata.type;
							iObjState = pWaypoint->objdata.state;
							
							if( iObjTeam == ent->client->sess.sessionTeam )
							{
								if( iObjState == STATE_UNACCOMPLISHED )
								{
									if( iObjType == OBJECTIVE_BOMB )
									{
										if( pBot->playerClass == PC_ENGINEER )
										{
											bottask_t temp;
											BOOL bPlant = TRUE;

											// first check if someone else has already put a bomb here
											pEntity = NULL;

											while( (pEntity = Bot_FindRadius( pEntity, ent->client->ps.origin, 300.0f )) != NULL )
											{
												if( !Q_stricmp( pEntity->classname, "dynamite" ) )
												{
													if( pEntity->think == G_ExplodeMissile ) // armed
													{
														if( pEntity->parent )
														{
															// owner of found dynamite is on same team as bot so there must be an armed dynamite already
															if( OnSameTeam( ent, pEntity->parent ) )
															{
																if( g_debugBots.integer )
																{
																	G_Printf("Bot (%i) : There is a dynamite already.\n", ent->client->ps.clientNum);
																}

																bPlant = FALSE;
																break;
															}
														}
													}
												}
											}

											if( bPlant == TRUE )
											{
												// if weapon not charged forget about this task
												if( level.time - ent->client->ps.classWeaponTime < g_engineerChargeTime.integer )
												{
													Bot_Log("Bot (%i) : Couldn't place dynamite because not charged", ent->client->ps.clientNum);
												}
												else
												{
													temp.iExpireTime = -1;
													temp.iIndex = -1;
													temp.iTask = TASK_PLANTDYNAMITE;
													temp.pNextTask = NULL;
													temp.pPrevTask = NULL;

													Bot_PushTask( pBot, &temp );
												}
											}
										}
									}
									else if( iObjType == OBJECTIVE_STEAL )
									{
										botTask.iExpireTime = -1;
										botTask.iIndex = -1;
										botTask.iTask = TASK_PICKUPOBJECT;
										botTask.pNextTask = NULL;
										botTask.pPrevTask = NULL;

										Bot_PushTask( pBot, &botTask );
									}
									else if( iObjType == OBJECTIVE_CAPTURE )
									{
										botTask.iExpireTime = -1;
										botTask.iIndex = -1;
										botTask.iTask = TASK_CAPTURE;
										botTask.pNextTask = NULL;
										botTask.pPrevTask = NULL;

										Bot_PushTask( pBot, &botTask );
									}
								}
							}
							else if( iObjTeam != ent->client->sess.sessionTeam )
							{
								// why is this here, move this....
								if( iObjType == OBJECTIVE_BOMB )
								{
									if( iObjState != STATE_ACCOMPLISHED )
									{
										if( pBot->playerClass == PC_ENGINEER )
										{
											// check for dyno now
											pEntity = NULL;

											while( (pEntity = Bot_FindRadius( pEntity, ent->client->ps.origin, 300 )) != NULL )
											{
												if( !Q_stricmp( pEntity->classname, "dynamite" ) )
												{
													if( pEntity->think == G_ExplodeMissile )
													{
														if( pEntity->parent )
														{
															// need to defuse NOW
															if( !OnSameTeam( ent, pEntity->parent ) )
															{
																pBot->pDynamite = pEntity;

																botTask.iExpireTime = -1;
																botTask.iIndex = -1;
																botTask.iTask = TASK_DISARMDYNAMITE;
																botTask.pNextTask = NULL;
																botTask.pPrevTask = NULL;

																Bot_PushTask( pBot, &botTask );

																break;
															}
														}
													}
												}
											}

										}	
									}
								}
							}
						}

						// bot has reached a camp waypoint
						if( pWaypoint->flags & FL_WP_CAMP )
						{
							BOOL bBlocked = FALSE;

							// is bot even allowed to camp
							if( level.time > pBot->iLastCampTime + 10000 )
							{
								// check if a teammate is camping here already
								for( i = 0; i < level.numConnectedClients; i++ )
								{
									if( i == ent->s.number )
										continue;

									if( g_entities[i].r.svFlags & SVF_BOT )
									{
										if( OnSameTeam( ent, &g_entities[i] ) )
										{
											if( g_entities[i].botClient->iCurWaypoint == pBot->iCurWaypoint )
											{
												bBlocked = TRUE;
											}
										}
									}
								}

								// OK camp here for a while
								if( bBlocked == FALSE )
								{
									botTask.iExpireTime = level.time + 15000 - pBot->pSkill->iCombatMoveSkill * 50;
									botTask.iIndex = -1;
									botTask.iTask = TASK_CAMP;
									botTask.pNextTask = NULL;
									botTask.pPrevTask = NULL;

									Bot_PushTask( pBot, &botTask );
								}

							}
						}

						// bot has reached enemy deliver waypoint
						if( pWaypoint->flags & FL_WP_AXIS_DOCS_DELIVER && ent->client->sess.sessionTeam == TEAM_ALLIES )
						{
							BOOL bBlocked = FALSE;

							// flag is not at home, may be camp here
							if( FlagAtHome(ent->client->sess.sessionTeam) == FALSE )
							{
								if( level.time > pBot->iLastCampTime + 10000 )
								{
									for( i = 0; i < level.numConnectedClients; i++ )
									{
										if( i == ent->s.number )
											continue;

										if( g_entities[i].r.svFlags & SVF_BOT )
										{
											if( OnSameTeam( ent, &g_entities[i] ) )
											{
												if( g_entities[i].botClient->iCurWaypoint == pBot->iCurWaypoint )
												{
													bBlocked = TRUE;
												}
											}
										}
									}

									if( bBlocked == FALSE )
									{
										botTask.iExpireTime = level.time + 15000 + crandom() * 5000;
										botTask.iIndex = -1;
										botTask.iTask = TASK_CAMP;
										botTask.pNextTask = NULL;
										botTask.pPrevTask = NULL;

										Bot_PushTask( pBot, &botTask );
									}
								}
							}
						}
						else if( pWaypoint->flags & FL_WP_ALLIES_DOCS_DELIVER && ent->client->sess.sessionTeam == TEAM_AXIS )
						{
							// same as above
							BOOL bBlocked = FALSE;

							if( FlagAtHome(ent->client->sess.sessionTeam) == FALSE )
							{
								if( level.time > pBot->iLastCampTime + 10000 )
								{
									for( i = 0; i < level.numConnectedClients; i++ )
									{
										if( i == ent->s.number )
											continue;

										if( g_entities[i].r.svFlags & SVF_BOT )
										{
											if( OnSameTeam( ent, &g_entities[i] ) )
											{
												if( g_entities[i].botClient->iCurWaypoint == pBot->iCurWaypoint )
												{
													bBlocked = TRUE;
												}
											}
										}
									}

									if( bBlocked == FALSE )
									{
										botTask.iExpireTime = level.time + 15000 + crandom() * 5000;
										botTask.iIndex = -1;
										botTask.iTask = TASK_CAMP;
										botTask.pNextTask = NULL;
										botTask.pPrevTask = NULL;

										Bot_PushTask( pBot, &botTask );
									}
								}
							}
						}

						// WolfBot 1.5 : Reached Waypoint is a MG42 Waypoint
						if( pWaypoint->flags & FL_WP_MG42 )
						{
							if( wb_allowMG42.integer )
							{
								if( (level.time > pBot->iLastMG42Time + 20000) || (pBot->iLastMG42Time == -1) )
								{
									pBot->pMG42 = NULL;

									botTask.iExpireTime = -1;
									botTask.iIndex = -1;
									botTask.iTask = TASK_ACTIVATEMG42;
									botTask.pNextTask = NULL;
									botTask.pPrevTask = NULL;

									Bot_PushTask( pBot, &botTask );
								}
							}
						}

					}
				}
			}

			break;

		case TASK_CAMP:

			pBot->ucmd.upmove = -127;
			pBot->ucmd.forwardmove = 0;

			pBot->iCurWaypointTime = level.time;

			// set last camp time to current time
			pBot->iLastCampTime = level.time;

			// react to suspicious noises when camping
			if( (pBot->iLastNoiseTime + 3000 > level.time) && (pBot->pSkill->iCombatMoveSkill > 40) )
			{
				vec3_t vRandomize;

				VectorSet( vRandomize, 100 - pBot->pSkill->iCombatMoveSkill, 100 - pBot->pSkill->iCombatMoveSkill, 100 - pBot->pSkill->iCombatMoveSkill );
				VectorScale( vRandomize, 0.75, vRandomize );
				VectorAdd( vRandomize, pBot->vNoiseOrigin, vRandomize );

				VectorSubtract( vRandomize, ent->client->ps.origin, vDir );
				vectoangles( vDir, vAngles );

				pBot->ideal_yaw = vAngles[YAW];
				pBot->ideal_pitch = vAngles[PITCH];
			}
			else
			{
				// turn between the paths when camping
				if( level.time > pBot->iCampTurnTime )
				{
					pBot->iCampTurnTime = level.time + 2500 + rand() % 1000;

					// low skill bots look longer in one direction
					if( pBot->pSkill->iCombatMoveSkill < 40 )
					{
						pBot->iCampTurnTime += rand() % 2000;
					}

					if( pBot->iCurWaypoint != -1 )
					{
						iIndex = rand() % Waypoints[ pBot->iCurWaypoint ].numPaths;

						VectorSubtract( Waypoints[ Waypoints[ pBot->iCurWaypoint ].paths[ iIndex ].indexTarget ].origin, ent->client->ps.origin, vDir );
						vectoangles( vDir, vAngles );

						pBot->ideal_yaw = vAngles[YAW];
						pBot->ideal_pitch = vAngles[PITCH];
					}
				}
			}

			// task has expired
			if( level.time > Bot_GetTask(pBot)->iExpireTime )
				Bot_TaskComplete(pBot);

		break;


		case TASK_ATTACK:

			if( !bHasEnemy )
			{
				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) : Lost my enemy, going to roam.\n", ent->client->ps.clientNum );
				}

				// forget about this task then
				Bot_TaskComplete(pBot);
			}
			else
			{
				// do combat move and attack stuff
				Bot_AttackMovement(ent);
				Bot_ShootAtEnemy(ent);
			}

			pBot->iCurWaypointTime = level.time;

			break;

		case TASK_ACCOMPANY:

			if( level.time > Bot_GetTask(pBot)->iExpireTime )
			{
				Bot_TaskComplete(pBot);
				pBot->pUser = NULL;
			}

			if( !bFollowMate )
			{
				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) : Lost my teammate, going to roam.\n", ent->client->ps.clientNum );
				}

				// forget about this task then
				Bot_TaskComplete(pBot);

				pBot->pUser = NULL;
			}
			else
			{
				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) : Following my teammate.\n", ent->client->ps.clientNum );
				}
			}

			break;

		// bot is attempting to plant dynamite at a bomb spot
		case TASK_PLANTDYNAMITE:

			// dynamite planted, arm it now
			if( pBot->pDynamite != NULL )
			{
				if( pBot->iWeapon != WP_PLIERS )
				{
					pBot->iWeapon = WP_PLIERS;
				}
				else
				{
					// face dynamite
					VectorCopy( ent->client->ps.origin, vViewOrigin );
					vViewOrigin[2] += ent->client->ps.crouchViewHeight;

					VectorSubtract( pBot->pDynamite->r.currentOrigin, ent->client->ps.origin, vDir );
					vectoangles( vDir, vAngles );

					pBot->ideal_yaw = vAngles[YAW];
					pBot->ideal_pitch = vAngles[PITCH];

					// need to be quite close to dynamite to arm it
					if( VectorLength(vDir) > 40 )
					{
						pBot->ucmd.forwardmove = 40;
					}
					else
					{
						// If dynamite is not armed , think points to DynaSink. If armed
						// think points to G_ExplodeMissile
						if( pBot->pDynamite->think == G_ExplodeMissile )
						{
							Bot_TaskComplete(pBot);

							// try to find a near camp spot
							iIndex = WaypointFindCampSpot( ent, 1000.0f );

							if( g_debugBots.integer )
							{
								G_Printf("Bot (%i) : Found %i as a cover waypoint.\n", ent->client->ps.clientNum, iIndex );
							}

							if( iIndex != -1 )
							{
								// just push a roam task on stack with camp waypoint as goal
								botTask.iExpireTime = -1;
								botTask.iIndex = iIndex;
								botTask.iTask = TASK_ROAM;
								botTask.pNextTask = NULL;
								botTask.pPrevTask = NULL;

								Bot_PushTask( pBot, &botTask );
								Bot_FreeSolutionNodes(pBot);

								pBot->iGoalWaypoint = iIndex;

								// reset the last camp time since bot should always try to
								// camp after planting the dynamite
								pBot->iLastCampTime = 0;

								// need to reset time for current waypoint
								pBot->iCurWaypointTime = level.time;
							}

							Bot_SendDelayedRadioCommand( ent, RADIO_NEEDBACKUP, TRUE );
				
							// null out dynamite pointer
							pBot->pDynamite = NULL;

							// select a good weapon again...
							pBot->iWeapon = Bot_SelectWeapon(ent);

							// mark this objective as "in progress"
							Waypoints[ pBot->iCurWaypoint ].objdata.state = STATE_INPROGRESS;
						}
						else
						{
							// ok arming it
							pBot->ucmd.buttons |= BUTTON_ATTACK;
						}
					}
				}
			}
			else
			{
				// select dynamite weapon
				if( pBot->iWeapon != WP_DYNAMITE )
				{
					pBot->iWeapon = WP_DYNAMITE;
				}
				else
				{
					// look down a little so bot won't throw dynamite too far away
					pBot->ideal_pitch = 40;

					// ready to throw dynamite now
					if( pitch_degrees < 10 )
					{
						// slight hack to emulate release of fire button...
						if( crandom() > 0 ) 
						{
							pBot->ucmd.buttons |= BUTTON_ATTACK;
							pBot->pDynamite = NULL;

							while( (pBot->pDynamite = Bot_FindRadius( pBot->pDynamite, ent->client->ps.origin, 150.0f )) != NULL )
							{
								if( !Q_stricmp( "dynamite", pBot->pDynamite->classname ) )
								{
									// found it so break out of the loop
									break;
								}
							}
						}
					}
				}
			}

			pBot->ucmd.upmove = -127;

			break;

		// bot is attempting to disarm dynamite
		case TASK_DISARMDYNAMITE:

			pBot->iWeapon = WP_PLIERS;

			if( pBot->pDynamite != NULL )
			{
				if( ent->client->ps.weapon == WP_PLIERS )
				{
					// face dynamite
					VectorCopy( ent->client->ps.origin, vViewOrigin );
					vViewOrigin[2] += ent->client->ps.crouchViewHeight;

					VectorSubtract( pBot->pDynamite->r.currentOrigin, ent->client->ps.origin, vDir );
					vectoangles( vDir, vAngles );

					pBot->ideal_yaw = vAngles[YAW];
					pBot->ideal_pitch = vAngles[PITCH];

					// need to be quite close to dynamite to arm it
					if( VectorLength(vDir) > 40 )
					{
						pBot->ucmd.forwardmove = 40;
					}
					else
					{
						// duck
						pBot->ucmd.upmove = -127;
						
						if( pBot->pDynamite->think == G_FreeEntity || pBot->pDynamite->inuse == FALSE )
						{
							pBot->pDynamite = NULL;

							Bot_TaskComplete(pBot);
							pBot->iWeapon = Bot_SelectWeapon(ent);

							Bot_Log("Bot(%i) : disarmed dynamited", ent->client->ps.clientNum);
						}
						else
						{
							pBot->ucmd.buttons |= BUTTON_ATTACK;
						}
					}
				}
			}
			else
			{
				// dynamite is gone forget about the task...
				Bot_TaskComplete(pBot);
			}

			break;

		// bot is attempting to pickup documents
		case TASK_PICKUPOBJECT:

			// got documents ?
			if( ((ent->client->sess.sessionTeam == TEAM_AXIS) && (ent->client->ps.powerups[PW_BLUEFLAG])) || ((ent->client->sess.sessionTeam == TEAM_ALLIES) && (ent->client->ps.powerups[PW_REDFLAG])) )
			{

				Bot_TaskComplete( pBot );
				pBot->pDocuments = NULL;

				Bot_Log("Bot(%i) : Picked up enemy documents", ent->client->ps.clientNum);

				break;
			}

			if( pBot->pDocuments != NULL )
			{
				VectorSubtract( pBot->pDocuments->r.currentOrigin, ent->client->ps.origin, vDir );
				vectoangles( vDir, vAngles );

				pBot->ideal_yaw = vAngles[YAW];

				if( yaw_degrees < 5 )
					pBot->ucmd.forwardmove = 40;
			}
			else
			{
				// find pickup object
				while( (pBot->pDocuments = Bot_FindRadius( pBot->pDocuments, ent->client->ps.origin, 150.0f)) != NULL )
				{
					if( ent->client->sess.sessionTeam == TEAM_AXIS )
					{
						if( !Q_stricmp( "team_CTF_blueflag", pBot->pDocuments->classname ) )
							break;
					}
					else if( ent->client->sess.sessionTeam == TEAM_ALLIES )
					{
						if( !Q_stricmp( "team_CTF_redflag", pBot->pDocuments->classname ) )
							break;
					}
				}

			}

			break;

		case TASK_CAPTURE:

			if( pBot->pCheckpoint != NULL )
			{
				int i, numEnts, touch[MAX_GENTITIES];

				VectorSubtract( pBot->pCheckpoint->r.currentOrigin, ent->client->ps.origin, vDir );
				vectoangles( vDir, vAngles );

				pBot->ideal_yaw = vAngles[YAW];

				if( yaw_degrees < 5 )
					pBot->ucmd.forwardmove = 40;

				// fixme find a better way to do this
				numEnts = trap_EntitiesInBox( ent->r.absmin, ent->r.absmax, touch, MAX_GENTITIES );

				for( i = 0; i < numEnts; i++ )
				{
					if( g_entities[ touch[i] ].s.number == pBot->pCheckpoint->s.number )
					{
						Bot_TaskComplete( pBot );
						pBot->pCheckpoint = NULL;
					
						break;
					}
				}
			}
			else
			{
				// find the enemy flag we wish to touch
				while( (pBot->pCheckpoint = Bot_FindRadius( pBot->pCheckpoint, ent->client->ps.origin, 150.0f)) != NULL )
				{
					// fixme : Axis muss auch capturen wenn state accomplished ist
					// und es ein allies objective is...
					if( !Q_stricmp( "team_WOLF_checkpoint", pBot->pCheckpoint->classname ) )
						break;
				}
			}	

			break;

		case TASK_DESTROYBARRIER:

			// something went wrong ?
			if( level.time > Bot_GetTask(pBot)->iExpireTime )
				Bot_TaskComplete(pBot);

			if( (ent->client->ps.weapon != WP_GRENADE_PINEAPPLE) && (ent->client->ps.weapon != WP_GRENADE_LAUNCHER) )
			{
				if( COM_BitCheck( ent->client->ps.weapons, WP_GRENADE_PINEAPPLE ) )
				{
					pBot->iWeapon = WP_GRENADE_PINEAPPLE;
				}
				else if( COM_BitCheck( ent->client->ps.weapons, WP_GRENADE_LAUNCHER ) )
				{
					pBot->iWeapon = WP_GRENADE_LAUNCHER;
				}
				else
				{
					// bot doesn't have grenades anymore... ?
					Bot_TaskComplete(pBot);
				}
			}
			else
			{

				// iIndex holds the entity num of the barrier the bot wishes to blow up
				pEntity = &g_entities[ Bot_GetTask(pBot)->iIndex ];

				// can happen if entity is blown up same frame
				if( pEntity->inuse == FALSE )
					Bot_TaskComplete(pBot);

				// need to calculate origin of brush model...
				VectorSubtract( pEntity->r.absmax, pEntity->r.absmin, vSize );
				VectorScale( vSize, 0.5, vSize );
				VectorAdd( pEntity->r.absmin, vSize, vOrigin );

				// turn towards blocker
				VectorSubtract( vOrigin, ent->client->ps.origin, vDir );
				vectoangles( vDir, vAngles );

				// fixme : calculate real distance to blocker and adjust throw angle
				pBot->ideal_yaw		= vAngles[YAW];
				pBot->ideal_pitch	= 30; 
			
				// stupid...
				yaw_degrees = Bot_ChangeYaw(ent);

				if( yaw_degrees < 5 )
				{
					if( ent->client->ps.weaponTime == 0 )
					{
						if( !(ent->client->pers.cmd.buttons & BUTTON_ATTACK) )
						{
							pBot->ucmd.buttons |= BUTTON_ATTACK;
						}
					}
					else
					{
						// weaponTime greater than 0 means grenade has been thrown
						Bot_TaskComplete(pBot);

						Bot_SendRadioCommand( ent, RADIO_FIREINTHEHOLE, TRUE );

						pBot->iWeapon = Bot_SelectWeapon(ent);
					}
				}

			}

			break;

			// but trying to press some button
		case TASK_PRESSBUTTON:

			if( pBot->pButton == NULL )
			{
				while( (pBot->pButton = Bot_FindRadius( pBot->pButton, ent->client->ps.origin, 300.0f )) != NULL )
				{
					// normal button
					if( !Q_stricmp( pBot->pButton->classname, "func_button" ) )
					{
						break;
					}
					else if( !Q_stricmp( pBot->pButton->classname, "func_invisible_user" ) )
					{
						// wolfenstein stuff
						break;
					}
				}

				if( pBot->pButton == NULL )
				{
					// didn't find anything so just consider this task as complete
					Bot_TaskComplete(pBot);

					Bot_Log( "Bot(%i) : couldn't find button to door at Waypoint %i", ent->client->ps.clientNum, pBot->iCurWaypoint );
				}
			}

			if( pBot->pButton )
			{
				vec3_t vBModelOrg, vDiff;

				VectorSubtract( pBot->pButton->r.maxs, pBot->pButton->r.mins, vDiff );
				VectorScale( vDiff, 0.5, vDiff );
				VectorAdd( pBot->pButton->r.mins, vDiff, vBModelOrg );

				VectorSubtract( vBModelOrg, ent->client->ps.origin, vDiff );

				vectoangles( vDiff, vAngles );
				pBot->ideal_yaw = vAngles[YAW];
				pBot->ideal_pitch = -vAngles[PITCH];

				if( VectorLength(vDiff) > 75 )
				{
					pBot->ucmd.forwardmove = 127;
				}
				else
				{
					// hack
					if( !Q_stricmp( pBot->pButton->classname, "func_button" ) )
					{
						Use_BinaryMover (pBot->pButton, ent, ent);
						pBot->pButton->active = qtrue;
					}
					else if( !Q_stricmp( pBot->pButton->classname, "func_invisible_user" ) )
					{
						pBot->pButton->use( pBot->pButton, ent, ent );
						pBot->pButton->flags &= ~FL_KICKACTIVATE;	// reset
					}

				//	pBot->ucmd.buttons |= BUTTON_ACTIVATE;
					pBot->pButton = NULL;
					Bot_TaskComplete(pBot);

					// free path nodes and give the bot back its old goal destination
					Bot_FreeSolutionNodes(pBot);
					pBot->iGoalWaypoint = pBot->iLongTermGoal;

					if( g_debugBots.integer )
					{
						G_Printf("New Goal : Long Term Goal %i\n", pBot->iLongTermGoal);
					}
				}
			}

			break;

			// bot trying to activate an MG42
		case TASK_ACTIVATEMG42:

			// check for expire time...

			if( pBot->pMG42 == NULL )
			{
				while( (pBot->pMG42 = Bot_FindRadius( pBot->pMG42, ent->client->ps.origin, 150.0f )) != NULL )
				{
					if( !Q_stricmp( pBot->pMG42->classname, "misc_mg42" ) )
					{
						break;
					}
				}

				// could not find the entity ?
				if( pBot->pMG42 == NULL )
				{
					if( g_debugBots.integer )
					{
						G_Printf("Bot (%i) : Warning , could not find MG42 at MG42 Waypoint\n", ent->client->ps.clientNum);
					}

					Bot_TaskComplete(pBot);
					break;
				}
			}

			if( pBot->pMG42 )
			{
				// If the gun is damaged bot can't operate it
				if( pBot->pMG42->s.eFlags & EF_SMOKING )
				{
					pBot->pMG42 = NULL;
					Bot_TaskComplete(pBot);
					break;
				}

				// trace a line to see if anyone else is already using the MG42
				trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, pBot->pMG42->r.currentOrigin, ent->client->ps.clientNum, MASK_PLAYERSOLID );

				if( tr.entityNum < MAX_CLIENTS )
				{
					if( g_debugBots.integer )
					{
						G_Printf("Bot (%i) : Can't operate MG42 because Client %i is already using it.\n", ent->client->ps.clientNum, tr.entityNum );
					}

					pBot->pMG42 = NULL;
					Bot_TaskComplete(pBot);
					break;
				}

				// face it
				VectorSubtract( pBot->pMG42->r.currentOrigin, ent->client->ps.origin, vDir );
				vectoangles( vDir, vAngles );

				pBot->ideal_yaw = vAngles[YAW];
				pBot->ideal_pitch = -vAngles[PITCH];

				// approach it slowly
				if( VectorLength(vDir) > 100 )
				{
					pBot->ucmd.forwardmove = 50;
				}
				else
				{
					// close enough to activate it
					if( !(ent->client->ps.eFlags & EF_MG42_ACTIVE) )
					{
						// keep pushing button until its activated
						if( crandom() > 0 )
							pBot->ucmd.buttons |= BUTTON_ACTIVATE;
					}
				}

				// activated ?
				if( ent->client->ps.eFlags & EF_MG42_ACTIVE )
				{
					pBot->iMG42StartTime = level.time;
					Bot_TaskComplete(pBot);

					if( g_debugBots.integer )
					{
						G_Printf("Bot (%i) : Successfully activated MG42 Entity.\n", ent->client->ps.clientNum);
					}
				}
			}

			break;
	}

	// WolfBot 1.5 : Don't use an MG42 for longer than 45 seconds
	if( ent->client->ps.eFlags & EF_MG42_ACTIVE )
	{
		// If bot has not seen an enemy but has been operating the MG42 for at least 8 seconds
		// allow to dismouns. We don't want camping bots at MG42s...
		if( (level.time > pBot->iMG42StartTime + 8000) && (pBot->iEnemyLastSeen < level.time - 10000) )
		{
			Cmd_Activate_f(ent);
		}

		if( level.time > pBot->iMG42StartTime + 45000 )
		{
			// unmount as soon as done fighting enemy
			if( Bot_GetTask(pBot)->iTask != TASK_ATTACK )
			{
				Cmd_Activate_f(ent);
			}
		}
	}

	// WolfBot 1.5 : Check for near teammats if carrying the enemy docs
	if( Bot_HasEnemyFlag(ent) )
	{
		if( level.time > pBot->iCheckBackupTime )
		{
			pBot->iCheckBackupTime = level.time + 10000 + rand() % 2000;
			
			if( Bot_LookForTeammates(ent) == TRUE )
			{
				if( (rand() % 100) > 50 )
				{
					Bot_SendDelayedRadioCommand( ent, RADIO_FOLLOWME, TRUE );
				}
				else
				{
					Bot_SendDelayedRadioCommand( ent, RADIO_COVERME, TRUE );
				}
			}
		}
	}

	// check if we need a medic to heal us
	if( ent->client->ps.stats[STAT_HEALTH] <= 35 )
	{
		if( level.time > pBot->iCheckMedicTime )
		{
			pBot->iCheckMedicTime = level.time + 8000;

			// maybe have the bot stand still for 2 or 3 seconds ?
			if( Bot_LookForMedic(ent) == TRUE )
			{
				if( (rand() % 100) > ent->client->ps.stats[STAT_HEALTH] )
				{
					Bot_SendDelayedRadioCommand( ent, RADIO_MEDIC, TRUE );
				}
			}
		}
	}

	// check for duck jump
	if( level.time - ent->client->ps.jumpTime < 1000 )
	{
		// still in air
		if( ent->s.groundEntityNum == ENTITYNUM_NONE )
		{
			if( pBot->iDuckJumpTime > level.time )
			{
				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) : Performing a duckjump\n", ent->client->ps.clientNum);
				}
				pBot->ucmd.upmove = -127;
			}
		}
	}

	if( pBot->iStrafeTime > level.time )
		pBot->ucmd.rightmove = 127 * pBot->iStrafeDir;

	// don't move if turning alot
	if( (yaw_degrees > 20) && (bHasEnemy == FALSE) )
	{
		pBot->ucmd.forwardmove = 0;

		// bot is NOT stuck so set this ...
		pBot->iLastMoveTime = level.time;
	}

	pBot->ucmd.weapon = pBot->iWeapon;

	pBot->iPrevSpeed = abs(pBot->ucmd.forwardmove);

	trap_BotUserCommand( ent->client->ps.clientNum, &pBot->ucmd );

}

//
// Bot Task functions
//

//
// Pushes a task onto the stack
//

void Bot_PushTask( botClient_t *pBot, bottask_t *pTask )
{
	bottask_t *newTask;

	if( pBot->pTasks != NULL )
	{
		if( pBot->pTasks->iTask == pTask->iTask )
			return;
	}

	if( g_debugBots.integer )
	{
		G_Printf("Pushing Task %i on Stack for Bot (%i)\n", pTask->iTask, pBot->clientNum );
	}

	newTask = (bottask_t*) malloc( sizeof(bottask_t) );

	mem_amount += sizeof(bottask_t);

	newTask->iTask			= pTask->iTask;
	newTask->iExpireTime	= pTask->iExpireTime;
	newTask->iIndex			= pTask->iIndex;
	newTask->pNextTask		= NULL;
	newTask->pPrevTask		= NULL;

	if( pBot->pTasks != NULL )
	{
		while( pBot->pTasks->pNextTask )
			pBot->pTasks = pBot->pTasks->pNextTask;

		pBot->pTasks->pNextTask = newTask;
		newTask->pPrevTask = pBot->pTasks;
	}

	pBot->pTasks = newTask;
}

//
// Gets the task of the top of the stack
//

bottask_t *Bot_GetTask( botClient_t *pBot )
{
	// always return a task
	if( pBot->pTasks == NULL )
	{
		bottask_t temp;

		temp.pPrevTask = NULL;
		temp.pNextTask = NULL;
		temp.iExpireTime = -1;
		temp.iIndex = -1;
		temp.iTask = TASK_ROAM;

		Bot_PushTask( pBot, &temp );
	}

	return pBot->pTasks;
}

//
// Removes task on top of stack
// fixme : make sure previous task is not expired 
//

void Bot_TaskComplete( botClient_t *pBot )
{
	bottask_t *pPrevTask;

	if( pBot->pTasks == NULL )
	{
		Bot_FreeSolutionNodes(pBot);
		return;
	}

	pPrevTask = pBot->pTasks->pPrevTask;

	free( pBot->pTasks );
	mem_amount -= sizeof(bottask_t);

	pBot->pTasks = NULL;

	if( pPrevTask != NULL )
	{
		pPrevTask->pNextTask = NULL;
		pBot->pTasks = pPrevTask;
	}

	Bot_FreeSolutionNodes(pBot);
}

//
// Removes all Tasks on stack and frees up allocated memory
//

void Bot_RemoveTasks( botClient_t *pBot )
{
	bottask_t *pNextTask;
	bottask_t *pPrevTask;

	if( pBot->pTasks == NULL )
		return;

	pNextTask = pBot->pTasks->pNextTask;
	pPrevTask = pBot->pTasks;
	
	while( pPrevTask != NULL )
	{
		pPrevTask = pBot->pTasks->pPrevTask;
		free( pBot->pTasks );
		mem_amount -= sizeof(bottask_t);
		pBot->pTasks = pPrevTask;
	}

	pBot->pTasks = pNextTask;

	while( pNextTask != NULL )
	{
		pNextTask = pBot->pTasks->pNextTask;
		free( pBot->pTasks );
		mem_amount -= sizeof(bottask_t);
		pBot->pTasks = pNextTask;
	}

	pBot->pTasks = NULL;
}



float Bot_ChangeYaw( gentity_t *ent )
{
	float total;
	float move;
	float angle;
	float ideal_angle;
	float speed;

	// cmd.angle ohne delta angle
	angle = AngleMod(ent->client->ps.viewangles[YAW] - SHORT2ANGLE(ent->client->ps.delta_angles[YAW]) );

  // calculate ideal angle
	ideal_angle = AngleMod(ent->botClient->ideal_yaw - SHORT2ANGLE(ent->client->ps.delta_angles[YAW]) );

	if( ent->botClient->pSkill->bInstantTurn )
	{
		ent->botClient->ucmd.angles[YAW] = ANGLE2SHORT(ideal_angle);
		return 0;
	}

	if( angle == ideal_angle )
	{
		ent->botClient->ucmd.angles[YAW] = ANGLE2SHORT(angle);
		return 0;
	}

	move = total = ideal_angle - angle;
	speed = (float) ent->botClient->pSkill->iMaxYawSpeed / trap_Cvar_VariableIntegerValue( "sv_fps" );
	
	if( ideal_angle > angle ) 
	{
		if( move > 180.0 ) 
			move -= 360.0;
	}
	else 
	{
		if( move < -180.0 ) 
			move += 360.0;
	}

	if( move > 0 ) 
	{
		if( move > speed ) 
			move = speed;
	}
	else 
	{
		if( move < -speed ) 
			move = -speed;
	}

	ent->botClient->ucmd.angles[YAW] = ANGLE2SHORT( AngleMod(angle + move) );

	return fabs(total);
}

float Bot_ChangePitch( gentity_t *ent )
{
	float total;
	float move;
	float angle;
	float ideal_angle;
	float speed;

	ent->botClient->ideal_pitch = AngleMod(ent->botClient->ideal_pitch);

	if( ent->botClient->ideal_pitch > 180 )
	{
		ent->botClient->ideal_pitch -= 360;
	}

	ideal_angle = ent->botClient->ideal_pitch - SHORT2ANGLE(ent->client->ps.delta_angles[PITCH]);
	angle = ent->client->ps.viewangles[PITCH] - SHORT2ANGLE(ent->client->ps.delta_angles[PITCH]);

	if( ent->botClient->pSkill->bInstantTurn )
	{
		ent->botClient->ucmd.angles[PITCH] = ANGLE2SHORT(ideal_angle);
		return 0;
	}

	if( ideal_angle == angle )
	{
		ent->botClient->ucmd.angles[PITCH] = ANGLE2SHORT(angle);
		return 0;
	}

	move = total = ideal_angle - angle;
	speed = ent->botClient->pSkill->iMaxPitchSpeed / trap_Cvar_VariableIntegerValue( "sv_fps" );

	if( move > 0 )
	{
		if( move > speed )
			move = speed;
	}
	else
	{
		if( move < -speed )
			move = -speed;
	}

	ent->botClient->ucmd.angles[PITCH] = ANGLE2SHORT( AngleMod(angle + move) );

	return fabs(total);
}

//
// Checks if bot needs to change player class 
//

BOOL Bot_CheckPlayerClass( gentity_t *ent )
{
	int count;
	int i;

	if( wb_autoEngineer.integer == 0 )
		return FALSE;

	// Bot might have to change the player class if
	// there is no engineer on his team anymore and there is 
	// a bomb objective on this map

	if( ent->client->sess.playerType == PC_ENGINEER )
		return FALSE;

	count = CountTeamPlayers( ent->client->sess.sessionTeam, PC_ENGINEER );

	if( count > 0 )
		return FALSE;

	for( i = 0; i < level.numConnectedClients; i++ )
	{
		if( g_entities[i].botClient )
		{
			if( g_entities[i].botClient->playerClass == PC_ENGINEER )
			{
				return FALSE;
			}
		}
	}

	ent->botClient->playerClass = PC_ENGINEER;
	ent->botClient->bChangedClass = TRUE;

	return TRUE;
}

//
// Called upon respawn to re-initialize bot stuff
//

void Bot_Initialize( botClient_t *pBot )
{

	pBot->pEnemy				= NULL;
	pBot->pUser					= NULL;
	pBot->bChangedClass			= FALSE;
	pBot->pItem					= NULL;
	pBot->iLookForItemsTime		= 0;
	pBot->iEnemyUpdateTime		= 0;
	pBot->iUseTime				= 0;
	pBot->ideal_yaw				= 0;
	pBot->ideal_pitch			= 0;
	pBot->iRadioOrder			= 0;
	pBot->pRadioEmitter			= NULL;
	pBot->iRadioDelayTimer		= 0;
	pBot->iRadioSendCmd			= -1;
	pBot->iRadioLastUseTime		= 0;

	pBot->iLastNoiseTime		= 0;
	VectorSet( pBot->vNoiseOrigin, 0, 0, 0 );

	pBot->iCurWaypoint			= -1;
	pBot->iGoalWaypoint			= -1;
	pBot->iCurWaypointTime		= level.time;
	pBot->iPathFlags			= 0;

	pBot->iGoalHistory[0]		= -1;
	pBot->iGoalHistory[1]		= -1;
	pBot->iGoalHistory[2]		= -1;
	pBot->iGoalHistory[3]		= -1;
	pBot->iGoalHistory[4]		= -1;

	pBot->iLongTermGoal			= -1;

	pBot->iCampTurnTime			= 0;
	pBot->iLastCampTime			= 0;

	pBot->pFriend				= NULL;
	pBot->iFriendHelpTime		= 0;
	pBot->iCheckMedicTime		= 0;

	pBot->iCheckBackupTime		= 0;

	pBot->iLastMoveTime			= 0;
	pBot->iStrafeTime			= 0;
	pBot->iLastJumpTime			= 0;

	VectorSet( pBot->vPrevOrigin, 0, 0, 0 );

	pBot->iWeapon				= 0;
	pBot->iEnemyLastSeen		= 0;
	pBot->iEnemyFirstSeen		= 0;
	pBot->bReloading			= FALSE;
	pBot->iFightStyle			= 0;
	pBot->iFSUpdateTime			= 0;

	pBot->pDynamite				= NULL;
	pBot->pDocuments			= NULL;
	pBot->pCheckpoint			= NULL;

	pBot->pButton				= NULL;

	pBot->iLastMG42Time			= -1;
	pBot->iMG42StartTime		= 0;
	pBot->pMG42					= NULL;

	// clear out all tasks
	Bot_RemoveTasks(pBot);

	// free path nodes
	Bot_FreeSolutionNodes(pBot);

	pBot->bInitialized = TRUE;

	if( g_debugBots.integer )
		G_Printf("Re-Initialized Bot (%i)\n", pBot->clientNum);
}

//
// Finds all entities within the specified radius
//

gentity_t *Bot_FindRadius( gentity_t *from, vec3_t org, float rad )
{
	vec3_t	eorg;
	int		j;

	if( !from )
		from = g_entities;
	else
		from++;

	for ( ; from < &g_entities[level.num_entities]; from++ )
	{
		if (!from->inuse)
			continue;

		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (from->r.currentOrigin[j] + (from->r.mins[j] + from->r.maxs[j])*0.5);

		if (VectorLength(eorg) > rad)
			continue;

		return from;
	}

	return NULL;
}

//
// Returns TRUE if ray trace from ent to other succeeds
//

BOOL Bot_EntityIsVisible( gentity_t *ent, gentity_t *other )
{
	trace_t tr;

	if( other->client )
	{
		// ugly
		if( other->client->ps.pm_type == PM_DEAD && !(other->client->ps.pm_flags & PMF_LIMBO) )
		{
			trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, other->client->ps.origin, ent->client->ps.clientNum, MASK_SHOT );
		}
		else
		{
			trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, other->client->ps.origin, ent->client->ps.clientNum, MASK_PLAYERSOLID );
		}
	}
	else
	{
		trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, other->r.currentOrigin, ent->client->ps.clientNum, MASK_SOLID|CONTENTS_ITEM );
	}

	if( tr.entityNum != other->s.number )
	{
		return FALSE;
	}

	return TRUE;
}

//
// Returns TRUE if bot can see ent
//

BOOL Bot_EntityInFOV( gentity_t *ent, gentity_t *other )
{
	trace_t tr;
	vec3_t	forward;
	vec3_t	vector;
	float	dot;

	if( other->client )
	{
		// WolfBot 1.5 Trace from the gun muzzle if operating an MG42
		if( ent->client->ps.eFlags & EF_MG42_ACTIVE )
		{
			vec3_t vMuzzle, vUp, vEnd;
			gentity_t *pMG42;

			pMG42 = &g_entities[ ent->client->ps.viewlocked_entNum ];

			AngleVectors( ent->client->ps.viewangles, NULL, NULL, vUp );
			VectorCopy( pMG42->s.pos.trBase, vMuzzle );
			VectorMA( vMuzzle, 16, vUp, vMuzzle );

			// shift up because MG42 locks view
			VectorCopy( other->client->ps.origin, vEnd );
			vEnd[2] += 20;

			trap_Trace( &tr, vMuzzle, NULL, NULL, vEnd, pMG42->s.number, MASK_PLAYERSOLID );
		}
		else
		{
			trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, other->client->ps.origin, ent->client->ps.clientNum, MASK_PLAYERSOLID );
		}
	}
	else
	{
		trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, other->r.currentOrigin, ent->client->ps.clientNum, MASK_SHOT|CONTENTS_ITEM );
	}

	if( tr.entityNum != other->s.number )
	{
		return FALSE;
	}

	// now check if the entity is in the fov of the bot
	AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
	VectorSubtract( other->r.currentOrigin, ent->client->ps.origin, vector );
	VectorNormalize(vector);
	
	dot = DotProduct( vector, forward );

	if( dot > 0.7f )
		return TRUE;

	return FALSE;
}


//
// Finds nearby items such as dropped weapons or ammo
//

BOOL Bot_FindItem( gentity_t *ent )
{
	float		fRadius		= 150.0f;
	float		fDistance;
	float		fBestDistance;
	vec3_t		ItemDir;
	int			iAmmo;
	int			iWeapon;
	int			i;
	gentity_t	*pEnt		= NULL;
	gentity_t	*pBestItem	= NULL;
	botClient_t *pBot		= ent->botClient;

	fBestDistance = fRadius;

	// don't look for items if trying to help a friend...
	if( pBot->pFriend )
	{
		pBot->pItem = NULL;
		return FALSE;
	}

	if( pBot->pItem )
	{
		// item was freed
		if( pBot->pItem->inuse == FALSE )
		{
			pBot->pItem = NULL;
		}
		else if( level.time > pBot->iItemPickupTime + 2000 )
		{
			// Don't let bot head for an item for a too long time, might get stuck...
			pBot->pItem = NULL;
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}

	if( pBot->pItem == NULL )
	{
		while( (pEnt = Bot_FindRadius( pEnt, ent->client->ps.origin, fRadius )) != NULL )
		{
			
			if( Bot_EntityInFOV( ent, pEnt ) == FALSE )
				continue;

			// Check if Bot should drop current weapon to pickup better weapon
			if( (pEnt->item) && (pEnt->item->giType == IT_WEAPON) && (pBot->pSkill->bPickupWeapons) )
			{
				for( i = 0; i < MAX_WEAPS_IN_BANK_MP; i++ )
				{
					if( COM_BitCheck( ent->client->ps.weapons, weapBanksMultiPlayer[3][i] ) )
					{
						iWeapon = weapBanksMultiPlayer[3][i];
					}
				}

				iAmmo = ent->client->ps.ammo[ BG_FindAmmoForWeapon(iWeapon) ];
				iAmmo += ent->client->ps.ammoclip[ BG_FindAmmoForWeapon(iWeapon) ];

				if( pBot->playerClass == PC_SOLDIER || pBot->playerClass == PC_LT )
				{
					// bot has no ammo for primary but pickup weapon has some ammo
					if( iAmmo == 0 && pEnt->count > 0 )
					{
						if( pEnt->item->giTag == WP_MP40 || pEnt->item->giTag == WP_THOMPSON || pEnt->item->giTag == WP_STEN )
						{
							// drop primary weapon now
							pBot->ucmd.wbuttons |= WBUTTON_DROP;

							pBestItem = pEnt;
							break;
						}
						else
						{
							// all other weapons can only be picked up by soldiers
							if( pBot->playerClass == PC_SOLDIER )
							{
								pBot->ucmd.wbuttons |= WBUTTON_DROP;
								pBestItem = pEnt;
								break;
							}
						}
					}
				}
			}

			// dynamite might need to defuse it
			else if( !Q_stricmp( pEnt->classname, "dynamite" ) )
			{
				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) : Found a dynamite.\n", ent->client->ps.clientNum);
				}

				if( pBot->playerClass == PC_ENGINEER )
				{
					// only disarm enemy dynamite
					if( pEnt->parent->client->sess.sessionTeam != ent->client->sess.sessionTeam )
					{
						// armed anyway ?
						if( pEnt->think == G_ExplodeMissile )
						{
							// need to disarm now
							bottask_t task;

							pBot->pDynamite = pEnt;

							task.iExpireTime = -1;
							task.iIndex = -1;
							task.iTask = TASK_DISARMDYNAMITE;
							task.pNextTask = NULL;
							task.pPrevTask = NULL;

							Bot_PushTask( pBot, &task );

							if( g_debugBots.integer )
							{
								G_Printf("Bot (%i) : Need to defuse enemy dynamite.\n", ent->client->ps.clientNum);
							}

							return FALSE;
						}
					}
				}
			}

			// bot can pick up this item
			else if( (pEnt->item) && (BG_CanItemBeGrabbed( &pEnt->s, &ent->client->ps )) )
			{
				vec3_t tmp;

				// don't pickup own items so Medics won't pickup their own medkits.
				// if a bot wants to heal himself he just drops a medkit while running
				if( pEnt->parent == ent )
					continue;

				VectorSubtract( pEnt->r.currentOrigin, ent->client->ps.origin, tmp );
				fDistance = VectorLength(tmp);

				if( fDistance < fBestDistance )
				{
					fBestDistance = fDistance;
					pBestItem = pEnt;
					VectorCopy( tmp, ItemDir );
				}

			}
		}
	}

	// did we find an item ?
	if( pBestItem )
	{
		vec3_t angles;
		vectoangles( ItemDir, angles );

		pBot->ideal_yaw = angles[YAW];
		pBot->pItem = pBestItem;
		pBot->iItemPickupTime = level.time;

		return TRUE;
	}

	return FALSE;
}

//
// Returns TRUE if bot can see a medic nearby
//

BOOL Bot_LookForMedic( gentity_t *ent )
{
	int i;
	gentity_t *other;

	for( i = 0; i < level.numConnectedClients; i++ )
	{
		other = &g_entities[i];

		if( !OnSameTeam( ent, other ) ) 
			continue;

		if( other->client->ps.stats[STAT_PLAYER_CLASS] != PC_MEDIC )
			continue;

		if( other->client->ps.pm_type == PM_DEAD )
			continue;

		if( Distance( other->client->ps.origin, ent->client->ps.origin ) > 700 )
			continue;

		if( Bot_EntityIsVisible( ent, other ) )
			return TRUE;
	}

	return FALSE;
}

//
// Returns TRUE if bot can see any teammates nearby
//

BOOL Bot_LookForTeammates( gentity_t *ent )
{
	int i;
	gentity_t *other;

	for( i = 0; i < level.numConnectedClients; i++ )
	{
		other = &g_entities[i];

		if( !OnSameTeam( ent, other ) )
			continue;

		if( other->client->ps.pm_type == PM_DEAD )
			continue;

		if( Distance( other->client->ps.origin, ent->client->ps.origin ) > 1000 )
			continue;

		if( Bot_EntityIsVisible( ent, other ) )
			return TRUE;
	}

	return FALSE;
}

//
// Returns TRUE if bot is following a teammate 
//

BOOL Bot_FollowUser( gentity_t *ent )
{
	BOOL	bVisible;
	vec3_t	distance, angles;
	float	fDist;
	botClient_t *pBot = ent->botClient;

	if( !pBot->pUser )
		return FALSE;

	if( pBot->pUser->client->ps.pm_type == PM_DEAD )
	{
		pBot->pUser = NULL;
		return FALSE;
	}

	bVisible = Bot_EntityInFOV( ent, pBot->pUser );

	if( (bVisible) || (pBot->iUseTime + 5000 > level.time) )
	{
		if( bVisible )
			pBot->iUseTime = level.time;

		VectorSubtract( pBot->pUser->client->ps.origin, ent->client->ps.origin, distance );
		vectoangles( distance, angles );

		// face the User
		pBot->ideal_yaw = angles[YAW];
		pBot->ideal_pitch = angles[PITCH];

		fDist = VectorLength(distance);

		if( fDist > 80 )
			pBot->ucmd.forwardmove = 127;
		else if( fDist > 80 )
			pBot->ucmd.forwardmove = 60;
		else
			pBot->ucmd.forwardmove = 0;
		
		if( pBot->pUser->client->pers.cmd.upmove < 0 ) 
			pBot->ucmd.upmove = -127; 

		pBot->ucmd.buttons = pBot->pUser->client->pers.cmd.buttons & BUTTON_SPRINT;
		
		return TRUE;
	}
	else
	{
		pBot->pUser = NULL;
	}

	return FALSE;
}

//
// Returns TRUE if bot is attempting to help a friend , i.e. dropping a healthkit or ammo
//

BOOL Bot_HelpingFriend( gentity_t *ent )
{
	vec3_t		vDir;
	vec3_t		vAngles;
	int			iChargeTime;
	botClient_t *pBot;
	BOOL		bSyringe;
	float		fDistance;

	pBot = ent->botClient;

	if( pBot->playerClass != PC_MEDIC && pBot->playerClass != PC_LT )
		return FALSE;

	iChargeTime = pBot->playerClass == PC_MEDIC ? g_medicChargeTime.integer : g_engineerChargeTime.integer;
	bSyringe = FALSE;

	if( pBot->pFriend )
	{
		// don't let the bot follow a friend for too long
		if( level.time > (pBot->iFriendHelpTime + 4000) )
		{
			Bot_Log("Bot (%i) : could not reach Friend in time", pBot->clientNum);
			
			pBot->pFriend = NULL;
			return FALSE;
		}

		// check if teammate needs syringe
		if( pBot->pFriend->client->ps.pm_type == PM_DEAD && !(pBot->pFriend->client->ps.pm_flags & PMF_LIMBO) )
		{
			bSyringe = TRUE;

			// if we don't have any syringes left , can't help him!
			if( ent->client->ps.ammoclip[ BG_FindAmmoForWeapon(WP_MEDIC_SYRINGE) ] == 0 )
			{
				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) : Sorry no Syringes left!\n", ent->client->ps.clientNum);
				}

				pBot->pFriend = NULL;
				return FALSE;
			}
		}

		// can't help him!
		if( pBot->playerClass == PC_LT && bSyringe == TRUE )
		{
			pBot->pFriend = NULL;
			return FALSE;
		}

		// this is another hack : effect3Time is set in Weapon_Syringe
		// so use it to check if we just revived our friend a few frames ago
		// I don't want to touch the original Game Code unless absolutely neccessary
    // so that's why I'm doing it like this.
		if( pBot->pFriend->s.effect3Time + 1000 > level.time )
		{
			if( g_debugBots.integer )
				G_Printf("Bot (%i) : I just revived you, now goodbye!\n", ent->client->ps.clientNum);

			pBot->pFriend = NULL;

			// select good weapon again
			if( ent->client->ps.weapon == WP_MEDIC_SYRINGE )
			{
				pBot->iWeapon = Bot_SelectWeapon(ent);
			}

			return FALSE;
		}

		// look down since friend is laying on the ground
		if( bSyringe == TRUE )
		{
			vec3_t vEyeVector;

			VectorCopy( ent->client->ps.origin, vEyeVector );
			vEyeVector[2] += ent->client->ps.viewheight;

			VectorSubtract( pBot->pFriend->client->ps.origin, vEyeVector, vDir );
		}
		else
		{
			VectorSubtract( pBot->pFriend->client->ps.origin, ent->client->ps.origin, vDir );
		}

		vectoangles( vDir, vAngles );
		fDistance = VectorLength(vDir);

		pBot->ideal_yaw = vAngles[YAW];
		pBot->ideal_pitch = vAngles[PITCH];

		// select the appropriate weapon
		if( pBot->playerClass == PC_MEDIC )
		{
			if( bSyringe )
				pBot->iWeapon = WP_MEDIC_SYRINGE;
			else
				pBot->iWeapon = WP_MEDKIT;
		}
		else if( pBot->playerClass == PC_LT )
		{
			pBot->iWeapon = WP_AMMO;
		}

		// get going
		pBot->ucmd.forwardmove = 127;

		if( fDistance < 150 && bSyringe == TRUE && pBot->playerClass == PC_MEDIC )
		{
			pBot->ucmd.upmove = -127;
		}

		if( fDistance < 50 && bSyringe == TRUE && pBot->playerClass == PC_MEDIC )
		{
			pBot->ucmd.upmove = -127;

			// need to get pretty close for syringe injection
			if( ent->client->ps.weapon == WP_MEDIC_SYRINGE )
			{
				if( ent->client->ps.weaponTime == 0 )
				{
					pBot->ucmd.buttons |= BUTTON_ATTACK;
				}
			}
		}
		else if( fDistance < 150 )
		{
			if( ent->client->ps.weapon == WP_MEDKIT || ent->client->ps.weapon == WP_AMMO )
			{
				if( ent->client->ps.weaponTime == 0 )
				{
					// check charge time again just to be sure
					if( level.time - ent->client->ps.classWeaponTime >= iChargeTime * 0.25f )
					{
						pBot->ucmd.buttons |= BUTTON_ATTACK;
					}

					pBot->pFriend = NULL;
				}
			}
		}

		return TRUE;
	}

	return FALSE;
}

//
// Returns TRUE if Bot is operating an MG42
//

BOOL Bot_OperatingMG42( gentity_t *ent )
{
	if( ent->client->ps.eFlags & EF_MG42_ACTIVE )
		return TRUE;

	return FALSE;
}

//
// Respond to Radio messages
//

void Bot_CheckRadioMessages( gentity_t *ent )
{
	botClient_t *pBot = ent->botClient;
	bottask_t task;
	int i;
	WAYPOINT *pWaypoint;
	int ObjIndex;
	float fDistance, fNearest;
	BOOL bFree;

	if( pBot->iRadioOrder == 0 )
		return;

	switch( pBot->iRadioOrder )
	{
		case RADIO_FOLLOWME:

			// Bot already following this or another player so ignore request
			if( pBot->pUser )
				break;

			// Bot can see player , so follow him
			if( Bot_EntityIsVisible( ent, pBot->pRadioEmitter ) )
			{
				pBot->pUser = pBot->pRadioEmitter;
				pBot->iUseTime = level.time;
				Bot_SendDelayedRadioCommand( ent, RADIO_YES, TRUE );

				// free all other tasks
				Bot_RemoveTasks(pBot);
				pBot->iCurWaypoint = -1;

				task.iExpireTime = level.time + 7000 + rand() % 5000;
				task.iIndex = -1;
				task.iTask = TASK_ACCOMPANY;
				task.pNextTask = NULL;
				task.pPrevTask = NULL;

				Bot_PushTask( pBot, &task );
			}

			break;

		case RADIO_MEDIC:

			if( pBot->playerClass == PC_MEDIC )
			{
				// only react if nothing important to do
				if( Bot_GetTask(pBot)->iTask == TASK_ROAM )
				{
					// charged ?
					if( level.time - ent->client->ps.classWeaponTime >= g_medicChargeTime.integer * 0.25f )
					{
						if( Bot_EntityIsVisible( ent, pBot->pRadioEmitter ) )
						{
							if( Distance( ent->client->ps.origin, pBot->pRadioEmitter->client->ps.origin ) < 600 )
							{
								// randomize a bit
								if( (rand() % 100) > 30 )
								{
									pBot->pFriend = pBot->pRadioEmitter;
									pBot->iFriendHelpTime = level.time;

									Bot_SendDelayedRadioCommand( ent, RADIO_YES, TRUE );
								}
							}
						}
					}
				}
			}

			break;

		case RADIO_NEEDAMMO:

			if( pBot->playerClass == PC_LT )
			{
				// only react if nothing important to do
				if( Bot_GetTask(pBot)->iTask == TASK_ROAM )
				{
					// charged ?
					if( level.time - ent->client->ps.classWeaponTime >= g_LTChargeTime.integer * 0.25f )
					{
						if( Bot_EntityIsVisible( ent, pBot->pRadioEmitter ) )
						{
							if( Distance( ent->client->ps.origin, pBot->pRadioEmitter->client->ps.origin ) < 600 )
							{
								// randomize a bit
								if( (rand() % 100) > 40 )
								{
									pBot->pFriend = pBot->pRadioEmitter;
									pBot->iFriendHelpTime = level.time;

									Bot_SendDelayedRadioCommand( ent, RADIO_YES, TRUE );
								}
							}
						}
					}
				}
			}

			break;

		// WolfBot 1.5 : This checks if there are any bomb spots near the radio emitter
		// and sets the bot off to the nearest bomb spot of the radio emitter
		case RADIO_NEEDENGINEER:

			fNearest	= 99999;
			fDistance	= 0;
			ObjIndex	= -1;
			bFree		= TRUE;

			if( pBot->playerClass == PC_ENGINEER )
			{
				if( Bot_GetTask(pBot)->iTask == TASK_ROAM )
				{
					// at least 3/4 charged
					if( level.time - ent->client->ps.classWeaponTime >= g_engineerChargeTime.integer * 0.75f )
					{
						if( (rand() % 100) > 40 )
						{
							if( Distance( pBot->pRadioEmitter->client->ps.origin, ent->client->ps.origin ) < 4000 )
							{
								// look if there's a bomb spot near the radio emitter guy
								for( i = 0; i < numObjWaypoints; i++ )
								{
									pWaypoint = &Waypoints[ ObjWaypoints[ i ] ];

									// even bother ?
									if( pWaypoint->objdata.state == STATE_ACCOMPLISHED )
										continue;

									if( pWaypoint->objdata.type != OBJECTIVE_BOMB )
										continue;

									if( (pWaypoint->flags & FL_WP_AXIS_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_AXIS) )
										continue;

									if( (pWaypoint->flags & FL_WP_ALLIES_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_ALLIES) )
										continue;

									// distance check
									fDistance = Distance( pWaypoint->origin, pBot->pRadioEmitter->client->ps.origin );

									if( fDistance > 500 )
										continue;

									// get nearest objective from radio guy
									if( fDistance < fNearest )
									{
										fNearest = fDistance;
										ObjIndex = pWaypoint->index;
									}
								}

								// found a spot see if we can calculate a path to it
								if( ObjIndex != -1 )
								{
									// look if any other teammates are already heading for this waypoint
									for( i = 0; i < level.numConnectedClients; i++ )
									{
										gentity_t *pPlayer;

										pPlayer = &g_entities[i];

										if( pPlayer->r.svFlags & SVF_BOT )
										{
											if( OnSameTeam( ent, pPlayer ) )
											{
												if( pPlayer->botClient->playerClass == PC_ENGINEER )
												{
													if( pPlayer->botClient->iGoalWaypoint == ObjIndex )
													{
														bFree = FALSE;
													}
												}
											}
										}
									}

									if( bFree == TRUE )
									{

										bottask_t botTask;

										Bot_RemoveTasks(pBot);
									
										botTask.iExpireTime		= -1;
										botTask.iIndex			= ObjIndex;
										botTask.iTask			= TASK_ROAM;
										botTask.pNextTask		= NULL;
										botTask.pPrevTask		= NULL;

										Bot_PushTask( pBot, &botTask );
										Bot_FreeSolutionNodes(pBot);

										pBot->iGoalWaypoint = ObjIndex;

										Bot_SendDelayedRadioCommand( ent, RADIO_YES, TRUE );
									}
								}
							}
						}
					}
				}
			}

			break;

		// If a player issues a disarym the dynamite and bot is roaming
		// eventually set him off to an enemy bomb objective waypoint
		case RADIO_DISARMDYNAMITE:

			if( pBot->playerClass == PC_ENGINEER )
			{
				if( Bot_GetTask(pBot)->iTask == TASK_ROAM )
				{
					if( (rand() % 100) > 40 )
					{
						int iMatches[64];
						int iFound = 0;

						// find a random enemy bomb spot and let the bot go for it
						for( i = 0; i < numObjWaypoints; i++ )
						{
							pWaypoint = &Waypoints[ ObjWaypoints[ i ] ];

							if( pWaypoint->objdata.type != OBJECTIVE_BOMB )
								continue;

							if( pWaypoint->objdata.team == ent->client->sess.sessionTeam )
								continue;

							if( (pWaypoint->flags & FL_WP_AXIS_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_AXIS) )
								continue;

							if( (pWaypoint->flags & FL_WP_ALLIES_UNREACHABLE) && (ent->client->sess.sessionTeam == TEAM_ALLIES) )
								continue;

							// don't bother if too far away
							if( Distance( pWaypoint->origin, ent->client->ps.origin ) > 3000 )
								continue;

							iMatches[ iFound ] = pWaypoint->index;
							iFound++;
						}

						// OK found some push a random one as waypoint goal on stack
						// may have to rewrite the check for enemy dynamite code too
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

							Bot_SendDelayedRadioCommand( ent, RADIO_YES, TRUE );
						}
						else
						{
							Bot_SendDelayedRadioCommand( ent, RADIO_NO, TRUE );
						}
						
					}
				}
			}

			break;
	}

	pBot->iRadioOrder	= 0;
	pBot->pRadioEmitter	= NULL;
}

//
// Send a Radio Command
//

void Bot_SendRadioCommand( gentity_t *ent, int iRadioCommand, BOOL bTeam )
{
	radioCommand_t *r;

	if( wb_allowRadio.integer == 0 )
		return;

	if( ent->botClient->iRadioLastUseTime + 5000 > level.time )
		return;

	ent->botClient->iRadioLastUseTime = level.time;

	for( r = radioCommands; r->sId; r++ )
	{
		if( r->iCommand == iRadioCommand )
		{
			G_Voice( ent, NULL, bTeam, r->sId, FALSE );
			return;
		}
	}

	if( g_debugBots.integer )
	{
		G_Printf("Bot (%i) : Bot_SendRadioCommand failed, iRadioCommand %i\n", ent->client->ps.clientNum, iRadioCommand );
	}
}

//
// Sends a delayed radio command
//

void Bot_SendDelayedRadioCommand( gentity_t *ent, int iRadioCommand, BOOL bTeam )
{
	// ignore if just recently used radio
	if( ent->botClient->iRadioLastUseTime + 5000 > level.time )
		return;

	ent->botClient->iRadioDelayTimer = level.time + 250 + rand() % 500;
	ent->botClient->iRadioSendCmd = iRadioCommand;
	ent->botClient->bRadioTeam = bTeam;
}


//
// Returns best Weapon bot owns (and he has ammo for)
//

int Bot_SelectWeapon( gentity_t *ent )
{
	int i;
	int iWeapon;
	int	iAmmo;
	int iAmmoClip; // ammo in weapons clip

	// go through primary weapons first and look what he got
	for( i = 0; i < MAX_WEAPS_IN_BANK_MP; i++ )
	{
		iWeapon = weapBanksMultiPlayer[3][i];

		if( COM_BitCheck( ent->client->ps.weapons, iWeapon ) )
		{
			// bot has got a primary weapon , lets see if he's got ammo too
			iAmmo = ent->client->ps.ammo[ BG_FindAmmoForWeapon(iWeapon) ];
			iAmmoClip = ent->client->ps.ammoclip[ BG_FindAmmoForWeapon(iWeapon) ];

			if( iAmmo + iAmmoClip > 0 )
				return iWeapon;
		}
	}

	// go through secondaries now
	for( i = 0; i < MAX_WEAPS_IN_BANK_MP; i++ )
	{
		iWeapon = weapBanksMultiPlayer[2][i];

		if( COM_BitCheck( ent->client->ps.weapons, iWeapon ) )
		{
			iAmmo = ent->client->ps.ammo[ BG_FindAmmoForWeapon(iWeapon) ];
			iAmmoClip = ent->client->ps.ammoclip[ BG_FindAmmoForWeapon(iWeapon) ];

			if( iAmmo + iAmmoClip > 0 )
				return iWeapon;
		}
	}

	return WP_KNIFE;
}

//
// Returns TRUE if bot can strafe to the right
//

BOOL Bot_CanStrafeRight( gentity_t *ent )
{
	trace_t tr;
	vec3_t	right;
	vec3_t	end;

	AngleVectors( ent->client->ps.viewangles, NULL, right, NULL );
	VectorMA( ent->client->ps.origin, 40, right, end );

	trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, end, ent->client->ps.clientNum, MASK_PLAYERSOLID );

	if( tr.fraction == 1.0f )
		return TRUE;

	return FALSE;
}

//
// Returns TRUE if bot can strafe to the left
//

BOOL Bot_CanStrafeLeft( gentity_t *ent )
{
	trace_t tr;
	vec3_t	right;
	vec3_t	end;

	AngleVectors( ent->client->ps.viewangles, NULL, right, NULL );
	VectorMA( ent->client->ps.origin, -40, right, end );

	trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, end, ent->client->ps.clientNum, MASK_PLAYERSOLID );

	if( tr.fraction == 1.0f )
		return TRUE;

	return FALSE;
}

//
// Returns TRUE if bot can jump onto a blocking obstacle such as a crate
//

BOOL Bot_CanJumpOnObstacle( gentity_t *ent, BOOL *bDuckJump )
{
	trace_t tr;
	vec3_t up, forward, right;
	vec3_t end;
	vec3_t start;

	// This checks if bot can jump onto an obstacle.
	// traces a line upward to check if theres enough space to perform a jump then
	// traces 3 lines ahead at maximum jump height , one line from the left edge of the 
	// bounding box , one line in the middle and one line from the right edge
	// if these succeed a line from the position of the max height of the players maxs
	// is traced forward to check if player can jump onto obstacle. if this fails finally
	// trace a line with the crouch maxs to check if a duckjump would work.

	// first check if there is enough space to perform a jump
	VectorSet( up, 0, 0, 1 ); // pointing straight up

	VectorCopy( ent->client->ps.origin, start );
	start[2] += ent->r.maxs[2];

	VectorMA( start, 30, up, end );

	trap_Trace( &tr, start, NULL, NULL, end, ent->client->ps.clientNum, MASK_PLAYERSOLID );

	if( tr.fraction != 1.0f )
	{
		if( g_debugBots.integer )
		{
			G_Printf("Bot (%i) : Cannot jump here.\n", ent->client->ps.clientNum );
		}
		return FALSE;
	}

	// now check obstacle height
	AngleVectors( ent->client->ps.viewangles, forward, right, NULL );
	VectorCopy( ent->client->ps.origin, start );

	start[2] += 26;	// if trace doesnt hit obstacle at this height a normal jump is feasible
	forward[2] = 0;	// ignore z component
	VectorMA( start, 30, forward, end );

	trap_Trace( &tr, start, NULL, NULL, end, ent->client->ps.clientNum, MASK_PLAYERSOLID );

	if( tr.fraction != 1.0f )
	{
		if( g_debugBots.integer )
		{
			G_Printf("Bot (%i) : Jump is not possible because obstacle is too high.\n", ent->client->ps.clientNum);
		}
		return FALSE;
	}

	// now check for obstacle height on the right edge of box
	VectorMA( start, 18, right, start );
	VectorMA( start, 30, forward, end );

	trap_Trace( &tr, start, NULL, NULL, end, ent->client->ps.clientNum, MASK_PLAYERSOLID );

	if( tr.fraction != 1.0f )
	{
		if( g_debugBots.integer )
		{
			G_Printf("Bot (%i) : Jump is not possible, Blocker on the RIGHT.\n", ent->client->ps.clientNum);
		}
		return FALSE;
	}

	// now check for obstacle height on the left edge of box
	VectorMA( start, -36, right, start );
	VectorMA( start, 30, forward, end );

	trap_Trace( &tr, start, NULL, NULL, end, ent->client->ps.clientNum, MASK_PLAYERSOLID );

	if( tr.fraction != 1.0f )
	{
		if( g_debugBots.integer )
		{
			G_Printf("Bot (%i) : Jump is not possible, Blocker on the LEFT.\n", ent->client->ps.clientNum);
		}
		return FALSE;
	}

	// now check for height on obstacle
	VectorCopy( ent->client->ps.origin, start );
	start[2] += ent->r.maxs[2] + 30; // hardkodierte werte benutzen..

	VectorMA( start, 30, forward, end );

	trap_Trace( &tr, start, NULL, NULL, end, ent->client->ps.clientNum, MASK_PLAYERSOLID );

	if( tr.fraction != 1.0f )
	{
		if( g_debugBots.integer )
		{
			G_Printf("Bot (%i) : Not enough space for player on obstacle.\n", ent->client->ps.clientNum);
		}

		// check if a duck jump would work
		VectorCopy( ent->client->ps.origin, start );
		start[2] += ent->client->ps.crouchMaxZ + 30;

		VectorMA( start, 30, forward, end );

		trap_Trace( &tr, start, NULL, NULL, end, ent->client->ps.clientNum, MASK_PLAYERSOLID );

		if( tr.fraction == 1.0f )
		{
			if( g_debugBots.integer )
			{
				G_Printf("Bot (%i) : Duck jump would work.\n", ent->client->ps.clientNum);
			}

			if( bDuckJump )
				*bDuckJump = TRUE;

			return TRUE;
		}

		return FALSE; // does not work

	}
	else
	{
		// enough height on obstacle it should be OK to jump
		return TRUE;
	}
}

//
// Returns TRUE if bot has the enemy documents
//

BOOL Bot_HasEnemyFlag( gentity_t *ent )
{

	if( ent->client->sess.sessionTeam == TEAM_AXIS )
	{
		if( ent->client->ps.powerups[PW_BLUEFLAG] )
			return TRUE;
	}
	else if( ent->client->sess.sessionTeam == TEAM_ALLIES )
	{
		if( ent->client->ps.powerups[PW_REDFLAG] )
			return TRUE;
	}

	return FALSE;
}

//
// Returns TRUE if carrying weapon that decreases move speed
//

BOOL Bot_HasHeavyWeapon( gentity_t *ent )
{
	// used for walk paths since bot is slow enough already if carrying a heavy weapon
	if( ent->client->ps.weapon == WP_VENOM )
		return TRUE;

	if( ent->client->ps.weapon == WP_PANZERFAUST )
		return TRUE;

	if( ent->client->ps.weapon == WP_FLAMETHROWER )
		return TRUE;

	return FALSE;
}
