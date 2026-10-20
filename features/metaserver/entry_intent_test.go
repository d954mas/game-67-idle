package metaserver

import (
	"net/http/httptest"
	"strings"
	"testing"
)

func TestReadPageEntryIntent(t *testing.T) {
	cases := []struct {
		name    string
		body    string
		want    bool
		invalid bool
	}{
		{"empty legacy body", "", false, false},
		{"ordinary reconnect", `{}`, false, false},
		{"page return", `{"page_entry":true}`, true, false},
		{"explicit false", `{"page_entry":false}`, false, false},
		{"null body", `null`, false, true},
		{"null flag", `{"page_entry":null}`, false, true},
		{"duplicate flag", `{"page_entry":false,"page_entry":true}`, false, true},
		{"case alias", `{"Page_Entry":true}`, false, true},
		{"numeric flag", `{"page_entry":1}`, false, true},
		{"array body", `[]`, false, true},
		{"unknown field", `{"page_entry":true,"x":1}`, false, true},
		{"trailing value", `{"page_entry":true} {}`, false, true},
		{"malformed", `{"page_entry":`, false, true},
		{"oversized", `{"page_entry":true,"padding":"` + strings.Repeat("x", 150) + `"}`, false, true},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			r := httptest.NewRequest("POST", "/api/enter", strings.NewReader(tc.body))
			got, err := readPageEntryIntent(httptest.NewRecorder(), r)
			if (err != nil) != tc.invalid || (!tc.invalid && got != tc.want) {
				t.Fatalf("intent=%v err=%v, want=%v invalid=%v", got, err, tc.want, tc.invalid)
			}
		})
	}
}
