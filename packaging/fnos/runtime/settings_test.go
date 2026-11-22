package main

import (
	"net/http/httptest"
	"strings"
	"testing"
)

func TestRejectsUnsafeCoturnConfig(t *testing.T) {
	base, err := newSettings()
	if err != nil {
		t.Fatal(err)
	}
	for name, change := range map[string]func(*settings){
		"directive injection":         func(s *settings) { s.Realm = "example.com\nno-auth" },
		"password injection":          func(s *settings) { s.TurnPassword = "abc#123456" },
		"malformed public IP mapping": func(s *settings) { s.ExternalIP = "192.0.2.5/example.com" },
		"duplicate listeners":         func(s *settings) { s.TurnPort = s.SignalPort },
		"relay overlap":               func(s *settings) { s.TurnPort = s.RelayMin },
		"privileged port":             func(s *settings) { s.SignalPort = 80 },
	} {
		t.Run(name, func(t *testing.T) {
			candidate := base
			change(&candidate)
			if err := candidate.validate(); err == nil {
				t.Fatal("accepted invalid settings")
			}
		})
	}
	base.ExternalIP = "192.0.2.5/192.168.1.5"
	if err := base.validate(); err != nil {
		t.Fatal(err)
	}
	conf := base.turnConfig()
	for _, directive := range []string{"\nlt-cred-mech\n", "\nno-tls\n", "\nexternal-ip=192.0.2.5/192.168.1.5\n"} {
		if !strings.Contains(conf, directive) {
			t.Fatalf("missing directive %q", directive)
		}
	}
}

func TestManagerRequiresUnambiguousAdministratorHeaders(t *testing.T) {
	r := httptest.NewRequest("GET", settingsPath, nil)
	r.Header.Set("X-Trim-Userid", "1001")
	if adminID(r) != "" {
		t.Fatal("accepted request with no administrator header")
	}
	r.Header.Set("X-Trim-Isadmin", "true")
	if adminID(r) != "1001" {
		t.Fatal("rejected valid administrator")
	}
	r.Header.Add("X-Trim-Isadmin", "false")
	if adminID(r) != "" {
		t.Fatal("accepted ambiguous administrator headers")
	}
}
