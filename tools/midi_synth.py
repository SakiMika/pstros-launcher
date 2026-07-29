#!/usr/bin/env python3
"""Small dependency-free Standard MIDI File scanner and PCM renderer.

The renderer is intentionally simple. It is used to pre-render J2ME MIDI
resources into signed 8-bit mono PCM that libnds can play without a realtime
MIDI synthesizer or a SoundFont on Nintendo DS.
"""

from __future__ import annotations

import math
import struct
import zlib
from array import array
from dataclasses import dataclass
from typing import Iterable


@dataclass(frozen=True)
class MidiBlob:
    data: bytes
    offset: int
    length: int


@dataclass
class NoteEvent:
    channel: int
    note: int
    velocity: int
    program: int
    volume: int
    start: float
    end: float


class MidiError(ValueError):
    pass


def _be16(data: bytes, pos: int) -> int:
    return struct.unpack_from(">H", data, pos)[0]


def _be32(data: bytes, pos: int) -> int:
    return struct.unpack_from(">I", data, pos)[0]


def _vlq(data: bytes, pos: int, end: int) -> tuple[int, int]:
    value = 0
    for _ in range(4):
        if pos >= end:
            raise MidiError("truncated variable-length value")
        byte = data[pos]
        pos += 1
        value = (value << 7) | (byte & 0x7F)
        if (byte & 0x80) == 0:
            return value, pos
    raise MidiError("invalid variable-length value")


def midi_file_length(data: bytes, start: int = 0) -> int:
    """Return the complete SMF byte length at *start*, or raise MidiError."""
    if start < 0 or start + 14 > len(data) or data[start : start + 4] != b"MThd":
        raise MidiError("missing MThd")
    header_size = _be32(data, start + 4)
    if header_size < 6 or start + 8 + header_size > len(data):
        raise MidiError("invalid MIDI header")
    track_count = _be16(data, start + 10)
    if track_count <= 0 or track_count > 256:
        raise MidiError("invalid MIDI track count")
    pos = start + 8 + header_size
    for _ in range(track_count):
        if pos + 8 > len(data) or data[pos : pos + 4] != b"MTrk":
            raise MidiError("missing MTrk")
        chunk_size = _be32(data, pos + 4)
        pos += 8
        if chunk_size > len(data) - pos:
            raise MidiError("truncated MTrk")
        pos += chunk_size
    return pos - start


def extract_midi_files(blob: bytes) -> list[MidiBlob]:
    """Find complete SMF files embedded anywhere in *blob*."""
    found: list[MidiBlob] = []
    pos = 0
    while True:
        pos = blob.find(b"MThd", pos)
        if pos < 0:
            break
        try:
            length = midi_file_length(blob, pos)
        except MidiError:
            pos += 4
            continue
        found.append(MidiBlob(blob[pos : pos + length], pos, length))
        pos += length
    return found


def _parse_track_events(track: bytes, track_index: int) -> list[tuple[int, int, str, tuple[int, ...]]]:
    events: list[tuple[int, int, str, tuple[int, ...]]] = []
    tick = 0
    pos = 0
    running = 0
    order = track_index * 1_000_000
    while pos < len(track):
        delta, pos = _vlq(track, pos, len(track))
        tick += delta
        if pos >= len(track):
            break
        first = track[pos]
        if first & 0x80:
            status = first
            pos += 1
            if status < 0xF0:
                running = status
        else:
            if running == 0:
                raise MidiError("running status without status byte")
            status = running

        order += 1
        if status == 0xFF:
            if pos >= len(track):
                raise MidiError("truncated meta event")
            meta_type = track[pos]
            pos += 1
            length, pos = _vlq(track, pos, len(track))
            if length > len(track) - pos:
                raise MidiError("truncated meta payload")
            payload = track[pos : pos + length]
            pos += length
            if meta_type == 0x51 and length == 3:
                tempo = (payload[0] << 16) | (payload[1] << 8) | payload[2]
                if tempo > 0:
                    events.append((tick, order, "tempo", (tempo,)))
            elif meta_type == 0x2F:
                break
            continue

        if status in (0xF0, 0xF7):
            length, pos = _vlq(track, pos, len(track))
            if length > len(track) - pos:
                raise MidiError("truncated SysEx")
            pos += length
            running = 0
            continue

        kind = status & 0xF0
        channel = status & 0x0F
        data_len = 1 if kind in (0xC0, 0xD0) else 2
        if pos + data_len > len(track):
            raise MidiError("truncated MIDI channel event")
        a = track[pos]
        b = track[pos + 1] if data_len == 2 else 0
        pos += data_len

        if kind == 0x80:
            events.append((tick, order, "off", (channel, a, b)))
        elif kind == 0x90:
            if b == 0:
                events.append((tick, order, "off", (channel, a, 0)))
            else:
                events.append((tick, order, "on", (channel, a, b)))
        elif kind == 0xB0:
            events.append((tick, order, "control", (channel, a, b)))
        elif kind == 0xC0:
            events.append((tick, order, "program", (channel, a)))
    return events


def parse_notes(midi: bytes) -> tuple[list[NoteEvent], float, int, int]:
    """Parse notes and return (notes, duration_seconds, format, ppq)."""
    total = midi_file_length(midi, 0)
    midi = midi[:total]
    header_size = _be32(midi, 4)
    fmt = _be16(midi, 8)
    track_count = _be16(midi, 10)
    division = _be16(midi, 12)
    if division & 0x8000:
        raise MidiError("SMPTE time division is not supported")
    ppq = division
    if ppq <= 0:
        raise MidiError("invalid PPQ")

    pos = 8 + header_size
    all_events: list[tuple[int, int, str, tuple[int, ...]]] = []
    for track_index in range(track_count):
        if midi[pos : pos + 4] != b"MTrk":
            raise MidiError("missing track")
        length = _be32(midi, pos + 4)
        start = pos + 8
        end = start + length
        all_events.extend(_parse_track_events(midi[start:end], track_index))
        pos = end

    # Tempo first at the same tick, then state changes, note-offs, note-ons.
    priority = {"tempo": 0, "program": 1, "control": 2, "off": 3, "on": 4}
    all_events.sort(key=lambda item: (item[0], priority.get(item[2], 9), item[1]))

    tempo_us = 500_000
    last_tick = 0
    now = 0.0
    programs = [0] * 16
    volumes = [100] * 16
    active: dict[tuple[int, int], tuple[float, int, int, int]] = {}
    notes: list[NoteEvent] = []

    for tick, _order, kind, args in all_events:
        if tick < last_tick:
            tick = last_tick
        now += ((tick - last_tick) * tempo_us) / (1_000_000.0 * ppq)
        last_tick = tick
        if kind == "tempo":
            tempo_us = args[0]
        elif kind == "program":
            channel, program = args
            programs[channel] = program & 0x7F
        elif kind == "control":
            channel, control, value = args
            if control == 7:
                volumes[channel] = value & 0x7F
            elif control in (120, 123):
                for key in [key for key in active if key[0] == channel]:
                    start, velocity, program, volume = active.pop(key)
                    notes.append(NoteEvent(channel, key[1], velocity, program, volume, start, max(now, start + 0.01)))
        elif kind == "on":
            channel, note, velocity = args
            key = (channel, note)
            if key in active:
                start, old_velocity, old_program, old_volume = active.pop(key)
                notes.append(NoteEvent(channel, note, old_velocity, old_program, old_volume, start, max(now, start + 0.01)))
            active[key] = (now, velocity, programs[channel], volumes[channel])
        elif kind == "off":
            channel, note, _velocity = args
            key = (channel, note)
            if key in active:
                start, velocity, program, volume = active.pop(key)
                notes.append(NoteEvent(channel, note, velocity, program, volume, start, max(now, start + 0.01)))

    close_time = now + 0.25
    for (channel, note), (start, velocity, program, volume) in active.items():
        notes.append(NoteEvent(channel, note, velocity, program, volume, start, max(close_time, start + 0.05)))
    duration = max([note.end for note in notes] + [now]) + 0.20
    return notes, duration, fmt, ppq


def _wave(program: int, phase: float) -> float:
    two_pi = math.tau
    p = phase % two_pi
    if program < 8:  # piano
        return 0.68 * math.sin(p) + 0.22 * math.sin(2 * p) + 0.10 * math.sin(3 * p)
    if program < 16:  # chromatic percussion
        return 0.75 * math.sin(p) + 0.25 * math.sin(4 * p)
    if program < 24:  # organ
        return 0.60 * math.sin(p) + 0.25 * math.sin(2 * p) + 0.15 * math.sin(4 * p)
    if program < 32:  # guitar
        return (2.0 / math.pi) * math.asin(math.sin(p))
    if program < 40:  # bass
        return 0.65 * math.sin(p) + 0.35 * (1.0 if p < math.pi else -1.0)
    if program < 56:  # strings / ensemble
        return 2.0 * (p / two_pi) - 1.0
    if program < 64:  # brass
        return 0.7 * (1.0 if p < math.pi else -1.0) + 0.3 * math.sin(p)
    if program < 80:  # reed / pipe
        return 0.8 * math.sin(p) + 0.2 * math.sin(3 * p)
    if program < 96:  # synth lead / pad
        return 0.55 * (2.0 * (p / two_pi) - 1.0) + 0.45 * math.sin(p)
    return math.sin(p)


def _render_drum(mix: array, start: int, end: int, note: int, velocity: int, sample_rate: int) -> None:
    if end <= start:
        return
    length = end - start
    amp = (velocity / 127.0) * 0.48
    seed = (note * 1103515245 + start * 12345 + 0x1234567) & 0x7FFFFFFF
    base = 50.0 if note in (35, 36) else 180.0 if note in (38, 40) else 320.0
    for i in range(length):
        t = i / sample_rate
        decay = math.exp(-t * (11.0 if note in (35, 36) else 18.0))
        seed = (1103515245 * seed + 12345) & 0x7FFFFFFF
        noise = ((seed >> 8) / 8388607.5) - 1.0
        if note in (35, 36):
            value = math.sin(math.tau * (base - min(25.0, t * 160.0)) * t) * 0.85 + noise * 0.15
        elif note in (38, 40):
            value = noise * 0.72 + math.sin(math.tau * base * t) * 0.28
        else:
            value = noise
        mix[start + i] += value * decay * amp


def render_midi_to_pcm8(midi: bytes, sample_rate: int = 11025, max_seconds: float = 90.0) -> tuple[bytes, dict[str, object]]:
    """Render an SMF to signed 8-bit mono PCM using a tiny GM-like synth."""
    notes, duration, fmt, ppq = parse_notes(midi)
    truncated = duration > max_seconds
    duration = min(duration, max_seconds)
    sample_count = max(1, int(duration * sample_rate))
    mix = array("f", [0.0]) * sample_count

    rendered_notes = 0
    for item in notes:
        if item.start >= duration:
            continue
        start = max(0, int(item.start * sample_rate))
        end_time = min(duration, item.end + (0.12 if item.channel != 9 else 0.03))
        end = min(sample_count, max(start + 1, int(end_time * sample_rate)))
        if item.channel == 9:
            _render_drum(mix, start, end, item.note, item.velocity, sample_rate)
            rendered_notes += 1
            continue

        frequency = 440.0 * (2.0 ** ((item.note - 69) / 12.0))
        if frequency > sample_rate * 0.45:
            frequency = sample_rate * 0.45
        phase_step = math.tau * frequency / sample_rate
        phase = 0.0
        note_length = max(0.01, item.end - item.start)
        attack = 0.008 if item.program < 40 else 0.025
        release = 0.10 if item.program < 40 else 0.20
        amp = (item.velocity / 127.0) * (item.volume / 127.0) * 0.34
        percussive = item.program < 16 or 24 <= item.program < 40
        for index in range(start, end):
            t = (index - start) / sample_rate
            if t < attack:
                envelope = t / attack
            elif t <= note_length:
                envelope = math.exp(-t * (1.8 if percussive else 0.18))
            else:
                envelope = max(0.0, 1.0 - ((t - note_length) / release))
                if percussive:
                    envelope *= math.exp(-note_length * 1.8)
            mix[index] += _wave(item.program, phase) * envelope * amp
            phase += phase_step
        rendered_notes += 1

    peak = max((abs(value) for value in mix), default=0.0)
    scale = 1.0 if peak <= 0.00001 else min(1.0, 0.94 / peak)
    output = bytearray(sample_count)
    for index, value in enumerate(mix):
        # Mild soft clipping prevents dense chords from becoming harsh.
        value = math.tanh(value * scale * 1.35) / math.tanh(1.35)
        signed = int(round(max(-1.0, min(1.0, value)) * 127.0))
        output[index] = signed & 0xFF

    info: dict[str, object] = {
        "format": fmt,
        "ppq": ppq,
        "notes": rendered_notes,
        "duration_seconds": round(duration, 3),
        "sample_rate": sample_rate,
        "pcm_bytes": len(output),
        "truncated": truncated,
        "crc32": zlib.crc32(midi) & 0xFFFFFFFF,
    }
    return bytes(output), info


def unique_midi_blobs(items: Iterable[tuple[str, MidiBlob]]) -> list[tuple[str, MidiBlob]]:
    seen: set[tuple[int, int]] = set()
    result: list[tuple[str, MidiBlob]] = []
    for source, blob in items:
        key = (len(blob.data), zlib.crc32(blob.data) & 0xFFFFFFFF)
        if key in seen:
            continue
        seen.add(key)
        result.append((source, blob))
    return result
