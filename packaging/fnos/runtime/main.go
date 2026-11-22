package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"syscall"
	"time"
)

type gatewayListener struct{ *net.UnixListener }

func (l gatewayListener) Accept() (net.Conn, error) {
	for {
		conn, err := l.AcceptUnix()
		if err != nil {
			return nil, err
		}
		if trustedPeer(conn) {
			return conn, nil
		}
		_ = conn.Close()
	}
}

func trustedPeer(conn *net.UnixConn) bool {
	raw, err := conn.SyscallConn()
	if err != nil {
		return false
	}
	var peer *syscall.Ucred
	var peerErr error
	err = raw.Control(func(fd uintptr) {
		peer, peerErr = syscall.GetsockoptUcred(int(fd), syscall.SOL_SOCKET, syscall.SO_PEERCRED)
	})
	// fnOS's trim_open_gateway service runs as root. The application can also probe its own socket.
	return err == nil && peerErr == nil && peer != nil &&
		(peer.Uid == 0 || peer.Uid == uint32(os.Geteuid()))
}

func openGateway(path string) (gatewayListener, error) {
	fi, err := os.Lstat(path)
	if err == nil {
		stat, owned := fi.Sys().(*syscall.Stat_t)
		if fi.Mode()&os.ModeSocket == 0 || !owned || stat.Uid != uint32(os.Geteuid()) {
			return gatewayListener{}, errors.New("socket path exists and is not an application-owned socket")
		}
		conn, err := net.DialTimeout("unix", path, 300*time.Millisecond)
		if err == nil {
			_ = conn.Close()
			return gatewayListener{}, errors.New("socket path is already in use")
		}
		if !errors.Is(err, syscall.ECONNREFUSED) {
			return gatewayListener{}, err
		}
		if err := os.Remove(path); err != nil {
			return gatewayListener{}, err
		}
	} else if !errors.Is(err, os.ErrNotExist) {
		return gatewayListener{}, err
	}
	listener, err := net.ListenUnix("unix", &net.UnixAddr{Name: path, Net: "unix"})
	if err != nil {
		return gatewayListener{}, err
	}
	if err := os.Chmod(path, 0600); err != nil {
		_ = listener.Close()
		return gatewayListener{}, err
	}
	return gatewayListener{listener}, nil
}

func run(appDir, stateDir, socket string) error {
	for _, p := range []string{appDir, stateDir, socket} {
		if !filepath.IsAbs(p) {
			return errors.New("--app-dir, --state-dir and --socket require absolute paths")
		}
	}
	syscall.Umask(0077)
	for _, p := range []string{stateDir, filepath.Join(stateDir, "signal"), filepath.Join(stateDir, "turn")} {
		if err := os.MkdirAll(p, 0700); err != nil {
			return err
		}
	}
	lock, err := os.OpenFile(filepath.Join(stateDir, "manager.lock"), os.O_CREATE|os.O_RDWR, 0600)
	if err != nil {
		return err
	}
	defer lock.Close()
	if err := syscall.Flock(int(lock.Fd()), syscall.LOCK_EX|syscall.LOCK_NB); err != nil {
		return fmt.Errorf("lock manager state: %w", err)
	}
	s, err := readSettings(stateDir)
	if err != nil {
		return err
	}
	m, err := newManager(s, appDir, stateDir)
	if err != nil {
		return err
	}
	ln, err := openGateway(socket)
	if err != nil {
		return err
	}
	defer ln.Close()

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, os.Interrupt)
	defer stop()
	_ = m.services.startSignal(s)
	_ = m.services.startTurn(s)
	defer func() {
		m.mu.Lock()
		defer m.mu.Unlock()
		m.services.signal.stop()
		m.services.turn.stop()
	}()
	server := &http.Server{
		Handler:           m,
		ReadHeaderTimeout: 4 * time.Second,
		ReadTimeout:       6 * time.Second,
		WriteTimeout:      24 * time.Second,
		IdleTimeout:       20 * time.Second,
		MaxHeaderBytes:    16384,
	}
	serve := make(chan error, 1)
	go func() { serve <- server.Serve(ln) }()
	select {
	case err := <-serve:
		_ = server.Close()
		return err
	case <-ctx.Done():
		// Finish an in-flight configuration transaction before stopping the children.
		deadline, cancel := context.WithTimeout(context.Background(), 16*time.Second)
		defer cancel()
		if err := server.Shutdown(deadline); err != nil {
			_ = server.Close()
		}
		return nil
	}
}

func main() {
	appDir := flag.String("app-dir", "", "absolute path of the unpacked fnOS application")
	stateDir := flag.String("state-dir", "", "absolute path of the persistent configuration directory")
	socket := flag.String("socket", "", "absolute path of the fnOS gateway Unix socket")
	flag.Parse()
	if err := run(*appDir, *stateDir, *socket); err != nil {
		log.Fatal(err)
	}
}
