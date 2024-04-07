#pragma once

//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
//

typedef struct Spinlock Spinlock;

struct Spinlock
{
	pthread_spinlock_t lock;
};

static inline void
spinlock_init(Spinlock* self)
{
	pthread_spin_init(&self->lock, PTHREAD_PROCESS_PRIVATE);
}

static inline void
spinlock_free(Spinlock* self)
{
	pthread_spin_destroy(&self->lock);
}

static inline void
spinlock_lock(Spinlock* self)
{
	pthread_spin_lock(&self->lock);
}

static inline void
spinlock_unlock(Spinlock* self)
{
	pthread_spin_unlock(&self->lock);
}

#if 0
typedef uint8_t Spinlock;

#if defined(__x86_64__) || defined(__i386) || defined(_X86_)
#  define MN_SPINLOCK_BACKOFF __asm__ ("pause")
#else
#  define MN_SPINLOCK_BACKOFF
#endif

static inline void
spinlock_init(Spinlock* self)
{
	*self = 0;
}

static inline void
spinlock_free(Spinlock* self)
{
	*self = 0;
}

static inline void
spinlock_lock(Spinlock* self)
{
	if (__sync_lock_test_and_set(self, 1) != 0) {
		unsigned int spcount = 0U;
		for (;;) {
			MN_SPINLOCK_BACKOFF;
			if (*self == 0U && __sync_lock_test_and_set(self, 1) == 0)
				break;
			if (++spcount > 100U)
				usleep(0);
		}
	}
}

static inline void
spinlock_unlock(Spinlock* self)
{
	__sync_lock_release(self);
}
#endif
