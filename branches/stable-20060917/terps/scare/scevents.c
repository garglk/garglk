/* vi: set ts=8:
 *
 * Copyright (C) 2003-2005  Simon Baldwin and Mark J. Tilford
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 */

/*
 * Module notes:
 *
 * o Event pause and resume tasks need more testing.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
enum {			ROOMLIST_NO_ROOMS	= 0,
			ROOMLIST_ONE_ROOM	= 1,
			ROOMLIST_SOME_ROOMS	= 2,
			ROOMLIST_ALL_ROOMS	= 3 };

/* Trace flag, set before running. */
static sc_bool		evt_trace		= FALSE;


/*
 * evt_any_task_in_state()
 *
 * Return TRUE if any task at all matches the given completion state.
 */
static sc_bool
evt_any_task_in_state (sc_gamestateref_t gamestate, sc_bool state)
{
	sc_int32	task;

	/* Scan tasks for any whose completion matches input. */
	for (task = 0; task < gamestate->task_count; task++)
	    {
		if (gs_get_task_done (gamestate, task) == state)
			return TRUE;
	    }

	/* No tasks matched. */
	return FALSE;
}


/*
 * evt_can_see_event()
 *
 * Return TRUE if player is in the right room for event text.
 */
sc_bool
evt_can_see_event (sc_gamestateref_t gamestate, sc_int32 event)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5];
	sc_int32		type;

	/* Check room list for the event and return it. */
	vt_key[0].string = "Events";
	vt_key[1].integer = event;
	vt_key[2].string = "Where";
	vt_key[3].string = "Type";
	type = prop_get_integer (bundle, "I<-siss", vt_key);
	switch (type)
	    {
	    case ROOMLIST_NO_ROOMS:			return FALSE;
	    case ROOMLIST_ALL_ROOMS:			return TRUE;

	    case ROOMLIST_ONE_ROOM:
		vt_key[3].string = "Room";
		return prop_get_integer (bundle, "I<-siss", vt_key)
						== gamestate->playerroom;

	    case ROOMLIST_SOME_ROOMS:
		vt_key[3].string = "Rooms";
		vt_key[4].integer = gamestate->playerroom;
		return prop_get_boolean (bundle, "B<-sissi", vt_key);

	    default:
		sc_fatal ("task_can_run_task: invalid type, %ld\n", type);
		return FALSE;
	    }
}


/*
 * evt_move_object()
 *
 * Move an object from within an event.
 */
static void
evt_move_object (sc_gamestateref_t gamestate, sc_int32 obj, sc_int32 dest)
{
	/* Ignore negative values of object. */
	if (obj >= 0)
	    {
		if (evt_trace)
		    {
			sc_trace ("Event: moving object %ld"
						" to room %ld\n", obj, dest);
		    }

		/* Move object depending on destination. */
		switch (dest)
		    {
		    case -1:			/* Hidden. */
			gs_object_make_hidden (gamestate, obj);
			break;

		    case 0:			/* Held by player. */
			gs_object_player_get (gamestate, obj);
			break;

		    case 1:			/* Same room as player. */
			gs_object_to_room (gamestate,
						obj, gamestate->playerroom);
			break;

		    default:
			if (dest < gamestate->room_count + 2)
				gs_object_to_room (gamestate, obj, dest - 2);
			else
				gs_object_to_room (gamestate, obj,
					lib_random_roomgroup_member
					(gamestate,
					    dest - gamestate->room_count - 2));
			break;
		    }
	    }
}


/*
 * evt_start_event()
 *
 * Change an event from WAITING to RUNNING.
 */
static void
evt_start_event (sc_gamestateref_t gamestate, sc_int32 event)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[4];
	sc_int32		time1, time2;
	sc_int32		obj1, obj1dest;

	if (evt_trace)
		sc_trace ("Event: starting event %ld\n", event);

	/* If event is visible, print its start text. */
	if (evt_can_see_event (gamestate, event))
	    {
		sc_char		*starttext;

		/* Get and print start text. */
		vt_key[0].string = "Events";
		vt_key[1].integer = event;
		vt_key[2].string = "StartText";
		starttext = prop_get_string (bundle, "S<-sis", vt_key);
		if (!sc_strempty (starttext))
		    {
			pf_buffer_string (filter, starttext);
			pf_buffer_character (filter, '\n');
		    }

		/* Handle any associated resource. */
		vt_key[2].string = "Res";
		vt_key[3].integer = 0;
		res_handle_resource (gamestate, "sisi", vt_key);
	    }

	/* Set the event's state and time. */
	gs_set_event_state (gamestate, event, ES_RUNNING);

	vt_key[0].string = "Events";
	vt_key[1].integer = event;
	vt_key[2].string = "Time1";
	time1 = prop_get_integer (bundle, "I<-sis", vt_key);
	vt_key[2].string = "Time2";
	time2 = prop_get_integer (bundle, "I<-sis", vt_key);
	gs_set_event_time (gamestate, event, sc_randomint (time1, time2));

	/* Move event object to destination. */
	vt_key[2].string = "Obj1";
	obj1 = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
	vt_key[2].string = "Obj1Dest";
	obj1dest = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
	evt_move_object (gamestate, obj1, obj1dest);
}


/*
 * evt_finish_event()
 *
 * Move an event to FINISHED, or restart it.
 */
static void
evt_finish_event (sc_gamestateref_t gamestate, sc_int32 event)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[4];
	sc_int32		obj2, obj2dest;
	sc_int32		obj3, obj3dest;
	sc_int32		task;
	sc_int32		startertype, restarttype;
	sc_bool			taskdir;

	if (evt_trace)
		sc_trace ("Event: finishing event %ld\n", event);

	/* If event is visible, print its finish text. */
	if (evt_can_see_event (gamestate, event))
	    {
		sc_char		*finishtext;

		/* Get and print finish text. */
		vt_key[0].string = "Events";
		vt_key[1].integer = event;
		vt_key[2].string = "FinishText";
		finishtext = prop_get_string (bundle, "S<-sis", vt_key);
		if (!sc_strempty (finishtext))
		    {
			pf_buffer_string (filter, finishtext);
			pf_buffer_character (filter, '\n');
		    }

		/* Handle any associated resource. */
		vt_key[2].string = "Res";
		vt_key[3].integer = 4;
		res_handle_resource (gamestate, "sisi", vt_key);
	    }

	/* Move event objects to destination. */
	vt_key[0].string = "Events";
	vt_key[1].integer = event;
	vt_key[2].string = "Obj2";
	obj2 = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
	vt_key[2].string = "Obj2Dest";
	obj2dest = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
	evt_move_object (gamestate, obj2, obj2dest);

	vt_key[2].string = "Obj3";
	obj3 = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
	vt_key[2].string = "Obj3Dest";
	obj3dest = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
	evt_move_object (gamestate, obj3, obj3dest);

	/* See if there is an affected task. */
	vt_key[2].string = "TaskAffected";
	task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
	if (task >= 0)
	    {
		vt_key[2].string = "TaskFinished";
		taskdir = !prop_get_boolean (bundle, "B<-sis", vt_key);
		if (task_can_run_task (gamestate, task))
		    {
			if (evt_trace)
			    {
				sc_trace ("Event: event running task %ld, %s\n",
				    task, taskdir ? "forwards" : "backwards");
			    }
			task_run_task (gamestate, task, taskdir);
		    }
		else
		    {
			if (evt_trace)
			    {
				sc_trace ("Event: event can't run"
							" task %ld\n", task);
			    }
		    }
	    }

	/* Handle possible restart. */
	vt_key[2].string = "RestartType";
	restarttype = prop_get_integer (bundle, "I<-sis", vt_key);
	switch (restarttype)
	    {
	    case 0:				/* Don't restart. */
		vt_key[2].string = "StarterType";
		startertype = prop_get_integer (bundle, "I<-sis", vt_key);
		switch (startertype)
		    {
		    case 1:			/* Immediate. */
		    case 2:			/* Random delay. */
		    case 3:			/* After task. */
			gs_set_event_state (gamestate, event, ES_FINISHED);
			break;

		    default:
			sc_fatal ("evt_finish_event: unknown value for"
					" starter type, %ld\n", startertype);
		    }
		break;

	    case 1:				/* Restart immediately. */
		gs_set_event_state (gamestate, event, ES_WAITING);
		gs_set_event_time (gamestate, event, 0);
		break;

	    case 2:				/* Restart after delay. */
		vt_key[2].string = "StarterType";
		startertype = prop_get_integer (bundle, "I<-sis", vt_key);
		switch (startertype)
		    {
		    case 1:			/* Immediate. */
			gs_set_event_state (gamestate, event, ES_WAITING);
			gs_set_event_time (gamestate, event, 0);
			break;

		    case 2:			/* Random delay. */
			{
			sc_int32	start, end;

			gs_set_event_state (gamestate, event, ES_WAITING);
			vt_key[2].string = "StartTime";
			start = prop_get_integer (bundle, "I<-sis", vt_key);
			vt_key[2].string = "EndTime";
			end = prop_get_integer (bundle, "I<-sis", vt_key);
			gs_set_event_time (gamestate, event,
						sc_randomint (start, end));
			break;
			}

		    case 3:			/* After event. */
			gs_set_event_state (gamestate, event, ES_AWAITING);
			gs_set_event_time (gamestate, event, 0);
			break;

		    default:
			sc_fatal ("evt_finish_event: unknown StarterType\n");
		    }
		break;

	    default:
		sc_fatal ("evt_finish_event: unknown RestartType\n");
	    }
}


/*
 * evt_tick_event()
 *
 * Attempt to advance an event by one turn.
 */
static void
evt_tick_event (sc_gamestateref_t gamestate, sc_int32 event)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[4];

	/* Handle call based on current event state. */
	switch (gs_get_event_state (gamestate, event))
	    {
	    case ES_WAITING:
		gs_dec_event_time (gamestate, event);
		if (gs_get_event_time (gamestate, event) <= 0)
			evt_start_event (gamestate, event);
		return;

	    case ES_RUNNING:
		{
		sc_int32	startertype;
		sc_int32	pausetask, resumetask;
		sc_bool		pcompleted, rcompleted, pause, resume;

		if (evt_trace)
			sc_trace ("Event: running event %ld\n", event);

		vt_key[0].string = "Events";
		vt_key[1].integer = event;
		vt_key[2].string = "StarterType";
		startertype = prop_get_integer (bundle, "I<-sis", vt_key);
		if (startertype == 3)
		    {
			sc_int32	task;

			vt_key[2].string = "TaskNum";
			task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
			if (!gs_get_task_done (gamestate, task))
			    {
				gs_set_event_state (gamestate,
							event, ES_AWAITING);
				return;
			    }
		    }

		vt_key[2].string = "PauseTask";
		pausetask = prop_get_integer (bundle, "I<-sis", vt_key);
		vt_key[2].string = "PauserCompleted";
		pcompleted = !prop_get_boolean (bundle, "B<-sis", vt_key);

		pause = FALSE;
		if (pausetask == 1)
		    {
			if (evt_any_task_in_state (gamestate, pcompleted))
				pause = TRUE;
		    }
		else if (pausetask > 1)
		    {
			if (pcompleted == gs_get_task_done
						(gamestate, pausetask - 2))
				pause = TRUE;
		    }

		vt_key[2].string = "ResumeTask";
		resumetask = prop_get_integer (bundle, "I<-sis", vt_key);
		vt_key[2].string = "ResumerCompleted";
		rcompleted = !prop_get_boolean (bundle, "B<-sis", vt_key);

		resume = FALSE;
		if (resumetask == 1)
		    {
			if (evt_any_task_in_state (gamestate, rcompleted))
				resume = TRUE;
		    }
		else if (resumetask > 1)
		    {
			if (rcompleted == gs_get_task_done
						(gamestate, resumetask - 2))
				resume = TRUE;
		    }

		if (pause && !resume)
		    {
			if (evt_trace)
				sc_trace ("Event: pausing event %ld\n", event);

			gs_set_event_state (gamestate, event, ES_PAUSED);
			return;
		    }

		gs_dec_event_time (gamestate, event);
		if (evt_can_see_event (gamestate, event))
		    {
			sc_int32	preftime1, preftime2;
			sc_char		*preftext;

			vt_key[2].string = "PrefTime1";
			preftime1 = prop_get_integer (bundle,
							"I<-sis", vt_key);
			if (preftime1
				== gs_get_event_time (gamestate, event))
			    {
				vt_key[2].string = "PrefText1";
				preftext = prop_get_string
						(bundle, "S<-sis", vt_key);
				if (!sc_strempty (preftext))
				    {
					pf_buffer_string (filter, preftext);
					pf_buffer_character (filter, '\n');
				    }

				vt_key[2].string = "Res";
				vt_key[3].integer = 2;
				res_handle_resource (gamestate, "sisi", vt_key);
			    }

			vt_key[2].string = "PrefTime2";
			preftime2 = prop_get_integer (bundle,
							"I<-sis", vt_key);
			if (preftime2
				== gs_get_event_time (gamestate, event))
			    {
				vt_key[2].string = "PrefText2";
				preftext = prop_get_string
						(bundle, "S<-sis", vt_key);
				if (!sc_strempty (preftext))
				    {
					pf_buffer_string (filter, preftext);
					pf_buffer_character (filter, '\n');
				    }

				vt_key[2].string = "Res";
				vt_key[3].integer = 3;
				res_handle_resource (gamestate, "sisi", vt_key);
			    }
		    }

		if (gs_get_event_time (gamestate, event) <= 0)
			evt_finish_event (gamestate, event);
		return;
		}

	    case ES_AWAITING:
		{
		sc_int32	task;

		vt_key[0].string = "Events";
		vt_key[1].integer = event;
		vt_key[2].string = "TaskNum";
		task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
		if (gs_get_task_done (gamestate, task))
			evt_start_event (gamestate, event);
		return;
		}

	    case ES_FINISHED:
		{
		sc_int32	startertype;

		if (evt_trace)
			sc_trace ("Event: finished event %ld\n", event);

		vt_key[0].string = "Events";
		vt_key[1].integer = event;
		vt_key[2].string = "StarterType";
		startertype = prop_get_integer (bundle, "I<-sis", vt_key);
		if (startertype == 3)
		    {
			sc_int32	task;

			vt_key[2].string = "TaskNum";
			task = prop_get_integer
					(bundle, "I<-sis", vt_key) - 1;
			if (!gs_get_task_done (gamestate, task))
			    {
				gs_set_event_state (gamestate,
							event, ES_AWAITING);
				return;
			    }
		    }
		return;
		}

	    case ES_PAUSED:
		{
		sc_int32	pausetask, resumetask;
		sc_bool		pcompleted, rcompleted;

		vt_key[0].string = "Events";
		vt_key[1].integer = event;

		vt_key[2].string = "PauseTask";
		pausetask = prop_get_integer (bundle, "I<-sis", vt_key);
		vt_key[2].string = "PauserCompleted";
		pcompleted = !prop_get_boolean (bundle, "B<-sis", vt_key);

		vt_key[2].string = "ResumeTask";
		resumetask = prop_get_integer (bundle, "I<-sis", vt_key);
		vt_key[2].string = "ResumerCompleted";
		rcompleted = !prop_get_boolean (bundle, "B<-sis", vt_key);

		if (pausetask == 1
			&& !evt_any_task_in_state (gamestate, pcompleted))
		    {
			if (evt_trace)
			    {
				sc_trace ("Event: resuming %ld, all tasks"
						" wrong state\n", event);
			    }

			gs_set_event_state (gamestate, event, ES_RUNNING);
		    }
		else if (pausetask > 1
			&& pcompleted != gs_get_task_done
						(gamestate, pausetask - 2))
		    {
			if (evt_trace)
			    {
				sc_trace ("Event: resuming %ld, task %ld"
				" wrong state\n", event, pausetask - 2);
			    }

			gs_set_event_state (gamestate, event, ES_RUNNING);
		    }
		else if (resumetask == 1
			&& evt_any_task_in_state (gamestate, rcompleted))
		    {
			if (evt_trace)
			    {
				sc_trace ("Event: resuming %ld, all tasks"
						" right state\n", event);
			    }

			gs_set_event_state (gamestate, event, ES_RUNNING);
		    }
		else if (resumetask > 1
			&& rcompleted == gs_get_task_done
						(gamestate, resumetask - 2))
		    {
			if (evt_trace)
			    {
				sc_trace ("Event: resuming %ld, task %ld"
				" right state\n", event, resumetask - 2);
			    }

			gs_set_event_state (gamestate, event, ES_RUNNING);
		    }
		else if (gs_get_event_time (gamestate, event) <= 0)
			evt_finish_event (gamestate, event);
		return;
		}

	    default:
		sc_fatal ("evt_tick: invalid event state\n");
	    }
}


/*
 * evt_tick_events()
 *
 * Attempt to advance each event by one turn.
 */
void
evt_tick_events (sc_gamestateref_t gamestate)
{
	sc_uint16	state;
	sc_int32	event;

	/* Iterate over event states. */
	for (state = ES_WAITING; state <= ES_PAUSED; state++)
	    {
		/* Tickle events in this state. */
		for (event = 0; event < gamestate->event_count; event++)
		    {
			if (gs_get_event_state (gamestate, event) == state)
				evt_tick_event (gamestate, event);
		    }
	    }
}


/*
 * run_debug_trace()
 *
 * Set run tracing on/off.
 */
void
evt_debug_trace (sc_bool flag)
{
	evt_trace = flag;
}
