package admin

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestAdminRequiresExplicitEnableAndHTTPSOrigin(t *testing.T) {
	t.Setenv("ADMIN_ENABLED", "")
	if _, err := loadAdminConfig(); err == nil {
		t.Fatal("admin service enabled by default")
	}
	t.Setenv("ADMIN_ENABLED", "1")
	t.Setenv("ADMIN_ORIGIN", "http://admin.example.test")
	if _, err := loadAdminConfig(); err == nil {
		t.Fatal("insecure admin origin accepted")
	}
}

func TestTOTPSettingDefaultsOnAndAllowsExplicitOff(t *testing.T) {
	t.Setenv("ADMIN_TOTP_REQUIRED", "")
	if required, err := parseTOTPRequired(); err != nil || !required {
		t.Fatalf("default: required=%v err=%v", required, err)
	}
	t.Setenv("ADMIN_TOTP_REQUIRED", "0")
	if required, err := parseTOTPRequired(); err != nil || required {
		t.Fatalf("disabled: required=%v err=%v", required, err)
	}
	t.Setenv("ADMIN_TOTP_REQUIRED", "off")
	if _, err := parseTOTPRequired(); err == nil {
		t.Fatal("ambiguous setting accepted")
	}
}

func TestAdminRefusesGameDatabaseRole(t *testing.T) {
	dir := t.TempDir()
	key := filepath.Join(dir, "totp.key")
	token := filepath.Join(dir, "meta.token")
	dbPassword := filepath.Join(dir, "admin-db.password")
	if err := os.WriteFile(key, []byte(strings.Repeat("1", 64)), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(token, []byte(strings.Repeat("t", 48)), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(dbPassword, []byte(strings.Repeat("p", 48)), 0600); err != nil {
		t.Fatal(err)
	}
	t.Setenv("ADMIN_ENABLED", "1")
	t.Setenv("ADMIN_ORIGIN", "https://admin.example.test")
	t.Setenv("ADMIN_DATABASE_URL", "postgres://metaserver@127.0.0.1:5432/metaserver")
	t.Setenv("ADMIN_DATABASE_PASSWORD_FILE", dbPassword)
	t.Setenv("ADMIN_TOTP_KEY_FILE", key)
	t.Setenv("ADMIN_META_TOKEN_FILE", token)
	if err := runAdmin(Brand{}); err == nil || !strings.Contains(err.Error(), "not meta_admin_service") {
		t.Fatalf("game database role accepted: %v", err)
	}
}
