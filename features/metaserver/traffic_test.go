package metaserver

import (
	"path/filepath"
	"testing"
	"time"
)

// Rooms of one machine report the same cumulative counters at their own
// moments: growth is counted once, a late reading is not new traffic, and a
// reboot starts the counters over without a negative day.
func TestTrafficCountsAMachineOnceByDay(t *testing.T) {
	l := newTrafficLedger()
	day1 := time.Date(2026, 9, 10, 23, 0, 0, 0, time.UTC)
	l.note(day1, "b", 1000*gb, 3000*gb) // the first reading only sets the base
	l.note(day1.Add(time.Minute), "b", 1001*gb, 3002*gb)
	l.note(day1.Add(time.Minute+time.Second), "b", 1000.5*gb, 3001*gb) // another room, read earlier
	l.note(day1.Add(2*time.Hour), "b", 1003*gb, 3006*gb)               // past midnight
	reboot := day1.Add(3 * time.Hour)
	l.note(reboot, "b", 1*gb, 2*gb)
	l.note(reboot.Add(time.Hour), "b", 2*gb, 5*gb)
	snap := l.snapshot(reboot.Add(time.Hour))
	if len(snap) != 1 {
		t.Fatalf("hosts %+v", snap)
	}
	v := snap[0]
	if len(v.Days) != 2 || v.Days[0].TxGB != 2 || v.Days[0].RxGB != 1 || v.Days[1].TxGB != 7 || v.Days[1].RxGB != 3 {
		t.Fatalf("days %+v", v.Days)
	}
	if v.TodayTx != 7 || v.MonthTx != 9 || v.MonthRx != 4 {
		t.Fatalf("today %v month %v/%v", v.TodayTx, v.MonthTx, v.MonthRx)
	}
	// Counting began on the 10th at 23:00: 9 GB in its first 4 hours, 481 hours to the month's end.
	if v.ProjectedTx < 1082 || v.ProjectedTx > 1083 {
		t.Fatalf("projected %v", v.ProjectedTx)
	}
	// Last month's days leave the month's total.
	october := l.snapshot(time.Date(2026, 10, 1, 12, 0, 0, 0, time.UTC))
	if october[0].MonthTx != 0 || len(october[0].Days) != 0 {
		t.Fatalf("october %+v", october[0])
	}
}

func TestTrafficSurvivesTheHistoryCheckpoint(t *testing.T) {
	path := filepath.Join(t.TempDir(), "history.json")
	l := newTrafficLedger()
	at := time.Date(2026, 9, 12, 10, 0, 0, 0, time.UTC)
	l.note(at, "b", 10*gb, 20*gb)
	l.note(at.Add(time.Hour), "b", 11*gb, 25*gb)
	if err := newObservationArchive(path, newObservations(), &dbHealth{}, l).save(); err != nil {
		t.Fatal(err)
	}
	restored := newTrafficLedger()
	if err := newObservationArchive(path, newObservations(), &dbHealth{}, restored).load(); err != nil {
		t.Fatal(err)
	}
	// The restored base counts on: no first-reading gap after a restart.
	restored.note(at.Add(2*time.Hour), "b", 12*gb, 26*gb)
	if v := restored.snapshot(at.Add(2 * time.Hour))[0]; v.MonthTx != 6 || v.MonthRx != 2 {
		t.Fatalf("restored %+v", v)
	}
}
