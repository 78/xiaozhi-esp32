package proxy

import (
	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/trace"
	"net/http"
	"net/http/httptest"
	"strconv"
	"testing"
	"time"

	"github.com/gorilla/websocket"
	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/store"
)

func TestProxyForwardsHeadersAndApplicationFramesInOrder(t *testing.T) {
	upgrader := websocket.Upgrader{CheckOrigin: func(*http.Request) bool { return true }}
	gotAuth := make(chan string, 1)
	gotMessages := make(chan string, 2)
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		gotAuth <- r.Header.Get("Authorization")
		conn, err := upgrader.Upgrade(w, r, nil)
		if err != nil {
			t.Errorf("upstream upgrade: %v", err)
			return
		}
		defer conn.Close()
		for i := 0; i < 2; i++ {
			kind, payload, err := conn.ReadMessage()
			if err != nil {
				return
			}
			gotMessages <- string(payload)
			if err := conn.WriteMessage(kind, payload); err != nil {
				return
			}
		}
	}))
	defer upstream.Close()

	data, err := store.Open(t.TempDir(), 16*1024*1024)
	if err != nil {
		t.Fatal(err)
	}
	defer data.Close()
	upstreamURL := "ws" + upstream.URL[len("http"):]
	service, err := New(Config{UpstreamURL: upstreamURL, Store: data})
	if err != nil {
		t.Fatal(err)
	}
	defer service.Close()
	proxyServer := httptest.NewServer(service.Handler())
	defer proxyServer.Close()
	proxyURL := "ws" + proxyServer.URL[len("http"):] + "/realtime"
	header := http.Header{"Authorization": []string{"Bearer test-key"}, "Device-Id": []string{"device-test"}}
	client, _, err := websocket.DefaultDialer.Dial(proxyURL, header)
	if err != nil {
		t.Fatal(err)
	}
	if err := client.WriteMessage(websocket.TextMessage, []byte(`{"type":"first"}`)); err != nil {
		t.Fatal(err)
	}
	if err := client.WriteMessage(websocket.BinaryMessage, []byte{1, 2, 3}); err != nil {
		t.Fatal(err)
	}
	for i := 0; i < 2; i++ {
		if _, _, err := client.ReadMessage(); err != nil {
			t.Fatal(err)
		}
	}
	client.Close()

	select {
	case auth := <-gotAuth:
		if auth != "Bearer test-key" {
			t.Fatalf("authorization = %q", auth)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("upstream did not receive handshake")
	}
	select {
	case first := <-gotMessages:
		if first != `{"type":"first"}` {
			t.Fatalf("first payload = %q", first)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("missing first frame")
	}
	select {
	case second := <-gotMessages:
		if second != string([]byte{1, 2, 3}) {
			t.Fatalf("second payload = %q", second)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("missing second frame")
	}
	service.Close()
	for _, session := range data.List() {
		events, err := data.Events(session.ID, "", 100)
		if err != nil {
			t.Fatal(err)
		}
		for _, event := range events {
			if event.Kind != "ws_tx" && event.Kind != "ws_rx" {
				continue
			}
			received, err := time.Parse(time.RFC3339Nano, event.Meta.ProxyReceivedAt)
			if err != nil {
				t.Fatal(err)
			}
			started, err := time.Parse(time.RFC3339Nano, event.Meta.ForwardStartAt)
			if err != nil {
				t.Fatal(err)
			}
			done, err := time.Parse(time.RFC3339Nano, event.Meta.ForwardDoneAt)
			if err != nil {
				t.Fatal(err)
			}
			if received.After(started) || started.After(done) {
				t.Fatalf("invalid forwarding times: %v, %v, %v", received, started, done)
			}
		}
	}
}

func TestRecorderKeepsRequestIDsAndAudioAssociation(t *testing.T) {
	data, err := store.Open(t.TempDir(), 8*1024*1024)
	if err != nil {
		t.Fatal(err)
	}
	defer data.Close()
	r := newRecorder(data, "boot", "trace", "device")
	now := time.Now().UTC()
	r.enqueue(trace.SessionOpen, trace.Meta{RequestID: "handshake-request"}, nil)
	r.message("downlink", websocket.TextMessage, []byte(`{"type":"response.audio.delta","reqid":"request-42","response_id":"reply-7","event_id":"event-3","delta":"AQACAAMA"}`), now, now, now)
	r.message("downlink", websocket.TextMessage, []byte(`{"type":"response.audio_transcript.done","request_id":"request-43","response_id":"reply-7","transcript":"hello"}`), now, now, now)
	r.enqueue(trace.SessionEnd, trace.Meta{}, nil)
	r.close()
	sessions := data.List()
	if len(sessions) != 1 {
		t.Fatalf("sessions=%d", len(sessions))
	}
	events, err := data.Events(sessions[0].ID, "", 100)
	if err != nil {
		t.Fatal(err)
	}
	var audioFound, transcriptFound bool
	var previous uint64
	for _, e := range events {
		seq, err := strconv.ParseUint(e.Meta.Seq, 10, 64)
		if err != nil || seq <= previous {
			t.Fatalf("non-increasing seq %s after %d", e.Meta.Seq, previous)
		}
		previous = seq
		if e.Kind == "pcm_playback" {
			audioFound = true
			if e.Meta.RequestID != "request-42" || e.Meta.ResponseID != "reply-7" || e.Meta.MessageID != "event-3" || e.AudioLength != 6 {
				t.Fatalf("lost audio association: %+v", e)
			}
		}
		if e.Meta.Transcript == "hello" {
			transcriptFound = true
			if e.Meta.Role != "assistant" || e.Meta.RequestID != "request-43" {
				t.Fatalf("bad transcript %+v", e)
			}
		}
	}
	if !audioFound || !transcriptFound {
		t.Fatalf("missing records: %+v", events)
	}
}

func TestRecorderReportsFinalQueueGap(t *testing.T) {
	data, err := store.Open(t.TempDir(), 8*1024*1024)
	if err != nil {
		t.Fatal(err)
	}
	defer data.Close()
	r := newRecorder(data, "boot", "trace", "device")
	r.enqueue(trace.SessionOpen, trace.Meta{}, nil)
	r.mu.Lock()
	r.pending = 17
	r.mu.Unlock()
	r.close()
	sessions := data.List()
	if len(sessions) != 1 || sessions[0].Gaps != 1 {
		t.Fatalf("final gap lost: %+v", sessions)
	}
}
