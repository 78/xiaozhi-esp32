package store

import (
	"encoding/binary"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"strconv"
)

type AECTrackStats struct {
	Samples         uint64  `json:"samples"`
	Discontinuities uint64  `json:"discontinuities"`
	Clipped         uint64  `json:"clipped"`
	RMS             float64 `json:"rms"`
}

type AECReport struct {
	Complete             bool                     `json:"complete"`
	Gaps                 uint64                   `json:"gaps"`
	Truncated            bool                     `json:"truncated"`
	Interrupted          bool                     `json:"interrupted"`
	Tracks               map[string]AECTrackStats `json:"tracks"`
	OverallAttenuationDB *float64                 `json:"overall_attenuation_db,omitempty"`
	Note                 string                   `json:"note"`
}

func (s *Store) AnalyzeAEC(id string) (AECReport, error) {
	summary, ok := s.Get(id)
	if !ok {
		return AECReport{}, os.ErrNotExist
	}
	dir, err := s.sessionDir(id)
	if err != nil {
		return AECReport{}, err
	}
	type accumulator struct {
		stats AECTrackStats
		power float64
		next  uint64
		seen  bool
	}
	tracks := map[string]*accumulator{
		"aec_mic": {}, "aec_ref": {}, "aec_out": {},
	}
	files := make(map[string]*os.File)
	defer func() {
		for _, file := range files {
			file.Close()
		}
	}()
	for track := range tracks {
		file, err := os.Open(filepath.Join(dir, audioFilename(track)))
		if err != nil {
			if os.IsNotExist(err) {
				continue
			}
			return AECReport{}, err
		}
		files[track] = file
	}
	var analysisErr error
	err = s.eachEvent(id, func(event Event) bool {
		acc := tracks[event.AudioTrack]
		if acc == nil || event.AudioLength == 0 {
			return true
		}
		index, err := strconv.ParseUint(event.Meta.SampleIndex, 10, 64)
		if err != nil || event.AudioLength%2 != 0 || files[event.AudioTrack] == nil {
			analysisErr = fmt.Errorf("invalid %s audio event", event.AudioTrack)
			return false
		}
		if acc.seen && index != acc.next {
			acc.stats.Discontinuities++
		}
		acc.seen = true
		acc.next = index + uint64(event.AudioLength/2)
		buf := make([]byte, event.AudioLength)
		if _, err := files[event.AudioTrack].ReadAt(buf, event.AudioOffset); err != nil {
			analysisErr = err
			return false
		}
		for i := 0; i < len(buf); i += 2 {
			value := int16(binary.LittleEndian.Uint16(buf[i:]))
			if value == 32767 || value == -32768 {
				acc.stats.Clipped++
			}
			acc.power += float64(value) * float64(value)
		}
		acc.stats.Samples += uint64(len(buf) / 2)
		return true
	})
	if err != nil {
		return AECReport{}, err
	}
	if analysisErr != nil {
		return AECReport{}, analysisErr
	}
	report := AECReport{Gaps: summary.Gaps, Truncated: summary.Truncated, Interrupted: summary.Interrupted,
		Tracks: make(map[string]AECTrackStats),
		Note:   "Overall attenuation compares full-track energy. It is only an echo estimate for far-end-only recordings; it does not measure double-talk quality.",
	}
	report.Complete = !summary.Live && !summary.Interrupted && !summary.Truncated && summary.Gaps == 0
	for name, acc := range tracks {
		if acc.stats.Samples == 0 || acc.stats.Discontinuities > 0 {
			report.Complete = false
		}
		if acc.stats.Samples > 0 {
			acc.stats.RMS = math.Sqrt(acc.power / float64(acc.stats.Samples))
		}
		report.Tracks[name] = acc.stats
	}
	if tracks["aec_mic"].stats.Samples != tracks["aec_ref"].stats.Samples {
		report.Complete = false
	}
	if mic, out := tracks["aec_mic"].stats.Samples, tracks["aec_out"].stats.Samples; mic > out+1600 || out > mic+1600 {
		report.Complete = false
	}
	if report.Complete && tracks["aec_ref"].stats.RMS > 100 && tracks["aec_mic"].power > 0 && tracks["aec_out"].power > 0 {
		value := 10 * math.Log10((tracks["aec_mic"].power/float64(tracks["aec_mic"].stats.Samples))/
			(tracks["aec_out"].power/float64(tracks["aec_out"].stats.Samples)))
		report.OverallAttenuationDB = &value
	}
	return report, nil
}
