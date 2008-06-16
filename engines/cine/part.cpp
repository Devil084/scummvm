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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */

#include "common/endian.h"

#include "cine/cine.h"
#include "cine/unpack.h"
#include "cine/various.h"

namespace Cine {

uint16 numElementInPart;

PartBuffer *partBuffer;

void loadPart(const char *partName) {
	memset(partBuffer, 0, sizeof(PartBuffer) * NUM_MAX_PARTDATA);
	numElementInPart = 0;

	g_cine->_partFileHandle.close();

	checkDataDisk(-1);

	if (!g_cine->_partFileHandle.open(partName))
		error("loadPart(): Cannot open file %s", partName);

	setMouseCursor(MOUSE_CURSOR_DISK);

	numElementInPart = g_cine->_partFileHandle.readUint16BE();
	g_cine->_partFileHandle.readUint16BE(); // entry size

	if (currentPartName != partName)
		strcpy(currentPartName, partName);

	for (uint16 i = 0; i < numElementInPart; i++) {
		g_cine->_partFileHandle.read(partBuffer[i].partName, 14);
		partBuffer[i].offset = g_cine->_partFileHandle.readUint32BE();
		partBuffer[i].packedSize = g_cine->_partFileHandle.readUint32BE();
		partBuffer[i].unpackedSize = g_cine->_partFileHandle.readUint32BE();
		g_cine->_partFileHandle.readUint32BE(); // unused
	}

	if (g_cine->getGameType() == Cine::GType_FW && g_cine->getPlatform() == Common::kPlatformPC && strcmp(partName, "BASESON.SND") != 0)
		loadPal(partName);
}

void closePart(void) {
	// TODO
}

static Common::String fixVolCnfFileName(const uint8 *src, uint len) {
	assert(len == 11 || len == 13);
	// Copy source to a temporary buffer and force a trailing zero for string manipulation
	char tmp[14];
	memcpy(tmp, src, len);
	tmp[len] = 0;

	if (len == 11) {
		// Filenames of length 11 have no separation of the extension and the basename
		// so that's why we have to convert them first. There's no trailing zero in them
		// either and they're always of the full length 11 with padding spaces. Extension
		// can be always found at offset 8 onwards.
		// 
		// Examples of filename mappings:
		// "AEROPORTMSG" -> "AEROPORT.MSG"
		// "MITRAILLHP " -> "MITRAILL.HP" (Notice the trailing space after the extension)
		// "BOND10     " -> "BOND10"
		// "GIRL    SET" -> "GIRL.SET"

		// Replace all space characters with zeroes
		for (uint i = 0; i < len; i++)
			if (tmp[i] == ' ')
				tmp[i] = 0;
		// Extract the filename's extension
		Common::String extension(tmp + 8);
		tmp[8] = 0; // Force separation of extension and basename
		Common::String basename(tmp);
		if (extension.empty()) {
			return basename;
		} else {
			return basename + "." + extension;
		}
	} else {
		// Filenames of length 13 are okay as they are, no need for conversion
		return Common::String(tmp);
	}
}

void CineEngine::readVolCnf() {
	Common::File f;
	if (!f.open("vol.cnf")) {
		error("Unable to open 'vol.cnf'");
	}
	uint32 unpackedSize, packedSize;
	char hdr[8];
	f.read(hdr, 8);
	bool compressed = (memcmp(hdr, "ABASECP", 7) == 0);
	if (compressed) {
		unpackedSize = f.readUint32BE();
		packedSize = f.readUint32BE();
	} else {
		f.seek(0);
		unpackedSize = packedSize = f.size();
	}
	uint8 *buf = new uint8[unpackedSize];
	f.read(buf, packedSize);
	if (packedSize != unpackedSize) {
		CineUnpacker cineUnpacker;
		if (!cineUnpacker.unpack(buf, packedSize, buf, unpackedSize)) {
			error("Error while unpacking 'vol.cnf' data");
		}
	}
	uint8 *p = buf;
	int resourceFilesCount = READ_BE_UINT16(p); p += 2;
	int entrySize = READ_BE_UINT16(p); p += 2;
	for (int i = 0; i < resourceFilesCount; ++i) {
		char volumeResourceFile[9];
		memcpy(volumeResourceFile, p, 8);
		volumeResourceFile[8] = 0;
		_volumeResourceFiles.push_back(volumeResourceFile);
		p += entrySize;
	}

	// Check file name blocks' sizes
	bool fileNameLenMod11, fileNameLenMod13;
	fileNameLenMod11 = fileNameLenMod13 = true;
	for (int i = 0; i < resourceFilesCount; ++i) {
		int size = READ_BE_UINT32(p); p += 4;
		fileNameLenMod11 &= ((size % 11) == 0);
		fileNameLenMod13 &= ((size % 13) == 0);
		p += size;
	}
	// Make sure at least one of the candidates for file name length fits the data
	assert(fileNameLenMod11 || fileNameLenMod13);

	// File name length used to be deduced from the fact whether the file
	// was compressed or not. Compressed files used file name length 11,
	// uncompressed files used file name length 13. It worked almost always,
	// but not with the game entry that's detected as the Operation Stealth's
	// US Amiga release. It uses a compressed 'vol.cnf' file but still uses
	// file names of length 13. So we try to deduce the file name length from
	// the data in the 'vol.cnf' file.
	int fileNameLength; 
	if (fileNameLenMod11 != fileNameLenMod13) {
		// All file name blocks' sizes were divisible by either 11 or 13, but not with both.
		fileNameLength = (fileNameLenMod11 ? 11 : 13);
	} else {
		warning("Couldn't deduce file name length from data in 'vol.cnf', using a backup deduction scheme.");
		// Here we use the former file name length detection method
		// if we couldn't deduce the file name length from the data.
		fileNameLength = (compressed ? 11 : 13);
	}

	p = buf + 4 + resourceFilesCount * entrySize;
	for (int i = 0; i < resourceFilesCount; ++i) {
		int count = READ_BE_UINT32(p) / fileNameLength; p += 4;
		while (count--) {
			Common::String volumeEntryName = fixVolCnfFileName(p, fileNameLength);
			_volumeEntriesMap.setVal(volumeEntryName, _volumeResourceFiles[i].c_str());
			debugC(5, kCineDebugPart, "Added volume entry name '%s' resource file '%s'", volumeEntryName.c_str(), _volumeResourceFiles[i].c_str());
			p += fileNameLength;
		}
	}

	delete[] buf;
}

int16 findFileInBundle(const char *fileName) {
	if (g_cine->getGameType() == Cine::GType_OS) {
		// look first in currently loaded resource file
		for (int i = 0; i < numElementInPart; i++) {
			if (!scumm_stricmp(fileName, partBuffer[i].partName)) {
				return i;
			}
		}
		// not found, open the required resource file
		StringPtrHashMap::const_iterator it = g_cine->_volumeEntriesMap.find(fileName);
		if (it == g_cine->_volumeEntriesMap.end()) {
			warning("Unable to find part file for filename '%s'", fileName);
			return -1;
		}
		const char *part = (*it)._value;
		loadPart(part);
	}
	for (int i = 0; i < numElementInPart; i++) {
		if (!scumm_stricmp(fileName, partBuffer[i].partName)) {
			return i;
		}
	}
	return -1;
}

void readFromPart(int16 idx, byte *dataPtr) {
	setMouseCursor(MOUSE_CURSOR_DISK);

	g_cine->_partFileHandle.seek(partBuffer[idx].offset, SEEK_SET);
	g_cine->_partFileHandle.read(dataPtr, partBuffer[idx].packedSize);
}

byte *readBundleFile(int16 foundFileIdx) {
	assert(foundFileIdx >= 0 && foundFileIdx < numElementInPart);
	byte *dataPtr = (byte *)calloc(partBuffer[foundFileIdx].unpackedSize, 1);
	if (partBuffer[foundFileIdx].unpackedSize != partBuffer[foundFileIdx].packedSize) {
		byte *unpackBuffer = (byte *)malloc(partBuffer[foundFileIdx].packedSize);
		readFromPart(foundFileIdx, unpackBuffer);
		CineUnpacker cineUnpacker;
		if (!cineUnpacker.unpack(unpackBuffer, partBuffer[foundFileIdx].packedSize, dataPtr, partBuffer[foundFileIdx].unpackedSize)) {
			warning("Error unpacking '%s' from bundle file '%s'", partBuffer[foundFileIdx].partName, currentPartName);
		}
		free(unpackBuffer);
	} else {
		readFromPart(foundFileIdx, dataPtr);
	}

	return dataPtr;
}

byte *readBundleSoundFile(const char *entryName, uint32 *size) {
	int16 index;
	byte *data = 0;
	char previousPartName[15] = "";

	if (g_cine->getGameType() == Cine::GType_FW) {
		strcpy(previousPartName, currentPartName);
		loadPart("BASESON.SND");
	}
	index = findFileInBundle((const char *)entryName);
	if (index != -1) {
		data = readBundleFile(index);
		if (size) {
			*size = partBuffer[index].unpackedSize;
		}
	}
	if (g_cine->getGameType() == Cine::GType_FW) {
		loadPart(previousPartName);
	}
	return data;
}

byte *readFile(const char *filename) {
	Common::File in;

	in.open(filename);

	if (!in.isOpen())
		error("readFile(): Cannot open file %s", filename);

	uint32 size = in.size();

	byte *dataPtr = (byte *)malloc(size);
	in.read(dataPtr, size);

	return dataPtr;
}

void checkDataDisk(int16 param) {
}

void dumpBundle(const char *fileName) {
	char tmpPart[15];

	strcpy(tmpPart, currentPartName);

	loadPart(fileName);
	for (int i = 0; i < numElementInPart; i++) {
		byte *data = readBundleFile(i);

		debug(0, "%s", partBuffer[i].partName);

		Common::File out;
		if (out.open(Common::String("dumps/") + partBuffer[i].partName, Common::File::kFileWriteMode)) {
			out.write(data, partBuffer[i].unpackedSize);
			out.close();
		}

		free(data);
	}

	loadPart(tmpPart);
}

} // End of namespace Cine
