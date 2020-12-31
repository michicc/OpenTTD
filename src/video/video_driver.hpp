/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file video_driver.hpp Base of all video drivers. */

#ifndef VIDEO_VIDEO_DRIVER_HPP
#define VIDEO_VIDEO_DRIVER_HPP

#include "../driver.h"
#include "../core/geometry_type.hpp"
#include <vector>

/** The base of all video drivers. */
class VideoDriver : public Driver {
public:
	/**
	 * Mark a particular area dirty.
	 * @param left   The left most line of the dirty area.
	 * @param top    The top most line of the dirty area.
	 * @param width  The width of the dirty area.
	 * @param height The height of the dirty area.
	 */
	virtual void MakeDirty(int left, int top, int width, int height) = 0;

	/**
	 * Perform the actual drawing.
	 */
	virtual void MainLoop() = 0;

	/**
	 * Change the resolution of the window.
	 * @param w The new width.
	 * @param h The new height.
	 * @return True if the change succeeded.
	 */
	virtual bool ChangeResolution(int w, int h) = 0;

	/**
	 * Change the full screen setting.
	 * @param fullscreen The new setting.
	 * @return True if the change succeeded.
	 */
	virtual bool ToggleFullscreen(bool fullscreen) = 0;

	/**
	 * Callback invoked after the blitter was changed.
	 * This may only be called between AcquireBlitterLock and ReleaseBlitterLock.
	 * @return True if no error.
	 */
	virtual bool AfterBlitterChange()
	{
		return true;
	}

	/**
	 * Acquire any lock(s) required to be held when changing blitters.
	 * These lock(s) may not be acquired recursively.
	 */
	virtual void AcquireBlitterLock() { }

	/**
	 * Release any lock(s) required to be held when changing blitters.
	 * These lock(s) may not be acquired recursively.
	 */
	virtual void ReleaseBlitterLock() { }

	virtual bool ClaimMousePointer()
	{
		return true;
	}

	/**
	 * Whether the driver has a graphical user interface with the end user.
	 * Or in other words, whether we should spawn a thread for world generation
	 * and NewGRF scanning so the graphical updates can keep coming. Otherwise
	 * progress has to be shown on the console, which uses by definition another
	 * thread/process for display purposes.
	 * @return True for all drivers except null and dedicated.
	 */
	virtual bool HasGUI() const
	{
		return true;
	}

	/**
	 * Has this video driver an efficient code path for palette animated 8-bpp sprites?
	 * @return True if the driver has an efficient code path for 8-bpp.
	 */
	virtual bool HasEfficient8Bpp() const
	{
		return false;
	}

	/**
	 * Does this video driver support a separate animation buffer in addition to the colour buffer?
	 * @return True if a separate animation buffer is supported.
	 */
	virtual bool HasAnimBuffer()
	{
		return false;
	}

	/**
	 * Get a pointer to the animation buffer of the video back-end.
	 * @return Pointer to the buffer or NULL if no animation buffer is supported.
	 */
	virtual uint8 *GetAnimBuffer()
	{
		return NULL;
	}

	/**
	 * An edit box lost the input focus. Abort character compositing if necessary.
	 */
	virtual void EditBoxLostFocus() {}

	/**
	 * An edit box gained the input focus
	 */
	virtual void EditBoxGainedFocus() {}

	/**
	 * Get the currently active instance of the video driver.
	 */
	static VideoDriver *GetInstance() {
		return static_cast<VideoDriver*>(*DriverFactoryBase::GetActiveDriver(Driver::DT_VIDEO));
	}

	/**
	 * Helper struct to ensure the video buffer is locked and ready for drawing. The destructor
	 * will make sure the buffer is unlocked no matter how the scope is exited.
	 */
	struct VideoBufferLocker {
		VideoBufferLocker()
		{
			this->unlock = VideoDriver::GetInstance()->LockVideoBuffer();
		}

		~VideoBufferLocker()
		{
			if (this->unlock) VideoDriver::GetInstance()->UnlockVideoBuffer();
		}

	private:
		bool unlock; ///< Stores if the lock did anything that has to be undone.
	};

protected:
	/**
	 * Make sure the video buffer is ready for drawing.
	 * @returns True if the video buffer has to be unlocked.
	 */
	virtual bool LockVideoBuffer()
	{
		return false;
	}

	/**
	 * Unlock a previously locked video buffer.
	 */
	virtual void UnlockVideoBuffer() {}
};

extern std::string _ini_videodriver;
extern std::vector<Dimension> _resolutions;
extern Dimension _cur_resolution;
extern bool _rightclick_emulate;

#endif /* VIDEO_VIDEO_DRIVER_HPP */
