/*
===========================================================================
Spectator teleporter editing: relocate brush entrances and target exits.
Session layout: layouts/session_teles.cfg (map_restart within match).
===========================================================================
*/

#include "g_local.h"
#include "g_spawnedit.h"
#include "g_spawnedit_tele.h"

#define SPAWNEDIT_TELE_MAX			64
#define SPAWNEDIT_TELE_ORIGIN_EPS	4.0f
#define SPAWNEDIT_TELE_LAYOUT_VER	"teledit_v1"
#define SPAWNEDIT_TELE_LAYOUT_PATH	"layouts/session_teles.cfg"

typedef struct {
	vec3_t		homeOrigin;
} teleEditBspTrigger_t;

typedef struct {
	vec3_t		homeOrigin;
	vec3_t		homeAngles;
} teleEditBspDest_t;

typedef struct {
	vec3_t		fromHome;
	vec3_t		toOrigin;
} teleEditTriggerMove_t;

typedef struct {
	vec3_t		fromHome;
	vec3_t		toOrigin;
	float		yaw;
} teleEditDestMove_t;

static teleEditBspTrigger_t	teleEdit_bspTriggers[SPAWNEDIT_TELE_MAX];
static int						teleEdit_bspTriggerCount;
static teleEditBspDest_t		teleEdit_bspDests[SPAWNEDIT_TELE_MAX];
static int						teleEdit_bspDestCount;
static teleEditTriggerMove_t	teleEdit_triggerMoves[SPAWNEDIT_TELE_MAX];
static int						teleEdit_triggerMoveCount;
static teleEditDestMove_t		teleEdit_destMoves[SPAWNEDIT_TELE_MAX];
static int						teleEdit_destMoveCount;
static char						teleEdit_mapName[MAX_QPATH];
static qboolean					teleEdit_dirty;

static qboolean TeleEdit_ClientMayEdit( gentity_t *ent );
static qboolean TeleEdit_OriginMatch( const vec3_t a, const vec3_t b );
static qboolean TeleEdit_IsDestClassname( const char *classname );
static qboolean TeleEdit_IsLiveTrigger( gentity_t *ent );
static qboolean TeleEdit_IsLiveDest( gentity_t *ent );
static gentity_t *TeleEdit_FindDestForTrigger( gentity_t *trigger );
static void TeleEdit_UpsertBspTrigger( const vec3_t homeOrigin );
static void TeleEdit_UpsertBspDest( const vec3_t homeOrigin, const vec3_t homeAngles );
static gentity_t *TeleEdit_FindTriggerByHome( const vec3_t home );
static gentity_t *TeleEdit_FindDestByHome( const vec3_t home );
static void TeleEdit_ApplyTriggerMove( gentity_t *ent, const vec3_t newOrigin );
static void TeleEdit_ApplyDestMove( gentity_t *ent, const vec3_t newOrigin, float yaw );
static void TeleEdit_UpsertTriggerMove( const vec3_t fromHome, const vec3_t toOrigin );
static void TeleEdit_UpsertDestMove( const vec3_t fromHome, const vec3_t toOrigin, float yaw );
static void TeleEdit_CommitLayout( void );
static void TeleEdit_PurgeSessionLayout( void );
static void TeleEdit_ClearRecords( void );
static void TeleEdit_ApplyAllMoves( void );
static int TeleEdit_RestoreMapDefaults( void );
static void TeleEdit_RefreshPrediction( void );
static void TeleEdit_HomeForTrigger( const gentity_t *ent, vec3_t homeOut );
static void TeleEdit_HomeForDest( const gentity_t *ent, vec3_t homeOut, vec3_t anglesOut );

static qboolean TeleEdit_ClientMayEdit( gentity_t *ent ) {
	if ( !ent || !ent->client ) {
		return qfalse;
	}
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean TeleEdit_OriginMatch( const vec3_t a, const vec3_t b ) {
	return ( Distance( a, b ) <= SPAWNEDIT_TELE_ORIGIN_EPS ) ? qtrue : qfalse;
}

static qboolean TeleEdit_IsDestClassname( const char *classname ) {
	if ( !classname || !classname[0] ) {
		return qfalse;
	}
	if ( !strcmp( classname, "target_position" )
			|| !strcmp( classname, "misc_teleporter_dest" ) ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean TeleEdit_IsLiveTrigger( gentity_t *ent ) {
	if ( !ent || !G_InUse( ent ) ) {
		return qfalse;
	}
	if ( ent->s.eType != ET_TELEPORT_TRIGGER ) {
		return qfalse;
	}
	if ( ent->spawnflags & 1 ) {
		return qfalse;
	}
	if ( ent->classname && strcmp( ent->classname, "trigger_teleport" ) ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean TeleEdit_IsLiveDest( gentity_t *ent ) {
	if ( !ent || !G_InUse( ent ) ) {
		return qfalse;
	}
	return TeleEdit_IsDestClassname( ent->classname );
}

static gentity_t *TeleEdit_FindDestForTrigger( gentity_t *trigger ) {
	gentity_t	*t;
	gentity_t	*first;

	if ( !trigger || !trigger->target || !trigger->target[0] ) {
		return NULL;
	}
	first = NULL;
	t = NULL;
	while ( 1 ) {
		t = G_Find( t, FOFS( targetname ), trigger->target );
		if ( !t ) {
			break;
		}
		if ( !TeleEdit_IsLiveDest( t ) ) {
			continue;
		}
		if ( first ) {
			return first;
		}
		first = t;
	}
	return first;
}

static void TeleEdit_UpsertBspTrigger( const vec3_t homeOrigin ) {
	int	i;

	for ( i = 0; i < teleEdit_bspTriggerCount; i++ ) {
		if ( TeleEdit_OriginMatch( teleEdit_bspTriggers[i].homeOrigin, homeOrigin ) ) {
			return;
		}
	}
	if ( teleEdit_bspTriggerCount >= SPAWNEDIT_TELE_MAX ) {
		return;
	}
	VectorCopy( homeOrigin, teleEdit_bspTriggers[teleEdit_bspTriggerCount].homeOrigin );
	teleEdit_bspTriggerCount++;
}

static void TeleEdit_UpsertBspDest( const vec3_t homeOrigin, const vec3_t homeAngles ) {
	int	i;

	for ( i = 0; i < teleEdit_bspDestCount; i++ ) {
		if ( TeleEdit_OriginMatch( teleEdit_bspDests[i].homeOrigin, homeOrigin ) ) {
			VectorCopy( homeAngles, teleEdit_bspDests[i].homeAngles );
			return;
		}
	}
	if ( teleEdit_bspDestCount >= SPAWNEDIT_TELE_MAX ) {
		return;
	}
	VectorCopy( homeOrigin, teleEdit_bspDests[teleEdit_bspDestCount].homeOrigin );
	VectorCopy( homeAngles, teleEdit_bspDests[teleEdit_bspDestCount].homeAngles );
	teleEdit_bspDestCount++;
}

void SpawnEdit_Tele_CommitBspTrigger( gentity_t *ent ) {
	vec3_t	origin;

	if ( !TeleEdit_IsLiveTrigger( ent ) ) {
		return;
	}
	VectorCopy( ent->s.pos.trBase, origin );
	if ( level.spawning ) {
		TeleEdit_UpsertBspTrigger( origin );
	}
	VectorCopy( origin, ent->s.origin2 );
}

void SpawnEdit_Tele_CommitBspDest( gentity_t *ent ) {
	vec3_t	origin;
	vec3_t	angles;

	if ( !TeleEdit_IsLiveDest( ent ) ) {
		return;
	}
	VectorCopy( ent->s.pos.trBase, origin );
	VectorCopy( ent->s.angles, angles );
	if ( level.spawning ) {
		TeleEdit_UpsertBspDest( origin, angles );
	}
	VectorCopy( origin, ent->s.origin2 );
}

static gentity_t *TeleEdit_FindTriggerByHome( const vec3_t home ) {
	int			i;
	gentity_t	*ent;

	for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
		ent = &g_entities[i];
		if ( !TeleEdit_IsLiveTrigger( ent ) ) {
			continue;
		}
		if ( TeleEdit_OriginMatch( ent->s.origin2, home ) ) {
			return ent;
		}
	}
	return NULL;
}

static gentity_t *TeleEdit_FindDestByHome( const vec3_t home ) {
	int			i;
	gentity_t	*ent;

	for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
		ent = &g_entities[i];
		if ( !TeleEdit_IsLiveDest( ent ) ) {
			continue;
		}
		if ( TeleEdit_OriginMatch( ent->s.origin2, home ) ) {
			return ent;
		}
	}
	return NULL;
}

static void TeleEdit_ApplyTriggerMove( gentity_t *ent, const vec3_t newOrigin ) {
	vec3_t	moved;

	if ( !ent || !newOrigin ) {
		return;
	}
	VectorCopy( newOrigin, moved );
	G_SetOrigin( ent, moved );
	VectorCopy( moved, ent->s.origin );
	trap_LinkEntity( ent );
}

static void TeleEdit_ApplyDestMove( gentity_t *ent, const vec3_t newOrigin, float yaw ) {
	vec3_t	moved;

	if ( !ent || !newOrigin ) {
		return;
	}
	VectorCopy( newOrigin, moved );
	G_SetOrigin( ent, moved );
	ent->s.angles[0] = 0.0f;
	ent->s.angles[1] = yaw;
	ent->s.angles[2] = 0.0f;
	trap_LinkEntity( ent );
}

static void TeleEdit_UpsertTriggerMove( const vec3_t fromHome, const vec3_t toOrigin ) {
	int	i;

	for ( i = 0; i < teleEdit_triggerMoveCount; i++ ) {
		if ( TeleEdit_OriginMatch( teleEdit_triggerMoves[i].fromHome, fromHome ) ) {
			VectorCopy( toOrigin, teleEdit_triggerMoves[i].toOrigin );
			teleEdit_dirty = qtrue;
			return;
		}
	}
	if ( teleEdit_triggerMoveCount >= SPAWNEDIT_TELE_MAX ) {
		return;
	}
	VectorCopy( fromHome, teleEdit_triggerMoves[teleEdit_triggerMoveCount].fromHome );
	VectorCopy( toOrigin, teleEdit_triggerMoves[teleEdit_triggerMoveCount].toOrigin );
	teleEdit_triggerMoveCount++;
	teleEdit_dirty = qtrue;
}

static void TeleEdit_UpsertDestMove( const vec3_t fromHome, const vec3_t toOrigin, float yaw ) {
	int	i;

	for ( i = 0; i < teleEdit_destMoveCount; i++ ) {
		if ( TeleEdit_OriginMatch( teleEdit_destMoves[i].fromHome, fromHome ) ) {
			VectorCopy( toOrigin, teleEdit_destMoves[i].toOrigin );
			teleEdit_destMoves[i].yaw = yaw;
			teleEdit_dirty = qtrue;
			return;
		}
	}
	if ( teleEdit_destMoveCount >= SPAWNEDIT_TELE_MAX ) {
		return;
	}
	VectorCopy( fromHome, teleEdit_destMoves[teleEdit_destMoveCount].fromHome );
	VectorCopy( toOrigin, teleEdit_destMoves[teleEdit_destMoveCount].toOrigin );
	teleEdit_destMoves[teleEdit_destMoveCount].yaw = yaw;
	teleEdit_destMoveCount++;
	teleEdit_dirty = qtrue;
}

static void TeleEdit_HomeForTrigger( const gentity_t *ent, vec3_t homeOut ) {
	if ( !ent || !homeOut ) {
		return;
	}
	VectorCopy( ent->s.origin2, homeOut );
}

static void TeleEdit_HomeForDest( const gentity_t *ent, vec3_t homeOut, vec3_t anglesOut ) {
	int	i;

	if ( !ent ) {
		return;
	}
	if ( homeOut ) {
		VectorCopy( ent->s.origin2, homeOut );
	}
	if ( !anglesOut ) {
		return;
	}
	for ( i = 0; i < teleEdit_bspDestCount; i++ ) {
		if ( TeleEdit_OriginMatch( teleEdit_bspDests[i].homeOrigin, ent->s.origin2 ) ) {
			VectorCopy( teleEdit_bspDests[i].homeAngles, anglesOut );
			return;
		}
	}
	VectorCopy( ent->s.angles, anglesOut );
}

static void TeleEdit_RefreshPrediction( void ) {
	G_SetTeleporterDestinations();
}

static void TeleEdit_ApplyAllMoves( void ) {
	int			i;
	gentity_t	*ent;

	for ( i = 0; i < teleEdit_triggerMoveCount; i++ ) {
		ent = TeleEdit_FindTriggerByHome( teleEdit_triggerMoves[i].fromHome );
		if ( ent ) {
			TeleEdit_ApplyTriggerMove( ent, teleEdit_triggerMoves[i].toOrigin );
		}
	}
	for ( i = 0; i < teleEdit_destMoveCount; i++ ) {
		ent = TeleEdit_FindDestByHome( teleEdit_destMoves[i].fromHome );
		if ( ent ) {
			TeleEdit_ApplyDestMove( ent, teleEdit_destMoves[i].toOrigin,
					teleEdit_destMoves[i].yaw );
		}
	}
	TeleEdit_RefreshPrediction();
}

static int TeleEdit_RestoreMapDefaults( void ) {
	int					i;
	int					restored;
	gentity_t			*ent;
	teleEditBspTrigger_t	*bspT;
	teleEditBspDest_t		*bspD;
	vec3_t				homeAngles;

	restored = 0;
	for ( i = 0; i < teleEdit_bspTriggerCount; i++ ) {
		bspT = &teleEdit_bspTriggers[i];
		ent = TeleEdit_FindTriggerByHome( bspT->homeOrigin );
		if ( !ent ) {
			continue;
		}
		TeleEdit_ApplyTriggerMove( ent, bspT->homeOrigin );
		restored++;
	}
	for ( i = 0; i < teleEdit_bspDestCount; i++ ) {
		bspD = &teleEdit_bspDests[i];
		ent = TeleEdit_FindDestByHome( bspD->homeOrigin );
		if ( !ent ) {
			continue;
		}
		TeleEdit_ApplyDestMove( ent, bspD->homeOrigin, bspD->homeAngles[1] );
		restored++;
	}
	TeleEdit_RefreshPrediction();
	return restored;
}

static void TeleEdit_PurgeSessionLayout( void ) {
	fileHandle_t	f;

	trap_FS_FOpenFile( SPAWNEDIT_TELE_LAYOUT_PATH, &f, FS_WRITE );
	if ( f ) {
		trap_FS_FCloseFile( f );
	}
	teleEdit_dirty = qfalse;
}

static void TeleEdit_CommitLayout( void ) {
	fileHandle_t	f;
	char			line[256];
	int				i;
	int				len;

	if ( !teleEdit_dirty || !teleEdit_mapName[0] ) {
		return;
	}
	if ( teleEdit_triggerMoveCount <= 0 && teleEdit_destMoveCount <= 0 ) {
		TeleEdit_PurgeSessionLayout();
		return;
	}

	trap_FS_FOpenFile( SPAWNEDIT_TELE_LAYOUT_PATH, &f, FS_WRITE );
	if ( !f ) {
		G_Printf( S_COLOR_YELLOW "WARNING: could not write tele layout %s\n", SPAWNEDIT_TELE_LAYOUT_PATH );
		return;
	}

	Com_sprintf( line, sizeof( line ), "%s\n", SPAWNEDIT_TELE_LAYOUT_VER );
	len = (int)strlen( line );
	trap_FS_Write( line, len, f );
	Com_sprintf( line, sizeof( line ), "meta %s %i\n", teleEdit_mapName, g_gametype.integer );
	len = (int)strlen( line );
	trap_FS_Write( line, len, f );

	for ( i = 0; i < teleEdit_triggerMoveCount; i++ ) {
		Com_sprintf( line, sizeof( line ), "tmove %.3f %.3f %.3f %.3f %.3f %.3f\n",
				teleEdit_triggerMoves[i].fromHome[0], teleEdit_triggerMoves[i].fromHome[1],
				teleEdit_triggerMoves[i].fromHome[2],
				teleEdit_triggerMoves[i].toOrigin[0], teleEdit_triggerMoves[i].toOrigin[1],
				teleEdit_triggerMoves[i].toOrigin[2] );
		len = (int)strlen( line );
		trap_FS_Write( line, len, f );
	}
	for ( i = 0; i < teleEdit_destMoveCount; i++ ) {
		Com_sprintf( line, sizeof( line ), "dmove %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
				teleEdit_destMoves[i].fromHome[0], teleEdit_destMoves[i].fromHome[1],
				teleEdit_destMoves[i].fromHome[2],
				teleEdit_destMoves[i].toOrigin[0], teleEdit_destMoves[i].toOrigin[1],
				teleEdit_destMoves[i].toOrigin[2], teleEdit_destMoves[i].yaw );
		len = (int)strlen( line );
		trap_FS_Write( line, len, f );
	}

	trap_FS_FCloseFile( f );
	teleEdit_dirty = qfalse;
}

static void TeleEdit_ClearRecords( void ) {
	teleEdit_triggerMoveCount = 0;
	teleEdit_destMoveCount = 0;
	teleEdit_dirty = qfalse;
}

void SpawnEdit_Tele_InitMap( void ) {
	char	mapname[MAX_QPATH];
	char	info[MAX_INFO_STRING];
	const char *s;

	if ( teleEdit_dirty ) {
		TeleEdit_CommitLayout();
	}

	trap_GetServerinfo( info, sizeof( info ) );
	s = Info_ValueForKey( info, "mapname" );
	if ( s && s[0] ) {
		Q_strncpyz( mapname, s, sizeof( mapname ) );
	} else {
		trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );
	}
	Q_strncpyz( teleEdit_mapName, mapname, sizeof( teleEdit_mapName ) );
	teleEdit_bspTriggerCount = 0;
	teleEdit_bspDestCount = 0;
	TeleEdit_ClearRecords();
}

void SpawnEdit_Tele_OnEntitiesSpawned( void ) {
	TeleEdit_ApplyAllMoves();
}

void SpawnEdit_Tele_Shutdown( int restart ) {
	if ( restart && teleEdit_mapName[0] ) {
		if ( teleEdit_dirty ) {
			TeleEdit_CommitLayout();
		}
		return;
	}
	TeleEdit_PurgeSessionLayout();
}

void SpawnEdit_Tele_SendRoster( gentity_t *ent ) {
	gentity_t	*trigger;
	gentity_t	*dest;
	char		entry[160];
	char		string[4096];
	int			count;
	int			i;
	int			j;
	int			stringlength;
	vec3_t		homeAngles;

	if ( !ent || !ent->client ) {
		return;
	}

	string[0] = '\0';
	stringlength = 0;
	count = 0;

	for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
		trigger = &g_entities[i];
		if ( !TeleEdit_IsLiveTrigger( trigger ) ) {
			continue;
		}
		dest = TeleEdit_FindDestForTrigger( trigger );
		TeleEdit_HomeForDest( dest, NULL, homeAngles );
		Com_sprintf( entry, sizeof( entry ),
				"%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i ",
				trigger->s.number,
				(int)trigger->s.pos.trBase[0], (int)trigger->s.pos.trBase[1], (int)trigger->s.pos.trBase[2],
				(int)trigger->s.origin2[0], (int)trigger->s.origin2[1], (int)trigger->s.origin2[2],
				dest ? dest->s.number : -1,
				dest ? (int)dest->s.pos.trBase[0] : 0,
				dest ? (int)dest->s.pos.trBase[1] : 0,
				dest ? (int)dest->s.pos.trBase[2] : 0,
				dest ? (int)dest->s.angles[1] : 0,
				dest ? (int)dest->s.origin2[0] : 0,
				dest ? (int)dest->s.origin2[1] : 0,
				dest ? (int)dest->s.origin2[2] : 0,
				dest ? (int)homeAngles[1] : 0,
				trigger->s.generic1 );
		j = (int)strlen( entry );
		if ( stringlength + j >= (int)sizeof( string ) ) {
			break;
		}
		strcat( string, entry );
		stringlength += j;
		count++;
	}

	trap_SendServerCommand( ent - g_entities, va( "spawnEditTeleRoster %i %s", count, string ) );
}

void SpawnEdit_Tele_BroadcastRoster( void ) {
	int			i;
	gentity_t	*ent;

	for ( i = 0; i < level.maxclients; i++ ) {
		ent = &g_entities[i];
		if ( !ent->client || ent->client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		SpawnEdit_Tele_SendRoster( ent );
	}
}

qboolean SpawnEdit_Tele_HandleCommand( gentity_t *ent, const char *sub ) {
	char		numBuf[MAX_TOKEN_CHARS];
	int			entNum;
	gentity_t	*trigger;
	gentity_t	*dest;
	vec3_t		toOrigin;
	vec3_t		home;
	vec3_t		homeAngles;
	float		yaw;

	if ( !TeleEdit_ClientMayEdit( ent ) ) {
		return qfalse;
	}

	if ( !Q_stricmp( sub, "telesync" ) ) {
		SpawnEdit_Tele_SendRoster( ent );
		return qtrue;
	}

	if ( !Q_stricmp( sub, "telereset" ) ) {
		int	restored;

		restored = TeleEdit_RestoreMapDefaults();
		TeleEdit_ClearRecords();
		TeleEdit_PurgeSessionLayout();
		SpawnEdit_Tele_BroadcastRoster();
		trap_SendServerCommand( ent - g_entities,
				va( "print \"Teleporter layout cleared; %i entit(y/ies) restored.\n\"", restored ) );
		return qtrue;
	}

	if ( !Q_stricmp( sub, "telemove" ) ) {
		if ( trap_Argc() < 6 ) {
			trap_SendServerCommand( ent - g_entities,
					"print \"usage: spawnedit telemove <triggerEnt> <x> <y> <z>\n\"" );
			return qtrue;
		}
		trap_Argv( 2, numBuf, sizeof( numBuf ) );
		entNum = atoi( numBuf );
		trap_Argv( 3, numBuf, sizeof( numBuf ) );
		toOrigin[0] = atof( numBuf );
		trap_Argv( 4, numBuf, sizeof( numBuf ) );
		toOrigin[1] = atof( numBuf );
		trap_Argv( 5, numBuf, sizeof( numBuf ) );
		toOrigin[2] = atof( numBuf );

		trigger = &g_entities[entNum];
		if ( !TeleEdit_IsLiveTrigger( trigger ) ) {
			return qtrue;
		}
		TeleEdit_HomeForTrigger( trigger, home );
		TeleEdit_ApplyTriggerMove( trigger, toOrigin );
		TeleEdit_UpsertTriggerMove( home, toOrigin );
		TeleEdit_CommitLayout();
		TeleEdit_RefreshPrediction();
		SpawnEdit_Tele_BroadcastRoster();
		trap_SendServerCommand( ent - g_entities,
				va( "print \"Teleporter entrance %i moved.\n\"", entNum ) );
		return qtrue;
	}

	if ( !Q_stricmp( sub, "teledestmove" ) ) {
		if ( trap_Argc() < 7 ) {
			trap_SendServerCommand( ent - g_entities,
					"print \"usage: spawnedit teledestmove <triggerEnt> <x> <y> <z> <yaw>\n\"" );
			return qtrue;
		}
		trap_Argv( 2, numBuf, sizeof( numBuf ) );
		entNum = atoi( numBuf );
		trap_Argv( 3, numBuf, sizeof( numBuf ) );
		toOrigin[0] = atof( numBuf );
		trap_Argv( 4, numBuf, sizeof( numBuf ) );
		toOrigin[1] = atof( numBuf );
		trap_Argv( 5, numBuf, sizeof( numBuf ) );
		toOrigin[2] = atof( numBuf );
		trap_Argv( 6, numBuf, sizeof( numBuf ) );
		yaw = atof( numBuf );

		trigger = &g_entities[entNum];
		if ( !TeleEdit_IsLiveTrigger( trigger ) ) {
			return qtrue;
		}
		dest = TeleEdit_FindDestForTrigger( trigger );
		if ( !dest ) {
			trap_SendServerCommand( ent - g_entities,
					"print \"Teleporter has no linked exit.\n\"" );
			return qtrue;
		}
		if ( !G_OriginFitsPlayerBox( toOrigin, ENTITYNUM_NONE ) ) {
			trap_SendServerCommand( ent - g_entities,
					"print \"Exit placement blocked by solid geometry.\n\"" );
			return qtrue;
		}
		TeleEdit_HomeForDest( dest, home, homeAngles );
		TeleEdit_ApplyDestMove( dest, toOrigin, yaw );
		TeleEdit_UpsertDestMove( home, toOrigin, yaw );
		TeleEdit_CommitLayout();
		TeleEdit_RefreshPrediction();
		SpawnEdit_Tele_BroadcastRoster();
		trap_SendServerCommand( ent - g_entities,
				va( "print \"Teleporter exit for %i moved.\n\"", entNum ) );
		return qtrue;
	}

	if ( !Q_stricmp( sub, "telehome" ) ) {
		if ( trap_Argc() < 3 ) {
			trap_SendServerCommand( ent - g_entities,
					"print \"usage: spawnedit telehome <triggerEnt>\n\"" );
			return qtrue;
		}
		trap_Argv( 2, numBuf, sizeof( numBuf ) );
		entNum = atoi( numBuf );
		trigger = &g_entities[entNum];
		if ( !TeleEdit_IsLiveTrigger( trigger ) ) {
			return qtrue;
		}
		TeleEdit_HomeForTrigger( trigger, home );
		TeleEdit_ApplyTriggerMove( trigger, home );
		TeleEdit_UpsertTriggerMove( home, home );
		dest = TeleEdit_FindDestForTrigger( trigger );
		if ( dest ) {
			vec3_t	destHome;

			TeleEdit_HomeForDest( dest, destHome, homeAngles );
			TeleEdit_ApplyDestMove( dest, destHome, homeAngles[1] );
			TeleEdit_UpsertDestMove( destHome, destHome, homeAngles[1] );
		}
		TeleEdit_CommitLayout();
		TeleEdit_RefreshPrediction();
		SpawnEdit_Tele_BroadcastRoster();
		trap_SendServerCommand( ent - g_entities,
				va( "print \"Teleporter %i restored to map defaults.\n\"", entNum ) );
		return qtrue;
	}

	return qfalse;
}
