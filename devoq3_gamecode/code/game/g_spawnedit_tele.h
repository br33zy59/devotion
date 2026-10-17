/*
===========================================================================
In-game teleporter editing (spectator, Spawn UI).
===========================================================================
*/

#ifndef G_SPAWNEDIT_TELE_H
#define G_SPAWNEDIT_TELE_H

void SpawnEdit_Tele_InitMap( void );
void SpawnEdit_Tele_CommitBspTrigger( gentity_t *ent );
void SpawnEdit_Tele_CommitBspDest( gentity_t *ent );
void SpawnEdit_Tele_OnEntitiesSpawned( void );
void SpawnEdit_Tele_SendRoster( gentity_t *ent );
void SpawnEdit_Tele_BroadcastRoster( void );
void SpawnEdit_Tele_Shutdown( int restart );
qboolean SpawnEdit_Tele_HandleCommand( gentity_t *ent, const char *sub );

#endif
