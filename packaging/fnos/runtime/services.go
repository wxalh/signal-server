package main

import (
	"bufio"
	"bytes"
	"context"
	"crypto/rand"
	"encoding/binary"
	"errors"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"syscall"
	"time"
)

type process struct {
	name    string
	cmd     *exec.Cmd
	done    chan struct{}
	failure string
}

func (p *process) running() bool {
	if p.done == nil {
		return false
	}
	select {
	case <-p.done:
		return false
	default:
		return true
	}
}

func (p *process) status() string {
	if p.running() {
		return "运行中"
	}
	if p.failure != "" {
		return p.failure
	}
	return "未运行"
}

func (p *process) stop() {
	if !p.running() {
		return
	}
	_ = p.cmd.Process.Signal(syscall.SIGTERM)
	select {
	case <-p.done:
	case <-time.After(1800 * time.Millisecond):
		_ = p.cmd.Process.Kill()
		<-p.done
	}
}

func (p *process) start(cmd *exec.Cmd, port int, turn bool, probe func(int) bool) (err error) {
	p.failure = ""
	defer func() {
		if err != nil {
			p.failure = err.Error()
			log.Printf("%s: %v", p.name, err)
		}
	}()
	if err := checkPort(port, turn); err != nil {
		return fmt.Errorf("%s 的端口 %d 无法监听（可能已被占用）", p.name, port)
	}
	cmd.Stdout, cmd.Stderr = os.Stdout, os.Stderr
	cmd.SysProcAttr = &syscall.SysProcAttr{Pdeathsig: syscall.SIGKILL}
	if err := cmd.Start(); err != nil {
		log.Printf("start %s: %v", p.name, err)
		return fmt.Errorf("无法启动 %s，请查看应用日志", p.name)
	}
	p.cmd, p.done = cmd, make(chan struct{})
	go func(done chan struct{}) {
		err := cmd.Wait()
		close(done)
		if err != nil {
			log.Printf("%s exited: %v", p.name, err)
		}
	}(p.done)

	deadline := time.Now().Add(3500 * time.Millisecond)
	for {
		if !p.running() {
			return fmt.Errorf("%s 启动后退出，请查看应用日志", p.name)
		}
		if probe(port) && p.running() {
			return nil
		}
		if time.Now().After(deadline) {
			p.stop()
			return fmt.Errorf("%s 未在端口 %d 上就绪，请查看应用日志", p.name, port)
		}
		select {
		case <-p.done:
		case <-time.After(120 * time.Millisecond):
		}
	}
}

func checkPort(port int, udp bool) error {
	address := ":" + strconv.Itoa(port)
	// Match the service listener's reuse behavior; TIME_WAIT is normal after restart.
	var cfg net.ListenConfig
	cfg.Control = func(_, _ string, c syscall.RawConn) error {
		var err error
		if ctrlErr := c.Control(func(fd uintptr) {
			err = syscall.SetsockoptInt(int(fd), syscall.SOL_SOCKET, syscall.SO_REUSEADDR, 1)
		}); ctrlErr != nil {
			return ctrlErr
		}
		return err
	}
	network := "tcp"
	if udp {
		network = "tcp4"
	}
	ln, err := cfg.Listen(context.Background(), network, address)
	if err != nil {
		return err
	}
	defer ln.Close()
	if udp {
		packet, err := net.ListenPacket("udp4", address)
		if err != nil {
			return err
		}
		_ = packet.Close()
	}
	return nil
}

func dialLocal(network string, port int) (net.Conn, error) {
	return net.DialTimeout(network, net.JoinHostPort("127.0.0.1", strconv.Itoa(port)), 250*time.Millisecond)
}

func probeSignal(port int) bool {
	conn, err := dialLocal("tcp", port)
	if err != nil {
		return false
	}
	defer conn.Close()
	_ = conn.SetDeadline(time.Now().Add(300 * time.Millisecond))
	_, err = fmt.Fprint(conn, "GET / HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"+
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n")
	if err != nil {
		return false
	}
	res, err := http.ReadResponse(bufio.NewReader(conn), nil)
	if err != nil {
		return false
	}
	if res.StatusCode != http.StatusSwitchingProtocols {
		return false
	}
	return res.Header.Get("Sec-WebSocket-Accept") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
}

func probeTurn(port int) bool {
	tcp, err := dialLocal("tcp", port)
	if err != nil {
		return false
	}
	_ = tcp.Close()
	conn, err := dialLocal("udp", port)
	if err != nil {
		return false
	}
	defer conn.Close()
	_ = conn.SetDeadline(time.Now().Add(300 * time.Millisecond))
	request := make([]byte, 20)
	binary.BigEndian.PutUint16(request, 0x0001) // STUN Binding Request
	binary.BigEndian.PutUint32(request[4:], 0x2112a442)
	if _, err := rand.Read(request[8:]); err != nil {
		return false
	}
	if _, err := conn.Write(request); err != nil {
		return false
	}
	result := make([]byte, 512)
	n, err := conn.Read(result)
	return err == nil && n >= 20 &&
		binary.BigEndian.Uint16(result) == 0x0101 &&
		n == 20+int(binary.BigEndian.Uint16(result[2:])) && bytes.Equal(result[4:20], request[4:20])
}

type services struct {
	appDir   string
	stateDir string
	signal   process
	turn     process
}

func (sv *services) startSignal(s settings) error {
	dir := filepath.Join(sv.stateDir, "signal")
	if err := writePrivate(filepath.Join(dir, "config.ini"), []byte(s.signalConfig())); err != nil {
		sv.signal.failure = "无法写入信令配置"
		log.Printf("write signal configuration: %v", err)
		return errors.New(sv.signal.failure)
	}
	cmd := exec.Command(filepath.Join(sv.appDir, "bin", "signal_server"))
	cmd.Dir = dir
	cmd.Env = append(os.Environ(), "SIGNAL_SERVER_DATA_DIR="+dir)
	return sv.signal.start(cmd, s.SignalPort, false, probeSignal)
}

func (sv *services) startTurn(s settings) error {
	dir := filepath.Join(sv.stateDir, "turn")
	configPath := filepath.Join(dir, "turnserver.conf")
	if err := writePrivate(configPath, []byte(s.turnConfig())); err != nil {
		sv.turn.failure = "无法写入 TURN 配置"
		log.Printf("write TURN configuration: %v", err)
		return errors.New(sv.turn.failure)
	}
	cmd := exec.Command(filepath.Join(sv.appDir, "bin", "turnserver"), "-c", configPath,
		"--pidfile", filepath.Join(dir, "turnserver.pid"))
	cmd.Dir = dir
	return sv.turn.start(cmd, s.TurnPort, true, probeTurn)
}

func (sv *services) apply(old, next settings) error {
	sigChanged := old.SignalPort != next.SignalPort || !sv.signal.running()
	turnChanged := old.turnConfig() != next.turnConfig() || !sv.turn.running()
	if sigChanged {
		sv.signal.stop()
	}
	if turnChanged {
		sv.turn.stop()
	}

	var err error
	if sigChanged {
		err = sv.startSignal(next)
	}
	if err == nil && turnChanged {
		err = sv.startTurn(next)
	}
	if err == nil {
		err = persist(sv.stateDir, next)
		if err != nil {
			log.Printf("save settings: %v", err)
			err = errors.New("无法保存设置")
		}
	}
	if err == nil {
		return nil
	}

	// Stop both candidates before restoring. Their ports may have been swapped.
	if sigChanged {
		sv.signal.stop()
	}
	if turnChanged {
		sv.turn.stop()
	}
	failedToRestore := false
	if sigChanged && sv.startSignal(old) != nil {
		failedToRestore = true
	}
	if turnChanged && sv.startTurn(old) != nil {
		failedToRestore = true
	}
	if failedToRestore {
		return fmt.Errorf("%w；设置未更改，请查看服务状态", err)
	}
	return fmt.Errorf("%w；已恢复原配置", err)
}
