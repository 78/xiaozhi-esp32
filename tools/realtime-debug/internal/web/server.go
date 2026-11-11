package web

import (
	"embed"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"io/fs"
	"net/http"
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
		jsonResponse(w, map[string]any{"summary": summary, "uplink": uplink, "playback": playback})
		return
	}
	switch parts[1] {
	case "events":
		s.events(w, r, id, parts)
	case "audio":
		s.audio(w, r, id, parts)
	case "export":
		if len(parts) != 2 {
			fail(w, http.StatusNotFound, "unknown endpoint")
			return
		}
		w.Header().Set("Content-Type", "application/zip")
		w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=%q", "realtime-"+id+".zip"))
		if err := s.store.Export(id, w); err != nil {
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
	index, err := strconv.Atoi(strings.TrimSuffix(parts[3], ".wav"))
	if err != nil {
		fail(w, http.StatusBadRequest, "invalid audio segment")
		return
	}
	segments, err := s.store.Segments(id, track)
	if err != nil || index < 0 || index >= len(segments) {
		fail(w, http.StatusNotFound, "audio segment not found")
		return
	}
	segment := segments[index]
	if segment.Length < 0 || segment.Length > 0xffffffff-36 {
		fail(w, http.StatusRequestEntityTooLarge, "WAV segment too large")
		return
	}
	file, err := s.store.OpenAudio(id, track)
	if err != nil {
		fail(w, http.StatusInternalServerError, err.Error())
		return
	}
	defer file.Close()
	w.Header().Set("Content-Type", "audio/wav")
	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("Content-Length", strconv.FormatInt(segment.Length+44, 10))
	w.Header().Set("Content-Disposition", fmt.Sprintf("inline; filename=%q", track+"-"+strconv.Itoa(index)+".wav"))
	w.Write(wavHeader(segment))
	io.Copy(w, io.NewSectionReader(file, segment.Offset, segment.Length))
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
