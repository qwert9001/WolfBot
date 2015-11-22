// Copyright (C) 2003 Torben Könke. Licensed under GPL, see LICENSE.
//
// g_botcombat.c -- combat ai

#include "g_local.h"

//
// Returns TRUE upon success
//

BOOL Bot_FindEnemy( gentity_t *ent )
{
	int i;
	gentity_t *pPlayer;
	vec3_t vDistance;
	float fLength;
	float fNearest;
	botClient_t *pBot = ent->botClient;

	// WolfBot 1.5 : Special Routine if operating MG42 so we can check for harc and varc
	if( ent->client->ps.eFlags & EF_MG42_ACTIVE )
	{
		return Bot_FindEnemyWithMG42(ent);
	}

	if( pBot->pEnemy )
	{
		if( pBot->pEnemy->client->ps.pm_type == PM_DEAD )
		{
			if( g_debugBots.integer )
			{
				G_Printf("Bot (%i) -> Enemy (%i) dead\n", ent->client->ps.clientNum, pBot->pEnemy->client->ps.clientNum );
			}

			// reload weapon ?
			if( (rand() % 100) > 30 )
			{
				if( ent->client->ps.weapon != WP_KNIFE && ent->client->ps.weapon != WP_FLAMETHROWER )
				{
					if( ent->client->ps.ammoclip[ BG_FindAmmoForWeapon(ent->client->ps.weapon) ] < ammoTable[ent->client->ps.weapon].maxclip )
					{
						if( ent->client->ps.ammo[ BG_FindAmmoForWeapon(ent->client->ps.weapon) ] > 0 )
						{
							pBot->ucmd.wbuttons |= WBUTTON_RELOAD;
							pBot->bReloading = TRUE;
						}
					}
				}
			}

			pBot->pEnemy = NULL;
		}
		else
		{
			if( Bot_EntityInFOV( ent, pBot->pEnemy ) )
			{
				pBot->iEnemyLastSeen = level.time;
			}

			// if bot has not seen enemy for longer than 5 seconds forget about enemy
			if( (pBot->iEnemyLastSeen + 5000) < level.time )
			{
				pBot->pEnemy = NULL;

				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) lost its enemy\n", ent->client->ps.clientNum );
				}
			}
			else
			{
				return TRUE;
			}
		}
	}

	if( pBot->pEnemy == NULL )
	{
		// time to look for enemies
		if( level.time > pBot->iEnemyUpdateTime )
		{
			pBot->iEnemyUpdateTime = level.time + 500;

			fNearest = 99999;

			for( i = 0; i < level.numConnectedClients; i++ )
			{
				pPlayer = &g_entities[i];

				if( OnSameTeam( ent, pPlayer ) )
					continue;

				if( pPlayer->client->ps.pm_type == PM_DEAD )
					continue;

				if( pPlayer->client->ps.pm_flags & PMF_LIMBO )
					continue;

				if( !trap_InPVS( ent->client->ps.origin, pPlayer->client->ps.origin ) )
					continue;

				if( Bot_EntityInFOV( ent, pPlayer ) )
				{
					VectorSubtract( pPlayer->client->ps.origin, ent->client->ps.origin, vDistance );
					fLength = VectorLength(vDistance);

					if( fLength < fNearest )
					{
						fNearest = fLength;
						pBot->pEnemy = pPlayer;

						// set first seen time for reaction delay time
						pBot->iEnemyFirstSeen = level.time;
					}
				}
			}
		}
	}

	if( pBot->pEnemy )
		return TRUE;

	return FALSE;
}

//
// Combat movement
//

void Bot_AttackMovement( gentity_t *ent )
{
	vec3_t vEnemy;
	vec3_t vAngles;
	vec3_t vForward;
	vec3_t vCross;
	float fDist;
	botClient_t *pBot = ent->botClient;

	if( pBot->pEnemy == NULL )
		return;

	// WolfBot 1.5 : Don't bother if operating an MG42
	if( ent->client->ps.eFlags & EF_MG42_ACTIVE )
	{
		pBot->ucmd.forwardmove = 0;
		pBot->ucmd.rightmove = 0;
		pBot->ucmd.upmove = 0;

		return;
	}

	VectorSubtract( pBot->pEnemy->client->ps.origin, ent->client->ps.origin, vEnemy );
	vectoangles( vEnemy, vAngles );
	fDist = VectorLength(vEnemy);

	AngleVectors( ent->client->ps.viewangles, vForward, NULL, NULL );
	CrossProduct( vForward, vEnemy, vCross );

	// face the enemy
	pBot->ideal_yaw = vAngles[YAW];
	pBot->ideal_pitch = vAngles[PITCH];

	{
		pBot->ucmd.forwardmove = 127;
	
		if( (fDist < 150) && (ent->client->ps.weapon != WP_KNIFE) )
			pBot->ucmd.forwardmove = -127;

		// update fighting style
		if( level.time > pBot->iFSUpdateTime )
		{
			if( fDist < 500 )
			{
				pBot->iFightStyle = 0;
			}
			else if( fDist < 1024 )
			{
				if( (rand() % 100) > 60 )
					pBot->iFightStyle = 0;
				else
					pBot->iFightStyle = 1;
			}
			else
			{
				if( (rand() % 100) < 90 )
					pBot->iFightStyle = 1;
				else
					pBot->iFightStyle = 0;
			}

			pBot->iFSUpdateTime = level.time + 3000;
		}

		if( ent->client->ps.weapon == WP_KNIFE )
			pBot->iFightStyle = 0;

		// fight style 0 -> circle strafing + jumping
		if( pBot->iFightStyle == 0 && pBot->pSkill->iCombatMoveSkill > 50 )
		{
			if( pBot->iStrafeTime < level.time )
			{
				if( vCross[2] > 0 )
				{
					if( g_debugBots.integer )
					{
						G_Printf("Bot (%i) -> Enemy is on left\n", ent->client->ps.clientNum );
					}

					pBot->iStrafeDir = -1;
					pBot->iStrafeTime = level.time + 1000 + rand() % 2000;
				}
				else
				{
					if( g_debugBots.integer )
					{
						G_Printf("Bot (%i) -> Enemy is on right\n", ent->client->ps.clientNum );
					}

					pBot->iStrafeDir = 1;
					pBot->iStrafeTime = level.time + 1000 + rand() % 2000;
				}
			}

			if( level.time > pBot->iJumpTime && pBot->pSkill->iCombatMoveSkill > 70 )
			{
				if( (rand() % 100) > 50 )
				{
					if( level.time - ent->client->ps.jumpTime > 1000 )
						pBot->ucmd.upmove = 127;

					pBot->iJumpTime = level.time + 1500 + rand() % 1500;
				}
			}
		
		}
		else if( pBot->iFightStyle == 1 )
		{
			// duck and shoot at enemy without doing too much movement
			pBot->ucmd.upmove = -127;

			if( pBot->iStrafeDir == 0 )
			{
				if( (rand() % 100) > 50 )
					pBot->iStrafeDir = 1;
				else
					pBot->iStrafeDir = -1;
			}

			if( pBot->iStrafeTime < level.time )
			{
				if( (rand() % 100) > 60 )
				{
					if( vCross[2] > 0 )
					{
						pBot->iStrafeDir = -1;
					}
					else
					{
						pBot->iStrafeDir = 1;
					}
				}

				if( pBot->iStrafeDir == -1 )
				{
					if( Bot_CanStrafeLeft(ent) == FALSE )
					{
						pBot->iStrafeDir = 1;
					}
				}
				else if( pBot->iStrafeDir == 1 )
				{
					if( Bot_CanStrafeRight(ent) == FALSE )
					{
						pBot->iStrafeDir = -1;
					}
				}

				pBot->iStrafeTime = level.time + 2000 + rand() % 1000;
			}

			if( pBot->ucmd.forwardmove > 0 )
				pBot->ucmd.forwardmove = 0;
		}
	}

	if( pBot->bReloading )
		pBot->ucmd.forwardmove = -127;

}

//
// Returns the Secondary Weapon bot is carrying , -1 if none
//

int Bot_GetSecondaryWeapon( gentity_t *ent )
{
	int i;

	for( i = 0; i < MAX_WEAPS_IN_BANK_MP; i++ )
	{
		if( COM_BitCheck( ent->client->ps.weapons, weapBanksMultiPlayer[2][i] ) )
			return weapBanksMultiPlayer[2][i];
	}

	return -1;
}

//
// Returns the Primary Weapon bot is carrying, -1 if none
//

int Bot_GetPrimaryWeapon( gentity_t *ent )
{
	int i;

	for( i = 0; i < MAX_WEAPS_IN_BANK_MP; i++ )
	{
		if( COM_BitCheck( ent->client->ps.weapons, weapBanksMultiPlayer[3][i] ) )
			return weapBanksMultiPlayer[3][i];
	}

	return -1;
}

//
// Returns TRUE if bot has ammo for weapon
//
BOOL Bot_HasAmmoForWeapon( gentity_t *ent, int iWeapon )
{
	switch( iWeapon )
	{
		case WP_KNIFE:
			return TRUE;

		case WP_LUGER:
		case WP_COLT:
		case WP_MP40:
		case WP_THOMPSON:
		case WP_STEN:
		case WP_MAUSER:
		case WP_GARAND:
		case WP_PANZERFAUST:
		case WP_VENOM:
		case WP_FLAMETHROWER:
		case WP_GRENADE_LAUNCHER:
		case WP_GRENADE_PINEAPPLE:
			return (ent->client->ps.ammo[ BG_FindAmmoForWeapon(iWeapon ) ] + ent->client->ps.ammoclip[ BG_FindAmmoForWeapon(iWeapon) ]) > 0 ? TRUE : FALSE;
	}

	G_Printf("Bot_HasAmmoForWeapon : Warning switch statement exceeded\n");

	return FALSE;
}

//
// Returns TRUE if bot is using secondary weapon
//

BOOL Bot_UsingSecondaryWeapon( gentity_t *ent )
{
	if( ent->client->ps.weapon == WP_LUGER )
		return TRUE;

	if( ent->client->ps.weapon == WP_COLT )
		return TRUE;

	return FALSE;
}

//
// Shooting at an enemy
//

void Bot_ShootAtEnemy( gentity_t *ent )
{
	vec3_t vEyeVector;
	vec3_t vMuzzlePoint;
	vec3_t vForward, vRight, vUp;
	vec3_t vEnemyBody, vEnemyHead;	// vectors from bots eyes to enemy body parts
	vec3_t vAim; // the final aim vector (either head, body or feet)
	vec3_t vAngles;

	BOOL bZooming;
	BOOL bHeadVisible, bBodyVisible;
	float f_X_AimOffset;
	float f_Y_AimOffset;
	float fDist;
	float fAim;		// spread multiplier
	trace_t tr;
	int iBetterWeapon;
	botClient_t *pBot = ent->botClient;

	if( pBot->pEnemy == NULL )
		return;

	// WolfBot 1.5 : Check for MG42
	if( ent->client->ps.eFlags & EF_MG42_ACTIVE )
	{
		Bot_ShootAtEnemyWithMG42(ent);
		return;
	}

	// WolfBot 1.5 : Check for a real weapon
	if( ent->client->ps.weapon == WP_MEDIC_SYRINGE || ent->client->ps.weapon == WP_MEDKIT || ent->client->ps.weapon == WP_AMMO || ent->client->ps.weapon == WP_PLIERS )
	{
		pBot->iWeapon = Bot_SelectWeapon(ent);
		return;
	}

	bZooming = FALSE;
	bHeadVisible = FALSE;
	bBodyVisible = FALSE;

	VectorCopy( ent->client->ps.origin, vEyeVector );
	vEyeVector[2] += ent->client->ps.viewheight;

	AngleVectors( ent->client->ps.viewangles, vForward, vRight, vUp );
	CalcMuzzlePoint( ent, ent->client->ps.weapon, vForward, vRight, vUp, vMuzzlePoint );

	fDist = Distance( pBot->pEnemy->client->ps.origin, ent->client->ps.origin );
		
	// need to reload ?
	if( ent->client->ps.weapon != WP_KNIFE )
	{
		if( ent->client->ps.ammoclip[ BG_FindAmmoForWeapon(ent->client->ps.weapon) ] == 0 )
		{
			if( ent->client->ps.ammo[ BG_FindAmmoForWeapon(ent->client->ps.weapon) ] > 0 )
			{
				pBot->ucmd.wbuttons |= WBUTTON_RELOAD;
				pBot->bReloading = TRUE;
			}
			else
			{
				pBot->iWeapon = Bot_SelectWeapon(ent);
			}
		}
		else
		{
			pBot->bReloading = FALSE;
		}
	}

	// check if enemy body is visible
	trap_Trace( &tr, vEyeVector, NULL, NULL, pBot->pEnemy->client->ps.origin, ent->client->ps.clientNum, MASK_SHOT );

	if( tr.entityNum == pBot->pEnemy->s.number )
	{
		vec3_t vTemp;

		// offset slightly...
		VectorCopy( pBot->pEnemy->client->ps.origin, vTemp );
		vTemp[2] += 20;

		VectorSubtract( vTemp, vEyeVector, vEnemyBody );

		bBodyVisible = TRUE;
	}

	// check if we can see head
	VectorCopy( pBot->pEnemy->client->ps.origin, vEnemyHead );
	vEnemyHead[2] += pBot->pEnemy->client->ps.viewheight;

	trap_Trace( &tr, vEyeVector, NULL, NULL, vEnemyHead, ent->client->ps.clientNum, MASK_SHOT );

	if( tr.entityNum == pBot->pEnemy->s.number )
	{
		VectorSubtract( vEnemyHead, vEyeVector, vEnemyHead );
			
		bHeadVisible = TRUE;
	}

	if( bBodyVisible == FALSE && bHeadVisible == FALSE )
	{
		if( g_debugBots.integer )
		{
			G_Printf("Bot (%i) : Can't see my enemy at all.\n", ent->client->ps.clientNum);
		}
		return;
	}

	// aim for body by default
	if( bBodyVisible )
	{
		VectorCopy( vEnemyBody, vAim );
	}
	else
	{
		VectorCopy( vEnemyHead, vAim );
	}

	if( ent->client->ps.weapon == WP_KNIFE )
	{
		if( fDist < 80 )
		{
			// rambo
			pBot->ucmd.buttons |= BUTTON_ATTACK;
		}

		pBot->bReloading = FALSE;

		return;
	}
	else if( ent->client->ps.weapon == WP_PANZERFAUST )
	{
		// try aiming at ground
		vec3_t vEnemyFeet;

		// if we just fired panzerfaust try switiching to pistol instead until
		// panzerfaust has recharged
		if( level.time - ent->client->ps.classWeaponTime < g_soldierChargeTime.integer )
		{
			iBetterWeapon = Bot_GetSecondaryWeapon(ent);

			if( iBetterWeapon != -1 )
			{
				if( Bot_HasAmmoForWeapon( ent, iBetterWeapon ) )
				{
					pBot->iWeapon = iBetterWeapon;
				}
			}
		}

		VectorCopy( pBot->pEnemy->client->ps.origin, vEnemyFeet );
		vEnemyFeet[2] += pBot->pEnemy->r.mins[2] + rand() % 10;

		trap_Trace( &tr, vEyeVector, NULL, NULL, vEnemyFeet, ent->client->ps.clientNum, MASK_SHOT );

		if( (tr.entityNum == pBot->pEnemy->s.number) || (tr.fraction >= 0.95f) )
		{
			if( g_debugBots.integer )
			{
				gentity_t *tent = G_TempEntity( vEyeVector, EV_RAILTRAIL );
				VectorCopy( vEnemyFeet, tent->s.origin2 );
		
				G_Printf("Bot (%i) : Can see the enemies feet\n", ent->client->ps.clientNum );
			}

			// aim for the enemy feet with panzerfaust
			VectorSubtract( vEnemyFeet, vEyeVector, vAim );
		}
		else
		{
			if( g_debugBots.integer )
			{
				G_Printf("Bot (%i) : I can't see the enemies feet so aiming for body\n", ent->client->ps.clientNum );
			}
		}
	}
	else if( ent->client->ps.weapon == WP_FLAMETHROWER )
	{
		// if too far away select pistole
		if( fDist > 1500 )
		{
			iBetterWeapon = Bot_GetSecondaryWeapon(ent);

			if( iBetterWeapon != -1 )
			{
				if( Bot_HasAmmoForWeapon( ent, iBetterWeapon ) )
				{
					pBot->iWeapon = iBetterWeapon;
				}
			}

			// could not select pistole...don't shoot since we're too far away
			if( pBot->iWeapon != iBetterWeapon )
			{
			}
		}
	}
	else if( ent->client->ps.weapon == WP_MAUSER )
	{
		// if close combat try to select pistol instead
		if( pBot->iFightStyle == 0 )
		{
			iBetterWeapon = Bot_GetSecondaryWeapon(ent);

			if( iBetterWeapon != -1 )
			{
				if( Bot_HasAmmoForWeapon( ent, iBetterWeapon ) )
					pBot->iWeapon = iBetterWeapon;
			}
			else
			{
				pBot->iWeapon = WP_SNIPERRIFLE;
			}
		}
		else
		{
			// zoom in
			// Alternative Fire in Wolf changes the actual weapon.
			// WP_MAUSER becomes WP_SNIPERRIFLE when going into zoom.
			pBot->iWeapon = WP_SNIPERRIFLE;
		}
	}
	else if( ent->client->ps.weapon == WP_SNIPERRIFLE )
	{
		// Mauser in Zoom Mode
		if( pBot->iFightStyle == 0 )
		{
			iBetterWeapon = Bot_GetSecondaryWeapon(ent);

			if( iBetterWeapon != -1 )
			{
				if( Bot_HasAmmoForWeapon( ent, iBetterWeapon ) )
					pBot->iWeapon = iBetterWeapon;
			}
		}

		bZooming = TRUE;	// more accurate

		// aim for head if its visible
		if( bHeadVisible )
		{
			if( (rand() % 100) > 40 )
			{
				if( g_debugBots.integer )
				{
					G_Printf("Bot (%i) : Aiming for enemy head\n", ent->client->ps.clientNum );
				}
				VectorCopy( vEnemyHead, vAim );
			}
		}
	}
	else if( Bot_UsingSecondaryWeapon(ent) )
	{
		// if distance combat try to select primary weapon
		if( pBot->iFightStyle == 1 )
		{
			iBetterWeapon = Bot_GetPrimaryWeapon(ent);

			// don't switch if primary is flamethrower since its limited in range
			// don't switch if primary is panzerfaust and it is still recharging
			if( iBetterWeapon != -1 )
			{
				if( (iBetterWeapon != WP_FLAMETHROWER) && ( (iBetterWeapon == WP_PANZERFAUST) && (level.time - ent->client->ps.classWeaponTime > g_soldierChargeTime.integer) ) )
				{
					if( Bot_HasAmmoForWeapon( ent, iBetterWeapon ) )
					{
						pBot->iWeapon = iBetterWeapon;
					}
				}
			}
		}
		else if( pBot->iFightStyle == 0 && fDist < 1500 )
		{
			if( Bot_GetPrimaryWeapon(ent) == WP_FLAMETHROWER )
			{
				if( Bot_HasAmmoForWeapon(ent, WP_FLAMETHROWER ) )
					pBot->iWeapon = WP_FLAMETHROWER;
			}
		}
	}

	// render optimal aim vector
	if( g_debugBots.integer )
	{
		gentity_t *tent = G_TempEntity( vEyeVector, EV_RAILTRAIL );

		VectorAdd( vEyeVector, vAim, tent->s.origin2 );
	}

	// Bot skill would come into play here.
	if( bZooming )
	{
		if( pBot->iFightStyle == 1 )
		{
			f_X_AimOffset = crandom() * 0.5f;
			f_Y_AimOffset = crandom() * 0.35f;
		}
		else
		{
			f_X_AimOffset = crandom() * 1.2;
			f_Y_AimOffset = crandom() * 1.2;
		}
	}
	else if( ent->client->ps.weapon == WP_KNIFE || ent->client->ps.weapon == WP_FLAMETHROWER )
	{
		f_X_AimOffset = 0;
		f_Y_AimOffset = 0;
	}
	else
	{
		if( fDist > 1000 )
		{
			f_X_AimOffset = crandom() * 4;
			f_Y_AimOffset = crandom() * 3.3;
		}
		{
			f_X_AimOffset = crandom() * 2;
			f_Y_AimOffset = crandom() * 1.8;
		}
	}

	// calculate spread multiplier depending on bot's skill
	fAim = 5.0f - pBot->pSkill->fAimSkill;

	// add fAim to aim offsets
	f_X_AimOffset *= fAim;
	f_Y_AimOffset *= fAim;

	// convert aim vector into a set of angles
	vectoangles( vAim, vAngles );

	pBot->ideal_yaw = vAngles[YAW];
	pBot->ideal_pitch = vAngles[PITCH];

	pBot->ideal_yaw += f_Y_AimOffset;
	pBot->ideal_pitch += f_X_AimOffset;

	// don't fire if reloading
	if( !pBot->bReloading )
	{
		pBot->ucmd.buttons |= BUTTON_ATTACK;
	}
}

//
// This just makes sure bot reacts if taking enemy fire
// We set the pain pointer of the bot to this , its 0 for normal clients
//

void Bot_TakeDamage( gentity_t *self, gentity_t *attacker, int damage, vec3_t point )
{
	botClient_t *pBot;

	if( self->botClient == NULL )
	{
		// just to be sure...
		return;
	}

	pBot = self->botClient;

	// WolfBot 1.5 : check for teammate shooting at us
	if( attacker->client && OnSameTeam( self, attacker ) && attacker->botClient == NULL )
	{
		if( (rand() % 100) > 50 )
		{
			Bot_SendDelayedRadioCommand( self, RADIO_HOLDFIRE, TRUE );
		}
	}


	if( Bot_GetTask(pBot)->iTask == TASK_ATTACK )
		return;

	if( attacker->client && !OnSameTeam( self, attacker ) )
	{
		pBot->pEnemy = &g_entities[attacker->s.number];
		pBot->iEnemyFirstSeen = level.time + 250 + rand() % 250; // higher reaction time
		pBot->iEnemyLastSeen = level.time;
	}
}

//
// Trying to find an enemy while operating an MG42 (limited view)
//

BOOL Bot_FindEnemyWithMG42( gentity_t *ent )
{
	int i;
	gentity_t *pPlayer;
	gentity_t *pMG42 = &g_entities[ ent->client->ps.viewlocked_entNum ];
	botClient_t *pBot = ent->botClient;
	int good_enemies[MAX_CLIENTS]; // inside harc and varc
	int bad_enemies[MAX_CLIENTS]; // outside harc or varc
	int numGood, numBad;

	if( pBot->pEnemy )
	{
		if( pBot->pEnemy->client->ps.pm_type == PM_DEAD )
		{
			pBot->pEnemy = NULL;
		}
		else
		{
			if( Bot_EntityInFOV( ent, pBot->pEnemy ) )
			{
				pBot->iEnemyLastSeen = level.time;
			}

			// longer timeout for MG42
			if( (pBot->iEnemyLastSeen + 5000) < level.time )
			{
				pBot->pEnemy = NULL;
			}
			else
			{
				return TRUE;
			}
		}
	}

	if( pBot->pEnemy == NULL )
	{
		numGood = numBad = 0;

		// time to look for enemies
		if( level.time > pBot->iEnemyUpdateTime )
		{
			pBot->iEnemyUpdateTime = level.time + 500;

			for( i = 0; i < level.numConnectedClients; i++ )
			{
				pPlayer = &g_entities[i];

				if( OnSameTeam( ent, pPlayer ) )
					continue;

				if( pPlayer->client->ps.pm_type == PM_DEAD )
					continue;

				if( pPlayer->client->ps.pm_flags & PMF_LIMBO )
					continue;

				if( !trap_InPVS( ent->client->ps.origin, pPlayer->client->ps.origin ) )
					continue;

				if( Bot_EntityInFOV( ent, pPlayer ) )
				{
					// do some extra checks to see if player is in varc and harc of gun
					// those enemies are preferred over those who are outside of MG42s range
					vec3_t vTemp, vTemp2, vMG42Base, vMG42Up;
					float fDot, fDegrees;

					AngleVectors( ent->client->pmext.centerangles, vMG42Base, NULL, vMG42Up );

					VectorCopy( pMG42->r.currentOrigin, vTemp );
					VectorCopy( pPlayer->client->ps.origin, vTemp2 );

					vTemp[2] = vTemp2[2] = 0;

					VectorSubtract( vTemp2, vTemp, vTemp );
					VectorNormalize(vTemp); // vector from enemy to gun

					fDot = DotProduct( vTemp, vMG42Base );
					fDegrees = RAD2DEG(acos(fDot));

					if( fDegrees > pMG42->harc )
					{
						// player not good since out of harc
						bad_enemies[ numBad ] = pPlayer->s.number;
						numBad++;
						continue;
					}

					// check for varc as well
					VectorCopy( pMG42->r.currentOrigin, vTemp );
					VectorCopy( pPlayer->client->ps.origin, vTemp2 );
					VectorSubtract( vTemp2, vTemp, vTemp );
					VectorNormalize(vTemp);

					fDot = DotProduct( vTemp, vMG42Up );
					fDegrees = RAD2DEG(acos(fDot))-90;

					if( fDegrees > pMG42->varc / 2 )
					{
						// player not good since out of varc
						bad_enemies[ numBad ] = pPlayer->s.number;
						numBad++;
						continue;
					}

					// still here, must be a good enemy
					good_enemies[ numGood ] = pPlayer->s.number;
					numGood++;
				}
			}

			// found any good ?
			if( numGood > 0 )
			{
				pBot->pEnemy = &g_entities[ good_enemies[ rand() % numGood ] ];
				pBot->iEnemyFirstSeen = level.time;
			}
			else if( numBad > 0 )
			{
				pBot->pEnemy = &g_entities[ bad_enemies[ rand() % numBad ] ];
				pBot->iEnemyFirstSeen = level.time;
			}
			else
			{
				pBot->pEnemy = NULL;
			}
		}
	}

	if( pBot->pEnemy )
		return TRUE;

	return FALSE;
}

//
// Shooting at an enemy with an MG42
//

void Bot_ShootAtEnemyWithMG42( gentity_t *ent )
{
	BOOL bBodyVisible, bHeadVisible;
	vec3_t vMuzzle, vUp, vForward, vRight;
	gentity_t *pMG42;
	trace_t tr;
	vec3_t vMidBody;		// slightly adjusted enemy origin
	vec3_t vEnemyHead;
	vec3_t vAim;
	vec3_t vTemp, vOrigin;
	vec3_t vMG42BaseVector, vMG42UpVector;
	float fDot, fDegrees;
	float r, u;

	botClient_t *pBot = ent->botClient;

	if( pBot->pEnemy == NULL )
		return;

	bBodyVisible = bHeadVisible = FALSE;

	// setup the muzzle origin
	pMG42 = &g_entities[ ent->client->ps.viewlocked_entNum ];

	AngleVectors( ent->client->ps.viewangles, vForward, vRight, vUp );
	VectorCopy( pMG42->s.pos.trBase, vMuzzle );
	VectorMA( vMuzzle, 16, vUp, vMuzzle );

	// check if enemy body is visible
	VectorCopy( pBot->pEnemy->client->ps.origin, vMidBody );
	vMidBody[2] += 20;

	trap_Trace( &tr, vMuzzle, NULL, NULL, vMidBody, pMG42->s.number, MASK_SHOT );

	if( tr.entityNum == pBot->pEnemy->s.number )
	{
		bBodyVisible = TRUE;
	}

	// check if enemy head is visible
	VectorCopy( pBot->pEnemy->client->ps.origin, vEnemyHead );
	vEnemyHead[2] += pBot->pEnemy->client->ps.viewheight;

	trap_Trace( &tr, vMuzzle, NULL, NULL, vEnemyHead, pMG42->s.number, MASK_SHOT );

	if( tr.entityNum == pBot->pEnemy->s.number )
	{
		bHeadVisible = TRUE;
	}

	if( bBodyVisible == FALSE && bHeadVisible == FALSE )
	{
		return;
	}

	// always aim at body with MG42 if visible 
	if( bBodyVisible )
	{
		VectorCopy( vMidBody, vAim );
	}
	else if( bBodyVisible == FALSE && bHeadVisible == TRUE )
	{
		VectorCopy( vEnemyHead, vAim );
	}

	// now check if enemy is behind the bot (out of horizontal arc).
	// If enemy is behind bot for longer than 2 seconds, dismount.
	// Get the angle between the basevector and the vector target and mg42.
	VectorCopy( pMG42->r.currentOrigin, vOrigin );
	vOrigin[2] = 0;

	VectorCopy( vMidBody, vTemp );
	vTemp[2] = 0;

	VectorSubtract( vTemp, vOrigin, vTemp );
	VectorNormalize(vTemp);

	AngleVectors( ent->client->pmext.centerangles, vMG42BaseVector, NULL, vMG42UpVector );

	fDot = DotProduct( vTemp, vMG42BaseVector );

	// convert into degrees
	fDegrees = RAD2DEG(acos(fDot));
		
	// if angle is bigger than harc of the gun target cannot be hit
	if( fDegrees > pMG42->harc )
	{
		if( g_debugBots.integer )
		{
			G_Printf("Bot (%i) : Target cannot be hit because out of harc\n", ent->client->ps.clientNum);
		}
		return;
	}

	// varc
	VectorCopy( pMG42->r.currentOrigin, vOrigin );
	VectorCopy( vMidBody, vTemp );
	VectorSubtract( vTemp, vOrigin, vTemp );
	VectorNormalize(vTemp);

	fDot = DotProduct( vTemp, vMG42UpVector );
	fDegrees = RAD2DEG(acos(fDot))-90;

	if( fDegrees > pMG42->varc / 2 )
	{
		if( g_debugBots.integer )
		{
			G_Printf("Bot (%i) : Target cannot be hit because out of varc\n", ent->client->ps.clientNum);
		}
		return;
	}

	r = random() * M_PI * 2.0f;
	u = sin(r) * crandom() * (8.0f - pBot->pSkill->fAimSkill) * 30;
	r = cos(r) * crandom() * (8.0f - pBot->pSkill->fAimSkill) * 30;
	
	VectorMA( vAim, r, vRight, vAim );
	VectorMA( vAim, u, vUp, vAim );

	VectorSubtract( vAim, vMuzzle, vAim );
	vectoangles( vAim, vAim );
	pBot->ideal_pitch = vAim[PITCH];
	pBot->ideal_yaw = vAim[YAW];

	// feuer frei
	pBot->ucmd.buttons |= BUTTON_ATTACK;

}

