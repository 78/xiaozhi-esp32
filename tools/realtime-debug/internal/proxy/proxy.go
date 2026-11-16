package proxy

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gorilla/websocket"
	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/store"
	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/trace"
)

const (
	maxMessageSize  = 8 * 1024 * 1024
	recordQueueSize = 256
)

type Config struct {
	UpstreamURL string
	Store       *store.Store
}

type Server struct {
	upstreamURL string
	store       *store.Store
	upgrader    websocket.Upgrader
	mu          sync.Mutex
	connections map[*websocket.Conn]*websocket.Conn
	wg          sync.WaitGroup
	closed      bool
}

func New(config Config) (*Server, error) {
	if config.UpstreamURL == "" || config.Store == nil {
		return nil, fmt.Errorf("proxy requires upstream URL and store")
	}
	if !strings.HasPrefix(config.UpstreamURL, "ws://") && !strings.HasPrefix(config.UpstreamURL, "wss://") {
		return nil, fmt.Errorf("unsupported upstream URL %q", config.UpstreamURL)
	}
	return &Server{
		upstreamURL: config.UpstreamURL,
		store:       config.Store,
		upgrader: websocket.Upgrader{
			ReadBufferSize:  16 * 1024,
			WriteBufferSize: 16 * 1024,
			CheckOrigin:     func(*http.Request) bool { return true },
		},
		connections: make(map[*websocket.Conn]*websocket.Conn),
	}, nil
}

func (s *Server) Close() {
	s.mu.Lock()
	s.closed = true
	type pair struct {
		device   *websocket.Conn
		upstream *websocket.Conn
	}
	connections := make([]pair, 0, len(s.connections))
	for device, upstream := range s.connections {
		connections = append(connections, pair{device: device, upstream: upstream})
	}
	s.connections = make(map[*websocket.Conn]*websocket.Conn)
	s.mu.Unlock()
	for _, pair := range connections {
		pair.device.Close()
		pair.upstream.Close()
	}
	s.wg.Wait()
}

func (s *Server) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/realtime", s.handle)
	mux.HandleFunc("/", s.handle)
	return mux
}

func (s *Server) handle(w http.ResponseWriter, r *http.Request) {
	if !websocket.IsWebSocketUpgrade(r) {
		http.Error(w, "websocket upgrade required", http.StatusUpgradeRequired)
		return
	}

	s.mu.Lock()
	if s.closed {
		s.mu.Unlock()
		http.Error(w, "proxy closed", http.StatusServiceUnavailable)
		return
	}
	s.wg.Add(1)
	s.mu.Unlock()
	defer s.wg.Done()
	upstreamHeader := cloneUpstreamHeaders(r.Header)
	upstream, response, err := websocket.DefaultDialer.Dial(s.upstreamURL, upstreamHeader)
	if err != nil {
		requestID, requestField := "", ""
		code := 0
		if response != nil {
			code = response.StatusCode
			requestID, requestField = requestIdentity(response.Header)
			if response.Body != nil {
				response.Body.Close()
			}
		}
		failureID := fmt.Sprintf("connect-%d", time.Now().UnixNano())
		record := newRecorder(s.store, failureID, failureID, firstHeader(r.Header, "Device-Id", "Client-Id"))
		record.enqueue(trace.SessionOpen, trace.Meta{Mode: "proxy", RequestID: requestID, RequestIDField: requestField, RequestIDSource: "handshake_response_header", RequestIDScope: "connection", EventType: "upstream_connect_failed", Detail: fmt.Sprintf("http_status=%d", code)}, nil)
		record.enqueue(trace.SessionEnd, trace.Meta{Mode: "proxy"}, nil)
		record.close()
		status := http.StatusBadGateway
		http.Error(w, "upstream websocket connection failed", status)
		log.Printf("proxy upstream connect failed: %v", err)
		return
	}
	device, err := s.upgrader.Upgrade(w, r, nil)
	if err != nil {
		upstream.Close()
		return
	}
	s.mu.Lock()
	if s.closed {
		s.mu.Unlock()
		device.Close()
		upstream.Close()
		return
	}
	s.connections[device] = upstream
	s.mu.Unlock()
	defer func() {
		s.mu.Lock()
		delete(s.connections, device)
		s.mu.Unlock()
	}()

	device.SetReadLimit(maxMessageSize)
	upstream.SetReadLimit(maxMessageSize)
	bootID := fmt.Sprintf("proxy-%d", time.Now().UnixNano())
	traceID := fmt.Sprintf("proxy-%d", time.Now().UnixNano())
	deviceID := firstHeader(r.Header, "Device-Id", "Client-Id")
	if deviceID == "" {
		deviceID = r.RemoteAddr
	}
	started := time.Now().UTC()
	s.store.SetDevice(store.DeviceStatus{Source: "proxy", Connected: true, DeviceID: deviceID, BootID: bootID, Remote: r.RemoteAddr, Since: started})
	defer s.store.SetDevice(store.DeviceStatus{Source: "proxy"})

	recorder := newRecorder(s.store, bootID, traceID, deviceID)
	requestID, requestField := "", ""
	if response != nil {
		requestID, requestField = requestIdentity(response.Header)
	}
	recorder.enqueue(trace.SessionOpen, trace.Meta{Mode: "proxy", RequestID: requestID, RequestIDField: requestField, RequestIDSource: "handshake_response_header", RequestIDScope: "connection", DeviceBootID: firstHeader(r.Header, "X-Trace-Boot-Id"), DeviceTraceID: firstHeader(r.Header, "X-Trace-Session-Id"), Detail: "websocket proxy session"}, nil)

	var closeOnce sync.Once
	closeBoth := func() {
		closeOnce.Do(func() {
			device.Close()
			upstream.Close()
		})
	}
	var workers sync.WaitGroup
	workers.Add(2)
	go func() {
		defer workers.Done()
		forward(device, upstream, "uplink", recorder, closeBoth)
	}()
	go func() {
		defer workers.Done()
		forward(upstream, device, "downlink", recorder, closeBoth)
	}()
	workers.Wait()
	closeBoth()
	recorder.enqueue(trace.SessionEnd, trace.Meta{Mode: "proxy", Detail: "websocket proxy closed"}, nil)
	recorder.close()
	log.Printf("proxy session closed: device=%s remote=%s", deviceID, r.RemoteAddr)
}

func cloneUpstreamHeaders(src http.Header) http.Header {
	dst := make(http.Header)
	for key, values := range src {
		lower := strings.ToLower(key)
		switch lower {
		case "connection", "upgrade", "host", "content-length", "sec-websocket-key", "sec-websocket-version", "sec-websocket-extensions", "sec-websocket-accept", "origin":
			continue
		}
		for _, value := range values {
			if strings.EqualFold(key, "authorization") || strings.EqualFold(key, "api-key") || strings.EqualFold(key, "workspace") || strings.EqualFold(key, "workspace-id") || strings.EqualFold(key, "client-id") || strings.EqualFold(key, "device-id") {
				dst.Add(key, value)
			}
		}
	}
	return dst
}

func firstHeader(headers http.Header, names ...string) string {
	for _, name := range names {
		if value := headers.Get(name); value != "" {
			return value
		}
	}
	return ""
}

func forward(src, dst *websocket.Conn, direction string, recorder *recorder, closeBoth func()) {
	for {
		kind, payload, err := src.ReadMessage()
		if err != nil {
			closeBoth()
			return
		}
		receivedAt := time.Now().UTC()
		forwardStartAt := time.Now().UTC()
		writeErr := dst.WriteMessage(kind, payload)
		forwardDoneAt := time.Now().UTC()
		recorder.message(direction, kind, payload, receivedAt, forwardStartAt, forwardDoneAt, writeErr)
		if writeErr != nil {
			closeBoth()
			return
		}
	}
}

type recorder struct {
	store       *store.Store
	bootID      string
	traceID     string
	device      string
	seq         atomic.Uint64
	queue       chan trace.Frame
	done        chan struct{}
	closeOnce   sync.Once
	mu          sync.Mutex
	pending     uint64
	queuedBytes int
	storedSeq   uint64
}

func newRecorder(s *store.Store, bootID, traceID, device string) *recorder {
	r := &recorder{store: s, bootID: bootID, traceID: traceID, device: device, queue: make(chan trace.Frame, recordQueueSize), done: make(chan struct{})}
	go r.run()
	return r
}

func (r *recorder) close() {
	r.closeOnce.Do(func() {
		r.mu.Lock()
		pending := r.pending
		r.pending = 0
		r.mu.Unlock()
		if pending > 0 {
			r.queue <- trace.Frame{Kind: trace.TraceGap, Meta: trace.Meta{BootID: r.bootID, TraceSessionID: r.traceID, EventType: "trace_gap", Detail: fmt.Sprintf("dropped_records=%d", pending)}}
		}
		close(r.queue)
		<-r.done
	})
}

func (r *recorder) run() {
	defer close(r.done)
	for frame := range r.queue {
		r.mu.Lock()
		r.queuedBytes -= len(frame.Payload)
		r.mu.Unlock()
		if frame.Kind == trace.WSTX || frame.Kind == trace.WSRX {
			r.decodeAndStore(frame)
		} else {
			r.append(frame)
		}
	}
}

func (r *recorder) enqueue(kind uint16, meta trace.Meta, payload []byte) {
	meta.BootID = r.bootID
	meta.TraceSessionID = r.traceID
	meta.DeviceID = r.device
	if kind != trace.SessionOpen && kind != trace.SessionEnd {
		meta.Seq = fmt.Sprintf("%d", r.seq.Add(1))
	}
	r.mu.Lock()
	if r.queuedBytes+len(payload) > 16*1024*1024 {
		r.pending++
		r.mu.Unlock()
		return
	}
	frame := trace.Frame{Kind: kind, Meta: meta, Payload: append([]byte(nil), payload...)}
	if r.pending > 0 {
		gapMeta := trace.Meta{
			BootID: r.bootID, TraceSessionID: r.traceID, DeviceID: r.device,
			Seq: fmt.Sprintf("%d", r.seq.Add(1)), Mode: "proxy",
			EventType: "trace_gap", Detail: fmt.Sprintf("dropped_records=%d", r.pending),
		}
		select {
		case r.queue <- trace.Frame{Kind: trace.TraceGap, Meta: gapMeta}:
			r.pending = 0
		default:
			r.pending++
			r.mu.Unlock()
			return
		}
	}
	select {
	case r.queue <- frame:
		r.queuedBytes += len(payload)
	default:
		r.pending++
	}
	r.mu.Unlock()
}

func (r *recorder) message(direction string, kind int, payload []byte, receivedAt, forwardStartAt, forwardDoneAt time.Time, writeErrors ...error) {
	if kind != websocket.TextMessage && kind != websocket.BinaryMessage {
		return
	}
	meta := trace.Meta{Mode: "proxy", ContentType: "application/json", Direction: direction, Detail: fmt.Sprintf("ws_type=%d", kind), ProxyReceivedAt: receivedAt.Format(time.RFC3339Nano), ForwardStartAt: forwardStartAt.Format(time.RFC3339Nano), ForwardDoneAt: forwardDoneAt.Format(time.RFC3339Nano)}
	if kind == websocket.BinaryMessage {
		meta.ContentType = "application/octet-stream"
	}
	if len(writeErrors) > 0 && writeErrors[0] != nil {
		meta.ForwardError = "websocket_write_failed"
	}
	frameKind := uint16(trace.WSRX)
	if direction == "uplink" {
		frameKind = trace.WSTX
	}
	r.enqueue(frameKind, meta, payload)
}

func (r *recorder) append(frame trace.Frame) {
	r.storedSeq++
	frame.Meta.Seq = fmt.Sprint(r.storedSeq)
	if _, err := r.store.Append(frame); err != nil {
		log.Printf("proxy trace write: %v", err)
	}
}

// Parsing and audio extraction run on the bounded recorder worker, away from forwarding.
func (r *recorder) decodeAndStore(frame trace.Frame) {
	var event struct {
		Transcript string `json:"transcript"`
		ItemID     string `json:"item_id"`
		Type       string `json:"type"`
		EventID    string `json:"event_id"`
		RequestID  string `json:"request_id"`
		ReqID      string `json:"reqid"`
		SessionID  string `json:"session_id"`
		ResponseID string `json:"response_id"`
		Session    struct {
			ID string `json:"id"`
		} `json:"session"`
		Response struct {
			ID string `json:"id"`
		} `json:"response"`
		Header struct {
			RequestID string `json:"request_id"`
		} `json:"header"`
		Audio string `json:"audio"`
		Delta string `json:"delta"`
	}
	if frame.Meta.Detail == "ws_type=1" && json.Unmarshal(frame.Payload, &event) == nil {
		frame.Meta.EventType = event.Type
		if event.Type == "conversation.item.input_audio_transcription.completed" {
			frame.Meta.Transcript = event.Transcript
			frame.Meta.Role = "user"
		}
		if event.Type == "response.audio_transcript.done" {
			frame.Meta.Transcript = event.Transcript
			frame.Meta.Role = "assistant"
		}
		frame.Meta.MessageID = event.EventID
		frame.Meta.RequestID = event.RequestID
		if event.RequestID != "" {
			frame.Meta.RequestIDField = "request_id"
		}
		if frame.Meta.RequestID == "" {
			frame.Meta.RequestID = event.ReqID
			if event.ReqID != "" {
				frame.Meta.RequestIDField = "reqid"
			}
		}
		if frame.Meta.RequestID == "" {
			frame.Meta.RequestID = event.Header.RequestID
			if event.Header.RequestID != "" {
				frame.Meta.RequestIDField = "header.request_id"
			}
		}
		if frame.Meta.RequestID != "" {
			frame.Meta.RequestIDSource = "protocol_event"
			frame.Meta.RequestIDScope = "event"
		}
		frame.Meta.ResponseID = event.ResponseID
		if frame.Meta.ResponseID == "" {
			frame.Meta.ResponseID = event.Response.ID
		}
		frame.Meta.SessionID = event.SessionID
		if frame.Meta.SessionID == "" {
			frame.Meta.SessionID = event.Session.ID
		}
	}
	if frame.Meta.EventType == "" {
		frame.Meta.EventType = "websocket.message"
	}
	r.append(frame)
	encoded := ""
	rate := 0
	kind := uint16(trace.PCMUplink)
	if event.Type == "input_audio_buffer.append" {
		encoded = event.Audio
		rate = 16000
	} else if event.Type == "response.audio.delta" {
		encoded = event.Delta
		rate = 24000
		kind = trace.PCMPlayback
	}
	if encoded == "" {
		return
	}
	pcm, err := base64.StdEncoding.DecodeString(encoded)
	if err != nil || len(pcm) == 0 || len(pcm)%2 != 0 {
		return
	}
	meta := frame.Meta
	meta.SampleRate = rate
	meta.Channels = 1
	meta.BitsPerSample = 16
	meta.SampleFormat = "signed_int"
	meta.ByteOrder = "little"
	r.append(trace.Frame{Kind: kind, Meta: meta, Payload: pcm})
}

func requestIdentity(headers http.Header) (string, string) {
	for _, field := range []string{"X-Request-Id", "Request-Id", "Reqid", "X-DashScope-Request-Id"} {
		if value := headers.Get(field); value != "" {
			return value, field
		}
	}
	return "", ""
}
