package main

import (
	"crypto/rand"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"net/netip"
	"os"
	"path/filepath"
	"regexp"
	"strings"
)

const settingsFile = "settings.json"

type settings struct {
	SignalPort   int    `json:"signal_port"`
	TurnPort     int    `json:"turn_port"`
	RelayMin     int    `json:"relay_min"`
	RelayMax     int    `json:"relay_max"`
	Realm        string `json:"realm"`
	ExternalIP   string `json:"external_ip"`
	TurnUser     string `json:"turn_user"`
	TurnPassword string `json:"turn_password"`
}

func newSettings() (settings, error) {
	random := make([]byte, 24)
	if _, err := rand.Read(random); err != nil {
		return settings{}, err
	}
	return settings{3480, 3478, 49160, 49259, "airan-turn", "", "webrtc",
		base64.RawURLEncoding.EncodeToString(random)}, nil
}

var (
	namePattern     = regexp.MustCompile("^[A-Za-z0-9]([A-Za-z0-9._-]{0,126}[A-Za-z0-9])?$")
	passwordPattern = regexp.MustCompile("^[A-Za-z0-9._~!@$%+_-]{12,128}$")
)

func (s settings) validate() error {
	validPort := func(n int) bool { return n >= 1024 && n <= 65535 }
	if !validPort(s.SignalPort) || !validPort(s.TurnPort) || !validPort(s.RelayMin) || !validPort(s.RelayMax) {
		return errors.New("端口号必须为 1024–65535")
	}
	if s.SignalPort == s.TurnPort {
		return errors.New("信令和 TURN 监听端口不能相同")
	}
	if s.RelayMin > s.RelayMax {
		return errors.New("中继端口范围起点不能大于终点")
	}
	if s.SignalPort >= s.RelayMin && s.SignalPort <= s.RelayMax ||
		s.TurnPort >= s.RelayMin && s.TurnPort <= s.RelayMax {
		return errors.New("中继端口范围不能包含信令或 TURN 监听端口")
	}
	if !namePattern.MatchString(s.Realm) || !namePattern.MatchString(s.TurnUser) {
		return errors.New("Realm 和用户名需以字母或数字开头及结尾，可包含点、下划线和短横线，最多 128 字符")
	}
	if !passwordPattern.MatchString(s.TurnPassword) {
		return errors.New("TURN 密码需为 12–128 位，可用字母、数字或 ._~!@$%+_-")
	}
	if s.ExternalIP != "" {
		parts := strings.Split(s.ExternalIP, "/")
		if len(parts) > 2 {
			return errors.New("外部地址须为 IPv4 或 外部IPv4/内网IPv4")
		}
		for _, value := range parts {
			ip, err := netip.ParseAddr(value)
			if err != nil || !ip.Is4() || ip.IsUnspecified() || ip.IsMulticast() || ip.IsLoopback() {
				return errors.New("外部地址须为 IPv4 或 外部IPv4/内网IPv4")
			}
		}
	}
	return nil
}

func (s settings) turnConfig() string {
	config := fmt.Sprintf("listening-port=%d\nlistening-ip=0.0.0.0\nmin-port=%d\nmax-port=%d\nrealm=%s\nlt-cred-mech\nuser=%s:%s\n",
		s.TurnPort, s.RelayMin, s.RelayMax, s.Realm, s.TurnUser, s.TurnPassword)
	if s.ExternalIP != "" {
		config += "external-ip=" + s.ExternalIP + "\n"
	}
	return config + "no-tls\nno-multicast-peers\nlog-file=stdout\nsimple-log\n"
}

func (s settings) signalConfig() string {
	return fmt.Sprintf("[local]\nlogLevel=info\n\n[signal_server]\nserverPort=%d\nserverName=Signal Server\n", s.SignalPort)
}

func persist(dir string, s settings) error {
	data, err := json.MarshalIndent(s, "", "  ")
	if err != nil {
		return err
	}
	return writePrivate(filepath.Join(dir, settingsFile), append(data, '\n'))
}

func readSettings(dir string) (settings, error) {
	data, err := os.ReadFile(filepath.Join(dir, settingsFile))
	if errors.Is(err, os.ErrNotExist) {
		s, err := newSettings()
		if err == nil {
			err = persist(dir, s)
		}
		return s, err
	}
	if err != nil {
		return settings{}, err
	}
	var s settings
	if err := json.Unmarshal(data, &s); err != nil {
		return s, errors.New("settings.json 无法解析")
	}
	return s, s.validate()
}

func writePrivate(path string, data []byte) error {
	f, err := os.CreateTemp(filepath.Dir(path), ".tmp-")
	if err != nil {
		return err
	}
	defer os.Remove(f.Name())
	if _, err = f.Write(data); err == nil {
		err = f.Sync()
	}
	if closeErr := f.Close(); err == nil {
		err = closeErr
	}
	if err != nil {
		return err
	}
	return os.Rename(f.Name(), path)
}
