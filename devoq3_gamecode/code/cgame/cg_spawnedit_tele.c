/*
===========================================================================
Teleporter editing (Spawn UI panel).
===========================================================================
*/

#include "cg_local.h"
#include "cg_overlay.h"
#include "cg_mapedit.h"
#include "cg_spawnedit_tele.h"
#include "../client/keycodes.h"

#define SPAWNEDIT_TELE_ROW_H			14
#define SPAWNEDIT_TELE_PANEL_PAD		8
#define SPAWNEDIT_TELE_MODE_GAP			4
#define SPAWNEDIT_TELE_ORIGIN_EPS		4.0f
#define SPAWNEDIT_TELE_HOME_Z_SLACK		24.0f
#define SPAWNEDIT_TELE_LOOK_MIN_DOT		0.95f
#define SPAWNEDIT_TELE_LOS_EPS			32.0f
#define SPAWNEDIT_TELE_VISIBLE_ROWS		6
#define SPAWNEDIT_TELE_RESET_BTN_H		18
#define SPAWNEDIT_TELE_MOVE_ROW_Y		( OVERLAY_SIDE_HDR_H + 30 )
#define SPAWNEDIT_TELE_HDR_LINE1_Y		( OVERLAY_SIDE_HDR_H + 6 )
#define SPAWNEDIT_TELE_HDR_LINE2_Y		( OVERLAY_SIDE_HDR_H + 16 )
#define SPAWNEDIT_TELE_CONTENT_TOP		( OVERLAY_SIDE_HDR_H + 4 + SPAWNEDIT_TELE_MODE_TAB_H + SPAWNEDIT_TELE_MODE_GAP )

static spawnEditTeleEntry_t	cg_spawnEditTeleEntries[SPAWNEDIT_TELE_MAX];
static int						cg_spawnEditTeleCount;
static int						cg_spawnEditTeleSel;
static int						cg_spawnEditTeleTarget = -1;
static int						cg_spawnEditTelePart;
static int						cg_spawnEditPanelMode;
static qboolean					cg_spawnEditTeleMoveActive;
static qboolean					cg_spawnEditTeleMoveValid;
static int						cg_spawnEditTeleMoveTrigger;
static vec3_t					cg_spawnEditTeleMoveGhostOrigin;
static vec3_t					cg_spawnEditTeleMoveGhostAngles;
static vec3_t					cg_spawnEditTeleCentroidOffset;
static int						cg_spawnEditTeleMoveBtnHover;
static int						cg_spawnEditTeleResetBtnHover;
static qboolean					cg_spawnEditTeleResetPrompt;
static int						cg_spawnEditTeleResetModalHover;
static qboolean					cg_spawnEditTeleDefaultLineHover;

static const char *cg_spawnEditTelePartLabel[SPAWNEDIT_TELE_PART_COUNT] = {
	"Move entrance",
	"Move exit"
};

static qboolean CG_SpawnEdit_Tele_Active( void );
static spawnEditTeleEntry_t *CG_SpawnEdit_Tele_EntryForTrigger( int triggerEnt );
static void CG_SpawnEdit_Tele_GetTriggerCentroid( int triggerEnt, vec3_t out );
static void CG_SpawnEdit_Tele_DrawBrushBBox( const vec3_t origin, int modelIndex, const float rgba[4] );
static void CG_SpawnEdit_Tele_ClampSel( void );
static qboolean CG_SpawnEdit_Tele_TargetRelocated( spawnEditTeleEntry_t *entry );
static void CG_SpawnEdit_Tele_ContentRect( int bodyX, int bodyY, int bodyW, int bodyH,
		int *x, int *y, int *w, int *h );

static qboolean CG_SpawnEdit_Tele_Active( void ) {
	if ( cg.demoPlayback || !CG_SpectatorView() || !CG_ItemEdit_ModeEnabled() ) {
		return qfalse;
	}
	return qtrue;
}

void CG_SpawnEdit_Tele_Init( void ) {
	cg_spawnEditTeleCount = 0;
	cg_spawnEditTeleSel = 0;
	cg_spawnEditTeleTarget = -1;
	cg_spawnEditTelePart = 0;
	cg_spawnEditPanelMode = 0;
	CG_SpawnEdit_Tele_CancelMove();
	cg_spawnEditTeleResetPrompt = qfalse;
}

int CG_SpawnEdit_Tele_PanelMode( void ) {
	return cg_spawnEditPanelMode;
}

void CG_SpawnEdit_Tele_SetPanelMode( int mode ) {
	if ( mode < 0 || mode > 1 ) {
		return;
	}
	if ( mode != cg_spawnEditPanelMode ) {
		CG_SpawnEdit_Tele_CancelMove();
		CG_SpawnEdit_AbortMove();
		cg_spawnEditTeleTarget = -1;
		cg_spawnEditPanelMode = mode;
		if ( mode == 0 ) {
			cg_spawnEditTeleResetPrompt = qfalse;
		}
	}
}

void CG_SpawnEdit_Tele_OnSidebarClose( void ) {
	CG_SpawnEdit_Tele_CancelMove();
	cg_spawnEditTeleTarget = -1;
	cg_spawnEditTeleResetPrompt = qfalse;
}

void CG_SpawnEdit_Tele_ParseRoster( void ) {
	int		i;
	int		count;
	int		base;

	count = atoi( CG_Argv( 1 ) );
	if ( count > SPAWNEDIT_TELE_MAX ) {
		count = SPAWNEDIT_TELE_MAX;
	} else if ( count < 0 ) {
		count = 0;
	}
	cg_spawnEditTeleCount = count;
	for ( i = 0; i < count; i++ ) {
		base = 2 + i * 17;
		cg_spawnEditTeleEntries[i].triggerEnt = atoi( CG_Argv( base ) );
		cg_spawnEditTeleEntries[i].triggerOrigin[0] = (float)atoi( CG_Argv( base + 1 ) );
		cg_spawnEditTeleEntries[i].triggerOrigin[1] = (float)atoi( CG_Argv( base + 2 ) );
		cg_spawnEditTeleEntries[i].triggerOrigin[2] = (float)atoi( CG_Argv( base + 3 ) );
		cg_spawnEditTeleEntries[i].triggerHome[0] = (float)atoi( CG_Argv( base + 4 ) );
		cg_spawnEditTeleEntries[i].triggerHome[1] = (float)atoi( CG_Argv( base + 5 ) );
		cg_spawnEditTeleEntries[i].triggerHome[2] = (float)atoi( CG_Argv( base + 6 ) );
		cg_spawnEditTeleEntries[i].destEnt = atoi( CG_Argv( base + 7 ) );
		cg_spawnEditTeleEntries[i].destOrigin[0] = (float)atoi( CG_Argv( base + 8 ) );
		cg_spawnEditTeleEntries[i].destOrigin[1] = (float)atoi( CG_Argv( base + 9 ) );
		cg_spawnEditTeleEntries[i].destOrigin[2] = (float)atoi( CG_Argv( base + 10 ) );
		cg_spawnEditTeleEntries[i].destYaw = (float)atoi( CG_Argv( base + 11 ) );
		cg_spawnEditTeleEntries[i].destHome[0] = (float)atoi( CG_Argv( base + 12 ) );
		cg_spawnEditTeleEntries[i].destHome[1] = (float)atoi( CG_Argv( base + 13 ) );
		cg_spawnEditTeleEntries[i].destHome[2] = (float)atoi( CG_Argv( base + 14 ) );
		cg_spawnEditTeleEntries[i].destHomeYaw = (float)atoi( CG_Argv( base + 15 ) );
		cg_spawnEditTeleEntries[i].predictable = atoi( CG_Argv( base + 16 ) );
	}
	if ( cg_spawnEditTeleTarget >= 0 && !CG_SpawnEdit_Tele_EntryForTrigger( cg_spawnEditTeleTarget ) ) {
		cg_spawnEditTeleTarget = -1;
	}
	CG_SpawnEdit_Tele_ClampSel();
}

static spawnEditTeleEntry_t *CG_SpawnEdit_Tele_EntryForTrigger( int triggerEnt ) {
	int	i;

	for ( i = 0; i < cg_spawnEditTeleCount; i++ ) {
		if ( cg_spawnEditTeleEntries[i].triggerEnt == triggerEnt ) {
			return &cg_spawnEditTeleEntries[i];
		}
	}
	return NULL;
}

static void CG_SpawnEdit_Tele_GetTriggerCentroid( int triggerEnt, vec3_t out ) {
	centity_t	*cent;
	vec3_t		mid;

	if ( !out ) {
		return;
	}
	if ( triggerEnt < 0 || triggerEnt >= MAX_GENTITIES ) {
		VectorClear( out );
		return;
	}
	cent = &cg_entities[triggerEnt];
	if ( cent->currentState.solid == SOLID_BMODEL && cent->currentState.modelindex > 0 ) {
		VectorCopy( cgs.inlineModelMidpoints[cent->currentState.modelindex], mid );
		VectorAdd( cent->lerpOrigin, mid, out );
		return;
	}
	VectorCopy( cent->lerpOrigin, out );
}

static void CG_SpawnEdit_Tele_ClampSel( void ) {
	if ( cg_spawnEditTeleCount <= 0 ) {
		cg_spawnEditTeleSel = 0;
		return;
	}
	if ( cg_spawnEditTeleSel < 0 ) {
		cg_spawnEditTeleSel = 0;
	}
	if ( cg_spawnEditTeleSel >= cg_spawnEditTeleCount ) {
		cg_spawnEditTeleSel = cg_spawnEditTeleCount - 1;
	}
}

static qboolean CG_SpawnEdit_Tele_HasLineOfSight( const vec3_t viewOrg, const vec3_t targetOrg ) {
	trace_t	tr;
	float	dist;

	CG_Trace( &tr, viewOrg, vec3_origin, vec3_origin, targetOrg, -1, MASK_SOLID );
	if ( tr.fraction >= 1.0f ) {
		return qtrue;
	}
	dist = Distance( tr.endpos, targetOrg );
	return ( dist <= SPAWNEDIT_TELE_LOS_EPS ) ? qtrue : qfalse;
}

static int CG_SpawnEdit_Tele_PickLookTarget( void ) {
	int			i;
	float		bestDot;
	float		dot;
	int			bestEnt;
	vec3_t		forward;
	vec3_t		to;
	vec3_t		centroid;
	spawnEditTeleEntry_t *entry;

	bestEnt = -1;
	bestDot = SPAWNEDIT_TELE_LOOK_MIN_DOT;
	VectorCopy( cg.refdef.viewaxis[0], forward );

	for ( i = 0; i < cg_spawnEditTeleCount; i++ ) {
		entry = &cg_spawnEditTeleEntries[i];
		if ( cg_spawnEditTeleMoveActive && entry->triggerEnt == cg_spawnEditTeleMoveTrigger ) {
			continue;
		}
		CG_SpawnEdit_Tele_GetTriggerCentroid( entry->triggerEnt, centroid );
		VectorSubtract( centroid, cg.refdef.vieworg, to );
		if ( VectorNormalize( to ) == 0.0f ) {
			continue;
		}
		dot = DotProduct( forward, to );
		if ( dot < SPAWNEDIT_TELE_LOOK_MIN_DOT ) {
			continue;
		}
		if ( !CG_SpawnEdit_Tele_HasLineOfSight( cg.refdef.vieworg, centroid ) ) {
			continue;
		}
		if ( dot > bestDot ) {
			bestDot = dot;
			bestEnt = entry->triggerEnt;
		}
	}
	return bestEnt;
}

void CG_SpawnEdit_Tele_Frame( void ) {
	spawnEditTeleEntry_t	*entry;

	if ( !CG_SpawnEdit_Tele_Active() || cg_spawnEditPanelMode != 1 ) {
		cg_spawnEditTeleTarget = -1;
		return;
	}

	if ( cg_spawnEditTeleMoveActive ) {
		cg_spawnEditTeleTarget = cg_spawnEditTeleMoveTrigger;
		return;
	}

	cg_spawnEditTeleTarget = CG_SpawnEdit_Tele_PickLookTarget();
	if ( cg_spawnEditTeleTarget >= 0 ) {
		entry = CG_SpawnEdit_Tele_EntryForTrigger( cg_spawnEditTeleTarget );
		if ( entry ) {
			for ( cg_spawnEditTeleSel = 0; cg_spawnEditTeleSel < cg_spawnEditTeleCount; cg_spawnEditTeleSel++ ) {
				if ( cg_spawnEditTeleEntries[cg_spawnEditTeleSel].triggerEnt == cg_spawnEditTeleTarget ) {
					break;
				}
			}
		}
	}
}

void CG_SpawnEdit_Tele_CancelMove( void ) {
	cg_spawnEditTeleMoveActive = qfalse;
	cg_spawnEditTeleMoveValid = qfalse;
	cg_spawnEditTeleMoveTrigger = -1;
	cg_spawnEditTeleMoveBtnHover = -1;
	VectorClear( cg_spawnEditTeleCentroidOffset );
}

qboolean CG_SpawnEdit_Tele_MoveActive( void ) {
	return cg_spawnEditTeleMoveActive;
}

void CG_SpawnEdit_Tele_UpdateMoveGhost( void ) {
	spawnEditTeleEntry_t	*entry;
	vec3_t					candidate;
	vec3_t					planeNormal;
	vec3_t					resolved;
	vec3_t					centroid;
	vec3_t					newOrigin;
	int						skipEnt;

	if ( !cg_spawnEditTeleMoveActive || cg_spawnEditTeleMoveTrigger < 0 ) {
		return;
	}
	entry = CG_SpawnEdit_Tele_EntryForTrigger( cg_spawnEditTeleMoveTrigger );
	if ( !entry ) {
		return;
	}

	skipEnt = cg_spawnEditTeleMoveTrigger;
	if ( CG_MapEdit_TraceCrosshairSurface( skipEnt, candidate, planeNormal ) ) {
		if ( cg_spawnEditTelePart == 1 ) {
			if ( entry->destEnt < 0 ) {
				cg_spawnEditTeleMoveValid = qfalse;
				VectorCopy( candidate, cg_spawnEditTeleMoveGhostOrigin );
			} else if ( CG_MapEdit_ResolvePlayerPlacement( candidate, cg.refdef.viewaxis[0], planeNormal,
					skipEnt, resolved ) ) {
				VectorCopy( resolved, cg_spawnEditTeleMoveGhostOrigin );
				cg_spawnEditTeleMoveValid = qtrue;
			} else {
				VectorCopy( candidate, cg_spawnEditTeleMoveGhostOrigin );
				cg_spawnEditTeleMoveValid = qfalse;
			}
			cg_spawnEditTeleMoveGhostAngles[0] = 0.0f;
			cg_spawnEditTeleMoveGhostAngles[1] = cg.refdefViewAngles[1];
			cg_spawnEditTeleMoveGhostAngles[2] = 0.0f;
		} else if ( CG_MapEdit_ResolvePlayerPlacement( candidate, cg.refdef.viewaxis[0], planeNormal,
				skipEnt, resolved ) ) {
			VectorSubtract( resolved, cg_spawnEditTeleCentroidOffset, newOrigin );
			VectorCopy( newOrigin, cg_spawnEditTeleMoveGhostOrigin );
			cg_spawnEditTeleMoveValid = qtrue;
		} else {
			VectorSubtract( candidate, cg_spawnEditTeleCentroidOffset, newOrigin );
			VectorCopy( newOrigin, cg_spawnEditTeleMoveGhostOrigin );
			cg_spawnEditTeleMoveValid = qfalse;
		}
	} else {
		VectorSubtract( candidate, cg_spawnEditTeleCentroidOffset, newOrigin );
		VectorCopy( newOrigin, cg_spawnEditTeleMoveGhostOrigin );
		cg_spawnEditTeleMoveValid = qfalse;
	}
}

void CG_SpawnEdit_Tele_StartMove( void ) {
	spawnEditTeleEntry_t	*entry;
	vec3_t					centroid;

	if ( cg_spawnEditTeleTarget < 0 ) {
		return;
	}
	entry = CG_SpawnEdit_Tele_EntryForTrigger( cg_spawnEditTeleTarget );
	if ( !entry ) {
		return;
	}
	if ( cg_spawnEditTelePart == 1 && entry->destEnt < 0 ) {
		return;
	}
	cg_spawnEditTeleMoveTrigger = cg_spawnEditTeleTarget;
	cg_spawnEditTeleMoveActive = qtrue;
	cg_spawnEditTeleMoveValid = qfalse;
	CG_SpawnEdit_Tele_GetTriggerCentroid( cg_spawnEditTeleMoveTrigger, centroid );
	VectorSubtract( centroid, entry->triggerOrigin, cg_spawnEditTeleCentroidOffset );
	CG_SpawnEdit_Tele_UpdateMoveGhost();
}

void CG_SpawnEdit_Tele_ConfirmMove( void ) {
	spawnEditTeleEntry_t	*entry;

	if ( !cg_spawnEditTeleMoveActive || !cg_spawnEditTeleMoveValid ) {
		return;
	}
	entry = CG_SpawnEdit_Tele_EntryForTrigger( cg_spawnEditTeleMoveTrigger );
	if ( !entry ) {
		CG_SpawnEdit_Tele_CancelMove();
		return;
	}
	if ( cg_spawnEditTelePart == 1 ) {
		trap_SendClientCommand( va( "spawnedit teledestmove %i %.3f %.3f %.3f %.3f",
				cg_spawnEditTeleMoveTrigger,
				cg_spawnEditTeleMoveGhostOrigin[0],
				cg_spawnEditTeleMoveGhostOrigin[1],
				cg_spawnEditTeleMoveGhostOrigin[2],
				cg_spawnEditTeleMoveGhostAngles[1] ) );
		VectorCopy( cg_spawnEditTeleMoveGhostOrigin, entry->destOrigin );
		entry->destYaw = cg_spawnEditTeleMoveGhostAngles[1];
	} else {
		trap_SendClientCommand( va( "spawnedit telemove %i %.3f %.3f %.3f",
				cg_spawnEditTeleMoveTrigger,
				cg_spawnEditTeleMoveGhostOrigin[0],
				cg_spawnEditTeleMoveGhostOrigin[1],
				cg_spawnEditTeleMoveGhostOrigin[2] ) );
		VectorCopy( cg_spawnEditTeleMoveGhostOrigin, entry->triggerOrigin );
	}
	CG_SpawnEdit_Tele_CancelMove();
}

void CG_SpawnEdit_Tele_RestoreHome( void ) {
	if ( cg_spawnEditTeleTarget < 0 ) {
		return;
	}
	trap_SendClientCommand( va( "spawnedit telehome %i", cg_spawnEditTeleTarget ) );
}

static void CG_SpawnEdit_Tele_DrawBrushBBox( const vec3_t origin, int modelIndex, const float rgba[4] ) {
	vec3_t			mins, maxs;
	vec3_t			corners[8];
	polyVert_t		verts[4];
	int				i, a, b;
	qhandle_t		shader;
	qhandle_t		model;
	static const int edges[12][2] = {
		{0,1},{1,3},{3,2},{2,0},
		{4,5},{5,7},{7,6},{6,4},
		{0,4},{1,5},{2,6},{3,7}
	};

	if ( modelIndex < 1 || modelIndex >= cgs.numInlineModels || !origin || !rgba ) {
		return;
	}
	model = cgs.inlineDrawModel[modelIndex];
	if ( !model ) {
		return;
	}
	trap_R_ModelBounds( model, mins, maxs );
	for ( i = 0; i < 8; i++ ) {
		corners[i][0] = origin[0] + ( ( i & 1 ) ? maxs[0] : mins[0] );
		corners[i][1] = origin[1] + ( ( i & 2 ) ? maxs[1] : mins[1] );
		corners[i][2] = origin[2] + ( ( i & 4 ) ? maxs[2] : mins[2] );
	}
	shader = trap_R_RegisterShader( "bbox_nocull" );
	if ( !shader ) {
		return;
	}
	for ( i = 0; i < 12; i++ ) {
		a = edges[i][0];
		b = edges[i][1];
		verts[0].xyz[0] = corners[a][0];
		verts[0].xyz[1] = corners[a][1];
		verts[0].xyz[2] = corners[a][2];
		verts[1].xyz[0] = corners[b][0];
		verts[1].xyz[1] = corners[b][1];
		verts[1].xyz[2] = corners[b][2];
		verts[2].xyz[0] = corners[b][0];
		verts[2].xyz[1] = corners[b][1];
		verts[2].xyz[2] = corners[b][2] + 0.1f;
		verts[3].xyz[0] = corners[a][0];
		verts[3].xyz[1] = corners[a][1];
		verts[3].xyz[2] = corners[a][2] + 0.1f;
		for ( a = 0; a < 4; a++ ) {
			verts[a].modulate[0] = (byte)( rgba[0] * 255.0f );
			verts[a].modulate[1] = (byte)( rgba[1] * 255.0f );
			verts[a].modulate[2] = (byte)( rgba[2] * 255.0f );
			verts[a].modulate[3] = (byte)( rgba[3] * 255.0f );
		}
		trap_R_AddPolyToScene( shader, 4, verts );
	}
}

void CG_SpawnEdit_Tele_AddSceneEntities( void ) {
	spawnEditTeleEntry_t	*entry;
	centity_t				*cent;
	spawnpoint_t			destSpot;
	float					rgba[4];
	float					rgbaDim[4];
	int						i;
	int						highlight;

	if ( !CG_SpawnEdit_Tele_Active() || cg_spawnEditPanelMode != 1 ) {
		return;
	}

	rgba[0] = 0.2f; rgba[1] = 0.85f; rgba[2] = 1.0f; rgba[3] = 0.85f;
	rgbaDim[0] = 0.35f; rgbaDim[1] = 0.45f; rgbaDim[2] = 0.55f; rgbaDim[3] = 0.45f;
	highlight = CG_SpawnEdit_Tele_HighlightEnt();

	for ( i = 0; i < cg_spawnEditTeleCount; i++ ) {
		entry = &cg_spawnEditTeleEntries[i];
		if ( cg_spawnEditTeleMoveActive && entry->triggerEnt == cg_spawnEditTeleMoveTrigger ) {
			continue;
		}
		cent = &cg_entities[entry->triggerEnt];
		if ( cent->currentState.eType != ET_TELEPORT_TRIGGER ) {
			continue;
		}
		CG_SpawnEdit_Tele_DrawBrushBBox( cent->lerpOrigin, cent->currentState.modelindex,
				( entry->triggerEnt == highlight ) ? rgba : rgbaDim );
		if ( entry->destEnt >= 0 ) {
			memset( &destSpot, 0, sizeof( destSpot ) );
			VectorCopy( entry->destOrigin, destSpot.origin );
			destSpot.angle[1] = entry->destYaw;
			destSpot.team = TEAM_BLUE;
			CG_DrawSpawnpointMarkers( &destSpot, NULL, 1,
					( entry->triggerEnt == highlight && cg_spawnEditTelePart == 1 ) ? entry->triggerEnt : -1 );
		}
	}

	if ( cg_spawnEditTeleMoveActive && cg_spawnEditTeleMoveValid ) {
		if ( cg_spawnEditTelePart == 1 ) {
			memset( &destSpot, 0, sizeof( destSpot ) );
			VectorCopy( cg_spawnEditTeleMoveGhostOrigin, destSpot.origin );
			destSpot.angle[1] = cg_spawnEditTeleMoveGhostAngles[1];
			destSpot.team = TEAM_RED;
			CG_DrawSpawnpointMarkers( &destSpot, NULL, 1, -1 );
		} else {
			cent = &cg_entities[cg_spawnEditTeleMoveTrigger];
			CG_SpawnEdit_Tele_DrawBrushBBox( cg_spawnEditTeleMoveGhostOrigin, cent->currentState.modelindex, rgba );
		}
	}
}

int CG_SpawnEdit_Tele_HighlightEnt( void ) {
	if ( cg_spawnEditPanelMode != 1 || cg_spawnEditTeleMoveActive ) {
		return -1;
	}
	return cg_spawnEditTeleTarget;
}

void CG_SpawnEdit_Tele_DrawModeTabs( int bodyX, int bodyY, int bodyW ) {
	vec4_t	active;
	vec4_t	idle;
	vec4_t	text;
	int		tabW;
	int		x;
	int		y;

	active[0] = 0.25f; active[1] = 0.45f; active[2] = 0.65f; active[3] = 0.75f;
	idle[0] = 0.10f; idle[1] = 0.12f; idle[2] = 0.16f; idle[3] = 0.55f;
	text[0] = 0.92f; text[1] = 0.94f; text[2] = 1.0f; text[3] = 1.0f;
	y = bodyY + OVERLAY_SIDE_HDR_H + 4;
	tabW = ( bodyW - SPAWNEDIT_TELE_PANEL_PAD * 2 - 4 ) / 2;
	x = bodyX + SPAWNEDIT_TELE_PANEL_PAD;
	CG_FillRect( x, y, tabW, SPAWNEDIT_TELE_MODE_TAB_H,
			( cg_spawnEditPanelMode == 0 ) ? active : idle );
	CG_DrawStringExt( x + 8, y + 3, "Spawns", text, qfalse, qtrue, 6, 10, 0 );
	x += tabW + 4;
	CG_FillRect( x, y, tabW, SPAWNEDIT_TELE_MODE_TAB_H,
			( cg_spawnEditPanelMode == 1 ) ? active : idle );
	CG_DrawStringExt( x + 4, y + 3, "Teleporters", text, qfalse, qtrue, 6, 10, 0 );
}

qboolean CG_SpawnEdit_Tele_ModeTabHit( int mx, int my, int bodyX, int bodyY, int bodyW ) {
	int	tabW;
	int	x;
	int	y;

	y = bodyY + OVERLAY_SIDE_HDR_H + 4;
	tabW = ( bodyW - SPAWNEDIT_TELE_PANEL_PAD * 2 - 4 ) / 2;
	x = bodyX + SPAWNEDIT_TELE_PANEL_PAD;
	if ( my >= y && my < y + SPAWNEDIT_TELE_MODE_TAB_H ) {
		if ( mx >= x && mx < x + tabW ) {
			CG_SpawnEdit_Tele_SetPanelMode( 0 );
			return qtrue;
		}
		x += tabW + 4;
		if ( mx >= x && mx < x + tabW ) {
			CG_SpawnEdit_Tele_SetPanelMode( 1 );
			return qtrue;
		}
	}
	return qfalse;
}

static void CG_SpawnEdit_Tele_ContentRect( int bodyX, int bodyY, int bodyW, int bodyH,
		int *x, int *y, int *w, int *h ) {
	*x = bodyX + SPAWNEDIT_TELE_PANEL_PAD;
	*y = bodyY + SPAWNEDIT_TELE_CONTENT_TOP;
	*w = bodyW - SPAWNEDIT_TELE_PANEL_PAD * 2;
	*h = bodyH - SPAWNEDIT_TELE_CONTENT_TOP - SPAWNEDIT_TELE_PANEL_PAD
			- SPAWNEDIT_TELE_RESET_BTN_H - 8;
	if ( *w < 1 ) {
		*w = 1;
	}
	if ( *h < 1 ) {
		*h = 1;
	}
}

static qboolean CG_SpawnEdit_Tele_TargetRelocated( spawnEditTeleEntry_t *entry ) {
	vec3_t	delta;
	float	xyDist;

	if ( !entry ) {
		return qfalse;
	}
	VectorSubtract( entry->triggerOrigin, entry->triggerHome, delta );
	xyDist = sqrt( delta[0] * delta[0] + delta[1] * delta[1] );
	if ( xyDist > SPAWNEDIT_TELE_ORIGIN_EPS ) {
		return qtrue;
	}
	if ( fabs( delta[2] ) > SPAWNEDIT_TELE_HOME_Z_SLACK ) {
		return qtrue;
	}
	if ( entry->destEnt >= 0 ) {
		VectorSubtract( entry->destOrigin, entry->destHome, delta );
		xyDist = sqrt( delta[0] * delta[0] + delta[1] * delta[1] );
		if ( xyDist > SPAWNEDIT_TELE_ORIGIN_EPS ) {
			return qtrue;
		}
		if ( fabs( delta[2] ) > SPAWNEDIT_TELE_HOME_Z_SLACK ) {
			return qtrue;
		}
	}
	return qfalse;
}

static void CG_SpawnEdit_Tele_DrawHeader( int bodyX, int bodyY, spawnEditTeleEntry_t *entry ) {
	char	line[96];
	vec4_t	value;
	vec4_t	muted;
	vec4_t	hover;

	value[0] = 0.9f; value[1] = 0.92f; value[2] = 1.0f; value[3] = 1.0f;
	muted[0] = 0.55f; muted[1] = 0.58f; muted[2] = 0.62f; muted[3] = 1.0f;
	hover[0] = 0.55f; hover[1] = 0.75f; hover[2] = 1.0f; hover[3] = 1.0f;

	if ( !entry ) {
		CG_DrawStringExt( bodyX + SPAWNEDIT_TELE_PANEL_PAD, bodyY + SPAWNEDIT_TELE_HDR_LINE1_Y,
				"No teleporter selected", muted, qtrue, qtrue, 6, 10, 0 );
		return;
	}
	Com_sprintf( line, sizeof( line ), "Teleporter ent %i", entry->triggerEnt );
	CG_DrawStringExt( bodyX + SPAWNEDIT_TELE_PANEL_PAD, bodyY + SPAWNEDIT_TELE_HDR_LINE1_Y,
			line, value, qtrue, qtrue, 6, 10, 0 );
	if ( entry->destEnt < 0 ) {
		Com_sprintf( line, sizeof( line ), "Exit: (none linked)" );
	} else if ( entry->predictable ) {
		Com_sprintf( line, sizeof( line ), "Exit ent %i (predicted)", entry->destEnt );
	} else {
		Com_sprintf( line, sizeof( line ), "Exit ent %i", entry->destEnt );
	}
	if ( CG_SpawnEdit_Tele_TargetRelocated( entry ) && cg_spawnEditTeleDefaultLineHover ) {
		CG_DrawStringExt( bodyX + SPAWNEDIT_TELE_PANEL_PAD, bodyY + SPAWNEDIT_TELE_HDR_LINE2_Y,
				"Restore map defaults", hover, qtrue, qtrue, 6, 10, 0 );
	} else if ( CG_SpawnEdit_Tele_TargetRelocated( entry ) ) {
		CG_DrawStringExt( bodyX + SPAWNEDIT_TELE_PANEL_PAD, bodyY + SPAWNEDIT_TELE_HDR_LINE2_Y,
				"Restore map defaults", value, qtrue, qtrue, 6, 10, 0 );
	} else {
		CG_DrawStringExt( bodyX + SPAWNEDIT_TELE_PANEL_PAD, bodyY + SPAWNEDIT_TELE_HDR_LINE2_Y,
				line, muted, qtrue, qtrue, 6, 10, 0 );
	}
}

static void CG_SpawnEdit_Tele_DrawList( int bodyX, int bodyY, int bodyW, int bodyH ) {
	int		x, y, w, h;
	int		i;
	int		rowY;
	char	line[48];
	vec4_t	row;
	vec4_t	text;

	CG_SpawnEdit_Tele_ContentRect( bodyX, bodyY, bodyW, bodyH, &x, &y, &w, &h );
	text[0] = 0.9f; text[1] = 0.92f; text[2] = 1.0f; text[3] = 1.0f;
	rowY = y;
	for ( i = 0; i < cg_spawnEditTeleCount && i < SPAWNEDIT_TELE_VISIBLE_ROWS; i++ ) {
		if ( i == cg_spawnEditTeleSel ) {
			row[0] = 0.25f; row[1] = 0.45f; row[2] = 0.65f; row[3] = 0.55f;
		} else {
			row[0] = 0.12f; row[1] = 0.14f; row[2] = 0.18f; row[3] = 0.45f;
		}
		CG_FillRect( x, rowY, w, SPAWNEDIT_TELE_ROW_H - 1, row );
		Com_sprintf( line, sizeof( line ), "Teleporter %i (ent %i)", i + 1,
				cg_spawnEditTeleEntries[i].triggerEnt );
		CG_DrawStringExt( x + 8, rowY + 2, line, text, qfalse, qtrue, 6, 10, 0 );
		rowY += SPAWNEDIT_TELE_ROW_H;
	}
	if ( cg_spawnEditTeleCount <= 0 ) {
		CG_DrawStringExt( x, rowY + 4, "No teleporters on this map", text, qfalse, qtrue, 6, 10, 0 );
	}
}

static void CG_SpawnEdit_Tele_DrawPartPicker( int bodyX, int bodyY, int bodyW, int bodyH ) {
	int		x, y, w, h;
	int		i;
	int		rowY;
	vec4_t	row;
	vec4_t	text;

	CG_SpawnEdit_Tele_ContentRect( bodyX, bodyY, bodyW, bodyH, &x, &y, &w, &h );
	rowY = y + cg_spawnEditTeleCount * SPAWNEDIT_TELE_ROW_H;
	if ( rowY > y + h - SPAWNEDIT_TELE_PART_COUNT * SPAWNEDIT_TELE_ROW_H ) {
		rowY = y + h - SPAWNEDIT_TELE_PART_COUNT * SPAWNEDIT_TELE_ROW_H;
	}
	text[0] = 0.9f; text[1] = 0.92f; text[2] = 1.0f; text[3] = 1.0f;
	for ( i = 0; i < SPAWNEDIT_TELE_PART_COUNT; i++ ) {
		if ( i == cg_spawnEditTelePart ) {
			row[0] = 0.25f; row[1] = 0.45f; row[2] = 0.65f; row[3] = 0.55f;
		} else {
			row[0] = 0.12f; row[1] = 0.14f; row[2] = 0.18f; row[3] = 0.45f;
		}
		CG_FillRect( x, rowY, w, SPAWNEDIT_TELE_ROW_H - 1, row );
		CG_DrawStringExt( x + 10, rowY + 2, cg_spawnEditTelePartLabel[i], text, qfalse, qtrue, 6, 10, 0 );
		rowY += SPAWNEDIT_TELE_ROW_H;
	}
}

void CG_SpawnEdit_Tele_DrawContent( int bodyX, int bodyY, int bodyW, int bodyH ) {
	spawnEditTeleEntry_t	*entry;

	CG_SpawnEdit_Tele_DrawModeTabs( bodyX, bodyY, bodyW );
	entry = ( cg_spawnEditTeleTarget >= 0 )
			? CG_SpawnEdit_Tele_EntryForTrigger( cg_spawnEditTeleTarget ) : NULL;
	if ( !entry && cg_spawnEditTeleCount > 0 && cg_spawnEditTeleSel >= 0
			&& cg_spawnEditTeleSel < cg_spawnEditTeleCount ) {
		cg_spawnEditTeleTarget = cg_spawnEditTeleEntries[cg_spawnEditTeleSel].triggerEnt;
		entry = &cg_spawnEditTeleEntries[cg_spawnEditTeleSel];
	}
	CG_SpawnEdit_Tele_DrawHeader( bodyX, bodyY, entry );
	CG_SpawnEdit_Tele_DrawMoveRow( bodyX, bodyY, bodyW );
	CG_SpawnEdit_Tele_DrawList( bodyX, bodyY, bodyW, bodyH );
	CG_SpawnEdit_Tele_DrawPartPicker( bodyX, bodyY, bodyW, bodyH );
	CG_SpawnEdit_Tele_DrawResetFooter( bodyX, bodyY, bodyW, bodyH );
}

static overlayEditMoveUi_t CG_SpawnEdit_Tele_MoveUi( void ) {
	overlayEditMoveUi_t	ui;

	ui.panelPad = SPAWNEDIT_TELE_PANEL_PAD;
	ui.rowY = SPAWNEDIT_TELE_MOVE_ROW_Y;
	ui.iconSize = 18;
	ui.btnH = 18;
	ui.gap = 4;
	ui.moveActive = cg_spawnEditTeleMoveActive;
	ui.moveValid = cg_spawnEditTeleMoveValid;
	ui.showHomeAux = ( cg_spawnEditTeleTarget >= 0
			&& CG_SpawnEdit_Tele_TargetRelocated( CG_SpawnEdit_Tele_EntryForTrigger( cg_spawnEditTeleTarget ) ) );
	ui.hoverBtn = cg_spawnEditTeleMoveBtnHover;
	return ui;
}

void CG_SpawnEdit_Tele_DrawMoveRow( int bodyX, int bodyY, int bodyW ) {
	overlayEditMoveUi_t	ui;
	int					moveX, moveY, moveW, moveH;
	int					auxX, auxY, auxW, auxH;
	qboolean			auxIsCancel;

	if ( cg_spawnEditTeleTarget < 0 && !cg_spawnEditTeleMoveActive ) {
		return;
	}
	ui = CG_SpawnEdit_Tele_MoveUi();
	Overlay_EditMoveLayout( bodyX, bodyY, &ui,
			&moveX, &moveY, &moveW, &moveH,
			&auxX, &auxY, &auxW, &auxH, &auxIsCancel );
	Overlay_DrawEditMoveButtons( &ui, moveX, moveY, moveW, moveH,
			auxX, auxY, auxW, auxH, auxIsCancel );
}

qboolean CG_SpawnEdit_Tele_MoveBtnHit( int which, int mx, int my, int bodyX, int bodyY, int bodyW ) {
	overlayEditMoveUi_t	ui;
	int					moveX, moveY, moveW, moveH;
	int					auxX, auxY, auxW, auxH;
	qboolean			auxIsCancel;

	if ( cg_spawnEditTeleTarget < 0 && !cg_spawnEditTeleMoveActive ) {
		return qfalse;
	}
	ui = CG_SpawnEdit_Tele_MoveUi();
	Overlay_EditMoveLayout( bodyX, bodyY, &ui,
			&moveX, &moveY, &moveW, &moveH,
			&auxX, &auxY, &auxW, &auxH, &auxIsCancel );
	return Overlay_EditMoveBtnHit( &ui, which, mx, my,
			moveX, moveY, moveW, moveH, auxX, auxY, auxW, auxH );
}

void CG_SpawnEdit_Tele_DrawResetFooter( int bodyX, int bodyY, int bodyW, int bodyH ) {
	int			x, y, w, h;
	vec4_t		btn;
	vec4_t		border;
	vec4_t		text;
	const char	*label;
	int			len;

	x = bodyX + SPAWNEDIT_TELE_PANEL_PAD;
	w = bodyW - SPAWNEDIT_TELE_PANEL_PAD * 2;
	h = SPAWNEDIT_TELE_RESET_BTN_H;
	y = bodyY + bodyH - SPAWNEDIT_TELE_PANEL_PAD - h;
	btn[0] = 0.22f; btn[1] = 0.10f; btn[2] = 0.10f;
	btn[3] = ( cg_spawnEditTeleResetBtnHover >= 0 ) ? 0.92f : 0.78f;
	border[0] = 0.85f; border[1] = 0.35f; border[2] = 0.35f; border[3] = 0.65f;
	text[0] = 1.0f; text[1] = 0.92f; text[2] = 0.92f; text[3] = 1.0f;
	CG_FillRect( x, y, w, h, btn );
	CG_DrawRect( x, y, w, h, 1, border );
	label = "Reset All Teles";
	len = CG_DrawStrlen( label );
	CG_DrawStringExt( x + ( w - len * 6 ) / 2, y + 4, label, text, qtrue, qtrue, 6, 10, 0 );
}

qboolean CG_SpawnEdit_Tele_ResetBtnHit( int mx, int my, int bodyX, int bodyY, int bodyW, int bodyH ) {
	int	x, y, w, h;

	x = bodyX + SPAWNEDIT_TELE_PANEL_PAD;
	w = bodyW - SPAWNEDIT_TELE_PANEL_PAD * 2;
	h = SPAWNEDIT_TELE_RESET_BTN_H;
	y = bodyY + bodyH - SPAWNEDIT_TELE_PANEL_PAD - h;
	return ( mx >= x && mx < x + w && my >= y && my < y + h );
}

qboolean CG_SpawnEdit_Tele_ResetPrompt( void ) {
	return cg_spawnEditTeleResetPrompt;
}

void CG_SpawnEdit_Tele_SetResetPrompt( qboolean on ) {
	cg_spawnEditTeleResetPrompt = on;
}

void CG_SpawnEdit_Tele_DrawResetModal( void ) {
	int			x, y, w, h;
	vec4_t		dim;
	vec4_t		panel;
	vec4_t		border;
	vec4_t		btnIdle;
	vec4_t		btnHover;
	vec4_t		textColor;

	if ( !cg_spawnEditTeleResetPrompt ) {
		return;
	}
	dim[0] = dim[1] = dim[2] = 0.0f; dim[3] = 0.55f;
	panel[0] = 0.06f; panel[1] = 0.06f; panel[2] = 0.08f; panel[3] = 0.94f;
	border[0] = border[1] = border[2] = 1.0f; border[3] = 0.45f;
	btnIdle[0] = btnIdle[1] = 0.14f; btnIdle[2] = 0.16f; btnIdle[3] = 0.95f;
	btnHover[0] = btnHover[1] = 0.30f; btnHover[2] = 0.34f; btnHover[3] = 0.95f;
	textColor[0] = textColor[1] = textColor[2] = textColor[3] = 1.0f;
	w = 400;
	h = 120;
	x = ( SCREEN_WIDTH - w ) / 2;
	y = ( SCREEN_HEIGHT - h ) / 2;
	CG_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, dim );
	CG_FillRect( x, y, w, h, panel );
	CG_DrawRect( x, y, w, h, 1, border );
	CG_DrawStringExt( x + 12, y + 18, "Revert all teleporters to BSP layout?", textColor, qtrue, qtrue, 6, 10, 0 );
	{
		int	bx, by, bw, bh;
		int	i;
		const char *label;
		const float *fill;

		for ( i = 0; i < 2; i++ ) {
			bw = 72;
			bh = 22;
			by = y + h - 38;
			bx = x + ( w - ( 2 * bw + 16 ) ) / 2;
			if ( i ) {
				bx += bw + 16;
			}
			fill = ( i == cg_spawnEditTeleResetModalHover ) ? btnHover : btnIdle;
			CG_FillRect( bx, by, bw, bh, fill );
			CG_DrawRect( bx, by, bw, bh, 1, border );
			label = i ? "Cancel" : "Reset";
			CG_DrawStringExt( bx + ( bw - CG_DrawStrlen( label ) * 6 ) / 2, by + 5,
					label, textColor, qtrue, qtrue, 6, 10, 0 );
		}
	}
}

int CG_SpawnEdit_Tele_ResetModalHitTest( int mx, int my ) {
	int	x, y, w, h;

	if ( !cg_spawnEditTeleResetPrompt ) {
		return -1;
	}
	w = 400;
	h = 120;
	x = ( SCREEN_WIDTH - w ) / 2;
	y = ( SCREEN_HEIGHT - h ) / 2;
	y += h - 38;
	w = 72;
	h = 22;
	x = ( SCREEN_WIDTH - ( 2 * w + 16 ) ) / 2;
	if ( mx >= x && mx <= x + w && my >= y && my <= y + h ) {
		return 0;
	}
	x += w + 16;
	if ( mx >= x && mx <= x + w && my >= y && my <= y + h ) {
		return 1;
	}
	return -1;
}

static qboolean CG_SpawnEdit_Tele_RowAt( int mx, int my, int bodyX, int bodyY, int bodyW, int bodyH,
		int *listIndex, int *partIndex ) {
	int	x, y, w, h;
	int	rowY;
	int	i;

	*listIndex = -1;
	*partIndex = -1;
	CG_SpawnEdit_Tele_ContentRect( bodyX, bodyY, bodyW, bodyH, &x, &y, &w, &h );
	if ( mx < x || mx >= x + w || my < y || my >= y + h ) {
		return qfalse;
	}
	rowY = y;
	for ( i = 0; i < cg_spawnEditTeleCount && i < SPAWNEDIT_TELE_VISIBLE_ROWS; i++ ) {
		if ( my >= rowY && my < rowY + SPAWNEDIT_TELE_ROW_H - 1 ) {
			*listIndex = i;
			return qtrue;
		}
		rowY += SPAWNEDIT_TELE_ROW_H;
	}
	rowY = y + cg_spawnEditTeleCount * SPAWNEDIT_TELE_ROW_H;
	if ( rowY > y + h - SPAWNEDIT_TELE_PART_COUNT * SPAWNEDIT_TELE_ROW_H ) {
		rowY = y + h - SPAWNEDIT_TELE_PART_COUNT * SPAWNEDIT_TELE_ROW_H;
	}
	for ( i = 0; i < SPAWNEDIT_TELE_PART_COUNT; i++ ) {
		if ( my >= rowY && my < rowY + SPAWNEDIT_TELE_ROW_H - 1 ) {
			*partIndex = i;
			return qtrue;
		}
		rowY += SPAWNEDIT_TELE_ROW_H;
	}
	return qfalse;
}

void CG_SpawnEdit_Tele_PointerMove( int mx, int my, int bodyX, int bodyY, int bodyW, int bodyH ) {
	spawnEditTeleEntry_t	*entry;
	int						listIndex;
	int						partIndex;

	cg_spawnEditTeleResetBtnHover = -1;
	cg_spawnEditTeleMoveBtnHover = -1;
	cg_spawnEditTeleDefaultLineHover = qfalse;
	if ( cg_spawnEditTeleResetPrompt ) {
		cg_spawnEditTeleResetModalHover = CG_SpawnEdit_Tele_ResetModalHitTest( mx, my );
		return;
	}
	if ( CG_SpawnEdit_Tele_ResetBtnHit( mx, my, bodyX, bodyY, bodyW, bodyH ) ) {
		cg_spawnEditTeleResetBtnHover = 0;
	}
	if ( CG_SpawnEdit_Tele_MoveBtnHit( 0, mx, my, bodyX, bodyY, bodyW ) ) {
		cg_spawnEditTeleMoveBtnHover = 0;
	} else if ( CG_SpawnEdit_Tele_MoveBtnHit( 1, mx, my, bodyX, bodyY, bodyW ) ) {
		cg_spawnEditTeleMoveBtnHover = 1;
	}
	entry = CG_SpawnEdit_Tele_EntryForTrigger( cg_spawnEditTeleTarget );
	if ( entry && CG_SpawnEdit_Tele_TargetRelocated( entry )
			&& mx >= bodyX + SPAWNEDIT_TELE_PANEL_PAD
			&& mx < bodyX + bodyW - SPAWNEDIT_TELE_PANEL_PAD
			&& my >= bodyY + SPAWNEDIT_TELE_HDR_LINE2_Y
			&& my < bodyY + SPAWNEDIT_TELE_HDR_LINE2_Y + 10 ) {
		cg_spawnEditTeleDefaultLineHover = qtrue;
	}
	if ( CG_SpawnEdit_Tele_RowAt( mx, my, bodyX, bodyY, bodyW, bodyH, &listIndex, &partIndex ) ) {
		if ( listIndex >= 0 ) {
			cg_spawnEditTeleSel = listIndex;
			CG_SpawnEdit_Tele_ClampSel();
		}
		if ( partIndex >= 0 ) {
			cg_spawnEditTelePart = partIndex;
		}
	}
	CG_SpawnEdit_Tele_UpdateMoveGhost();
}

qboolean CG_SpawnEdit_Tele_HandleClick( int mx, int my, int bodyX, int bodyY, int bodyW, int bodyH ) {
	int	listIndex;
	int	partIndex;
	int	modalBtn;

	if ( CG_SpawnEdit_Tele_ModeTabHit( mx, my, bodyX, bodyY, bodyW ) ) {
		return qtrue;
	}
	if ( cg_spawnEditPanelMode != 1 ) {
		return qfalse;
	}
	if ( cg_spawnEditTeleResetPrompt ) {
		modalBtn = CG_SpawnEdit_Tele_ResetModalHitTest( mx, my );
		if ( modalBtn == 0 ) {
			trap_SendClientCommand( "spawnedit telereset\n" );
			cg_spawnEditTeleResetPrompt = qfalse;
			return qtrue;
		}
		cg_spawnEditTeleResetPrompt = qfalse;
		return qtrue;
	}
	if ( CG_SpawnEdit_Tele_ResetBtnHit( mx, my, bodyX, bodyY, bodyW, bodyH ) ) {
		cg_spawnEditTeleResetPrompt = qtrue;
		return qtrue;
	}
	if ( CG_SpawnEdit_Tele_MoveBtnHit( 1, mx, my, bodyX, bodyY, bodyW ) ) {
		if ( cg_spawnEditTeleMoveActive ) {
			CG_SpawnEdit_Tele_CancelMove();
		} else {
			CG_SpawnEdit_Tele_RestoreHome();
		}
		return qtrue;
	}
	if ( CG_SpawnEdit_Tele_MoveBtnHit( 0, mx, my, bodyX, bodyY, bodyW ) ) {
		if ( cg_spawnEditTeleMoveActive ) {
			CG_SpawnEdit_Tele_ConfirmMove();
		} else {
			CG_SpawnEdit_Tele_StartMove();
		}
		return qtrue;
	}
	if ( cg_spawnEditTeleDefaultLineHover ) {
		CG_SpawnEdit_Tele_RestoreHome();
		return qtrue;
	}
	if ( CG_SpawnEdit_Tele_RowAt( mx, my, bodyX, bodyY, bodyW, bodyH, &listIndex, &partIndex ) ) {
		if ( listIndex >= 0 ) {
			cg_spawnEditTeleSel = listIndex;
			cg_spawnEditTeleTarget = cg_spawnEditTeleEntries[listIndex].triggerEnt;
		}
		if ( partIndex >= 0 ) {
			cg_spawnEditTelePart = partIndex;
		}
		return qtrue;
	}
	return qfalse;
}

qboolean CG_SpawnEdit_Tele_KeyEvent( int key, qboolean down ) {
	if ( cg_spawnEditPanelMode != 1 || !down ) {
		return qfalse;
	}
	if ( cg_spawnEditTeleMoveActive && key == K_ESCAPE ) {
		CG_SpawnEdit_Tele_CancelMove();
		return qtrue;
	}
	if ( cg_spawnEditTeleResetPrompt ) {
		if ( key == K_ESCAPE ) {
			cg_spawnEditTeleResetPrompt = qfalse;
			return qtrue;
		}
		if ( key == K_ENTER || key == K_KP_ENTER ) {
			trap_SendClientCommand( "spawnedit telereset\n" );
			cg_spawnEditTeleResetPrompt = qfalse;
			return qtrue;
		}
		return qtrue;
	}
	if ( key == K_MWHEELUP || key == K_UPARROW ) {
		cg_spawnEditTeleSel--;
		CG_SpawnEdit_Tele_ClampSel();
		if ( cg_spawnEditTeleSel >= 0 && cg_spawnEditTeleSel < cg_spawnEditTeleCount ) {
			cg_spawnEditTeleTarget = cg_spawnEditTeleEntries[cg_spawnEditTeleSel].triggerEnt;
		}
		return qtrue;
	}
	if ( key == K_MWHEELDOWN || key == K_DOWNARROW ) {
		cg_spawnEditTeleSel++;
		CG_SpawnEdit_Tele_ClampSel();
		if ( cg_spawnEditTeleSel >= 0 && cg_spawnEditTeleSel < cg_spawnEditTeleCount ) {
			cg_spawnEditTeleTarget = cg_spawnEditTeleEntries[cg_spawnEditTeleSel].triggerEnt;
		}
		return qtrue;
	}
	return qfalse;
}
