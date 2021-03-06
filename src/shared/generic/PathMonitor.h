/*
 * Copyright 2007, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _PATH_MONITOR_H
#define _PATH_MONITOR_H


#include <NodeMonitor.h>


// additional flags (combined with those in NodeMonitor.h)
#define	B_WATCH_FILES_ONLY	0x0100
#define B_WATCH_RECURSIVELY	0x0200

#define B_PATH_MONITOR		'_PMN'

namespace BPrivate {

class BPathMonitor {
	public:
		static status_t StartWatching(const char* path, uint32 flags,
			BMessenger target, BLooper* looper = NULL);
			// If specified, looper should be a running looper, the path monitor
			// can use to add its BHandler. Defaults to be_app, if already
			// initialized, otherwise a private looper will be created.

		static status_t StopWatching(const char* path, BMessenger target);
		static status_t StopWatching(BMessenger target);

	private:
		BPathMonitor();
		~BPathMonitor();

		static status_t _InitIfNeeded();
};

}	// namespace BPrivate

#endif	// _PATH_MONITOR_H
