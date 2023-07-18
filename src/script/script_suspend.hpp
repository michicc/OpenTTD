/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_suspend.hpp The Script_Suspend tracks the suspension of a script. */

#ifndef SCRIPT_SUSPEND_HPP
#define SCRIPT_SUSPEND_HPP

/**
 * The callback function when a script suspends.
 */
typedef void (Script_SuspendCallbackProc)(class ScriptInstance *instance);

/**
 * A throw-class that is given when the script wants to suspend.
 */
class Script_Suspend {
public:
	/**
	 * Create the suspend exception.
	 * @param time The amount of ticks to suspend.
	 * @param callback The callback to call when the script may resume again.
	 * @param wake_event Stop suspend if an event is in the event queue?
	 */
	Script_Suspend(int time, Script_SuspendCallbackProc *callback, bool wake_event = false) :
		time(time),
		callback(callback),
		wake_event(wake_event)
	{}

	/**
	 * Get the amount of ticks the script should be suspended.
	 * @return The amount of ticks to suspend the script.
	 */
	int GetSuspendTime() { return time; }

	/**
	 * Get the callback to call when the script can run again.
	 * @return The callback function to run.
	 */
	Script_SuspendCallbackProc *GetSuspendCallback() { return callback; }

	/**
	 * Stop suspend if an event is in the event queue?
	 * @return True if the suspend should end on an event.
	 */
	bool WakeForEvent() { return wake_event; }

private:
	int time;                             ///< Amount of ticks to suspend the script.
	Script_SuspendCallbackProc *callback; ///< Callback function to call when the script can run again.
	bool wake_event;                      ///< Stop suspend if an event is in the event queue?
};

#endif /* SCRIPT_SUSPEND_HPP */
