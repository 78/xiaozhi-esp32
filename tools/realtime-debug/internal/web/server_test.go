package web

import (
	"archive/zip"
	"bytes"
	"encoding/json"
	"fmt"
	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/store"
	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/trace"
	"net/http/httptest"
	"testing"
)

func TestLinkedAudioConversationAndBundle(t *testing.T) {
	data, err := store.Open(t.TempDir(), 8*1024*1024)
	if err != nil {
		t.Fatal(err)
	}
	defer data.Close()
	appendFrame := func(kind uint16, meta trace.Meta, payload []byte) string {
		t.Helper()
		id, err := data.Append(trace.Frame{Kind: kind, Meta: meta, Payload: payload})
		if err != nil {
			t.Fatal(err)
		}
		return id
	}
	raw := trace.Meta{BootID: "device-boot", TraceSessionID: "device-trace", Seq: "1", SampleRate: 24000, Channels: 1, BitsPerSample: 16, SampleFormat: "signed_int", ByteOrder: "little"}
	rawID := appendFrame(trace.PCMInput, raw, []byte{0, 0, 255, 127, 0, 128, 0, 0})
	meta := trace.Meta{BootID: "proxy-boot", TraceSessionID: "proxy-trace", Seq: "1", Mode: "proxy", DeviceBootID: raw.BootID, DeviceTraceID: raw.TraceSessionID}
	proxyID := appendFrame(trace.SessionOpen, meta, nil)
	for i := 2; i < 605; i++ {
		meta.Seq = fmt.Sprint(i)
		appendFrame(trace.LocalEvent, meta, nil)
	}
	meta.Seq = "605"
	meta.Transcript = "after page 500"
	meta.Role = "assistant"
	appendFrame(trace.WSRX, meta, nil)
	handler := New(data)
	req := httptest.NewRequest("GET", "/api/sessions/"+proxyID, nil)
	response := httptest.NewRecorder()
	handler.ServeHTTP(response, req)
	if response.Code != 200 {
		t.Fatal(response.Body.String())
	}
	var info struct {
		InputSessionID string          `json:"input_session_id"`
		Conversation   []store.Event   `json:"conversation"`
		Input          []store.Segment `json:"input"`
	}
	if err := json.Unmarshal(response.Body.Bytes(), &info); err != nil {
		t.Fatal(err)
	}
	if info.InputSessionID != rawID || len(info.Input) != 1 || len(info.Conversation) != 1 || info.Conversation[0].Meta.Transcript != "after page 500" {
		t.Fatalf("lost linked audio or late transcript %+v", info)
	}
	response = httptest.NewRecorder()
	handler.ServeHTTP(response, httptest.NewRequest("GET", "/api/sessions/"+rawID+"/waveform/input/0", nil))
	if response.Code != 200 || !bytes.Contains(response.Body.Bytes(), []byte("0.999")) || !bytes.Contains(response.Body.Bytes(), []byte("-1")) {
		t.Fatalf("bad waveform %s", response.Body.String())
	}
	response = httptest.NewRecorder()
	handler.ServeHTTP(response, httptest.NewRequest("GET", "/api/sessions/"+proxyID+"/export", nil))
	archive, err := zip.NewReader(bytes.NewReader(response.Body.Bytes()), int64(response.Body.Len()))
	if err != nil {
		t.Fatal(err)
	}
	if len(archive.File) != 2 {
		t.Fatalf("bundle must include proxy and diagnostic sessions, got %d", len(archive.File))
	}
}
