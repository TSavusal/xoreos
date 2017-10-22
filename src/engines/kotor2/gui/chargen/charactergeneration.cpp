/* xoreos - A reimplementation of BioWare's Aurora engine
 *
 * xoreos is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * xoreos is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * xoreos is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xoreos. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  The KotOR 2 character generation.
 */

#include "src/aurora/talkman.h"

#include "src/engines/kotor2/gui/widgets/label.h"
#include "src/engines/kotor2/gui/chargen/charactergeneration.h"

namespace Engines {

namespace KotOR2 {

CharacterGeneration::CharacterGeneration(Module *module, CharacterGenerationInfo *info, Engines::Console *console) :
		GUI(console), _module(module), _chargenInfo(info) {
	load("maincg_p");

	switch (_chargenInfo->getClass()) {
		case kClassJediConsular:
			getLabel("LBL_CLASS")->setText(TalkMan.getString(354));
			break;
		case kClassJediGuardian:
			getLabel("LBL_CLASS")->setText(TalkMan.getString(355));
			break;
		case kClassJediSentinel:
			getLabel("LBL_CLASS")->setText(TalkMan.getString(353));
			break;
		default:
			getLabel("LBL_CLASS")->setText("");
	}

	getLabel("LBL_NAME")->setText("");
	getLabel("LBL_LEVEL_VAL")->setText("");
}

} // End of namespace KotOR2

} // End of namespace Engines
