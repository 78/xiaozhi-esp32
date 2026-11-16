package trace

import (
	"bytes"
	"encoding/binary"
	"testing"
)

func aecFrame(meta string) []byte {
	content := make([]byte, 8+len(meta)+4)
	binary.BigEndian.PutUint16(content[0:2], Version)
	binary.BigEndian.PutUint16(content[2:4], AECMic)
	binary.BigEndian.PutUint32(content[4:8], uint32(len(meta)))
	copy(content[8:], meta)
	frame := make([]byte, 4+len(content))
	binary.BigEndian.PutUint32(frame[:4], uint32(len(content)))
	copy(frame[4:], content)
	return frame
}

func TestAECFrameRequiresTimeline(t *testing.T) {
	base := `{"boot_id":"boot","trace_session_id":"session","seq":"1","sample_rate":16000,"channels":1,"bits_per_sample":16,"sample_format":"signed_int","byte_order":"little","sample_start_us":1000000,`
	if _, err := ReadFrame(bytes.NewReader(aecFrame(base + `"sample_index":"0"}`))); err != nil {
		t.Fatalf("valid AEC frame rejected: %v", err)
	}
	if _, err := ReadFrame(bytes.NewReader(aecFrame(base + `"sample_index":"bad"}`))); err == nil {
		t.Fatal("invalid sample index accepted")
	}
	if _, err := ReadFrame(bytes.NewReader(aecFrame(`{"boot_id":"boot","trace_session_id":"session","seq":"1","sample_rate":16000,"channels":1,"bits_per_sample":16,"sample_format":"signed_int","byte_order":"little"}`))); err == nil {
		t.Fatal("missing timeline accepted")
	}
}
