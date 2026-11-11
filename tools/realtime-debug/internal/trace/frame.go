package trace

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"strconv"
)

const (
	Version      = 1
	MaxFrameSize = 256 * 1024
	MaxMetaSize  = 4096

	Hello       = 1
	SessionOpen = 2
	SessionEnd  = 3
	PCMUplink   = 4
	WSTX        = 5
	WSTXResult  = 6
	WSRX        = 7
	PCMPlayback = 8
	LocalEvent  = 9
	TraceGap    = 10
)

var kindNames = map[uint16]string{
	Hello: "hello", SessionOpen: "session_open", SessionEnd: "session_close",
	PCMUplink: "pcm_uplink", WSTX: "ws_tx", WSTXResult: "ws_tx_result",
	WSRX: "ws_rx", PCMPlayback: "pcm_playback", LocalEvent: "local_event",
	TraceGap: "trace_gap",
}

type Meta struct {
	BootID               string `json:"boot_id"`
	TraceSessionID       string `json:"trace_session_id"`
	DeviceID             string `json:"device_id,omitempty"`
	FirmwareVersion      string `json:"firmware_version,omitempty"`
	Seq                  string `json:"seq,omitempty"`
	MonoUS               int64  `json:"mono_us,omitempty"`
	ConnectionGeneration uint32 `json:"connection_generation,omitempty"`
	SessionID            string `json:"session_id,omitempty"`
	ResponseID           string `json:"response_id,omitempty"`
	MessageID            string `json:"message_id,omitempty"`
	EventType            string `json:"event_type,omitempty"`
	Detail               string `json:"detail,omitempty"`
	Mode                 string `json:"mode,omitempty"`
	ContentType          string `json:"content_type,omitempty"`
	SampleRate           int    `json:"sample_rate,omitempty"`
	Channels             int    `json:"channels,omitempty"`
	BitsPerSample        int    `json:"bits_per_sample,omitempty"`
	SampleFormat         string `json:"sample_format,omitempty"`
	ByteOrder            string `json:"byte_order,omitempty"`
	PartIndex            int    `json:"part_index,omitempty"`
	PartCount            int    `json:"part_count,omitempty"`
	TotalLength          int    `json:"total_length,omitempty"`
}

type Frame struct {
	Kind    uint16
	Meta    Meta
	RawMeta json.RawMessage
	Payload []byte
}

func KindName(kind uint16) string { return kindNames[kind] }

func ReadFrame(r io.Reader) (Frame, error) {
	var sizeBuf [4]byte
	if _, err := io.ReadFull(r, sizeBuf[:]); err != nil {
		return Frame{}, err
	}
	size := binary.BigEndian.Uint32(sizeBuf[:])
	if size < 8 || size > MaxFrameSize {
		return Frame{}, fmt.Errorf("invalid frame length %d", size)
	}
	data := make([]byte, size)
	if _, err := io.ReadFull(r, data); err != nil {
		return Frame{}, err
	}
	version := binary.BigEndian.Uint16(data[:2])
	if version != Version {
		return Frame{}, fmt.Errorf("unsupported trace version %d", version)
	}
	kind := binary.BigEndian.Uint16(data[2:4])
	if KindName(kind) == "" {
		return Frame{}, fmt.Errorf("unknown frame kind %d", kind)
	}
	metaSize := binary.BigEndian.Uint32(data[4:8])
	if metaSize == 0 || metaSize > MaxMetaSize || metaSize > size-8 {
		return Frame{}, fmt.Errorf("invalid metadata length %d", metaSize)
	}
	rawMeta := data[8 : 8+metaSize]
	dec := json.NewDecoder(bytes.NewReader(rawMeta))
	var meta Meta
	if err := dec.Decode(&meta); err != nil {
		return Frame{}, fmt.Errorf("metadata: %w", err)
	}
	var trailing any
	if err := dec.Decode(&trailing); !errors.Is(err, io.EOF) {
		return Frame{}, errors.New("metadata has trailing content")
	}
	if meta.BootID == "" || len(meta.BootID) > 96 {
		return Frame{}, errors.New("invalid boot_id")
	}
	if kind == Hello && (meta.DeviceID == "" || len(meta.DeviceID) > 96 || meta.FirmwareVersion == "" || len(meta.FirmwareVersion) > 96) {
		return Frame{}, errors.New("invalid hello identity")
	}
	if kind != Hello {
		if meta.TraceSessionID == "" || len(meta.TraceSessionID) > 96 {
			return Frame{}, errors.New("invalid trace_session_id")
		}
		seq, err := strconv.ParseUint(meta.Seq, 10, 64)
		if err != nil || seq == 0 {
			return Frame{}, errors.New("invalid seq")
		}
	}
	for _, value := range []string{meta.SessionID, meta.ResponseID, meta.MessageID, meta.EventType, meta.Detail} {
		if len(value) > 512 {
			return Frame{}, errors.New("metadata field too long")
		}
	}
	if kind == PCMUplink || kind == PCMPlayback {
		if meta.SampleRate <= 0 || meta.SampleRate > 192000 || meta.Channels < 1 || meta.Channels > 8 ||
			(meta.BitsPerSample != 16 && meta.BitsPerSample != 24 && meta.BitsPerSample != 32) ||
			meta.ByteOrder != "little" || meta.SampleFormat != "signed_int" {
			return Frame{}, errors.New("unsupported PCM format")
		}
		if len(data[8+metaSize:])%(meta.Channels*meta.BitsPerSample/8) != 0 {
			return Frame{}, errors.New("unaligned PCM payload")
		}
	}
	if meta.PartCount < 0 || meta.PartIndex < 0 || (meta.PartCount == 0 && meta.PartIndex != 0) || (meta.PartCount > 0 && meta.PartIndex >= meta.PartCount) {
		return Frame{}, errors.New("invalid part index")
	}
	if meta.PartCount > 1 && (meta.MessageID == "" || meta.PartCount > 128 || meta.TotalLength <= 0 || meta.TotalLength > 8*1024*1024) {
		return Frame{}, errors.New("invalid message fragments")
	}
	if meta.PartCount > 1 && kind != WSTX && kind != WSRX {
		return Frame{}, errors.New("fragments are only valid for WebSocket messages")
	}
	return Frame{Kind: kind, Meta: meta, RawMeta: append([]byte(nil), rawMeta...), Payload: data[8+metaSize:]}, nil
}
