/* eos - A reimplementation of BioWare's Aurora engine
 * Copyright (c) 2010 Sven Hesse (DrMcCoy), Matthew Hoops (clone2727)
 *
 * The Infinity, Aurora, Odyssey and Eclipse engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and BioWare corp.
 *
 * This file is part of eos and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

/** @file aurora/aurorafile.h
 *  Utility class for handling special data structures found in BioWare's Aurora files.
 */

#ifndef AURORA_AURORAFILE_H
#define AURORA_AURORAFILE_H

#include <string>

namespace Common {
	class SeekableReadStream;
	class UString;
}

namespace Aurora {

/** Utility class for handling data found in Aurora files. */
class AuroraFile {
public:
	/** Convert a binary representation of an IEEE 754-1985 float into a system-useable format. */
	static float convertFloat(uint32 data);

	/** Convert a binary representation of an IEEE 754-1985 double into a system-useable format. */
	static double convertDouble(uint64 data);

	/** Read a float (IEEE 754-1985) */
	static float readFloat(Common::SeekableReadStream &stream);

	/** Read a double (IEEE 754-1985) */
	static double readDouble(Common::SeekableReadStream &stream);

	/** Clean up a path string for portable use. */
	static void cleanupPath(Common::UString &path);
};

/** Base class for most files found in games using BioWare's Aurora engine. */
class AuroraBase {
public:
	AuroraBase();

	void clear();

	/** Return the file's ID. */
	uint32 getID() const;

	/** Return the file's version. */
	uint32 getVersion() const;

protected:
	uint32 _id;      ///< The file's ID.
	uint32 _version; ///< The file's version.

	void readHeader(Common::SeekableReadStream &stream);
};

} // End of namespace Aurora

#endif // AURORA_AURORAFILE_H
