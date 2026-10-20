package metaserver

import (
	"sync"
	"time"

	"golang.org/x/time/rate"
)

// limiter holds one token bucket per key (a client address, a login): a
// burst of the full allowance, then a steady trickle. When the map fills,
// a few of the buckets nobody is drawing on are dropped per new key; if
// none can be, the call is refused, so a spray of fresh keys cannot wash
// out a lockout and the map can never grow with every key ever seen.
type limiter struct {
	mu      sync.Mutex
	burst   int
	every   time.Duration
	buckets map[string]*rate.Limiter
	// A full map refuses new keys, unless the keys are something anyone
	// can mint (a session bearer): then a spray of fakes must not lock the
	// honest out, and a new key passes while the map is full.
	lenient bool
}

const maxBuckets = 10000

func newLimiter(burst int, every time.Duration) *limiter {
	return &limiter{burst: burst, every: every, buckets: map[string]*rate.Limiter{}}
}

func newLenientLimiter(burst int, every time.Duration) *limiter {
	l := newLimiter(burst, every)
	l.lenient = true
	return l
}

// A full map sheds at most this many idle buckets per new key: bounded
// work under the lock, and a spray of fresh keys pays for its own room.
const evictionScan = 64

func (l *limiter) allow(key string, now time.Time) bool {
	l.mu.Lock()
	defer l.mu.Unlock()
	b, ok := l.buckets[key]
	if !ok && len(l.buckets) >= maxBuckets {
		scanned := 0
		for k, full := range l.buckets {
			if full.TokensAt(now) >= float64(l.burst) {
				delete(l.buckets, k)
			}
			if scanned++; scanned == evictionScan {
				break
			}
		}
		if len(l.buckets) >= maxBuckets {
			return l.lenient
		}
	}
	if !ok {
		b = rate.NewLimiter(rate.Every(l.every), l.burst)
		l.buckets[key] = b
	}
	return b.AllowN(now, 1)
}

// forget refills a key: a password that matched is not an attempt.
func (l *limiter) forget(key string) {
	l.mu.Lock()
	delete(l.buckets, key)
	l.mu.Unlock()
}
