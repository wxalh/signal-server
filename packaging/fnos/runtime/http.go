package main

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"embed"
	"encoding/base64"
	"errors"
	"html/template"
	"log"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"sync"
)

const settingsPath = "/app/signal-server/"

//go:embed settings.html settings.css
var assets embed.FS

var page = template.Must(template.ParseFS(assets, "settings.html"))

type manager struct {
	mu       sync.Mutex
	current  settings
	services services
	csrfKey  [32]byte
}

func newManager(s settings, appDir, stateDir string) (*manager, error) {
	m := &manager{
		current: s,
		services: services{
			appDir: appDir, stateDir: stateDir,
			signal: process{name: "信令服务"},
			turn:   process{name: "coturn"},
		},
	}
	_, err := rand.Read(m.csrfKey[:])
	return m, err
}

func adminID(r *http.Request) string {
	admin, uid := r.Header.Values("X-Trim-Isadmin"), r.Header.Values("X-Trim-Userid")
	if len(admin) != 1 || admin[0] != "true" || len(uid) != 1 || uid[0] == "" {
		return ""
	}
	for _, c := range uid[0] {
		if c < '0' || c > '9' {
			return ""
		}
	}
	id, err := strconv.ParseUint(uid[0], 10, 64)
	if err != nil || id == 0 {
		return ""
	}
	return strconv.FormatUint(id, 10)
}

func (m *manager) token(id string) string {
	mac := hmac.New(sha256.New, m.csrfKey[:])
	_, _ = mac.Write([]byte(id))
	return base64.RawURLEncoding.EncodeToString(mac.Sum(nil))
}

type view struct {
	Settings      settings
	CSRF          string
	SignalStatus  string
	TurnStatus    string
	SignalRunning bool
	TurnRunning   bool
	Notice        string
	Error         string
}

func (m *manager) show(w http.ResponseWriter, token string, status int, notice string) {
	data := view{
		Settings: m.current, CSRF: token,
		SignalStatus: m.services.signal.status(), TurnStatus: m.services.turn.status(),
		SignalRunning: m.services.signal.running(), TurnRunning: m.services.turn.running(),
	}
	if status == http.StatusOK {
		data.Notice = notice
	} else {
		data.Error = notice
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.WriteHeader(status)
	if err := page.ExecuteTemplate(w, "settings.html", data); err != nil {
		log.Printf("render settings: %v", err)
	}
}

func fromForm(form url.Values, current settings) (settings, error) {
	names := []string{"signal_port", "turn_port", "relay_min", "relay_max", "realm", "external_ip", "turn_user", "turn_password"}
	for _, name := range names {
		if len(form[name]) != 1 {
			return settings{}, errors.New("表单字段缺失或重复")
		}
	}
	next := current
	for i, target := range []*int{&next.SignalPort, &next.TurnPort, &next.RelayMin, &next.RelayMax} {
		port, err := strconv.Atoi(form.Get(names[i]))
		if err != nil {
			return settings{}, errors.New("请填写有效的端口号")
		}
		*target = port
	}
	next.Realm = strings.TrimSpace(form.Get("realm"))
	next.ExternalIP = strings.TrimSpace(form.Get("external_ip"))
	next.TurnUser = strings.TrimSpace(form.Get("turn_user"))
	if pass := form.Get("turn_password"); pass != "" {
		next.TurnPassword = pass
	}
	return next, next.validate()
}

func (m *manager) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.Header().Set("Referrer-Policy", "no-referrer")
	w.Header().Set("Content-Security-Policy", "default-src 'none'; style-src 'self'; form-action 'self'; frame-ancestors 'self'; base-uri 'none'")
	if r.URL.Path == "/_health" && r.Method == http.MethodGet {
		_, _ = w.Write([]byte("ok\n"))
		return
	}
	id := adminID(r)
	if id == "" {
		http.Error(w, "仅飞牛管理员可访问此设置", http.StatusForbidden)
		return
	}
	if r.URL.Path == strings.TrimSuffix(settingsPath, "/") && r.Method == http.MethodGet {
		http.Redirect(w, r, settingsPath, http.StatusPermanentRedirect)
		return
	}
	if r.URL.Path == settingsPath+"settings.css" && r.Method == http.MethodGet {
		css, _ := assets.ReadFile("settings.css")
		w.Header().Set("Content-Type", "text/css; charset=utf-8")
		_, _ = w.Write(css)
		return
	}
	if r.URL.Path != settingsPath {
		http.NotFound(w, r)
		return
	}
	token := m.token(id)
	switch r.Method {
	case http.MethodGet:
		m.mu.Lock()
		defer m.mu.Unlock()
		message := ""
		if r.URL.Query().Get("saved") == "1" {
			message = "配置已保存并生效"
		}
		m.show(w, token, http.StatusOK, message)
	case http.MethodPost:
		r.Body = http.MaxBytesReader(w, r.Body, 16<<10)
		if err := r.ParseForm(); err != nil || len(r.PostForm["action"]) != 1 || r.PostForm.Get("action") != "save" {
			http.Error(w, "设置请求格式无效", http.StatusBadRequest)
			return
		}
		if len(r.PostForm["csrf"]) != 1 || !hmac.Equal([]byte(r.PostForm.Get("csrf")), []byte(token)) {
			http.Error(w, "设置页面已过期，请刷新后重试", http.StatusForbidden)
			return
		}
		m.mu.Lock()
		defer m.mu.Unlock()
		next, err := fromForm(r.PostForm, m.current)
		if err != nil {
			m.show(w, token, http.StatusBadRequest, err.Error())
			return
		}
		if err := m.services.apply(m.current, next); err != nil {
			m.show(w, token, http.StatusConflict, err.Error())
			return
		}
		m.current = next
		http.Redirect(w, r, settingsPath+"?saved=1", http.StatusSeeOther)
	default:
		w.Header().Set("Allow", "GET, POST")
		http.Error(w, "不支持此请求方法", http.StatusMethodNotAllowed)
	}
}
