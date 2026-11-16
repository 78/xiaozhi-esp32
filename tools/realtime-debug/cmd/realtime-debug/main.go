package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"strconv"
	"sync"
	"syscall"
	"time"

	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/proxy"
	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/store"
	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/trace"
	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/web"
)

func defaultDataDir() string {
	base, err := os.UserCacheDir()
	if err != nil {
		return "xiaozhi-realtime-debug-data"
	}
	return filepath.Join(base, "xiaozhi-realtime-debug")
}

func main() {
	collectorAddr := flag.String("collector-listen", "127.0.0.1:7319", "TCP address for the device")
	webAddr := flag.String("web-listen", "127.0.0.1:7320", "HTTP address for the browser")
	proxyAddr := flag.String("proxy-listen", "0.0.0.0:7321", "WebSocket address for the device proxy")
	upstreamURL := flag.String("upstream-url", "", "Aliyun Realtime WebSocket URL")
	dataDir := flag.String("data-dir", defaultDataDir(), "recording directory")
	maxMiB := flag.Int64("max-session-mib", 256, "maximum payload MiB per session")
	maxTotalMiB := flag.Int64("max-total-mib", 1024, "total recording limit; stop recording at the limit")
	retentionDays := flag.Int("retention-days", 0, "opt-in deletion of ended recordings older than this many days; 0 keeps existing recordings")
	flag.Parse()
	if *maxTotalMiB < 1 || *maxTotalMiB > 1048576 || *retentionDays < 0 || *retentionDays > 36500 {
		log.Fatal("invalid total size or retention")
	}
	if *maxMiB < 1 || *maxMiB > 4096 {
		log.Fatal("max-session-mib must be between 1 and 4096")
	}
	data, err := store.Open(*dataDir, *maxMiB*1024*1024)
	if err != nil {
		log.Fatal(err)
	}
	data.SetTotalLimit(*maxTotalMiB * 1024 * 1024)
	if *retentionDays > 0 {
		if err := data.Prune(time.Now().Add(-time.Duration(*retentionDays) * 24 * time.Hour)); err != nil {
			log.Fatal(err)
		}
	}
	collector, err := net.Listen("tcp", *collectorAddr)
	if err != nil {
		log.Fatal(err)
	}
	webListener, err := net.Listen("tcp", *webAddr)
	if err != nil {
		collector.Close()
		log.Fatal(err)
	}
	var proxyService *proxy.Server
	var proxyListener net.Listener
	var proxyServer *http.Server
	if *upstreamURL != "" {
		proxyService, err = proxy.New(proxy.Config{UpstreamURL: *upstreamURL, Store: data})
		if err != nil {
			collector.Close()
			webListener.Close()
			log.Fatal(err)
		}
		proxyListener, err = net.Listen("tcp", *proxyAddr)
		if err != nil {
			collector.Close()
			webListener.Close()
			log.Fatal(err)
		}
		proxyServer = &http.Server{Handler: proxyService.Handler(), ReadHeaderTimeout: 10 * time.Second}
	}
	server := &http.Server{Handler: web.New(data), ReadHeaderTimeout: 5 * time.Second}
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	var currentMu sync.Mutex
	var current net.Conn
	var workers sync.WaitGroup
	go func() {
		if err := server.Serve(webListener); err != nil && !errors.Is(err, http.ErrServerClosed) {
			log.Printf("web server: %v", err)
			stop()
		}
	}()
	if proxyServer != nil {
		go func() {
			if err := proxyServer.Serve(proxyListener); err != nil && !errors.Is(err, http.ErrServerClosed) {
				log.Printf("proxy server: %v", err)
				stop()
			}
		}()
	}
	go func() {
		for {
			conn, err := collector.Accept()
			if err != nil {
				if ctx.Err() == nil {
					log.Printf("collector accept: %v", err)
					stop()
				}
				return
			}
			currentMu.Lock()
			if current != nil {
				currentMu.Unlock()
				conn.Close()
				continue
			}
			current = conn
			currentMu.Unlock()
			workers.Add(1)
			go func() {
				defer workers.Done()
				defer func() {
					conn.Close()
					currentMu.Lock()
					current = nil
					currentMu.Unlock()
				}()
				if err := receive(conn, data); err != nil && !errors.Is(err, io.EOF) {
					log.Printf("collector %s: %v", conn.RemoteAddr(), err)
				}
			}()
		}
	}()
	log.Printf("collector: tcp://%s", collector.Addr())
	log.Printf("web: http://%s", webListener.Addr())
	if proxyListener != nil {
		log.Printf("proxy: ws://%s/realtime -> %s", proxyListener.Addr(), *upstreamURL)
	}
	log.Printf("data: %s", *dataDir)
	<-ctx.Done()
	collector.Close()
	if proxyServer != nil {
		proxyServer.Close()
	}
	if proxyService != nil {
		proxyService.Close()
	}
	currentMu.Lock()
	if current != nil {
		current.Close()
	}
	currentMu.Unlock()
	shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	server.Shutdown(shutdownCtx)
	workers.Wait()
	if err := data.Close(); err != nil {
		log.Printf("close recordings: %v", err)
	}
}

func receive(conn net.Conn, data *store.Store) error {
	conn.SetReadDeadline(time.Now().Add(30 * time.Second))
	hello, err := trace.ReadFrame(conn)
	if err != nil {
		return err
	}
	if hello.Kind != trace.Hello {
		return errors.New("first frame must be hello")
	}
	bootID := hello.Meta.BootID
	log.Printf("device connected: %s (%s)", hello.Meta.DeviceID, conn.RemoteAddr())
	data.SetDevice(store.DeviceStatus{Connected: true, DeviceID: hello.Meta.DeviceID, BootID: bootID, Remote: conn.RemoteAddr().String(), Since: time.Now().UTC()})
	defer data.SetDevice(store.DeviceStatus{})
	defer data.EndBoot(bootID)
	var lastSeq uint64
	for {
		conn.SetReadDeadline(time.Now().Add(5 * time.Minute))
		frame, err := trace.ReadFrame(conn)
		if err != nil {
			return err
		}
		if frame.Kind == trace.Hello || frame.Meta.BootID != bootID {
			return fmt.Errorf("unexpected boot identity or repeated hello")
		}
		if frame.Meta.DeviceID == "" {
			frame.Meta.DeviceID = hello.Meta.DeviceID
		}
		seq, _ := strconv.ParseUint(frame.Meta.Seq, 10, 64)
		if seq <= lastSeq {
			return fmt.Errorf("out-of-order trace sequence %s", frame.Meta.Seq)
		}
		lastSeq = seq
		if _, err := data.Append(frame); err != nil {
			return err
		}
	}
}
