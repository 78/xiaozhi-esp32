package web

import (
	"bufio"
	"embed"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/jarviszhe/xiaozhi-realtime/tools/realtime-debug/internal/store"
)

//go:embed static/*
var staticFiles embed.FS

type Server struct {
	store *store.Store
}

func New(data *store.Store) http.Handler {
	server := &Server{store: data}
	mux := http.NewServeMux()
	mux.HandleFunc("/api/sessions", server.sessions)
	mux.HandleFunc("/api/sessions/", server.session)
	mux.HandleFunc("/api/status", server.status)
	mux.HandleFunc("/api/live", server.live)
	static, _ := fs.Sub(staticFiles, "static")
	mux.Handle("/", http.FileServer(http.FS(static)))
	return mux
}

func jsonResponse(w http.ResponseWriter, value any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store")
	json.NewEncoder(w).Encode(value)
}

func fail(w http.ResponseWriter, code int, message string) {
	http.Error(w, message, code)
}

func (s *Server) sessions(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		fail(w, http.StatusMethodNotAllowed, "GET required")
		return
	}
	jsonResponse(w, s.store.List())
}

func (s *Server) status(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		fail(w, http.StatusMethodNotAllowed, "GET required")
		return
	}
	jsonResponse(w, s.store.Device())
}

func (s *Server) session(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		fail(w, http.StatusMethodNotAllowed, "GET required")
		return
	}
	parts := strings.Split(strings.TrimPrefix(r.URL.Path, "/api/sessions/"), "/")
	if len(parts) == 0 || parts[0] == "" {
		fail(w, http.StatusNotFound, "session not found")
		return
	}
	id := parts[0]
	summary, found := s.store.Get(id)
	if !found {
		fail(w, http.StatusNotFound, "session not found")
		return
	}
	if len(parts) == 1 {
		uplink, err := s.store.Segments(id, "uplink")
		if err != nil {
			fail(w, http.StatusInternalServerError, err.Error())
			return
		}
		playback, err := s.store.Segments(id, "playback")
		if err != nil {
			fail(w, http.StatusInternalServerError, err.Error())
			return
		}
		inputID := id
		related, hasRelated := s.store.Related(id)
		if summary.Mode == "proxy" && hasRelated {
			inputID = related.ID
		}
		input, err := s.store.Segments(inputID, "input")
		if err != nil {
			fail(w, http.StatusInternalServerError, err.Error())
			return
		}
		mic, err := s.store.Segments(id, "aec_mic")
		if err != nil {
			fail(w, http.StatusInternalServerError, err.Error())
			return
		}
		ref, err := s.store.Segments(id, "aec_ref")
		if err != nil {
			fail(w, http.StatusInternalServerError, err.Error())
			return
		}
		out, err := s.store.Segments(id, "aec_out")
		if err != nil {
			fail(w, http.StatusInternalServerError, err.Error())
			return
		}
		conversation, err := s.store.Conversation(id)
		if err != nil {
			fail(w, http.StatusInternalServerError, err.Error())
			return
		}
		jsonResponse(w, map[string]any{"conversation": conversation, "summary": summary, "uplink": uplink, "playback": playback, "input": input, "input_session_id": inputID, "related": related, "aec_mic": mic, "aec_ref": ref, "aec_out": out})
		return
	}
	switch parts[1] {
	case "events":
		s.events(w, r, id, parts)
	case "waveform":
		s.waveform(w, r, id, parts)
	case "audio":
		s.audio(w, r, id, parts)
	case "aec-report":
		if len(parts) != 2 {
			fail(w, http.StatusNotFound, "unknown endpoint")
			return
		}
		report, err := s.store.AnalyzeAEC(id)
		if err != nil {
			fail(w, http.StatusInternalServerError, err.Error())
			return
		}
		jsonResponse(w, report)
	case "export":
		if len(parts) != 2 {
			fail(w, http.StatusNotFound, "unknown endpoint")
			return
		}
		w.Header().Set("Content-Type", "application/zip")
		w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=%q", "realtime-"+id+".zip"))
		if err := s.store.ExportBundle(id, w); err != nil {
			return
		}
	default:
		fail(w, http.StatusNotFound, "unknown endpoint")
	}
}

func (s *Server) events(w http.ResponseWriter, r *http.Request, id string, parts []string) {
	if len(parts) == 2 {
		limit, _ := strconv.Atoi(r.URL.Query().Get("limit"))
		page, err := s.store.Events(id, r.URL.Query().Get("after"), limit)
		if err != nil {
			fail(w, http.StatusBadRequest, err.Error())
			return
		}
		jsonResponse(w, page)
		return
	}
	if len(parts) != 4 || parts[2] != "payload" {
		fail(w, http.StatusNotFound, "unknown endpoint")
		return
	}
	event, found, err := s.store.FindEvent(id, parts[3])
	if err != nil {
		fail(w, http.StatusInternalServerError, err.Error())
		return
	}
	if !found {
		fail(w, http.StatusNotFound, "event not found")
		return
	}
	payload, err := s.store.Payload(id, event)
	if err != nil {
		fail(w, http.StatusNotFound, "no raw payload")
		return
	}
	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.Header().Set("Content-Type", "application/octet-stream")
	w.Write(payload)
}

func (s *Server) audio(w http.ResponseWriter, r *http.Request, id string, parts []string) {
	if len(parts) != 4 || !strings.HasSuffix(parts[3], ".wav") {
		fail(w, http.StatusNotFound, "audio segment not found")
		return
	}
	track := parts[2]
	sourceTrack := track
	channel := -1
	if track == "input-left" {
		sourceTrack = "input"
		channel = 0
	} else if track == "input-right" {
		sourceTrack = "input"
		channel = 1
	}
	index, err := strconv.Atoi(strings.TrimSuffix(parts[3], ".wav"))
	if err != nil {
		fail(w, http.StatusBadRequest, "invalid audio segment")
		return
	}
	segments, err := s.store.Segments(id, sourceTrack)
	if err != nil || index < 0 || index >= len(segments) {
		fail(w, http.StatusNotFound, "audio segment not found")
		return
	}
	segment := segments[index]
	if channel >= segment.Channels {
		fail(w, http.StatusNotFound, "audio channel not found")
		return
	}
	if segment.Length < 0 || segment.Length > 0xffffffff-36 {
		fail(w, http.StatusRequestEntityTooLarge, "WAV segment too large")
		return
	}
	file, err := s.store.OpenAudio(id, sourceTrack)
	if err != nil {
		fail(w, http.StatusInternalServerError, err.Error())
		return
	}
	defer file.Close()
	channelAudio := channel >= 0 && segment.Channels > 1
	if channelAudio {
		segment.Channels = 1
		segment.Length /= int64(segments[index].Channels)
	}
	w.Header().Set("Content-Type", "audio/wav")
	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("Content-Length", strconv.FormatInt(segment.Length+44, 10))
	filename := track + "-" + strconv.Itoa(index)
	if channelAudio {
		if channel == 0 {
			filename += "-left"
		} else {
			filename += "-right"
		}
	}
	w.Header().Set("Content-Disposition", fmt.Sprintf("inline; filename=%q", filename+".wav"))
	w.Write(wavHeader(segment))
	if channelAudio {
		if err := copyChannel(w, file, segments[index], channel); err != nil {
			return
		}
		return
	}
	io.Copy(w, io.NewSectionReader(file, segment.Offset, segment.Length))
}

func copyChannel(dst io.Writer, file *os.File, segment store.Segment, channel int) error {
	if segment.Channels < 2 || segment.BitsPerSample%8 != 0 {
		return errors.New("unsupported channel layout")
	}
	if channel < 0 || channel >= segment.Channels {
		return errors.New("invalid channel")
	}
	sampleBytes := segment.BitsPerSample / 8
	frameBytes := segment.Channels * sampleBytes
	if frameBytes <= 0 || segment.Length%int64(frameBytes) != 0 {
		return errors.New("unaligned PCM segment")
	}
	const bufferSize = 8192
	readSize := bufferSize - bufferSize%frameBytes
	input := make([]byte, readSize)
	output := make([]byte, readSize/frameBytes*sampleBytes)
	reader := io.NewSectionReader(file, segment.Offset, segment.Length)
	remaining := segment.Length
	for remaining > 0 {
		count := int64(len(input))
		if count > remaining {
			count = remaining
		}
		count -= count % int64(frameBytes)
		if count == 0 {
			return errors.New("unaligned PCM read")
		}
		n, err := io.ReadFull(reader, input[:count])
		if err != nil {
			return err
		}
		frames := n / frameBytes
		for i := 0; i < frames; i++ {
			inputOffset := i*frameBytes + channel*sampleBytes
			copy(output[i*sampleBytes:(i+1)*sampleBytes], input[inputOffset:inputOffset+sampleBytes])
		}
		if _, err := dst.Write(output[:frames*sampleBytes]); err != nil {
			return err
		}
		remaining -= int64(n)
	}
	return nil
}

func wavHeader(segment store.Segment) []byte {
	header := make([]byte, 44)
	copy(header[:4], "RIFF")
	binary.LittleEndian.PutUint32(header[4:8], uint32(segment.Length+36))
	copy(header[8:12], "WAVE")
	copy(header[12:16], "fmt ")
	binary.LittleEndian.PutUint32(header[16:20], 16)
	binary.LittleEndian.PutUint16(header[20:22], 1)
	binary.LittleEndian.PutUint16(header[22:24], uint16(segment.Channels))
	binary.LittleEndian.PutUint32(header[24:28], uint32(segment.SampleRate))
	bytesPerSample := segment.Channels * segment.BitsPerSample / 8
	binary.LittleEndian.PutUint32(header[28:32], uint32(segment.SampleRate*bytesPerSample))
	binary.LittleEndian.PutUint16(header[32:34], uint16(bytesPerSample))
	binary.LittleEndian.PutUint16(header[34:36], uint16(segment.BitsPerSample))
	copy(header[36:40], "data")
	binary.LittleEndian.PutUint32(header[40:44], uint32(segment.Length))
	return header
}

func (s *Server) live(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		fail(w, http.StatusMethodNotAllowed, "GET required")
		return
	}
	flusher, ok := w.(http.Flusher)
	if !ok {
		fail(w, http.StatusInternalServerError, "streaming unavailable")
		return
	}
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("X-Accel-Buffering", "no")
	updates, cancel := s.store.Subscribe()
	defer cancel()
	keepAlive := time.NewTicker(20 * time.Second)
	defer keepAlive.Stop()
	fmt.Fprint(w, ": connected\n\n")
	flusher.Flush()
	for {
		select {
		case <-r.Context().Done():
			return
		case id := <-updates:
			fmt.Fprintf(w, "event: update\ndata: %s\n\n", id)
			flusher.Flush()
		case <-keepAlive.C:
			fmt.Fprint(w, ": keepalive\n\n")
			flusher.Flush()
		}
	}
}

// Waveform computes at most 600 extrema buckets; the browser never loads a full PCM file.
func (s *Server) waveform(w http.ResponseWriter, r *http.Request, id string, parts []string) {
	if len(parts) != 4 {
		fail(w, 404, "waveform not found")
		return
	}
	track := parts[2]
	index, err := strconv.Atoi(parts[3])
	segments, segErr := s.store.Segments(id, track)
	if err != nil || segErr != nil || index < 0 || index >= len(segments) {
		fail(w, 404, "segment not found")
		return
	}
	segment := segments[index]
	if segment.BitsPerSample != 16 {
		fail(w, 400, "waveform requires PCM16")
		return
	}
	file, err := s.store.OpenAudio(id, track)
	if err != nil {
		fail(w, 500, "audio unavailable")
		return
	}
	defer file.Close()
	samples := segment.Length / 2
	bucket := (samples + 599) / 600
	if bucket < 1 {
		bucket = 1
	}
	reader := bufio.NewReaderSize(io.NewSectionReader(file, segment.Offset, segment.Length), 32768)
	peaks := make([][2]float64, 0, 600)
	var pair [2]byte
	var low, high int16
	var count int64
	for i := int64(0); i < samples; i++ {
		if _, err := io.ReadFull(reader, pair[:]); err != nil {
			fail(w, 500, "audio read failed")
			return
		}
		sample := int16(binary.LittleEndian.Uint16(pair[:]))
		if count == 0 || sample < low {
			low = sample
		}
		if count == 0 || sample > high {
			high = sample
		}
		count++
		if count == bucket || i == samples-1 {
			peaks = append(peaks, [2]float64{float64(low) / 32768, float64(high) / 32768})
			count = 0
		}
	}
	jsonResponse(w, map[string]any{"peaks": peaks, "duration_seconds": float64(samples) / float64(segment.Channels*segment.SampleRate)})
}
