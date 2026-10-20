package admin

import (
	"bufio"
	"bytes"
	"errors"
	"strconv"
	"strings"
	"time"
)

// A saved profile is an NTGS text document (tools/schema/gen_profile.mjs):
// a header line, document fields, then one [system version] section per
// game system, each holding key=value lines. The console shows the sections
// as saved: it knows no game's fields, and a field a game adds shows up
// without a console change.

const profileDocMax = 1 << 20

type profileField struct {
	Key   string `json:"key"`
	Value string `json:"value"`
}

type profileSystem struct {
	Name    string         `json:"name"`
	Version int            `json:"version"`
	Fields  []profileField `json:"fields"`
}

type playerProfile struct {
	Rev     int64           `json:"rev"`
	SavedAt time.Time       `json:"saved_at"`
	Format  string          `json:"format"`
	Systems []profileSystem `json:"systems"`
}

var errProfileFormat = errors.New("profile is not an NTGS document")

func parseProfile(doc []byte) (string, []profileSystem, error) {
	if len(doc) > profileDocMax {
		return "", nil, errProfileFormat
	}
	scanner := bufio.NewScanner(bytes.NewReader(doc))
	scanner.Buffer(make([]byte, 0, 4096), profileDocMax)
	if !scanner.Scan() || !strings.HasPrefix(scanner.Text(), "NTGS ") {
		return "", nil, errProfileFormat
	}
	format := strings.TrimSpace(scanner.Text())
	systems := []profileSystem{}
	var current *profileSystem
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}
		if strings.HasPrefix(line, "[") && strings.HasSuffix(line, "]") {
			name, version, ok := strings.Cut(strings.TrimSpace(line[1:len(line)-1]), " ")
			parsed, err := strconv.Atoi(strings.TrimSpace(version))
			if !ok || err != nil || name == "" {
				return "", nil, errProfileFormat
			}
			systems = append(systems, profileSystem{Name: name, Version: parsed, Fields: []profileField{}})
			current = &systems[len(systems)-1]
			continue
		}
		key, value, ok := strings.Cut(line, "=")
		if !ok || key == "" {
			return "", nil, errProfileFormat
		}
		// Document fields before the first section (save id, rev) are bookkeeping.
		if current != nil {
			current.Fields = append(current.Fields, profileField{Key: key, Value: value})
		}
	}
	if err := scanner.Err(); err != nil {
		return "", nil, errProfileFormat
	}
	return format, systems, nil
}
