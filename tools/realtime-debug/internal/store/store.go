package store

import (
	"archive/zip"
	"bufio"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/trace"
)

type Summary struct {
	ID            string    `json:"id"`
	BootID        string    `json:"boot_id"`
	TraceID       string    `json:"trace_session_id"`
	DeviceID      string    `json:"device_id,omitempty"`
	Mode          string    `json:"mode,omitempty"`
	RequestID     string    `json:"request_id,omitempty"`
	DeviceBootID  string    `json:"device_boot_id,omitempty"`
	DeviceTraceID string    `json:"device_trace_id,omitempty"`
	StartedAt     time.Time `json:"started_at"`
	EndedAt       time.Time `json:"ended_at,omitempty"`
	Events        uint64    `json:"events"`
	Gaps          uint64    `json:"gaps"`
	Bytes         int64     `json:"bytes"`
	Truncated     bool      `json:"truncated"`
	Interrupted   bool      `json:"interrupted"`
	Live          bool      `json:"live"`
}

type Event struct {
	Kind          string          `json:"kind"`
	Meta          trace.Meta      `json:"meta"`
	RawMeta       json.RawMessage `json:"raw_meta,omitempty"`
	ReceivedAt    time.Time       `json:"received_at"`
	PayloadOffset int64           `json:"payload_offset,omitempty"`
	PayloadLength int             `json:"payload_length,omitempty"`
	AudioTrack    string          `json:"audio_track,omitempty"`
	AudioOffset   int64           `json:"audio_offset,omitempty"`
	AudioLength   int             `json:"audio_length,omitempty"`
}

type Segment struct {
	ResponseID    string    `json:"response_id,omitempty"`
	RequestID     string    `json:"request_id,omitempty"`
	MonoUS        int64     `json:"mono_us,omitempty"`
	ReceivedAt    time.Time `json:"received_at"`
	GapBefore     bool      `json:"gap_before,omitempty"`
	Track         string    `json:"track"`
	Index         int       `json:"index"`
	Offset        int64     `json:"offset"`
	Length        int64     `json:"length"`
	SampleRate    int       `json:"sample_rate"`
	Channels      int       `json:"channels"`
	BitsPerSample int       `json:"bits_per_sample"`
}

type DeviceStatus struct {
	Source             string    `json:"source,omitempty"`
	ProxyConnected     bool      `json:"proxy_connected"`
	CollectorConnected bool      `json:"collector_connected"`
	Connected          bool      `json:"connected"`
	DeviceID           string    `json:"device_id,omitempty"`
	BootID             string    `json:"boot_id,omitempty"`
	Remote             string    `json:"remote,omitempty"`
	Since              time.Time `json:"since,omitempty"`
}

type session struct {
	summary  Summary
	dir      string
	events   *os.File
	payloads *os.File
	uplink   *os.File
	playback *os.File
	input    *os.File
	aecMic   *os.File
	aecRef   *os.File
	aecOut   *os.File
}

type Store struct {
	mu              sync.Mutex
	root            string
	maxSessionBytes int64
	maxTotalBytes   int64
	totalBytes      int64
	sessions        map[string]*session
	byKey           map[string]string
	subscribers     map[chan string]struct{}
	device          DeviceStatus
	devices         map[string]DeviceStatus
}

func Open(root string, maxSessionBytes int64) (*Store, error) {
	if err := os.MkdirAll(root, 0700); err != nil {
		return nil, err
	}
	s := &Store{root: root, maxSessionBytes: maxSessionBytes, sessions: make(map[string]*session), byKey: make(map[string]string), subscribers: make(map[chan string]struct{})}
	err := filepath.WalkDir(root, func(path string, entry os.DirEntry, walkErr error) error {
		if walkErr != nil || entry == nil || entry.IsDir() || entry.Name() != "manifest.json" {
			return walkErr
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		var summary Summary
		if err := json.Unmarshal(data, &summary); err != nil {
			return err
		}
		if summary.ID == "" || summary.ID != filepath.Base(filepath.Dir(path)) {
			return fmt.Errorf("invalid session manifest %s", path)
		}
		loaded := &session{summary: summary, dir: filepath.Dir(path)}
		if err := s.recoverSession(loaded); err != nil {
			return fmt.Errorf("recover session %s: %w", summary.ID, err)
		}
		s.sessions[summary.ID] = loaded
		s.totalBytes += loaded.summary.Bytes
		if err := s.writeManifest(loaded); err != nil {
			return err
		}
		return nil
	})
	return s, err
}

func (s *Store) recoverSession(entry *session) error {
	file, err := os.OpenFile(filepath.Join(entry.dir, "events.jsonl"), os.O_RDWR, 0600)
	if err != nil {
		return err
	}
	defer file.Close()
	reader := bufio.NewReaderSize(file, 64*1024)
	var offset int64
	var ended bool
	ends := map[string]int64{"payloads.dat": 0, "pcm-uplink.dat": 0, "pcm-playback.dat": 0, "pcm-input.dat": 0, "pcm-aec_mic.dat": 0, "pcm-aec_ref.dat": 0, "pcm-aec_out.dat": 0}
	entry.summary.Events = 0
	entry.summary.Gaps = 0
	entry.summary.Bytes = 0
	for {
		line, err := reader.ReadSlice('\n')
		if errors.Is(err, io.EOF) {
			if len(line) != 0 {
				if err := file.Truncate(offset); err != nil {
					return err
				}
			}
			break
		}
		if err != nil {
			return err
		}
		var event Event
		if err := json.Unmarshal(line, &event); err != nil {
			return fmt.Errorf("invalid event at offset %d: %w", offset, err)
		}
		offset += int64(len(line))
		entry.summary.Events++
		entry.summary.Bytes += int64(len(line) + event.PayloadLength + event.AudioLength)
		if end := event.PayloadOffset + int64(event.PayloadLength); end > ends["payloads.dat"] {
			ends["payloads.dat"] = end
		}
		if audioFile := audioFilename(event.AudioTrack); audioFile != "" {
			if end := event.AudioOffset + int64(event.AudioLength); end > ends[audioFile] {
				ends[audioFile] = end
			}
		}
		if event.Kind == "trace_gap" {
			entry.summary.Gaps++
		}
		if event.Meta.RequestID != "" {
			entry.summary.RequestID = event.Meta.RequestID
		}
		if event.Meta.DeviceBootID != "" {
			entry.summary.DeviceBootID = event.Meta.DeviceBootID
		}
		if event.Meta.DeviceTraceID != "" {
			entry.summary.DeviceTraceID = event.Meta.DeviceTraceID
		}
		if event.Meta.Mode != "" {
			entry.summary.Mode = event.Meta.Mode
		}
		if entry.summary.DeviceID == "" && event.Meta.DeviceID != "" {
			entry.summary.DeviceID = event.Meta.DeviceID
		}
		if event.Kind == "session_close" {
			entry.summary.EndedAt = event.ReceivedAt
			ended = true
		}
	}
	for name, end := range ends {
		path := filepath.Join(entry.dir, name)
		info, err := os.Stat(path)
		if err != nil {
			if errors.Is(err, os.ErrNotExist) && end == 0 && (name == "pcm-input.dat" || name == "pcm-aec_mic.dat" || name == "pcm-aec_ref.dat" || name == "pcm-aec_out.dat") {
				continue
			}
			return err
		}
		if info.Size() < end {
			return fmt.Errorf("indexed data missing from %s", name)
		}
		if info.Size() > end {
			if err := os.Truncate(path, end); err != nil {
				return err
			}
		}
	}
	if !ended && (entry.summary.Live || entry.summary.Events > 0) {
		entry.summary.Interrupted = true
		if entry.summary.EndedAt.IsZero() {
			entry.summary.EndedAt = time.Now().UTC()
		}
	} else if ended {
		entry.summary.Interrupted = false
	}
	entry.summary.Live = false
	return nil
}

func idFor(bootID, traceID string) string {
	sum := sha256.Sum256([]byte(bootID + "\x00" + traceID))
	return hex.EncodeToString(sum[:12])
}

func openAppend(path string) (*os.File, error) {
	return os.OpenFile(path, os.O_CREATE|os.O_RDWR|os.O_APPEND, 0600)
}

func (s *Store) ensureSession(meta trace.Meta) (*session, error) {
	key := meta.BootID + "\x00" + meta.TraceSessionID
	if id, ok := s.byKey[key]; ok {
		return s.sessions[id], nil
	}
	id := idFor(meta.BootID, meta.TraceSessionID)
	if existing := s.sessions[id]; existing != nil {
		if existing.summary.BootID != meta.BootID || existing.summary.TraceID != meta.TraceSessionID {
			return nil, errors.New("session ID collision")
		}
		return nil, errors.New("trace session ID already ended")
	}
	dir := filepath.Join(s.root, time.Now().UTC().Format("2006-01-02"), id)
	if err := os.MkdirAll(dir, 0700); err != nil {
		return nil, err
	}
	entry := &session{summary: Summary{ID: id, BootID: meta.BootID, TraceID: meta.TraceSessionID, DeviceID: meta.DeviceID, Mode: meta.Mode, StartedAt: time.Now().UTC(), Live: true}, dir: dir}
	if err := s.openFiles(entry); err != nil {
		return nil, err
	}
	s.sessions[id] = entry
	s.byKey[key] = id
	if err := s.writeManifest(entry); err != nil {
		return nil, err
	}
	return entry, nil
}

func (s *Store) openFiles(entry *session) error {
	if entry.events != nil {
		return nil
	}
	files := []struct {
		name string
		dest **os.File
	}{{"events.jsonl", &entry.events}, {"payloads.dat", &entry.payloads}, {"pcm-uplink.dat", &entry.uplink}, {"pcm-playback.dat", &entry.playback}, {"pcm-input.dat", &entry.input}, {"pcm-aec_mic.dat", &entry.aecMic}, {"pcm-aec_ref.dat", &entry.aecRef}, {"pcm-aec_out.dat", &entry.aecOut}}
	for _, item := range files {
		file, err := openAppend(filepath.Join(entry.dir, item.name))
		if err != nil {
			entry.closeFiles()
			return err
		}
		*item.dest = file
	}
	return nil
}

func (entry *session) closeFiles() {
	for _, file := range []*os.File{entry.events, entry.payloads, entry.uplink, entry.playback, entry.input, entry.aecMic, entry.aecRef, entry.aecOut} {
		if file != nil {
			file.Close()
		}
	}
	entry.events, entry.payloads, entry.uplink, entry.playback, entry.input, entry.aecMic, entry.aecRef, entry.aecOut = nil, nil, nil, nil, nil, nil, nil, nil
}

func (s *Store) writeManifest(entry *session) error {
	data, err := json.MarshalIndent(entry.summary, "", "  ")
	if err != nil {
		return err
	}
	path := filepath.Join(entry.dir, "manifest.json")
	temp := path + ".tmp"
	if err := os.WriteFile(temp, append(data, '\n'), 0600); err != nil {
		return err
	}
	if err := os.Rename(temp, path); err != nil {
		os.Remove(temp)
		return err
	}
	return nil
}

func appendBytes(file *os.File, payload []byte) (int64, error) {
	offset, err := file.Seek(0, io.SeekEnd)
	if err != nil {
		return 0, err
	}
	_, err = file.Write(payload)
	return offset, err
}

func audioFilename(track string) string {
	switch track {
	case "uplink", "playback", "input", "aec_mic", "aec_ref", "aec_out":
		return "pcm-" + track + ".dat"
	default:
		return ""
	}
}

func (s *Store) Append(frame trace.Frame) (string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	entry, err := s.ensureSession(frame.Meta)
	if err != nil {
		return "", err
	}
	if entry.summary.Truncated {
		return entry.summary.ID, nil
	}
	event := Event{Kind: trace.KindName(frame.Kind), Meta: frame.Meta, RawMeta: frame.RawMeta, ReceivedAt: time.Now().UTC()}
	var payloadFile *os.File
	if frame.Kind == trace.PCMUplink || frame.Kind == trace.PCMPlayback || frame.Kind == trace.AECMic || frame.Kind == trace.AECRef || frame.Kind == trace.AECOut || frame.Kind == trace.PCMInput {
		switch frame.Kind {
		case trace.PCMUplink:
			payloadFile, event.AudioTrack = entry.uplink, "uplink"
		case trace.PCMPlayback:
			payloadFile, event.AudioTrack = entry.playback, "playback"
		case trace.PCMInput:
			payloadFile, event.AudioTrack = entry.input, "input"
		case trace.AECMic:
			payloadFile, event.AudioTrack = entry.aecMic, "aec_mic"
		case trace.AECRef:
			payloadFile, event.AudioTrack = entry.aecRef, "aec_ref"
		case trace.AECOut:
			payloadFile, event.AudioTrack = entry.aecOut, "aec_out"
		}
		event.AudioOffset, err = payloadFile.Seek(0, io.SeekEnd)
		event.AudioLength = len(frame.Payload)
	} else if len(frame.Payload) > 0 {
		payloadFile = entry.payloads
		event.PayloadOffset, err = payloadFile.Seek(0, io.SeekEnd)
		event.PayloadLength = len(frame.Payload)
	}
	if err != nil {
		return entry.summary.ID, err
	}
	encoded, err := json.Marshal(event)
	if err != nil {
		return entry.summary.ID, err
	}
	storedBytes := int64(len(frame.Payload) + len(encoded) + 1)
	if (s.maxSessionBytes > 0 && entry.summary.Bytes+storedBytes > s.maxSessionBytes) || (s.maxTotalBytes > 0 && s.totalBytes+storedBytes > s.maxTotalBytes) {
		entry.summary.Truncated = true
		if err := s.writeManifest(entry); err != nil {
			return entry.summary.ID, err
		}
		return entry.summary.ID, nil
	}
	if payloadFile != nil {
		if _, err := appendBytes(payloadFile, frame.Payload); err != nil {
			return entry.summary.ID, err
		}
	}
	if _, err := entry.events.Write(append(encoded, '\n')); err != nil {
		return entry.summary.ID, err
	}
	entry.summary.Events++
	entry.summary.Bytes += storedBytes
	s.totalBytes += storedBytes
	if frame.Kind == trace.TraceGap {
		entry.summary.Gaps++
	}
	if frame.Meta.RequestID != "" {
		entry.summary.RequestID = frame.Meta.RequestID
	}
	if frame.Meta.DeviceBootID != "" {
		entry.summary.DeviceBootID = frame.Meta.DeviceBootID
	}
	if frame.Meta.DeviceTraceID != "" {
		entry.summary.DeviceTraceID = frame.Meta.DeviceTraceID
	}
	if frame.Meta.Mode != "" {
		entry.summary.Mode = frame.Meta.Mode
	}
	if entry.summary.DeviceID == "" && frame.Meta.DeviceID != "" {
		entry.summary.DeviceID = frame.Meta.DeviceID
	}
	if frame.Kind == trace.SessionEnd {
		entry.summary.EndedAt = event.ReceivedAt
		entry.summary.Live = false
		entry.summary.Interrupted = false
		entry.closeFiles()
		delete(s.byKey, frame.Meta.BootID+"\x00"+frame.Meta.TraceSessionID)
	}
	if frame.Kind == trace.SessionOpen || frame.Kind == trace.SessionEnd || entry.summary.Events%100 == 0 {
		if err := s.writeManifest(entry); err != nil {
			return entry.summary.ID, err
		}
	}
	s.notify(entry.summary.ID)
	return entry.summary.ID, nil
}

func (s *Store) List() []Summary {
	s.mu.Lock()
	defer s.mu.Unlock()
	items := make([]Summary, 0, len(s.sessions))
	for _, entry := range s.sessions {
		items = append(items, entry.summary)
	}
	sort.Slice(items, func(i, j int) bool { return items[i].StartedAt.After(items[j].StartedAt) })
	return items
}

func (s *Store) notify(id string) {
	for ch := range s.subscribers {
		select {
		case ch <- id:
		default:
		}
	}
}

func (s *Store) SetDevice(status DeviceStatus) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.device = status
	if s.devices == nil {
		s.devices = make(map[string]DeviceStatus)
	}
	source := status.Source
	if source == "" {
		source = "collector"
	}
	s.devices[source] = status
	s.notify("status")
}

func (s *Store) Device() DeviceStatus {
	s.mu.Lock()
	defer s.mu.Unlock()
	status := s.devices["proxy"]
	if !status.Connected {
		status = s.devices["collector"]
	}
	status.ProxyConnected = s.devices["proxy"].Connected
	status.CollectorConnected = s.devices["collector"].Connected
	return status
}

func (s *Store) EndBoot(bootID string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, entry := range s.sessions {
		if entry.summary.BootID == bootID && entry.summary.Live {
			entry.summary.Live = false
			entry.summary.Interrupted = true
			entry.summary.EndedAt = time.Now().UTC()
			entry.closeFiles()
			delete(s.byKey, entry.summary.BootID+"\x00"+entry.summary.TraceID)
			if err := s.writeManifest(entry); err != nil {
				fmt.Fprintf(os.Stderr, "save manifest: %v\n", err)
			}
			s.notify(entry.summary.ID)
		}
	}
}

func (s *Store) Get(id string) (Summary, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	entry := s.sessions[id]
	if entry == nil {
		return Summary{}, false
	}
	return entry.summary, true
}

func (s *Store) sessionDir(id string) (string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	entry := s.sessions[id]
	if entry == nil {
		return "", os.ErrNotExist
	}
	return entry.dir, nil
}

func (s *Store) eachEvent(id string, visit func(Event) bool) error {
	dir, err := s.sessionDir(id)
	if err != nil {
		return err
	}
	file, err := os.Open(filepath.Join(dir, "events.jsonl"))
	if err != nil {
		return err
	}
	defer file.Close()
	reader := bufio.NewReaderSize(file, 64*1024)
	for {
		line, err := reader.ReadBytes('\n')
		if errors.Is(err, io.EOF) {
			return nil
		} // A live append may not have committed its final newline.
		if err != nil {
			return err
		}
		if len(line) > 64*1024 {
			return errors.New("event metadata exceeds index limit")
		}
		var event Event
		if err := json.Unmarshal(line, &event); err != nil {
			return fmt.Errorf("corrupt event index: %w", err)
		}
		if !visit(event) {
			return nil
		}
	}
}

func (s *Store) Events(id, after string, limit int) ([]Event, error) {
	if limit < 1 || limit > 500 {
		limit = 200
	}
	var afterSeq uint64
	if after != "" {
		value, err := strconv.ParseUint(after, 10, 64)
		if err != nil {
			return nil, err
		}
		afterSeq = value
	}
	events := make([]Event, 0, limit)
	err := s.eachEvent(id, func(event Event) bool {
		seq, err := strconv.ParseUint(event.Meta.Seq, 10, 64)
		if err != nil || (after != "" && seq <= afterSeq) {
			return true
		}
		events = append(events, event)
		return len(events) < limit
	})
	return events, err
}

func (s *Store) FindEvent(id, seq string) (Event, bool, error) {
	var found Event
	ok := false
	err := s.eachEvent(id, func(event Event) bool {
		if event.Meta.Seq == seq {
			found, ok = event, true
			return false
		}
		return true
	})
	return found, ok, err
}

func (s *Store) Payload(id string, event Event) ([]byte, error) {
	if event.Meta.PartCount > 1 {
		parts := make([]Event, event.Meta.PartCount)
		seen := make([]bool, event.Meta.PartCount)
		duplicate := false
		err := s.eachEvent(id, func(candidate Event) bool {
			meta := candidate.Meta
			if candidate.Kind == event.Kind && meta.MessageID == event.Meta.MessageID &&
				meta.ConnectionGeneration == event.Meta.ConnectionGeneration && meta.PartCount == event.Meta.PartCount &&
				meta.TotalLength == event.Meta.TotalLength &&
				meta.PartIndex >= 0 && meta.PartIndex < len(parts) {
				if seen[meta.PartIndex] {
					duplicate = true
					return false
				}
				parts[meta.PartIndex] = candidate
				seen[meta.PartIndex] = true
			}
			return true
		})
		if err != nil {
			return nil, err
		}
		if duplicate {
			return nil, errors.New("duplicate message fragment")
		}
		payload := make([]byte, 0, event.Meta.TotalLength)
		for i, part := range parts {
			if !seen[i] {
				return nil, errors.New("message fragments incomplete")
			}
			chunk, err := s.readPayload(id, part)
			if err != nil {
				return nil, err
			}
			payload = append(payload, chunk...)
			if len(payload) > event.Meta.TotalLength {
				return nil, errors.New("message fragments exceed declared length")
			}
		}
		if len(payload) != event.Meta.TotalLength {
			return nil, errors.New("message fragments have wrong length")
		}
		return payload, nil
	}
	return s.readPayload(id, event)
}

func (s *Store) readPayload(id string, event Event) ([]byte, error) {
	if event.PayloadLength <= 0 || event.PayloadLength > trace.MaxFrameSize {
		return nil, os.ErrNotExist
	}
	dir, err := s.sessionDir(id)
	if err != nil {
		return nil, err
	}
	file, err := os.Open(filepath.Join(dir, "payloads.dat"))
	if err != nil {
		return nil, err
	}
	defer file.Close()
	buf := make([]byte, event.PayloadLength)
	_, err = file.ReadAt(buf, event.PayloadOffset)
	return buf, err
}

func (s *Store) Segments(id, track string) ([]Segment, error) {
	if audioFilename(track) == "" {
		return nil, os.ErrNotExist
	}
	var segments []Segment
	boundary := false
	var nextSample uint64
	haveSample := false
	err := s.eachEvent(id, func(event Event) bool {
		if event.Kind == "trace_gap" || (track == "uplink" && event.Meta.EventType == "response.created") {
			boundary = true
		}
		if event.AudioTrack != track || event.AudioLength == 0 {
			return true
		}
		meta := event.Meta
		if meta.SampleIndex != "" {
			index, err := strconv.ParseUint(meta.SampleIndex, 10, 64)
			if err == nil {
				if haveSample && index != nextSample {
					boundary = true
				}
				nextSample = index + uint64(event.AudioLength/(meta.Channels*meta.BitsPerSample/8))
				haveSample = true
			}
		}
		if len(segments) > 0 {
			last := &segments[len(segments)-1]
			if !boundary && last.ResponseID == meta.ResponseID && last.Offset+last.Length == event.AudioOffset && last.SampleRate == meta.SampleRate && last.Channels == meta.Channels && last.BitsPerSample == meta.BitsPerSample {
				last.Length += int64(event.AudioLength)
				return true
			}
		}
		segments = append(segments, Segment{ResponseID: meta.ResponseID, RequestID: meta.RequestID, MonoUS: meta.MonoUS, ReceivedAt: event.ReceivedAt, GapBefore: boundary, Track: track, Index: len(segments), Offset: event.AudioOffset, Length: int64(event.AudioLength), SampleRate: meta.SampleRate, Channels: meta.Channels, BitsPerSample: meta.BitsPerSample})
		boundary = false
		return true
	})
	return segments, err
}

func (s *Store) OpenAudio(id, track string) (*os.File, error) {
	dir, err := s.sessionDir(id)
	if err != nil {
		return nil, err
	}
	if audioFilename(track) == "" {
		return nil, os.ErrNotExist
	}
	return os.Open(filepath.Join(dir, audioFilename(track)))
}

func (s *Store) Export(id string, output io.Writer) error {
	dir, err := s.sessionDir(id)
	if err != nil {
		return err
	}
	archive := zip.NewWriter(output)
	for _, name := range []string{"manifest.json", "events.jsonl", "payloads.dat", "pcm-uplink.dat", "pcm-playback.dat", "pcm-input.dat", "pcm-aec_mic.dat", "pcm-aec_ref.dat", "pcm-aec_out.dat"} {
		file, err := os.Open(filepath.Join(dir, name))
		if err != nil {
			if errors.Is(err, os.ErrNotExist) && (name == "pcm-input.dat" || name == "pcm-aec_mic.dat" || name == "pcm-aec_ref.dat" || name == "pcm-aec_out.dat") {
				continue
			}
			archive.Close()
			return err
		}
		part, err := archive.Create(name)
		if err == nil {
			_, err = io.Copy(part, file)
		}
		file.Close()
		if err != nil {
			archive.Close()
			return err
		}
	}
	return archive.Close()
}

func (s *Store) Subscribe() (<-chan string, func()) {
	ch := make(chan string, 16)
	s.mu.Lock()
	s.subscribers[ch] = struct{}{}
	s.mu.Unlock()
	return ch, func() {
		s.mu.Lock()
		delete(s.subscribers, ch)
		close(ch)
		s.mu.Unlock()
	}
}

func (s *Store) Close() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	var first error
	for _, entry := range s.sessions {
		if entry.summary.Live {
			entry.summary.Live = false
			entry.summary.Interrupted = true
			entry.summary.EndedAt = time.Now().UTC()
		}
		if err := s.writeManifest(entry); err != nil && first == nil {
			first = err
		}
		entry.closeFiles()
	}
	return first
}

// Related uses only explicit boot/session identity, never timing proximity.
func (s *Store) Related(id string) (Summary, bool) {
	source, ok := s.Get(id)
	if !ok {
		return Summary{}, false
	}
	for _, candidate := range s.List() {
		if source.DeviceBootID != "" && candidate.BootID == source.DeviceBootID && candidate.TraceID == source.DeviceTraceID {
			return candidate, true
		}
		if candidate.DeviceBootID == source.BootID && candidate.DeviceTraceID == source.TraceID {
			return candidate, true
		}
	}
	return Summary{}, false
}

func (s *Store) ExportBundle(id string, destination io.Writer) error {
	related, ok := s.Related(id)
	if !ok {
		return s.Export(id, destination)
	}
	archive := zip.NewWriter(destination)
	for _, sourceID := range []string{id, related.ID} {
		entry, err := archive.Create(sourceID + ".zip")
		if err != nil {
			return err
		}
		if err = s.Export(sourceID, entry); err != nil {
			return err
		}
	}
	return archive.Close()
}

func (s *Store) Conversation(id string) ([]Event, error) {
	events := make([]Event, 0)
	err := s.eachEvent(id, func(event Event) bool {
		if event.Meta.Transcript != "" {
			events = append(events, event)
		}
		return true
	})
	return events, err
}

func (s *Store) SetTotalLimit(bytes int64) { s.mu.Lock(); defer s.mu.Unlock(); s.maxTotalBytes = bytes }

// Pruning is opt-in. Callers choose the retention window explicitly.
func (s *Store) Prune(before time.Time) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	root, err := filepath.Abs(s.root)
	if err != nil {
		return err
	}
	for id, entry := range s.sessions {
		if entry.summary.Live || entry.summary.EndedAt.IsZero() || !entry.summary.EndedAt.Before(before) {
			continue
		}
		target, err := filepath.Abs(entry.dir)
		if err != nil {
			return err
		}
		relative, err := filepath.Rel(root, target)
		if err != nil || relative == "." || relative == ".." || strings.HasPrefix(relative, ".."+string(os.PathSeparator)) || filepath.Base(target) != id {
			return errors.New("unsafe retention path")
		}
		entry.closeFiles()
		if err := os.RemoveAll(target); err != nil {
			return err
		}
		s.totalBytes -= entry.summary.Bytes
		delete(s.sessions, id)
	}
	return nil
}
