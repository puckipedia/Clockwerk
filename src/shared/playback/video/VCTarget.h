/*
 * Copyright 2000-2006, Ingo Weinhold <ingo_weinhold@gmx.de>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

// Derived classes are video consumer targets. Each time the consumer
// has received a frame (that is not late and thus dropped) it calls
// SetBitmap(). This method should immediately do whatever has to be done.
// Until the next call to SetBitmap() the bitmap can be used -- thereafter
// it is not allowed to use it any longer. Therefore the bitmap variable
// is protected by a lock. Anytime it is going to be accessed, the object
// must be locked.

#ifndef VC_TARGET_H
#define VC_TARGET_H

#include <Locker.h>

class BBitmap;

class VCTarget {
 public:
								VCTarget();
	virtual						~VCTarget();

			bool				LockBitmap();
			void				UnlockBitmap();

	virtual	void				SetBitmap(const BBitmap* bitmap);
			const BBitmap*		GetBitmap() const;

 protected:
			BLocker				fBitmapLock;
			const BBitmap* volatile	fBitmap;
};

#endif	// VC_TARGET_H
