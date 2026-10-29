#include "MidiFile.hpp"
#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <map>
#include <deque>


namespace {

/** Big-endian reader over a byte buffer that reports running past the end instead of crashing. */
struct Reader {
	const std::vector<uint8_t>& bytes;
	size_t pos;
	size_t end;
	bool overrun = false;

	Reader(const std::vector<uint8_t>& bytes, size_t pos, size_t end) : bytes(bytes), pos(pos), end(end) {}

	bool atEnd() {
		return pos >= end;
	}
	uint8_t byte() {
		if (pos >= end) {
			overrun = true;
			return 0;
		}
		return bytes[pos++];
	}
	uint32_t uint(int count) {
		uint32_t v = 0;
		for (int i = 0; i < count; i++)
			v = (v << 8) | byte();
		return v;
	}
	/** MIDI variable-length quantity */
	uint32_t varLen() {
		uint32_t v = 0;
		for (int i = 0; i < 4; i++) {
			uint8_t b = byte();
			v = (v << 7) | (b & 0x7f);
			if (!(b & 0x80))
				break;
		}
		return v;
	}
	void skip(uint32_t count) {
		pos = (count > end - std::min(pos, end)) ? end : pos + count;
	}
};

} // namespace


bool readMidiFile(const std::string& path, MidiFileData& data, std::string& error) {
	data = MidiFileData();

	std::vector<uint8_t> bytes;
	FILE* f = std::fopen(path.c_str(), "rb");
	if (!f) {
		error = "Could not open the file.";
		return false;
	}
	uint8_t buffer[4096];
	size_t n;
	while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0)
		bytes.insert(bytes.end(), buffer, buffer + n);
	std::fclose(f);

	Reader r(bytes, 0, bytes.size());
	if (r.uint(4) != 0x4d546864) {  // "MThd"
		error = "This is not a standard MIDI file.";
		return false;
	}
	uint32_t headerLen = r.uint(4);
	size_t headerEnd = r.pos + headerLen;
	int format = r.uint(2);
	int trackCount = r.uint(2);
	int division = r.uint(2);
	r.pos = headerEnd;
	if (r.overrun || format > 1) {
		error = (format == 2) ? "MIDI files of format 2 (separate songs per track) aren't supported." : "The MIDI file header is damaged.";
		return false;
	}
	if (division & 0x8000) {
		error = "MIDI files timed in SMPTE frames aren't supported.";
		return false;
	}
	data.ppq = std::max(division, 1);

	bool haveTempo = false;
	// Part index for each (track, channel), and each track's name
	std::map<std::pair<int, int>, int> partIndex;
	for (int track = 0; track < trackCount && !r.atEnd(); track++) {
		uint32_t id = r.uint(4);
		uint32_t len = r.uint(4);
		if (r.overrun)
			break;
		size_t trackEnd = std::min(r.pos + len, bytes.size());
		if (id != 0x4d54726b) {  // not "MTrk": skip unknown chunks
			r.pos = trackEnd;
			track--;
			continue;
		}
		Reader t(bytes, r.pos, trackEnd);
		std::string trackName;
		long tick = 0;
		uint8_t status = 0;
		// Notes waiting for their note-off, per (channel, pitch), oldest first
		std::map<std::pair<int, int>, std::deque<size_t>> open;

		auto partFor = [&](int channel) {
			auto key = std::make_pair(track, channel);
			auto it = partIndex.find(key);
			if (it != partIndex.end())
				return it->second;
			int index = data.parts.size();
			partIndex[key] = index;
			std::string name = trackName.empty() ? ("Track " + std::to_string(track + 1)) : trackName;
			data.parts.push_back(name + " (ch " + std::to_string(channel + 1) + ")");
			return index;
		};

		while (!t.atEnd() && !t.overrun) {
			tick += t.varLen();
			uint8_t b = t.byte();
			if (b == 0xff) {
				// Meta event
				uint8_t type = t.byte();
				uint32_t metaLen = t.varLen();
				size_t metaStart = t.pos;
				if (type == 0x51 && metaLen == 3 && !haveTempo) {
					uint32_t usPerQuarter = t.uint(3);
					if (usPerQuarter > 0) {
						data.bpm = 60000000.0 / usPerQuarter;
						haveTempo = true;
					}
				}
				else if (type == 0x03 && trackName.empty()) {
					for (uint32_t i = 0; i < metaLen && !t.atEnd(); i++)
						trackName += (char) t.byte();
				}
				t.pos = metaStart;
				t.skip(metaLen);
				if (type == 0x2f)
					break;
				continue;
			}
			if (b == 0xf0 || b == 0xf7) {
				// SysEx
				t.skip(t.varLen());
				continue;
			}
			uint8_t data1;
			if (b & 0x80) {
				status = b;
				data1 = t.byte();
			}
			else {
				// Running status: this byte is the first data byte
				data1 = b;
			}
			if (!(status & 0x80))
				break;  // data with no status: give up on this track
			uint8_t kind = status & 0xf0;
			int channel = status & 0x0f;
			uint8_t data2 = (kind == 0xc0 || kind == 0xd0) ? 0 : t.byte();

			if (kind == 0x90 && data2 > 0) {
				MidiNote note;
				note.part = partFor(channel);
				note.pitch = data1 & 0x7f;
				note.velocity = data2 & 0x7f;
				note.start = tick;
				note.end = -1;
				open[std::make_pair(channel, note.pitch)].push_back(data.notes.size());
				data.notes.push_back(note);
			}
			else if (kind == 0x80 || (kind == 0x90 && data2 == 0)) {
				auto it = open.find(std::make_pair(channel, data1 & 0x7f));
				if (it != open.end() && !it->second.empty()) {
					data.notes[it->second.front()].end = tick;
					it->second.pop_front();
				}
			}
		}
		// Notes never released end with their track
		for (auto& entry : open)
			for (size_t i : entry.second)
				data.notes[i].end = tick;
		r.pos = trackEnd;
	}

	if (data.notes.empty()) {
		error = "The MIDI file has no notes.";
		return false;
	}
	return true;
}
