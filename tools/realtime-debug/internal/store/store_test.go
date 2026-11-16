package store

import (
	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/trace"
	"testing"
)

func TestAudioSegmentsKeepPauseAndReplyBoundaries(t *testing.T) {
	data, err := Open(t.TempDir(), 1024*1024)
	if err != nil {
		t.Fatal(err)
	}
	defer data.Close()
	meta := trace.Meta{BootID: "boot", TraceSessionID: "trace", Seq: "1", SampleRate: 16000, Channels: 1, BitsPerSample: 16, SampleFormat: "signed_int", ByteOrder: "little"}
	id, err := data.Append(trace.Frame{Kind: trace.PCMUplink, Meta: meta, Payload: []byte{1, 0}})
	if err != nil {
		t.Fatal(err)
	}
	meta.Seq = "2"
	meta.EventType = "response.created"
	data.Append(trace.Frame{Kind: trace.WSRX, Meta: meta})
	meta.Seq = "3"
	meta.EventType = "input_audio_buffer.append"
	data.Append(trace.Frame{Kind: trace.PCMUplink, Meta: meta, Payload: []byte{2, 0}})
	segments, err := data.Segments(id, "uplink")
	if err != nil {
		t.Fatal(err)
	}
	if len(segments) != 2 || !segments[1].GapBefore {
		t.Fatalf("pause was concatenated: %+v", segments)
	}
}

func TestTotalLimitKeepsExistingRecordings(t *testing.T) {
	data, err := Open(t.TempDir(), 1024*1024)
	if err != nil {
		t.Fatal(err)
	}
	defer data.Close()
	data.SetTotalLimit(800)
	meta := trace.Meta{BootID: "boot", TraceSessionID: "trace", Seq: "1"}
	id, err := data.Append(trace.Frame{Kind: trace.SessionOpen, Meta: meta})
	if err != nil {
		t.Fatal(err)
	}
	meta.Seq = "2"
	data.Append(trace.Frame{Kind: trace.WSRX, Meta: meta, Payload: make([]byte, 900)})
	summary, _ := data.Get(id)
	if !summary.Truncated {
		t.Fatal("total budget not enforced")
	}
	events, err := data.Events(id, "", 100)
	if err != nil || len(events) != 1 {
		t.Fatalf("existing records were lost %v %v", events, err)
	}
}
