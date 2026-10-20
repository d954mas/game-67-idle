package admin

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"
)

const maxSnapshotBytes = 1 << 20

type metaClient struct {
	baseURL string
	token   string
	http    *http.Client
}

type metaRoom struct {
	ID          int64   `json:"id"`
	Incarnation int64   `json:"incarnation"`
	Host        string  `json:"host"`
	Port        int     `json:"port"`
	URL         string  `json:"url"`
	Free        int     `json:"free"`
	Seated      int     `json:"seated"`
	Away        int     `json:"away"`
	Capacity    int     `json:"capacity"`
	AgeS        float64 `json:"age_s"`
	Alive       bool    `json:"alive"`
	PassAvgUs   int     `json:"pass_avg_us"`
	PassMaxUs   int     `json:"pass_max_us"`
	RoundTripMs int     `json:"round_trip_ms"`
	// Against its machine's plan (roomplan.go): HasPlan says the machine
	// reports one, Planned that this room is in it, NeverReported that the
	// plan names it and the registry has never heard it.
	HasPlan       bool `json:"has_plan"`
	Planned       bool `json:"planned"`
	NeverReported bool `json:"never_reported,omitempty"`
}

type metaPlayers struct {
	Guests   int64 `json:"guests"`
	Accounts int64 `json:"accounts"`
	Sessions int64 `json:"sessions"`
	Profiles int64 `json:"profiles"`
}

type metaProcess struct {
	HeapBytes     int64 `json:"heap_bytes"`
	Goroutines    int   `json:"goroutines"`
	DBConnections int   `json:"db_connections"`
	DBAcquired    int   `json:"db_acquired"`
	DBMax         int   `json:"db_max"`
}

type metaDB struct {
	Queries   int          `json:"queries"`
	Errors    int          `json:"errors"`
	AvgMs     float64      `json:"avg_ms"`
	MaxMs     float64      `json:"max_ms"`
	LastError *metaDBError `json:"last_error"`
}

type metaDBError struct {
	At   time.Time `json:"at"`
	Text string    `json:"text"`
}

// The metaserver's 24-hour observations (observability.go): requests by
// route, the newest events and error groups. Details pass the metaserver's
// redaction of URLs, tokens and secrets; certificate and source errors may
// still name a host or an address.
type metaObservability struct {
	Endpoints map[string]metaEndpoint `json:"endpoints"`
	Events    []metaEvent             `json:"events"`
	Groups    []metaErrorGroup        `json:"groups"`
	History   struct {
		RetentionHours int       `json:"retention_hours"`
		Persistent     bool      `json:"persistent"`
		LastSaved      time.Time `json:"last_saved"`
		Error          string    `json:"error,omitempty"`
	} `json:"history"`
}

type metaEndpoint struct {
	Requests     int64   `json:"requests"`
	ClientErrors int64   `json:"client_errors"`
	ServerErrors int64   `json:"server_errors"`
	RateLimited  int64   `json:"rate_limited"`
	AvgMs        float64 `json:"avg_ms"`
	MaxMs        float64 `json:"max_ms"`
}

type metaEvent struct {
	At      time.Time `json:"at"`
	Level   string    `json:"level"`
	Message string    `json:"message"`
	Detail  string    `json:"detail,omitempty"`
}

type metaErrorGroup struct {
	Level   string    `json:"level"`
	Message string    `json:"message"`
	Detail  string    `json:"detail,omitempty"`
	Count   int64     `json:"count"`
	FirstAt time.Time `json:"first_at"`
	LastAt  time.Time `json:"last_at"`
}

// metaAlert holds once its condition lasted alertHold; firing ones page the operator.
type metaAlert struct {
	Kind   string    `json:"kind"`
	Text   string    `json:"text"`
	Since  time.Time `json:"since"`
	Firing bool      `json:"firing"`
}

type metaCertificate struct {
	Host        string    `json:"host"`
	Checked     time.Time `json:"checked"`
	Expires     time.Time `json:"expires"`
	Invalid     string    `json:"invalid,omitempty"`
	Unreachable string    `json:"unreachable,omitempty"`
}

type metaRoomsAPI struct {
	TicketsRefused  int64 `json:"tickets_refused"`
	Statuses        int64 `json:"statuses"`
	StatusesRefused int64 `json:"statuses_refused"`
	Loads           int64 `json:"loads"`
	LoadsRefused    int64 `json:"loads_refused"`
	LoadsFailed     int64 `json:"loads_failed"`
	Saves           int64 `json:"saves"`
	SavesRefused    int64 `json:"saves_refused"`
	SavesFailed     int64 `json:"saves_failed"`
}

type metaTrafficDay struct {
	Day  string  `json:"day"`
	RxGB float64 `json:"rx_gb"`
	TxGB float64 `json:"tx_gb"`
}

// metaTrafficHost is a room machine's interface counters by UTC day (metaserver traffic.go).
type metaTrafficHost struct {
	Host         string           `json:"host"`
	AgeS         float64          `json:"age_s"`
	TodayRxGB    float64          `json:"today_rx_gb"`
	TodayTxGB    float64          `json:"today_tx_gb"`
	MonthRxGB    float64          `json:"month_rx_gb"`
	MonthTxGB    float64          `json:"month_tx_gb"`
	CountedHours float64          `json:"counted_hours"`
	Days         []metaTrafficDay `json:"days"`
}

type metaSnapshot struct {
	Now            time.Time         `json:"now"`
	Since          time.Time         `json:"since"`
	Rooms          []metaRoom        `json:"rooms"`
	Players        *metaPlayers      `json:"players"`
	Process        *metaProcess      `json:"process"`
	DB             *metaDB           `json:"db"`
	RoomsAPI       *metaRoomsAPI     `json:"rooms_api"`
	RoomPlans      []roomPlanView    `json:"room_plans,omitempty"`
	RoomsRetired   int               `json:"rooms_retired"`
	Traffic        []metaTrafficHost `json:"traffic"`
	TrafficMonthGB float64           `json:"traffic_month_gb"`
	Error          string            `json:"error,omitempty"`
}

// metaStatusError is a refused internal view; a 404 is an answer, not an outage.
type metaStatusError struct {
	path string
	code int
}

func (e metaStatusError) Error() string {
	return fmt.Sprintf("metaserver %s returned %d", e.path, e.code)
}

// get reads one internal JSON view, refusing a body past limit bytes.
func (c metaClient) get(ctx context.Context, path string, limit int, out any) error {
	request, err := http.NewRequestWithContext(ctx, http.MethodGet,
		strings.TrimRight(c.baseURL, "/")+path, nil)
	if err != nil {
		return err
	}
	request.Header.Set("Authorization", "Bearer "+c.token)
	client := c.http
	if client == nil {
		client = &http.Client{Timeout: 2 * time.Second}
	}
	response, err := client.Do(request)
	if err != nil {
		return err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		return metaStatusError{path: path, code: response.StatusCode}
	}
	body, err := io.ReadAll(io.LimitReader(response.Body, int64(limit)+1))
	if err != nil {
		return err
	}
	if len(body) > limit {
		return errors.New("metaserver view exceeded size limit")
	}
	return json.Unmarshal(body, out)
}

func (c metaClient) snapshot(ctx context.Context) (metaSnapshot, error) {
	var out metaSnapshot
	if err := c.get(ctx, "/internal/admin/v1/snapshot", maxSnapshotBytes, &out); err != nil {
		return out, err
	}
	if out.Now.IsZero() {
		return out, errors.New("metaserver snapshot has no timestamp")
	}
	return out, nil
}

// The room summary takes under 400 bytes a room and the metaserver keeps at
// most 4096 rooms (roomperf.go perfRoomsMax).
const maxPerfBytes = 4 << 20

// perf is the fleet's last minute as the metaserver summarizes it; the
// console reads its fields, so the admin service passes it through.
func (c metaClient) perf(ctx context.Context) (json.RawMessage, error) {
	var out json.RawMessage
	err := c.get(ctx, "/internal/admin/v1/perf", maxPerfBytes, &out)
	return out, err
}

func (c metaClient) perfRoom(ctx context.Context, id int64) (json.RawMessage, error) {
	var out json.RawMessage
	err := c.get(ctx, "/internal/admin/v1/perf/rooms/"+strconv.FormatInt(id, 10), maxSnapshotBytes, &out)
	return out, err
}

// metaIncidents is the bounded incidents view: the newest events and short details.
type metaIncidents struct {
	Now           time.Time          `json:"now"`
	Since         time.Time          `json:"since"`
	Observability *metaObservability `json:"observability"`
	Alerts        []metaAlert        `json:"alerts"`
	Certificates  []metaCertificate  `json:"certificates"`
	DB            *metaDB            `json:"db"`
	RoomsAPI      *metaRoomsAPI      `json:"rooms_api"`
}

func (c metaClient) incidents(ctx context.Context) (metaIncidents, error) {
	var out metaIncidents
	if err := c.get(ctx, "/internal/admin/v1/incidents", maxSnapshotBytes, &out); err != nil {
		return out, err
	}
	if out.Now.IsZero() {
		return out, errors.New("metaserver incidents have no timestamp")
	}
	return out, nil
}
