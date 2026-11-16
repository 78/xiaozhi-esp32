package store

import (
	"bytes"
	"encoding/binary"
	"math"
	"strconv"
	"testing"

	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/trace"
)

func TestAECReportAndRecovery(t *testing.T) {
	root := t.TempDir()
	s, err := Open(root, 1<<20)
	if err != nil {
		t.Fatal(err)
	}
	meta := trace.Meta{BootID: "boot", TraceSessionID: "session", SampleRate: 16000, Channels: 1, BitsPerSample: 16}
	var id string
	for block := 0; block < 2; block++ {
		for _, track := range []struct {
			kind  uint16
			value int16
		}{{trace.AECMic, 1000}, {trace.AECRef, 2000}, {trace.AECOut, 500}} {
			payload := make([]byte, 320)
			for i := 0; i < len(payload); i += 2 {
				binary.LittleEndian.PutUint16(payload[i:], uint16(track.value))
			}
			meta.SampleIndex = strconv.Itoa(block * 160)
			meta.SampleStartUS = int64(1000000 + block*10000)
			id, err = s.Append(trace.Frame{Kind: track.kind, Meta: meta, Payload: payload})
			if err != nil {
				t.Fatal(err)
			}
		}
	}
	if _, err := s.Append(trace.Frame{Kind: trace.SessionEnd, Meta: meta}); err != nil {
		t.Fatal(err)
	}
	report, err := s.AnalyzeAEC(id)
	if err != nil {
		t.Fatal(err)
	}
	if !report.Complete || report.OverallAttenuationDB == nil || math.Abs(*report.OverallAttenuationDB-6.0206) > 0.001 {
		t.Fatalf("unexpected AEC report: %+v", report)
	}
	if err := s.Close(); err != nil {
		t.Fatal(err)
	}
	s, err = Open(root, 1<<20)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	report, err = s.AnalyzeAEC(id)
	if err != nil || !report.Complete {
		t.Fatalf("recovered report: %+v, %v", report, err)
	}
	var export bytes.Buffer
	if err := s.Export(id, &export); err != nil {
		t.Fatal(err)
	}
	if export.Len() == 0 {
		t.Fatal("empty export")
	}
}
