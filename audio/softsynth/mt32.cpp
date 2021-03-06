/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "common/scummsys.h"
#include "common/system.h"

#ifdef USE_MT32EMU

#include "audio/softsynth/mt32/mt32emu.h"
#include "audio/softsynth/mt32/ROMInfo.h"

#include "audio/softsynth/emumidi.h"
#include "audio/musicplugin.h"
#include "audio/mpu401.h"

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/error.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"
#include "common/util.h"
#include "common/archive.h"
#include "common/textconsole.h"
#include "common/translation.h"

#include "graphics/fontman.h"
#include "graphics/surface.h"
#include "graphics/pixelformat.h"
#include "graphics/palette.h"
#include "graphics/font.h"

#include "gui/message.h"

namespace MT32Emu {

class ReportHandlerScummVM : public ReportHandler {
friend class Synth;

public:
	virtual ~ReportHandlerScummVM() {}

protected:

	// Callback for debug messages, in vprintf() format
	void printDebug(const char *fmt, va_list list) {
		debug(4, fmt, list);
	}

	// Callbacks for reporting various errors and information
	void onErrorControlROM() {
		GUI::MessageDialog dialog("MT32emu: Init Error - Missing or invalid Control ROM image", "OK");
		dialog.runModal();
		error("MT32emu: Init Error - Missing or invalid Control ROM image");
	}
	void onErrorPCMROM() {
		GUI::MessageDialog dialog("MT32emu: Init Error - Missing PCM ROM image", "OK");
		dialog.runModal();
		error("MT32emu: Init Error - Missing PCM ROM image");
	}
	void showLCDMessage(const char *message) {
		g_system->displayMessageOnOSD(message);
	}
	void onDeviceReset() {}
	void onDeviceReconfig() {}
	void onNewReverbMode(Bit8u /* mode */) {}
	void onNewReverbTime(Bit8u /* time */) {}
	void onNewReverbLevel(Bit8u /* level */) {}
	void onPartStateChanged(int /* partNum */, bool /* isActive */) {}
	void onPolyStateChanged(int /* partNum */) {}
	void onPartialStateChanged(int /* partialNum */, int /* oldPartialPhase */, int /* newPartialPhase */) {}
	void onProgramChanged(int /* partNum */, char * /* patchName */) {}
};

}	// end of namespace MT32Emu

class MidiChannel_MT32 : public MidiChannel_MPU401 {
	void effectLevel(byte value) { }
	void chorusLevel(byte value) { }
};

class MidiDriver_MT32 : public MidiDriver_Emulated {
private:
	MidiChannel_MT32 _midiChannels[16];
	uint16 _channelMask;
	MT32Emu::Synth *_synth;
	MT32Emu::ReportHandlerScummVM *_reportHandler;
	const MT32Emu::ROMImage *_controlROM, *_pcmROM;
	Common::File *_controlFile, *_pcmFile;
	void deleteMuntStructures();

	int _outputRate;

protected:
	void generateSamples(int16 *buf, int len);

public:
	bool _initializing;

	MidiDriver_MT32(Audio::Mixer *mixer);
	virtual ~MidiDriver_MT32();

	int open();
	void close();
	void send(uint32 b);
	void setPitchBendRange (byte channel, uint range);
	void sysEx(const byte *msg, uint16 length);

	uint32 property(int prop, uint32 param);
	MidiChannel *allocateChannel();
	MidiChannel *getPercussionChannel();

	// AudioStream API
	bool isStereo() const { return true; }
	int getRate() const { return _outputRate; }
};

////////////////////////////////////////
//
// MidiDriver_MT32
//
////////////////////////////////////////

MidiDriver_MT32::MidiDriver_MT32(Audio::Mixer *mixer) : MidiDriver_Emulated(mixer) {
	_channelMask = 0xFFFF; // Permit all 16 channels by default
	uint i;
	for (i = 0; i < ARRAYSIZE(_midiChannels); ++i) {
		_midiChannels[i].init(this, i);
	}
	_reportHandler = NULL;
	_synth = NULL;
	// A higher baseFreq reduces the length used in generateSamples(),
	// and means that the timer callback will be called more often.
	// That results in more accurate timing.
	_baseFreq = 10000;
	// Unfortunately bugs in the emulator cause inaccurate tuning
	// at rates other than 32KHz, thus we produce data at 32KHz and
	// rely on Mixer to convert.
	_outputRate = 32000; //_mixer->getOutputRate();
	_initializing = false;
}

MidiDriver_MT32::~MidiDriver_MT32() {
	deleteMuntStructures();
}

void MidiDriver_MT32::deleteMuntStructures() {
	delete _synth;
	_synth = NULL;
	delete _reportHandler;
	_reportHandler = NULL;

	if (_controlROM)
		MT32Emu::ROMImage::freeROMImage(_controlROM);
	_controlROM = NULL;
	if (_pcmROM)
		MT32Emu::ROMImage::freeROMImage(_pcmROM);
	_pcmROM = NULL;

	delete _controlFile;
	_controlFile = NULL;
	delete _pcmFile;
	_pcmFile = NULL;
}

int MidiDriver_MT32::open() {
	if (_isOpen)
		return MERR_ALREADY_OPEN;

	MidiDriver_Emulated::open();
	_reportHandler = new MT32Emu::ReportHandlerScummVM();
	_synth = new MT32Emu::Synth(_reportHandler);

	Graphics::PixelFormat screenFormat = g_system->getScreenFormat();

	if (screenFormat.bytesPerPixel == 1) {
		const byte dummy_palette[] = {
			0, 0, 0,		// background
			0, 171, 0,	// border, font
			171, 0, 0	// fill
		};

		g_system->getPaletteManager()->setPalette(dummy_palette, 0, 3);
	}

	_initializing = true;
	debug(4, _s("Initializing MT-32 Emulator"));
	_controlFile = new Common::File();
	if (!_controlFile->open("MT32_CONTROL.ROM") && !_controlFile->open("CM32L_CONTROL.ROM"))
		error("Error opening MT32_CONTROL.ROM / CM32L_CONTROL.ROM");
	_pcmFile = new Common::File();
	if (!_pcmFile->open("MT32_PCM.ROM") && !_pcmFile->open("CM32L_PCM.ROM"))
		error("Error opening MT32_PCM.ROM / CM32L_PCM.ROM");
	_controlROM = MT32Emu::ROMImage::makeROMImage(_controlFile);
	_pcmROM = MT32Emu::ROMImage::makeROMImage(_pcmFile);
	if (!_synth->open(*_controlROM, *_pcmROM))
		return MERR_DEVICE_NOT_AVAILABLE;

	double gain = (double)ConfMan.getInt("midi_gain") / 100.0;
	_synth->setOutputGain(1.0f * gain);
	_synth->setReverbOutputGain(0.68f * gain);

	_initializing = false;

	if (screenFormat.bytesPerPixel > 1)
		g_system->fillScreen(screenFormat.RGBToColor(0, 0, 0));
	else
		g_system->fillScreen(0);

	g_system->updateScreen();

	_mixer->playStream(Audio::Mixer::kSFXSoundType, &_mixerSoundHandle, this, -1, Audio::Mixer::kMaxChannelVolume, 0, DisposeAfterUse::NO, true);

	return 0;
}

void MidiDriver_MT32::send(uint32 b) {
	_synth->playMsg(b);
}

void MidiDriver_MT32::setPitchBendRange(byte channel, uint range) {
	if (range > 24) {
		warning("setPitchBendRange() called with range > 24: %d", range);
	}
	byte benderRangeSysex[9];
	benderRangeSysex[0] = 0x41; // Roland
	benderRangeSysex[1] = channel;
	benderRangeSysex[2] = 0x16; // MT-32
	benderRangeSysex[3] = 0x12; // Write
	benderRangeSysex[4] = 0x00;
	benderRangeSysex[5] = 0x00;
	benderRangeSysex[6] = 0x04;
	benderRangeSysex[7] = (byte)range;
	benderRangeSysex[8] = MT32Emu::Synth::calcSysexChecksum(&benderRangeSysex[4], 4, 0);
	sysEx(benderRangeSysex, 9);
}

void MidiDriver_MT32::sysEx(const byte *msg, uint16 length) {
	if (msg[0] == 0xf0) {
		_synth->playSysex(msg, length);
	} else {
		_synth->playSysexWithoutFraming(msg, length);
	}
}

void MidiDriver_MT32::close() {
	if (!_isOpen)
		return;
	_isOpen = false;

	// Detach the player callback handler
	setTimerCallback(NULL, NULL);
	// Detach the mixer callback handler
	_mixer->stopHandle(_mixerSoundHandle);

	_synth->close();
	deleteMuntStructures();
}

void MidiDriver_MT32::generateSamples(int16 *data, int len) {
	_synth->render(data, len);
}

uint32 MidiDriver_MT32::property(int prop, uint32 param) {
	switch (prop) {
	case PROP_CHANNEL_MASK:
		_channelMask = param & 0xFFFF;
		return 1;
	}

	return 0;
}

MidiChannel *MidiDriver_MT32::allocateChannel() {
	MidiChannel_MT32 *chan;
	uint i;

	for (i = 0; i < ARRAYSIZE(_midiChannels); ++i) {
		if (i == 9 || !(_channelMask & (1 << i)))
			continue;
		chan = &_midiChannels[i];
		if (chan->allocate()) {
			return chan;
		}
	}
	return NULL;
}

MidiChannel *MidiDriver_MT32::getPercussionChannel() {
	return &_midiChannels[9];
}

// This code should be used when calling the timer callback from the mixer thread is undesirable.
// Note that it results in less accurate timing.
#if 0
class MidiEvent_MT32 {
public:
	MidiEvent_MT32 *_next;
	uint32 _msg; // 0xFFFFFFFF indicates a sysex message
	byte *_data;
	uint32 _len;

	MidiEvent_MT32(uint32 msg, byte *data, uint32 len) {
		_msg = msg;
		if (len > 0) {
			_data = new byte[len];
			memcpy(_data, data, len);
		}
		_len = len;
		_next = NULL;
	}

	MidiEvent_MT32() {
		if (_len > 0)
			delete _data;
	}
};

class MidiDriver_ThreadedMT32 : public MidiDriver_MT32 {
private:
	OSystem::Mutex _eventMutex;
	MidiEvent_MT32 *_events;
	TimerManager::TimerProc _timer_proc;

	void pushMidiEvent(MidiEvent_MT32 *event);
	MidiEvent_MT32 *popMidiEvent();

protected:
	void send(uint32 b);
	void sysEx(const byte *msg, uint16 length);

public:
	MidiDriver_ThreadedMT32(Audio::Mixer *mixer);

	void onTimer();
	void close();
	void setTimerCallback(void *timer_param, TimerManager::TimerProc timer_proc);
};


MidiDriver_ThreadedMT32::MidiDriver_ThreadedMT32(Audio::Mixer *mixer) : MidiDriver_MT32(mixer) {
	_events = NULL;
	_timer_proc = NULL;
}

void MidiDriver_ThreadedMT32::close() {
	MidiDriver_MT32::close();
	while ((popMidiEvent() != NULL)) {
		// Just eat any leftover events
	}
}

void MidiDriver_ThreadedMT32::setTimerCallback(void *timer_param, TimerManager::TimerProc timer_proc) {
	if (!_timer_proc || !timer_proc) {
		if (_timer_proc)
			_vm->_timer->removeTimerProc(_timer_proc);
		_timer_proc = timer_proc;
		if (timer_proc)
			_vm->_timer->installTimerProc(timer_proc, getBaseTempo(), timer_param, "MT32tempo");
	}
}

void MidiDriver_ThreadedMT32::pushMidiEvent(MidiEvent_MT32 *event) {
	Common::StackLock lock(_eventMutex);
	if (_events == NULL) {
		_events = event;
	} else {
		MidiEvent_MT32 *last = _events;
		while (last->_next != NULL)
			last = last->_next;
		last->_next = event;
	}
}

MidiEvent_MT32 *MidiDriver_ThreadedMT32::popMidiEvent() {
	Common::StackLock lock(_eventMutex);
	MidiEvent_MT32 *event;
	event = _events;
	if (event != NULL)
		_events = event->_next;
	return event;
}

void MidiDriver_ThreadedMT32::send(uint32 b) {
	MidiEvent_MT32 *event = new MidiEvent_MT32(b, NULL, 0);
	pushMidiEvent(event);
}

void MidiDriver_ThreadedMT32::sysEx(const byte *msg, uint16 length) {
	MidiEvent_MT32 *event = new MidiEvent_MT32(0xFFFFFFFF, msg, length);
	pushMidiEvent(event);
}

void MidiDriver_ThreadedMT32::onTimer() {
	MidiEvent_MT32 *event;
	while ((event = popMidiEvent()) != NULL) {
		if (event->_msg == 0xFFFFFFFF) {
			MidiDriver_MT32::sysEx(event->_data, event->_len);
		} else {
			MidiDriver_MT32::send(event->_msg);
		}
		delete event;
	}
}
#endif


// Plugin interface

class MT32EmuMusicPlugin : public MusicPluginObject {
public:
	const char *getName() const {
		return _s("MT-32 Emulator");
	}

	const char *getId() const {
		return "mt32";
	}

	MusicDevices getDevices() const;
	bool checkDevice(MidiDriver::DeviceHandle) const;
	Common::Error createInstance(MidiDriver **mididriver, MidiDriver::DeviceHandle = 0) const;
};

MusicDevices MT32EmuMusicPlugin::getDevices() const {
	MusicDevices devices;
	devices.push_back(MusicDevice(this, "", MT_MT32));
	return devices;
}

bool MT32EmuMusicPlugin::checkDevice(MidiDriver::DeviceHandle) const {
	if (!((Common::File::exists("MT32_CONTROL.ROM") && Common::File::exists("MT32_PCM.ROM")) ||
		(Common::File::exists("CM32L_CONTROL.ROM") && Common::File::exists("CM32L_PCM.ROM")))) {
			warning("The MT-32 emulator requires one of the two following file sets (not bundled with ScummVM):\n Either 'MT32_CONTROL.ROM' and 'MT32_PCM.ROM' or 'CM32L_CONTROL.ROM' and 'CM32L_PCM.ROM'");
			return false;
	}

	return true;
}

Common::Error MT32EmuMusicPlugin::createInstance(MidiDriver **mididriver, MidiDriver::DeviceHandle) const {
	if (ConfMan.hasKey("extrapath"))
		SearchMan.addDirectory("extrapath", ConfMan.get("extrapath"));

	*mididriver = new MidiDriver_MT32(g_system->getMixer());

	return Common::kNoError;
}

//#if PLUGIN_ENABLED_DYNAMIC(MT32)
	//REGISTER_PLUGIN_DYNAMIC(MT32, PLUGIN_TYPE_MUSIC, MT32EmuMusicPlugin);
//#else
	REGISTER_PLUGIN_STATIC(MT32, PLUGIN_TYPE_MUSIC, MT32EmuMusicPlugin);
//#endif

#endif
